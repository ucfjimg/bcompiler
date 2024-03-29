#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "b.h"
#include "lex.h"

#define MAXFNARG 64

#define LVAL  0
#define RVAL  1

struct scase {
    struct scase *next;
    unsigned caseval;
    struct stabent *label;
};

struct swtch {
    struct swtch *prev;
    struct scase *head, *tail;
};

static char *srcfn;
static int errf = 0;
static struct token *curtok = NULL;
static struct stablist global = { NULL, NULL };
static struct stablist *local = NULL;
static struct stablist datasyms = { NULL, NULL };
static struct swtch *swtchstk = NULL;
static struct stabent *retlabel = NULL;
static int nextlab = 0;
static int nextauto = 0;

static void program(void);
static void definition(void);
static void funcdef(struct codefrag *prog);
static void funcparms(void);
static void statement(struct codefrag *prog);
static void stmtextrn(void);
static void stmtauto(void);
static void stmtlabel(struct codefrag *prog, int line, const char *nm);
static void stmtgoto(struct codefrag *prog);
static void stmtif(struct codefrag *prog);
static void stmtwhile(struct codefrag *prog);
static void stmtcase(struct codefrag *prog);
static void stmtswitch(struct codefrag *prog);
static void stmtreturn(struct codefrag *prog);
static void datadef(struct stabent *sym);
static void pushtok(const struct token *tok);
static void nextok(void);

static void pushop(struct codefrag *prog, enum codeop op);
static void pushopn(struct codefrag *prog, enum codeop op, unsigned n);
static void pushicon(struct codefrag *prog, unsigned val);
static void pushbr(struct codefrag *prog, enum codeop op, struct stabent *target);
static void pushlbl(struct codefrag *prog, struct stabent *lbl);
static void pushcase(struct codefrag *prog, unsigned caseval, struct stabent *target);
static void torval(struct codefrag *prog);
static int expr(struct codefrag *prog);

static struct stabent *stabget(struct stablist *root, const char *name);
static struct stabent *stabfind(const char *name);
static struct stabent *mklabel(void);

static struct codenode *cnalloc(void);
static void cnpush(struct codefrag *frag, struct codenode *node);
static void cnappend(struct codefrag *fragl, struct codefrag *fragr);
static void cnsplice(struct codefrag *fragl, struct codefrag *fragr, struct codenode *after);
static void cfprint(struct codefrag *frag);

static void ddprint(struct stabent *sym);

void err(int sline, int line, const char *fmt, ...);

static void
usage()
{
    fprintf(stderr, "bc: [-o outfile] infile\n");
    exit(1);
}

static char *
mkoutf(const char *inf)
{
    char *dot = strrchr(inf, '.');
    char *sep = strrchr(inf, '/');
    char *out;
    int l;

    if (dot && (sep == NULL || sep < dot)) {
        l = dot - inf;
        if (sep) {
            l -= (sep - inf + 1);
            inf = sep + 1;
        }
    } else {
        l = strlen(inf);
    }

    out = malloc(l + 3);
    memcpy(out, inf, l);
    out[l] = '.';
    out[l+1] = 'i';
    out[l+2] = '\0';

    return out;
}

int 
main(int argc, char **argv)
{
    FILE *fp;
    struct stabent *sym;
    char *outfn = NULL;
    int listing = 0;
    int ch;

    while ((ch = getopt(argc, argv, "lo:")) != -1) {
        switch (ch) {
        case 'o':
            outfn = optarg;
            break;

        case 'l':
            listing = 1;
            break;

        default:
            usage();
            break;
        }
    }

    if (optind >= argc) {
        usage();
    }
    srcfn = argv[optind];

    if (outfn == NULL) {
        outfn = mkoutf(srcfn);
    }

    if ((fp = fopen(srcfn, "r")) == NULL) {
        perror(srcfn);
        return 1;
    }
    
    lexinit(fp);
    nextok();
    program();

    if (errf) {
        return 1;
    }

    if (bifwrite(outfn, global) != 0) {
        return 1;
    }

    if (listing) {
        for (sym = global.head; sym; sym = sym->next) {
            if (sym->type == FUNC) {
                printf("function %s:\n", sym->name);
                cfprint(&sym->fn);
            }
        }

        for (sym = datasyms.head; sym; sym = sym->nextData) {
            if (sym->type != FUNC) {
                ddprint(sym);
            }
        }
    }
    return 0;
}

// Parse the whole program
// 
void 
program(void)
{
    int ch;

    while (!errf && curtok->type != TEOF) {
        definition();
    }
}

// Parse a definition
//
void
definition(void)
{
    struct stabent *sym;

    if (curtok->type != TNAME) {
        err(__LINE__,curtok->line, "name expected");
        nextok();
        return;
    }

    sym = stabget(&global, curtok->val.name);
    if (sym->sc == NEW) {
        sym->sc = EXTERN;
    } else {
        err(__LINE__,curtok->line, "'%s' is previously defined", curtok->val.name);
    }

    nextok();

    local = &sym->scope;
    if (curtok->type == TLPAREN) {
        sym->type = FUNC;
        nextok();
        funcdef(&sym->fn);
    } else {
        datadef(sym);
    }
}

// parse a function definition
//
void 
funcdef(struct codefrag *prog)
{
    int autoidx;
    struct codenode *enter;
    struct codefrag avinit;

    retlabel = mklabel();

    struct stabent *sym;
    if (curtok->type == TRPAREN) {
        nextok();
    } else {
        funcparms();
    }
    
    pushop(prog, OENTER);
    enter = prog->tail;

    statement(prog);

    avinit.head = avinit.tail = NULL;

    for (sym = local->head; sym; sym = sym->next) {
        if (sym->forward) {
            err(__LINE__,curtok->line, "'%s': goto target was never defined.", sym->name);
        }

        if (sym->sc == AUTO && sym->type == VECTOR) {
            pushopn(&avinit, OAVINIT, sym->stkoffs);
        }
    }
    
    cnsplice(&avinit, prog, enter);

    enter->arg.n = nextauto;
   
    pushicon(prog, 0);
    pushlbl(prog, retlabel);
    pushop(prog, OPOPT);
    pushop(prog, OLEAVE);
    pushop(prog, OPUSHT);
    pushop(prog, ORET);
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
            err(__LINE__,curtok->line, "name expected");
            return;
        }

        sym = stabget(local, curtok->val.name);

        nextok();
        
        if (sym->sc == NEW) {
            sym->sc = AUTO;
            sym->stkoffs = nextarg++;
        } else {
            err(__LINE__,curtok->line, "'%s' is multiply defined", sym->name);
        }

        if (curtok->type == TRPAREN) {
            break;
        } else if (curtok->type != TCOMMA) {
            err(__LINE__,curtok->line, "')' or ',' expected");
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
    struct token savtok;
    const char *nm, *p;
    struct codenode *cn;

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
        case TAUTO: 
            nextok();
            stmtauto();
            statement(prog);
            break;

        case TEXTRN: 
            nextok();
            stmtextrn();
            statement(prog);
            break;

        case TCASE: 
            nextok();
            stmtcase(prog);
            break;

        case TIF:
            nextok();
            stmtif(prog);
            break;

        case TWHILE:
            nextok();
            stmtwhile(prog);
            break;

        case TSWITCH: 
            nextok();
            stmtswitch(prog);
            break;
        
        case TGOTO: 
            nextok();
            stmtgoto(prog);
            break;

        case TRETURN: 
            nextok();
            stmtreturn(prog);
            break;

        case TNAME:
            // this could either be the start of an rvalue or
            // a label
            savtok = *curtok;
            nextok();
            if (curtok->type == TCOLON) {
                stmtlabel(prog, curtok->line, savtok.val.name);
                nextok();
                break;
            }

            pushtok(curtok);
            pushtok(&savtok);
            nextok();

            
            // falling through
        default:

            // rvalue ';'
            expr(prog);
    
            cn = cnalloc();
            cn->op = OPOP;
            cnpush(prog, cn);
            
            if (curtok->type == TSCOLON) {
                nextok();
            } else {
                err(__LINE__,curtok->line, "';' expected");
            }

            break;
    }
}

// Parse an auto statement
//
void
stmtauto()
{
    int done;
    struct stabent *sym;

    for (done = 0; !done;) {
        if (curtok->type != TNAME) {
            err(__LINE__,curtok->line, "name expected");
            nextok();
            break;
        }

        sym = stabget(local, curtok->val.name);
        if (sym->sc != NEW) {
            err(__LINE__,curtok->line, "'%s' is already defined");
        } else {
            sym->sc = AUTO;
            sym->type = SIMPLE;
        }
        nextok();

        if (curtok->type == TINTCON) {
            sym->type = VECTOR;
            sym->vecsize = curtok->val.con.v.intcon;
            nextauto += 1 + sym->vecsize;
            sym->stkoffs = -nextauto;
            nextok();
        } else {
            nextauto++;
            sym->stkoffs = -nextauto;
        }

        switch (curtok->type) {
        case TCOMMA:
            nextok();
            break;

        case TSCOLON:
            nextok();
            done = 1;
            break;

        default:
            err(__LINE__,curtok->line, "',' or ';' expected");
            nextok();
        }
    }
}

// Parse an extrn statement
//
void
stmtextrn()
{
    int done;
    struct stabent *sym;

    for (done = 0; !done;) {
        if (curtok->type != TNAME) {
            err(__LINE__,curtok->line, "name expected");
            nextok();
            break;
        }

        sym = stabget(local, curtok->val.name);
        if (sym->sc != NEW && sym->sc != EXTERN) {
            err(__LINE__,curtok->line, "'%s' is already defined");
        } else {
            sym->sc = EXTERN;
        }
        nextok();

        switch (curtok->type) {
        case TCOMMA:
            nextok();
            break;

        case TSCOLON:
            nextok();
            done = 1;
            break;

        default:
            err(__LINE__,curtok->line, "',' or ';' expected");
            nextok();
        }
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

    cnpush(prog, cn);

    sym = stabget(local, nm);
    if (sym->sc == NEW) {
        sym->sc = INTERNAL;
        sym->type = LABEL;
        sym->cptr = cn; 
        sym->labpc = nextlab++;
    } else if (sym->sc == INTERNAL && sym->type == LABEL) {
        sym->forward = 0;
    } else {
        err(__LINE__,line, "'%s' is already defined", nm);
    }

    cn->arg.target = sym;
}

// Goto statement
//
void
stmtgoto(struct codefrag *prog)
{
    struct stabent *sym;

    if (curtok->type != TNAME) {
        err(__LINE__, curtok->line, "name expected");
        nextok();
        return;
    }

    sym = stabget(local, curtok->val.name);
    
    switch (sym->sc) {
    case INTERNAL:
        break;

    case NEW:
        sym->sc = INTERNAL;
        sym->type = LABEL;
        sym->forward = 1;
        sym->labpc = nextlab++;
        break;

    default:
        err(__LINE__,curtok->line, "'%s' is not a label", curtok->val.name);
        break;
    }

    pushbr(prog, OJMP, sym);

    nextok();
    if (curtok->type != TSCOLON) {
        err(__LINE__, curtok->line, "name expected");
    }
    nextok();
}

// Parse an if statement
//
void
stmtif(struct codefrag *prog)
{
    struct stabent *elsepart = mklabel();
    struct stabent *donepart = mklabel();
    
    if (curtok->type != TLPAREN) {
        err(__LINE__,curtok->line, "'(' expected");
        return;
    }
    nextok();
    
    if (expr(prog) == LVAL) {
        torval(prog);
    }

    if (curtok->type != TRPAREN) {
        err(__LINE__,curtok->line, "')' expected");
        return;
    }
    nextok();

    pushbr(prog, OBZ, elsepart);
    statement(prog);
    
    if (curtok->type != TELSE) {
        pushlbl(prog, elsepart); 
    } else {
        nextok();
        pushbr(prog, OJMP, donepart);
        pushlbl(prog, elsepart);
        statement(prog);
        pushlbl(prog, donepart);
    }
}

// Parse a while statement
//
void
stmtwhile(struct codefrag *prog)
{
    struct stabent *top = mklabel();
    struct stabent *bottom = mklabel();

    pushlbl(prog, top);

    if (curtok->type != TLPAREN) {
        err(__LINE__,curtok->line, "'(' expected");
        return;
    }
    nextok();
    
    if (expr(prog) == LVAL) {
        torval(prog);
    }

    if (curtok->type != TRPAREN) {
        err(__LINE__,curtok->line, "')' expected");
        return;
    }
    nextok();

    pushbr(prog, OBZ, bottom);
    statement(prog);
    pushbr(prog, OJMP, top);
    pushlbl(prog, bottom);
}

// Parse a switch statement
//
void
stmtswitch(struct codefrag *prog)
{
    struct swtch *sw = calloc(1, sizeof(struct swtch));
    struct scase *scase;
    struct stabent *nomatch = mklabel();
    struct codenode *here;
    struct codefrag cases = { NULL, NULL };

    sw->prev = swtchstk;
    swtchstk = sw;

    // NB unlike C, B doesn't require parens around the discriminating
    // expression
    //
    if (expr(prog) == LVAL) {
        torval(prog);
    }

    here = prog->tail;

    statement(prog);

    pushlbl(prog, nomatch);
    swtchstk = sw->prev;

    for (scase = sw->head; scase; scase = scase->next) {
        pushcase(&cases, scase->caseval, scase->label);
    }

    free(sw);
    
    pushop(&cases, OPOP);
    pushbr(&cases, OJMP, nomatch);

    cnsplice(&cases, prog, here);
}

// Parse a case statement
//
void
stmtcase(struct codefrag *prog)
{
    struct scase *scase;

    if (swtchstk == NULL) {
        err(__LINE__,curtok->line, "case statement outside of switch");
    }

    if (curtok->type != TINTCON) {
        err(__LINE__,curtok->line, "integer constant expected");
        nextok();
        return;
    }

    scase = malloc(sizeof(struct scase));
    scase->caseval = curtok->val.con.v.intcon;
    scase->label = mklabel();
    scase->next = NULL;

    if (swtchstk->head == NULL) {
        swtchstk->head = scase;
    } else {
        swtchstk->tail->next = scase;
    }
    swtchstk->tail = scase;

    pushlbl(prog, scase->label);

    nextok();
    if (curtok->type != TCOLON) {
        err(__LINE__,curtok->line, "':' expected");
    }

    nextok();
}

// Return from a function
//
void
stmtreturn(struct codefrag *prog)
{
    if (curtok->type == TSCOLON) {
        pushicon(prog, 0);
        pushbr(prog, OJMP, retlabel);
        nextok();
        return;
    }

    if (curtok->type != TLPAREN) {
        err(__LINE__,curtok->line, "'(' or ';' expected");
        nextok();
        return;
    }
    nextok();

    if (expr(prog) == LVAL) {
        torval(prog);
    }

    pushbr(prog, OJMP, retlabel);

    if (curtok->type != TRPAREN) {
        err(__LINE__,curtok->line, "')' expected");
        nextok();
        return;
    }
    nextok();

    if (curtok->type != TSCOLON) {
        err(__LINE__,curtok->line, "';' expected");
    }
    nextok();
}


static void
pushival(struct ivallist *l, struct ival *i)
{
    if (l->head == NULL) {
        l->head = i;
    } else {
        l->tail->next = i;
    }
    l->tail = i;
}

static struct ival *
ivalsym(struct stabent *sym)
{
    struct ival *i = calloc(1, sizeof(struct ival));
    i->isconst = 0;
    i->v.name = sym;
    return i;
}

static struct ival *
ivalcon(struct constant *con)
{
    struct ival *i = calloc(1, sizeof(struct ival));
    i->isconst = 1;
    i->v.con = *con;
    return i;
}

// Parse a data definition
//
void
datadef(struct stabent *sym)
{
    struct stabent *refsym;
    struct ival iv;
    int emitted = 0;
    struct constant con;
    
    sym->type = SIMPLE;
    if (curtok->type == TLBRACKET) {
        sym->type = VECTOR;

        nextok();
        if (curtok->type != TRBRACKET && curtok->type != TINTCON) {
            err(__LINE__,curtok->line, "']' or integer constant expected");
            nextok();
            return;
        }

        if (curtok->type == TINTCON) {
            sym->vecsize = curtok->val.con.v.intcon;
            nextok();
        }

        if (curtok->type != TRBRACKET) {
            err(__LINE__,curtok->line, "']' expected");
            nextok();
            return;
        } 

        nextok();
    }

    while (curtok->type != TSCOLON) {
        switch (curtok->type) {
        case TNAME:
            refsym = stabfind(curtok->val.name);
            if (refsym == NULL) {
                err(__LINE__,curtok->line, "'%s' is not defined", curtok->val.name);
            } else {
                pushival(&sym->ivals, ivalsym(refsym));
            }
            break;
                
        case TINTCON:
        case TSTRCON:
            pushival(&sym->ivals, ivalcon(&curtok->val.con));
            break;

        default:
            err(__LINE__,curtok->line, "name or constant expected");
            nextok();
            return;
        }

        nextok();
            
        emitted++;

        if (curtok->type == TSCOLON) {
            break;
        } else if (curtok->type != TCOMMA) {
            err(__LINE__,curtok->line, "';' or ',' expected");
            break; 
        }
        nextok();
    }

    nextok();

    if (sym->type == SIMPLE && emitted == 0) {
        con.strlen = INTCONST;
        con.v.intcon = 0;
        pushival(&sym->ivals, ivalcon(&con));
    } 

    if (sym->type == VECTOR) {
        if (emitted > sym->vecsize) {
            sym->vecsize = emitted;
        }
    }

    if (datasyms.head == NULL) {
        datasyms.head = sym;
    } else {
        datasyms.tail->nextData = sym;
    }
    datasyms.tail = sym;
}

#define BAKTOK 2
static struct token baktok[BAKTOK];
static int ibaktok = -1;

// put a token back onto the parsing stream
//
void
pushtok(const struct token *tok)
{
    if (ibaktok < BAKTOK - 1) {
        baktok[++ibaktok] = *tok;
    }
}

// advance to the next token
//
void
nextok(void)
{
    if (ibaktok >= 0) {
         curtok = &baktok[ibaktok--];
         return;
    }
    curtok = token();
}

/******************************************************************************
 *
 * Expression (rvalue) evaluation
 *
 */

// Add an op with no parameters
//
void
pushop(struct codefrag *prog, enum codeop op)
{
    struct codenode *cn = cnalloc();
    cn->op = op;
    cnpush(prog, cn);
}

// Add an op with an integer parameter
//
void
pushopn(struct codefrag *prog, enum codeop op, unsigned n)
{
    struct codenode *cn = cnalloc();
    cn->op = op;
    cn->arg.n = n;
    cnpush(prog, cn);
}

// Push an integer constant onto the stack
//
void
pushicon(struct codefrag *prog, unsigned val)
{
    struct codenode *cn = cnalloc();
    cn->op = OPSHCON;
    cn->arg.con.strlen = INTCONST;
    cn->arg.con.v.intcon = val;

    cnpush(prog, cn);
}

// Add a branch op
//
void
pushbr(struct codefrag *prog, enum codeop op, struct stabent *target)
{
    struct codenode *cn = cnalloc();
    cn->op = op;
    cn->arg.target = target;

    cnpush(prog, cn);
}

// Add a label
//
void
pushlbl(struct codefrag *prog, struct stabent *lbl)
{
    struct codenode *cn = cnalloc();
    cn->op = ONAMDEF;
    cn->arg.target = lbl;

    cnpush(prog, cn);
}

// Add a case label
//
static void 
pushcase(struct codefrag *prog, unsigned caseval, struct stabent *target)
{
    struct codenode *cn = cnalloc();
    cn->op = OCASE;
    cn->arg.caseval.disc = caseval;
    cn->arg.caseval.label = target;

    cnpush(prog, cn);
}

// Convert the top of the stack to an rvalue
//
void
torval(struct codefrag *prog)
{
    pushop(prog, ODEREF);
}

static const char lvalex[] = "lvalue expected";

// parse a function call. returns the number of arguments
// pushed on the stack
//
static int 
ecall(struct codefrag *prog)
{
    struct codefrag args[64], t, *parg;

    int n = 0, i, done;

    if (curtok->type == TRPAREN) {
        nextok();
    } else {
        for(done = 0; !done; n++) {
            if (n >= MAXFNARG) {
                parg = &t;  
            } else {
                parg = &args[n];
            }

            parg->head = parg->tail = NULL;

            if (expr(parg) == LVAL) {
                torval(parg);
            }

            switch (curtok->type) {
            case TRPAREN:
                nextok();
                done = 1;
                break;

            case TCOMMA:
                nextok();
                break;

            default:
                err(__LINE__,curtok->line, "')' or ',' expected");
                nextok();
                break;
            }
        }

        if (n > MAXFNARG) {
            err(__LINE__,curtok->line, "too many function call args (max %d)", MAXFNARG);
            n = MAXFNARG;
        }

        for (i = n-1; i >= 0; i--) {
            cnappend(prog, &args[i]);
        }
    }

    return n;
}


// Parse a primary expression
//
static int 
eprimary(struct codefrag *prog)
{
    int type;
    int args;

    struct codenode *cn;
    struct stabent *sym;
    int done;

    switch (curtok->type) {
    case TNAME: 
        sym = stabfind(curtok->val.name);
        if (sym) {
            cn = cnalloc();
            cn->op = OPSHSYM;
            cn->arg.target = sym;
            cnpush(prog, cn);
            nextok();
            type = LVAL;
        } else {
            err(__LINE__,curtok->line, "'%s' is not defined", curtok->val.name);
            nextok();
        }
        break;

    case TINTCON: 
    case TSTRCON:
        cn = cnalloc();
        cn->op = OPSHCON;
        cn->arg.con = curtok->val.con;
        cnpush(prog, cn);
        nextok();
        type = RVAL;
        break;

    case TLPAREN:
        nextok();
        type = expr(prog);
        if (curtok->type == TRPAREN) {
            nextok();
        } else {
            err(__LINE__,curtok->line, "')' expected");
        }
        break;
    }

    for(done = 0; !done;) {
        switch (curtok->type) {
        case TLPAREN:
            nextok();
            args = ecall(prog);
            
            pushopn(prog, ODUPN, args);     // fn argn argn-1 ... arg0 fn 
            if (type == LVAL) {
                torval(prog);
            }
            pushop(prog, OCALL);            // fn argn argn-1 ... arg0 retval 
            pushop(prog, OPOPT);            // fn argn argn-1 ... arg0 ; retval in T
            pushopn(prog, OPOPN, args+1);   // ; retval in T
            pushop(prog, OPUSHT);           // retval

            type = RVAL;
            break;

        case TLBRACKET:
            nextok();
            if (type == LVAL) {
                torval(prog);
            }
            if (expr(prog) == LVAL) {
                torval(prog);
            }
            if (curtok->type == TRBRACKET) {
                nextok();
            } else {
                err(__LINE__,curtok->line, "']' expected");
            }
            
            pushop(prog, OADD);
            type = LVAL;
            break;

        case TPP:
        case TMM:
            // post-increment, post-decrement
            if (type != LVAL) {
                err(__LINE__,curtok->line, lvalex);
            } else {
                                        // lval
                pushop(prog, ODUP);     // lval lval
                pushop(prog, ODEREF);   // lval old-val
                pushop(prog, ODUP);     // lval old-val old-val
                pushop(prog, OROT);     // old-val lval old-val
                pushicon(prog, 1);
                pushop(prog, curtok->type == TPP ? OADD : OSUB);
                                        // old-val lval new-val
                pushop(prog, OSTORE);   // old-val, m[lval] = newval
            }
            
            type = RVAL;
            nextok();
            break;

        default:
            done = 1;
        }
    }

    return type;
}

// Parse unary operators
//
static int 
eunary(struct codefrag *prog)
{
    enum toktyp ttype = curtok->type;
    int type;
        
    if (ttype == TDEREF || ttype == TADDR || ttype == TMINUS || ttype == TNOT || ttype == TPP || ttype == TMM) {
        nextok();
        type = eunary(prog);

        switch (ttype) {
        case TDEREF:
            if (type == LVAL) {
                torval(prog);
            }
            type = LVAL;
            break;

        case TADDR:
            if (type == RVAL) {
                err(__LINE__,curtok->line, lvalex);
            } else {
                type = RVAL;
            }
            break;

        case TMINUS:
        case TNOT:
            if (type == LVAL) {
                torval(prog);
                type = RVAL;
            }
            pushop(prog, ttype == TMINUS ? ONEG : ONOT);
            break;

        case TMM:
        case TPP:
            if (type == LVAL) {
                                        // lval
                pushop(prog, ODUP);     // lval lval
                pushop(prog, ODEREF);   // lval old-val
                pushicon(prog, 1);
                pushop(prog, ttype == TPP ? OADD : OSUB);
                                        // lval new-val
                pushop(prog, ODUP);     // lval new-val new-val
                pushop(prog, OROT);     // new-val lval new-val
                pushop(prog, OSTORE);   // new-val, m[lval] = newval

                type = RVAL;
            } else {
                err(__LINE__,curtok->line, lvalex);
            }
            break;
        }

        return type;
    } 

    return eprimary(prog);
}

// Parse a multiplicative expression
//
static int
emul(struct codefrag *prog)
{
    int type = eunary(prog);
    enum toktyp ttype = curtok->type;
    enum codeop op;

    while (ttype == TTIMES || ttype == TDIV || ttype == TMOD) {
        if (type == LVAL) {
            torval(prog);
        }

        nextok();
        if (eunary(prog) == LVAL) {
            torval(prog);
        }
        
        switch (ttype) {
            case TTIMES: op = OMUL; break;
            case TDIV: op = ODIV; break;
            case TMOD: op = OMOD; break;
        }

        pushop(prog, op);
        type = RVAL;
        ttype = curtok->type;
    }

    return type;
}

// Parse an additive expression
//
static int
eadd(struct codefrag *prog)
{
    int type = emul(prog);
    enum toktyp ttype = curtok->type;
    enum codeop op;

    while (ttype == TPLUS || ttype == TMINUS) {
        if (type == LVAL) {
            torval(prog);
        }

        nextok();
        if (emul(prog) == LVAL) {
            torval(prog);
        }
        
        switch (ttype) {
        case TPLUS: op = OADD; break;
        case TMINUS: op = OSUB; break;
        }

        pushop(prog, op);
        type = RVAL;
        ttype = curtok->type;
    }

    return type;
}

// Parse a shift expression
//
static int
eshift(struct codefrag *prog)
{
    int type = eadd(prog);
    enum toktyp ttype = curtok->type;
    enum codeop op;

    while (ttype == TLSHIFT || ttype == TRSHIFT) {
        if (type == LVAL) {
            torval(prog);
        }

        nextok();
        if (eadd(prog) == LVAL) {
            torval(prog);
        }
        
        switch (ttype) {
        case TLSHIFT: op = OSHL; break;
        case TRSHIFT: op = OSHR; break;
        }

        pushop(prog, op);
        type = RVAL;
        ttype = curtok->type;
    }

    return type;
}

// Parse a relational expression
//
static int
erel(struct codefrag *prog)
{
    int type = eshift(prog);
    enum toktyp ttype = curtok->type;
    enum codeop op;
    
    if (ttype == TGT || ttype == TGE || ttype == TLE || ttype == TLT) {
        if (type == LVAL) {
            torval(prog);
        }
        nextok();
        if (eshift(prog) == LVAL) {
            torval(prog);
        }

        switch (ttype) {
        case TGT: op = OGT; break;
        case TGE: op = OGE; break;
        case TLT: op = OLT; break;
        case TLE: op = OLE; break;
        }

        pushop(prog, op);
        type = RVAL;
        ttype = curtok->type;
    }

    return type;
}

// Parse an equality expression
//
static int
eeq(struct codefrag *prog)
{
    int type = erel(prog);
    enum toktyp ttype = curtok->type;
    enum codeop op;

    if (ttype == TEQ || ttype == TNE) {
        if (type == LVAL) {
            torval(prog);
        }
        nextok();
        if (erel(prog) == LVAL) {
            torval(prog);
        }

        switch (ttype) {
        case TEQ: op = OEQ; break;
        case TNE: op = ONE; break;
        }

        pushop(prog, op);
        type = RVAL;
        ttype = curtok->type;
    }

    return type;
}

// Parse a logical AND expression
//
static int
eand(struct codefrag *prog)
{
    int type = eeq(prog);

    while (curtok->type == TAND) {
        if (type == LVAL) {
            torval(prog);
        }

        nextok();
        if (eeq(prog) == LVAL) {
            torval(prog);
        }

        pushop(prog, OAND);
    }

    return type;
}

// Parse a logical OR expression
//
static int
eor(struct codefrag *prog)
{
    int type = eand(prog);

    while (curtok->type == TOR) {
        if (type == LVAL) {
            torval(prog);
        }

        nextok();
        if (eand(prog) == LVAL) {
            torval(prog);
        }

        pushop(prog, OOR);
    }

    return type;
}

// Parse a conditional (?:) expression
//
static int
econd(struct codefrag *prog)
{
    int type = eor(prog);
    struct stabent *skip;
    struct stabent *done;

    if (curtok->type == TQUES) {
        skip = mklabel();
        done = mklabel();

        if (type == LVAL) {
            torval(prog);
        } 
        nextok();

        pushbr(prog, OBZ, skip);
        
        if (econd(prog) == LVAL) {
            torval(prog);
        }

        pushbr(prog, OJMP, done);
        pushlbl(prog, skip);

        if (curtok->type == TCOLON) {
            nextok();
            if (econd(prog) == LVAL) {
                torval(prog);
            }
            pushlbl(prog, done);
        } else {
            err(__LINE__,curtok->line, "':' expected");
        }

        type = RVAL;
    }

    return type;
}

static struct {
    enum toktyp assntok;
    enum codeop binop;
} assneqs[] = {
    { TAPLUS,   OADD },
    { TAMINUS,  OSUB },
    { TAAND,    OAND },
    { TAOR,     OOR },
    { TAEQ,     OEQ },
    { TANE,     ONE },
    { TALT,     OLT },
    { TALE,     OLE },
    { TAGT,     OGT },
    { TAGE,     OGE },
    { TALSHIFT, OSHL },
    { TARSHIFT, OSHR },
    { TAMOD,    OMOD },
    { TATIMES,  OMUL },
    { TADIV,    ODIV },
};

static const int nassneqs = sizeof(assneqs) / sizeof(assneqs[0]);

static enum codeop assnop(int line, enum toktyp type)
{
    int i;

    for (i = 0; i < nassneqs; i++) {
        if (assneqs[i].assntok == type) {
            return assneqs[i].binop;
        }
    }

    err(__LINE__,line, "internal compiler error");
    return OADD;
}

static int
eassign(struct codefrag *prog)
{
    int type = econd(prog);
    enum toktyp tt;

    // TODO =+ et al
    //
    if (curtok->type >= TAFIRST && curtok->type <= TALAST) {
        if (type != LVAL) {
            err(__LINE__,curtok->line, lvalex);
        }
        tt = curtok->type;

        nextok();

        if (tt != TASSIGN) {
            pushop(prog, ODUP);  // lval lval
            torval(prog);        // lval rval-left
        }

        if (eassign(prog) == LVAL) {
            torval(prog);
        }

        if (tt != TASSIGN) {
                                // lval rval-left rval-right
            pushop(prog, assnop(curtok->line, tt));
                                // lval rval
        }

        // we need to save the result; an assignment produces 
        // the assigned value as its result
        //
                                // lval rval
        pushop(prog, ODUP);     // lval rval rval 
        pushop(prog, OROT);     // rval lval rval
        pushop(prog, OSTORE);   // rval  ; and value is stored in memory
         
        type = RVAL;
    }

    return type;
}

// Parse an rvalue (expression)
//
int
expr(struct codefrag *frag)
{
    return eassign(frag);
}

/******************************************************************************
 *
 * Symbol table routines
 *
 */

// get or create a symbol of the given name
//
struct stabent *
stabget(struct stablist *root, const char *name)
{
    struct stabent *p;

    for (p = root->head; p; p = p->next) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }

    p = calloc(1, sizeof(struct stabent));
    strcpy(p->name, name);
    p->sc = NEW;

    if (root->head == NULL) {
        root->head = p;
    } else {
        root->tail->next = p;
    }
    root->tail = p;

    return p;
}

// Find the given symbol, either in local or global scope
//
struct stabent *
stabfind(const char *name)
{
    struct stablist *search[2] = { local, &global };
    struct stabent *stab;
    int i;

    for (i = 0; i < 2; i++) {
        if (search[i] == NULL) {
            continue;
        }

        for (stab = search[i]->head; stab; stab = stab->next) {
            if (strcmp(stab->name, name) == 0) {
                return stab;
            }
        }
    }
    return NULL;
}

// Create a temporary label in local scope
//
struct stabent *
mklabel(void)
{
    char nm[MAXNAM + 1];
    struct stabent *sym;

    sprintf(nm, "@%d", nextlab);
    sym = stabget(local, nm);

    sym->sc = INTERNAL;
    sym->type = LABEL;
    sym->labpc = nextlab++;

    return sym;
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

// append a code fragment to the end of another fragment
//
void 
cnappend(struct codefrag *fragl, struct codefrag *fragr)
{
    if (fragl->head == NULL) {
        fragl->head = fragr->head;
    } else {
        fragl->tail->next = fragr->head;
    }
    fragl->tail = fragr->tail;

    fragr->head = fragr->tail = NULL;
}

// splice a code fragment into another, after a given node
//
void
cnsplice(struct codefrag *fragl, struct codefrag *fragr, struct codenode *after)
{
    struct codenode *next = after ? after->next : fragr->head;

    if (fragl->head == NULL) {
        return;
    }

    if (after) {
        after->next = fragl->head;
    } else {
        fragr->head = fragl->head;
    }

    fragl->tail->next = next;
    if (next == NULL) {
        fragr->tail = fragl->tail;
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

// print an op with a constant arg
static void
prcon(const char *spaces, const char *op, struct constant *con)
{
    if (con->strlen == INTCONST) {
        printf("%s%s %u\n", spaces, op, con->v.intcon);
    } else {
        printf("%s%s strcon\n", spaces, op);
        hexdump(spaces, con->v.strcon, con->strlen);
    }
}

// Print a code fragment to stdout
//
static struct {
    enum codeop op;
    const char *text;
} simpleops[] = {
    { OPOP,   "POP" },
    { OPOPT,  "POPT" },
    { OPUSHT, "PUSHT" },
    { OROT,   "ROT" },
    { ODUP,   "DUP" },
    { ODEREF, "DEREF" },
    { OSTORE, "STORE" },
    { OLEAVE, "LEAVE" },
    { OCALL,  "CALL" },
    { ORET,   "RET" },
    { OADD,   "ADD" },
    { OSUB,   "SUB" },
    { OMUL,   "MUL" },
    { ODIV,   "DIV" },
    { OMOD,   "MOD" },
    { OSHL,   "SHL" },
    { OSHR,   "SHR" },
    { ONEG,   "NEG" },
    { ONOT,   "NOT" },
    { OAND,   "AND" },
    { OOR,    "OR" },
    { OEQ,    "EQ" },
    { ONE,    "NE" },
    { OLT,    "LT" },
    { OLE,    "LE" },
    { OGT,    "GT" },
    { OGE,    "GE" },
};
static int nsimpleops = sizeof(simpleops) / sizeof(simpleops[0]);

void 
cfprint(struct codefrag *frag) 
{
    static const char spaces[] = "          ";
    struct codenode *n;

    int i, j, l;
    char ch;

    for (n = frag->head; n; n = n->next) {
        if (n->op != ONAMDEF) {
            printf("%s%02u ", spaces, n->op);
        }

        for (i = 0; i < nsimpleops; i++) {
            if (n->op == simpleops[i].op) {
                break;
            }
        }

        if (i < nsimpleops) {
            printf("%s\n", simpleops[i].text);
            continue;
        }

        switch (n->op) {
        case ONAMDEF:
            printf("@%d:\n", n->arg.target->labpc);
            break;

        case OPOPN:
            printf("POPN %d\n", n->arg.n);
             break;

        case ODUPN:
            printf("DUPN %d\n", n->arg.n);
            break;

        case OENTER:
            printf("ENTER %d\n", n->arg.n);
            break;

        case OAVINIT:
            printf("AVINIT %d\n", n->arg.n);
            break;

        case OJMP:
        case OBZ:
            printf("%s @%d\n", (n->op == OJMP) ? "JMP" : "BZ", n->arg.target->labpc);
            break;

        case OPSHCON:
            prcon("", "PSHCON", &n->arg.con);
            break;

        case OPSHSYM:
            printf("PSHSYM %s FP[%d]\n", n->arg.target->name, n->arg.target->stkoffs);
            break;

        case OCASE:
            printf("OCASE %u: @%d\n", n->arg.caseval.disc, n->arg.caseval.label->labpc);
            break;
        }
    }
}

// Print a symbol that represents data
//
void 
ddprint(struct stabent *sym)
{
    struct ival *iv;
    int i;
    char idx[20];

    printf("%s", sym->name);
    if (sym->type == VECTOR) {
        printf("[%u]", sym->vecsize);
    }
    printf("\n");
    for (i = 0, iv = sym->ivals.head; iv; i++, iv = iv->next) {
        if (iv->isconst) {
            sprintf(idx, "[%d]", i);
            prcon("  ", idx, &iv->v.con);
        } else {
            printf("  [%d] %s\n", i, iv->v.name->name);
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
err(int sl,int l, const char *fmt, ...)
{
    fprintf(stderr, "%s:%d: ", srcfn, l);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, " (%d)\n", sl);
    errf = 1;
}
