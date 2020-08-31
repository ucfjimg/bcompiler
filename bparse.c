#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"

#define MAXFNARG 64

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

struct ival {
    int isconst;
    union {
        struct constant con;
        struct stabent *name;
    } v; 
};

// stack op notation: p1 p0 /op/ r1 r0
// p1 is the next to top of stack before op, p0 is the top of stack before op,
// r1 is the next to top of stack after op, r0 is the top of stack after op
//
enum codeop {
    ONAMDEF,                        // define a name for an address
    OIVAL,                          // store a constant in memory (data def)
    OZERO,                          // put 'n' zeroes in memory (data def)
    OJMP,                           // jump to a code location
    OBZ,                            // branch if top of stack is zero, and pops condition
    OPOP,                           // p0 /pop/ 
    OPOPT,                          // p1 p0 /popt/ p1 ; T has p0 
    OPUSHT,                         // p1 p0 /pusht/ p1 p0 T
    OPOPN,                          // pn pn-1 ... p0 /popn n/ 
    ODUP,                           // p0 /dup/ p0 p0
    ODUPN,                          // pn pn-1 ... p0 /dup/ pn pn-1 ... p0 pn
    OROT,                           // p2 p1 p0 /rot/ p0 p2 p1
    OPSHCON,                        // /pshcon/ constant
    OPSHSYM,                        // /pshsym/ symbol-value
    ODEREF,                         // addr /deref/ value-ateaddr 
    OSTORE,                         // p1 p0 /store/     ; mem[p1] = p0
    OCALL,                          // pn pn-1 ... p0 fn /call(fn)/ pn pn-1 ... p0 retval 

    OADD,                           // a1 a0 /add/ a1+a0
    OSUB,                           // a1 a0 /sub/ a1-a0
    OMUL,                           // a1 a0 /mul/ a1*a0
    ODIV,                           // a1 a0 /div/ a1/a0
    OMOD,                           // a1 a0 /mod/ a1%a0
    OSHL,                           // a1 a0 /shl/ a1<<a0
    OSHR,                           // a1 a0 /shr/ a1>>a0
    ONEG,                           //    a0 /neg/ -a0
    ONOT,                           //    a0 /not/ !a0   ; always 1 or 0
    OAND,                           // a1 a0 /and/ a0&a1 ; bitwise
    OOR,                            // a1 a0 /or/  a0|a1 ; bitwise

    // these operators leave 1 for true or 0 for false on the stack
    OEQ,                            // a1 a0 /eq/  a1==a0
    ONE,                            // a1 a0 /ne/  a1!=a0
    OLE,                            // a1 a0 /le/  a1<=a0
    OLT,                            // a1 a0 /lt/  a1<a0
    OGE,                            // a1 a0 /ge/  a1>=a0
    OGT,                            // a1 a0 /gt/  a1>a0
};

struct codenode {
    struct codenode *next;
    enum codeop op;
    union {
        char name[MAXNAM+1];
        struct ival ival;
        unsigned n;
        struct stabent *target;
        struct constant con;
    } arg;
};

struct codefrag {
    struct codenode *head, *tail;
};

#define LVAL  0
#define RVAL  1

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
static void stmtextrn(void);
static void stmtlabel(struct codefrag *prog, int line, const char *nm);
static void stmtif(struct codefrag *prog);
static void stmtwhile(struct codefrag *prog);
static void datadef(struct codefrag *prog);
static void pushtok(const struct token *tok);
static void nextok(void);

static void pushop(struct codefrag *prog, enum codeop op);
static void pushopn(struct codefrag *prog, enum codeop op, unsigned n);
static void pushbr(struct codefrag *prog, enum codeop op, struct stabent *target);
static void pushlbl(struct codefrag *prog, const char *lbl);
static void torval(struct codefrag *prog);
static int expr(struct codefrag *prog);

static struct stabent *stabget(struct stabent **root, const char *name);
static struct stabent *stabfind(const char *name);
static struct stabent *mklabel(void);

static struct codenode *cnalloc(void);
static void cnpush(struct codefrag *frag, struct codenode *node);
static void cnappend(struct codefrag *fragl, struct codefrag *fragr);
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
        case TAUTO: break;
        case TEXTRN: 
            nextok();
            stmtextrn();
            statement(prog);
            break;

        case TCASE: break;
        case TIF:
            nextok();
            stmtif(prog);
            break;

        case TWHILE:
            nextok();
            stmtwhile(prog);
            break;

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
                err(curtok->line, "';' expected");
            }

            break;
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
            err(curtok->line, "name expected");
            nextok();
            break;
        }

        sym = stabget(&local, curtok->val.name);
        if (sym->sc != NEW && sym->sc != EXTERN) {
            err(curtok->line, "'%s' is already defined");
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
            err(curtok->line, "',' or ';' expected");
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

// Parse an if statement
//
void
stmtif(struct codefrag *prog)
{
    struct stabent *elsepart = mklabel();
    struct stabent *donepart = mklabel();
    
    if (curtok->type != TLPAREN) {
        err(curtok->line, "'(' expected");
        return;
    }
    nextok();
    
    if (expr(prog) == LVAL) {
        torval(prog);
    }

    if (curtok->type != TRPAREN) {
        err(curtok->line, "')' expected");
        return;
    }
    nextok();

    pushbr(prog, OBZ, elsepart);
    statement(prog);
    
    if (curtok->type != TELSE) {
        pushlbl(prog, elsepart->name); 
    } else {
        nextok();
        pushbr(prog, OJMP, donepart);
        pushlbl(prog, elsepart->name);
        statement(prog);
        pushlbl(prog, donepart->name);
    }
}

// Parse a while statement
//
void
stmtwhile(struct codefrag *prog)
{
    struct stabent *top = mklabel();
    struct stabent *bottom = mklabel();

    pushlbl(prog, top->name);

    if (curtok->type != TLPAREN) {
        err(curtok->line, "'(' expected");
        return;
    }
    nextok();
    
    if (expr(prog) == LVAL) {
        torval(prog);
    }

    if (curtok->type != TRPAREN) {
        err(curtok->line, "')' expected");
        return;
    }
    nextok();

    pushbr(prog, OBZ, bottom);
    statement(prog);
    pushbr(prog, OJMP, top);
    pushlbl(prog, bottom->name);
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
            size = curtok->val.con.v.intcon;
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

    // TODO if it's a vector, the symbol points to a word in memory,
    // which points to the base of the vector!

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
            case TSTRCON:
                cn->arg.ival.isconst = 1;
                cn->arg.ival.v.con = curtok->val.con;
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
        cn->arg.n = size - emitted;
        cnpush(prog, cn);
    }
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
static void
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
pushlbl(struct codefrag *prog, const char *lbl)
{
    struct codenode *cn = cnalloc();
    cn->op = ONAMDEF;
    strcpy(cn->arg.name, lbl);

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
            if (expr(parg) == RVAL) {
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
                err(curtok->line, "')' or ',' expected");
                nextok();
                break;
            }
        }

        if (n > MAXFNARG) {
            err(curtok->line, "too many function call args (max %d)", MAXFNARG);
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
            err(curtok->line, "'%s' is not defined", curtok->val.name);
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
            err(curtok->line, "')' expected");
        }
        break;
    }

    for(done = 0; !done;) {
        switch (curtok->type) {
        case TLPAREN:
            // TODO is fn an lval or rval? 

            nextok();
            args = ecall(prog);
            
            pushopn(prog, ODUPN, args);     // fn argn argn-1 ... arg0 fn 
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
                //torval(prog);
            }
            if (curtok->type == TRBRACKET) {
                nextok();
            } else {
                err(curtok->line, "']' expected");
            }
            
            pushop(prog, OADD);
            type = LVAL;
            break;

        case TPP:
        case TMM:
            // post-increment, post-decrement
            if (type != LVAL) {
                err(curtok->line, lvalex);
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
                err(curtok->line, lvalex);
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
                err(curtok->line, lvalex);
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
        pushlbl(prog, skip->name);

        if (curtok->type == TCOLON) {
            nextok();
            if (econd(prog) == LVAL) {
                torval(prog);
            }
            pushlbl(prog, done->name);
        } else {
            err(curtok->line, "':' expected");
        }

        type = RVAL;
    }

    return type;
}

static int
eassign(struct codefrag *prog)
{
    int type = econd(prog);

    // TODO =+ et al
    //
    if (curtok->type == TASSIGN) {
        if (type != LVAL) {
            err(curtok->line, lvalex);
        }

        nextok();

        if (eassign(prog) == LVAL) {
            torval(prog);
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

// Find the given symbol, either in local or global scope
//
struct stabent *
stabfind(const char *name)
{
    struct stabent *search[2] = { local, global };
    struct stabent *stab;
    int i;

    for (i = 0; i < 2; i++) {
        for (stab = search[i]; stab; stab = stab->next) {
            if (strcmp(stab->name, name) == 0) {
                return stab;
            }
        }
    }
    return NULL;
}

// Create a temporary label in local scope
//
struct stabent *mklabel(void)
{
    static int nextag = 1;
    char nm[MAXNAM + 1];
    struct stabent *sym;

    sprintf(nm, "@%d", nextag++);
    sym = stabget(&local, nm);

    sym->sc = INTERNAL;

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
    { OCALL,  "CALL" },
    { OADD,   "ADD" },
    { OSUB,   "SUB" },
    { OMUL,   "MUL" },
    { ODIV,   "DIV" },
    { OMOD,   "SUB" },
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
        for (i = 0; i < nsimpleops; i++) {
            if (n->op == simpleops[i].op) {
                break;
            }
        }

        if (i < nsimpleops) {
            printf("%s%s\n", spaces, simpleops[i].text);
            continue;
        }

        switch (n->op) {
        case ONAMDEF:
            printf("%s:\n", n->arg.name);
            break;

        case OIVAL:
            if (n->arg.ival.isconst) {
                prcon(spaces, "IVAL", &n->arg.ival.v.con);
            } else {
                printf("%sIVAL extrn %s\n", spaces, n->arg.ival.v.name->name);
            }
            break;

        case OPOPN:
            printf("%sPOPN %d\n", spaces, n->arg.n);
            break;

        case ODUPN:
            printf("%sDUPN %d\n", spaces, n->arg.n);
            break;

        case OZERO:
            printf("%sZERO %d\n", spaces, n->arg.n);
            break;

        case OJMP:
        case OBZ:
            printf("%s%s %s\n", spaces, (n->op == OJMP) ? "JMP" : "BZ", n->arg.target->name);
            break;

        case OPSHCON:
            prcon(spaces, "PSHCON", &n->arg.con);
            break;

        case OPSHSYM:
            printf("%sPSHSYM %s\n", spaces, n->arg.target->name);
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
