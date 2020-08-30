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
    struct codenode *exdef;         // if extrn and we have a definition
    // if AUTO, offset into stack
    // if INTERNAL, offset into code
    int offset;
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
    TNAMDEF,
    TIVAL,
    TZERO,
};

struct codenode {
    struct codenode *next;
    enum codeop op;
    union {
        char name[MAXNAM+1];
        struct ival ival;
        unsigned rept;
    } arg;
};

struct codefrag {
    struct codenode *head, *tail;
};

FILE *fp;

static char *srcfn;
static int errf = 0;
static struct token *curtok;
static struct stabent *global;
static struct stabent *local;

static void program(struct codefrag *prog);
static void definition(struct codefrag *prog);
static void funcdef(struct codefrag *prog);
static void funcparms(void);
static void statement(struct codefrag *prog);
static void datadef(struct codefrag *prog);
static void nextok(void);

static struct stabent *stabget(struct stabent **root, const char *name);

static struct codenode *cnalloc(void);
static void cnpush(struct codefrag *frag, struct codenode *node);
static void cfprint(struct codefrag *frag);

static void err(int line, const char *fmt, ...);

int 
main(int argc, char **argv)
{
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

    cn->op = TNAMDEF;
    strcpy(cn->arg.name, curtok->val.name);
    cnpush(prog, cn);

    sym = stabget(&global, cn->arg.name);
    if (sym->sc == NEW) {
        sym->sc = EXTERN;
        sym->exdef = cn;
    } else {
        err(curtok->line, "'%s' is previously defined", cn->arg.name);
    }

    nextok();

    if (curtok->type == TLPAREN) {
        nextok();
        funcdef(prog);
        return;
    }

    datadef(prog);
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
void statement(struct codefrag *prog)
{

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
            err(curtok->line, "']' constant expected");
            nextok();
            return;
        }
        isvec = 1;
    }

    if (curtok->type == TSCOLON) {
        if (!isvec) {
            cn = cnalloc();
            cn->op = TIVAL;
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
            cn->op = TIVAL;
            
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
                cn->arg.ival.v.con.strlen = INTCONST;
                cn->arg.ival.v.con.v.intcon = curtok->val.intcon;
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
        cn->op = TZERO;
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

// Print a code fragment to stdout
//
void 
cfprint(struct codefrag *frag) 
{
    static const char spaces[] = "          ";
    struct codenode *n;

    const int XPERLINE = 16;
    int i, j, l;
    char ch;

    for (n = frag->head; n; n = n->next) {
        switch (n->op) {
        case TNAMDEF:
            printf("%s:\n", n->arg.name);
            break;

        case TIVAL:
            if (n->arg.ival.isconst) {
                if (n->arg.ival.v.con.strlen == INTCONST) {
                    printf("%sIVAL %u\n", spaces, n->arg.ival.v.con.v.intcon);
                } else {
                    printf("%sIVAL strcon\n", spaces);
                    for (i = 0; i < n->arg.ival.v.con.strlen; i += XPERLINE) {
                        printf("%s", spaces);

                        l = n->arg.ival.v.con.strlen - i;
                        l = l > XPERLINE ? XPERLINE : l;

                        for (j = 0; j < l; j++) {
                            printf("%02x ", n->arg.ival.v.con.v.strcon[i+j] & 0xff);
                        }

                        for (; j < XPERLINE; j++) {
                            printf("   ");
                        }

                        printf(" |");
                        for (j = 0; j < l; j++) {
                            ch = n->arg.ival.v.con.v.strcon[i+j];
                            putchar(isprint(ch) ? ch : '.'); 
                        }

                        for (; j < XPERLINE; j++) {
                            putchar(' ');
                        }
                        printf("|\n");
                    }
                }
            } else {
                printf("%sIVAL extrn %s\n", spaces, n->arg.ival.v.name->name);
            }
            break;

        case TZERO:
            printf("%sZERO %d\n", spaces, n->arg.rept);
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
