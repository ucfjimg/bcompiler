#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"

#define INTCONST (-1)

enum storclas {
    NEW,
    EXTERN,
    AUTO,
    INTERNAL,
};

struct codenode;

struct stabent {
    struct stabent *next;           // next in chain
    char name[MAXNAM + 1];          // symbol name
    enum storclas sc;               // storage class
    struct codenode *def;           // if extrn and we have a definition, or label: loc
    int offset;                     // offset on stack
};

struct constant {
    int strlen;
    union {
        char *strcon;
        unsigned intcon;
    } v;
};

struct ival {
    int isconst;
    union {
        struct constant con;
        struct stabent *name;
    } v; 
};

enum codeop {
    ONAMDEF,
    OIVAL,
    OZERO,
    OJMP,
    OPSHCON,
    OPSHSYM,
};

struct codenode {
    struct codenode *next;
    enum codeop op;
    union {
        char name[MAXNAM+1];
        struct ival ival;
        unsigned rept;
        struct stabent *target;
        struct constant *con;
        struct stabent *sym;
    } arg;
};

struct codefrag {
    struct codenode *head, *tail;
};

enum vtype {
    LVALUE,
    RVALUE,
};

static char *srcfn;
static int errf = 0;
static struct token *curtok;
static struct stabent *global;
static struct stabent *local;
static struct token savtok;

static void program(struct codefrag *prog);
static void definition(struct codefrag *prog);
static void funcdef(struct codefrag *prog);
static void funcparms(void);
static void statement(struct codefrag *prog);
static void stmtlabel(struct codefrag *prog, int line, const char *nm);
static void datadef(struct codefrag *prog);
static void nextok(void);

static void rvalue(struct codefrag *prog);

static struct stabent *stabget(struct stabent **root, const char *name);

static struct codenode *cnalloc(void);
static void cnpush(struct codefrag *frag, struct codenode *node);
static void cfprint(struct codefrag *frag);

static void err(int line, const char *fmt, ...);

int 
main(int argc, char **argv)
{
    FILE *fp;
    struct codefrag prog = { NULL, NULL };
    
    if (argc != 2) {
        fprintf(stderr, "b: source-file\n");
        return 1;
    }

    if (strcmp(srcfn = argv[1], "-") == 0) {
        srcfn = "<stdin>";
        fp = stdin;
    } else if ((fp = fopen(srcfn, "r")) == NULL) {
        perror(srcfn);
        return 1;
    }
    
    lexinit(fp);
    nextok();
    program(&prog);
    cfprint(&prog);
}

// Parse the whole program
// 
void 
program(struct codefrag *prog)
{
    int ch;

    while (!errf && curtok->type != TEOF) {
        definition(prog);
    }
}

// Parse a definition
//
void
definition(struct codefrag *prog)
{
    struct codenode *cn = cnalloc();
    struct stabent *sym;

    if (curtok->type != TNAME) {
        err(curtok->line, "name expected");
        nextok();
        return;
    }

    cn->op = ONAMDEF;
    strcpy(cn->arg.name, curtok->val.name);
    cnpush(prog, cn);

    sym = stabget(&global, cn->arg.name);
    if (sym->sc == NEW) {
        sym->sc = EXTERN;
        sym->def = cn;
    } else {
        err(curtok->line, "'%s' is previously defined", cn->arg.name);
    }

    nextok();

    if (curtok->type == TLPAREN) {
        nextok();
        funcdef(prog);
    } else {
        datadef(prog);
    }
}

// parse a function definition
//
void 
funcdef(struct codefrag *prog)
{
    local = NULL;

    if (curtok->type == TRPAREN) {
        nextok();
    } else {
        funcparms();
    }

    statement(prog);
}

// parse function parameters
//
void
funcparms(void)
{
    const char *nm;
    struct stabent *sym;
    int nextarg = 0;

    for(;;) {
        if (curtok->type != TNAME) {
            nextok();
            err(curtok->line, "name expected");
            return;
        }

        sym = stabget(&local, curtok->val.name);

        nextok();
        
        if (sym->sc == NEW) {
            sym->sc = AUTO;
            sym->offset = nextarg++;
        } else {
            err(curtok->line, "'%s' is multiply defined", sym->name);
        }

        if (curtok->type == TRPAREN) {
            break;
        } else if (curtok->type != TCOMMA) {
            err(curtok->line, "')' or ',' expected");
            break;
        }

        nextok();
    }

    nextok();
}

// parse a statement
//
void 
statement(struct codefrag *prog)
{
    const char *nm, *p;

    if (curtok->type == TSCOLON) {
        nextok();
        return;
    }

    if (curtok->type == TLBRACE) {
        nextok();
        for(;;) {
            if (curtok->type == TRBRACE) {
                nextok();
                break;
            }
            statement(prog);
        }
        return;
    }

    switch (curtok->type) {
        case TAUTO: break;
        case TEXTRN: break;
        case TCASE: break;
        case TIF: break;
        case TWHILE: break;
        case TSWITCH: break;
        case TGOTO: break;
        case TRETURN: break;

        case TNAME:
            // this could either be the start of an rvalue or
            // a label
            savtok = *curtok;
            nextok();
            if (curtok->type == TCOLON) {
                stmtlabel(prog, curtok->line, savtok.val.name);
                nextok();
            }
            break;
    }
}

// Assign a label at the current point in the code
//
void
stmtlabel(struct codefrag *prog, int line, const char *nm)
{
    struct codenode *cn;
    struct stabent *sym;

    cn = cnalloc();
    cn->op = ONAMDEF;
    strcpy(cn->arg.name, nm);

    cnpush(prog, cn);

    sym = stabget(&local, nm);
    if (sym->sc == NEW) {
        sym->sc = INTERNAL;
        sym->def = cn; 
    } else {
        err(line, "'%s' is already defined", nm);
    }
}

// Parse a data definition
//
void
datadef(struct codefrag *prog)
{
    struct constant con;
    struct codenode *cn;
    int isvec = 0;
    unsigned size = 0;
    unsigned emitted = 0;
    int ch;

    if (curtok->type == TLBRACKET) {
        nextok();
        if (curtok->type != TRBRACKET && curtok->type != TINTCON) {
            err(curtok->line, "']' or integer constant expected");
            nextok();
            return;
        }

        if (curtok->type == TINTCON) {
            size = curtok->val.intcon;
            nextok();
        }

        if (curtok->type != TRBRACKET) {
            err(curtok->line, "']' expected");
            nextok();
            return;
        } 

        nextok();
        isvec = 1;
    }

    if (curtok->type == TSCOLON) {
        if (!isvec) {
            cn = cnalloc();
            cn->op = OIVAL;
            cn->arg.ival.isconst = 1;
            cn->arg.ival.v.con.strlen = INTCONST;
            cn->arg.ival.v.con.v.intcon = 0;
            cnpush(prog, cn);
            emitted++;
        }
        nextok();
    } else {
        for(;;) {
            cn = cnalloc();
            cn->op = OIVAL;
            
            switch (curtok->type) {
            case TNAME:
                cn->arg.ival.isconst = 0;
                cn->arg.ival.v.name = stabget(&global, curtok->val.name);
                break;
                
            case TINTCON:
                cn->arg.ival.isconst = 1;
                cn->arg.ival.v.con.strlen = INTCONST;
                cn->arg.ival.v.con.v.intcon = curtok->val.intcon;
                break; 

            case TSTRCON:
                cn->arg.ival.isconst = 1;
                cn->arg.ival.v.con.strlen = curtok->val.strcon.len; 
                cn->arg.ival.v.con.v.strcon = curtok->val.strcon.bytes;
                break;

            default:
                err(curtok->line, "name or constant expected");
                nextok();
                return;
            }

            nextok();
            
            cnpush(prog, cn);
            emitted++;

            if (curtok->type == TSCOLON) {
                nextok();
                break;
            } else if (curtok->type == TCOMMA) {
                nextok();
                continue;
            }

            err(curtok->line, "';' or ',' expected");
            break; 
        }
    }

    if (size > emitted) {
        cn = cnalloc();
        cn->op = OZERO;
        cn->arg.rept = size - emitted;
        cnpush(prog, cn);
    }
}

// advance to the next token
//
void
nextok(void)
{
    curtok = token();
}

/******************************************************************************
 *
 * Expression (rvalue) evaluation
 *
 */

// Parse a primary expression
//
static void
rvalprimary(struct codefrag *frag)
{
    switch (curtok->type) {
    case TNAME: break;
    case TINTCON: break;
    case TSTRCON: break;
    case TLPAREN:
        nextok();
        rvalue(frag);
        if (curtok->type == TRPAREN) {
            nextok();
        } else {
            err(curtok->line, "')' expected");
        }
        break;
    }

    for(;;) {
        if (curtok->type == TLPAREN) {
            // TODO function invocation
            //
        } else if (curtok->type = TLBRACKET) {
            nextok();
            rvalue(frag);
            if (curtok->type == TRBRACKET) {
                nextok();
            } else {
                err(curtok->line, "']' expected");
            }
        }
    }
}

// Parse unary operators
//
static void
rvalunary(struct codefrag *frag)
{
    if (curtok->type == TTIMES || curtok->type == TAND || curtok->type == TMINUS || curtok->type == TNOT || curtok->type == TPP || curtok->type == TMM) {
        nextok();
        rvalunary(frag);
    } else {
        rvalprimary(frag);
        if (curtok->type == TPP || curtok->type == TMM) {
            nextok();
        }
    }
}

// Parse a multiplicative expression
//
static void
rvalmul(struct codefrag *frag)
{
    rvalunary(frag);
    while (curtok->type == TTIMES || curtok->type == TDIV || curtok->type == TMOD) {
        nextok();
        rvalunary(frag);
    }
}

// Parse an additive expression
//
static void
rvaladd(struct codefrag *frag)
{
    rvalmul(frag);
    while (curtok->type == TPLUS || curtok->type == TMINUS) {
        nextok();
        rvalmul(frag);
    }
}

// Parse a shift expression
//
static void
rvalshift(struct codefrag *frag)
{
    rvaladd(frag);
    while (curtok->type == TLSHIFT || curtok->type == TRSHIFT) {
        nextok();
        rvaladd(frag);
    }
}

// Parse a relational expression
//
static void
rvalrel(struct codefrag *frag)
{
    rvalshift(frag);
    while (curtok->type == TGT || curtok->type == TGE || curtok->type == TLE || curtok->type == TLT) {
        nextok();
        rvalshift(frag);
    }
}

// Parse an equality expression
//
static void
rvaleq(struct codefrag *frag)
{
    rvalrel(frag);
    while (curtok->type == TEQ || curtok->type == TNE) {
        nextok();
        rvalrel(frag);
    }
}

// Parse a logical AND expression
//
static void
rvaland(struct codefrag *frag)
{
    rvaleq(frag);
    while (curtok->type == TAND) {
        nextok();
        rvaleq(frag);
    }
}

// Parse a logical OR expression
//
static void
rvalor(struct codefrag *frag)
{
    rvaland(frag);
    while (curtok->type == TOR) {
        nextok();
        rvaland(frag);
    }
}

// Parse a conditional (?:) expression
//
static void
rvalcond(struct codefrag *frag)
{
    rvalor(frag);

    if (curtok->type == TQUES) {
        nextok();
        rvalcond(frag);
        if (curtok->type == TCOLON) {
            nextok();
            rvalcond(frag);
        } else {
            err(curtok->line, "':' expected");
        }
    }
}

// Parse an rvalue (expression)
//
void
rvalue(struct codefrag *frag)
{
    rvalcond(frag);
}

/******************************************************************************
 *
 * Symbol table routines
 *
 */

// get or create a symbol of the given name
//
struct stabent *
stabget(struct stabent **root, const char *name)
{
    struct stabent *p;

    for (p = *root; p; p = p->next) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }

    p = calloc(1, sizeof(struct stabent));
    strcpy(p->name, name);
    p->sc = NEW;
    p->next = *root;
    *root = p;

    return p;
}

/******************************************************************************
 *
 * Code fragment routines
 *
 */

// Allocate a new code node
//
struct codenode *
cnalloc(void)
{
    struct codenode *cn = (struct codenode *)calloc(1, sizeof(struct codenode));
    if (cn == NULL) {
        fprintf(stderr, "\n\nFATAL: out of memory\n");
        exit(1);
    }
    return cn;
}

// Push a code node onto the end of a code fragment
//
void 
cnpush(struct codefrag *frag, struct codenode *node)
{
    node->next = NULL;
    if (frag->head == NULL) {
        frag->head = frag->tail = node;
    } else {
        frag->tail->next = node;
        frag->tail = node;
    }
}

// Hex dump
//
void
hexdump(const char *indent, const char *data, int len)
{
    int i, j, l;
    const int XPERLINE = 16;

    for (i = 0; i < len; i += XPERLINE) {
        printf("%s", indent);

        l = (len - i) > XPERLINE ? XPERLINE : (len - i);

        for (j = 0; j < l; j++)   printf("%02x ", data[i+j] & 0xff);
        for (; j < XPERLINE; j++) printf("   ");
     
        printf(" |");
        
        for (j = 0; j < l; j++)   putchar(isprint(data[i+j]) ? data[i+j] : '.'); 
        for (; j < XPERLINE; j++) putchar(' ');

        printf("|\n");
    }
}

// Print a code fragment to stdout
//
void 
cfprint(struct codefrag *frag) 
{
    static const char spaces[] = "          ";
    struct codenode *n;

    int i, j, l;
    char ch;

    for (n = frag->head; n; n = n->next) {
        switch (n->op) {
        case ONAMDEF:
            printf("%s:\n", n->arg.name);
            break;

        case OIVAL:
            if (n->arg.ival.isconst) {
                if (n->arg.ival.v.con.strlen == INTCONST) {
                    printf("%sIVAL %u\n", spaces, n->arg.ival.v.con.v.intcon);
                } else {
                    printf("%sIVAL strcon\n", spaces);
                    hexdump(
                        spaces, 
                        n->arg.ival.v.con.v.strcon, 
                        n->arg.ival.v.con.strlen);
                }
            } else {
                printf("%sIVAL extrn %s\n", spaces, n->arg.ival.v.name->name);
            }
            break;

        case OZERO:
            printf("%sZERO %d\n", spaces, n->arg.rept);
            break;

        case OJMP:
            printf("%sJMP %s\n", spaces, n->arg.target->name);
            break;

        case OPSHCON:
            break;
        }
    }
}

/******************************************************************************
 *
 * Low level lexical routines
 *
 */

// report an error, and set a flag that the compilation
// has failed.
//
void 
err(int l, const char *fmt, ...)
{
    fprintf(stderr, "%s:%d: ", srcfn, l);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    errf = 1;
}
