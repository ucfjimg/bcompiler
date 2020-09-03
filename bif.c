#include "bif.h"

#include "b.h"
#include "lex.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int err = 0;
static FILE *fp;
static char *strpool;
static int strpnext;
static int strpsize;

static void wrdata(struct stabent *syms);
static void wrcode(struct stabent *syms);
static void wrfunc(struct stabent *func);
static void wrbytes(unsigned val, int bytes);
static void wrname(const char *name);
static void wrchars(const char *str, int bytes);
static int  strpadd(const char *str, int len);
static int  wrstrp(void);


#define WRINT(v) wrbytes(v, INTSIZE)
#define WRBYTE(v) wrbytes(v, 1)

// write the intermediate file
//
int 
bifwrite(const char *fn, struct stabent *syms)
{
    if ((fp = fopen(fn, "wb")) == NULL) {
        perror(fn);
        return 2;
    }

    WRINT(BIFMAGIC);

    wrdata(syms);
    wrcode(syms);
    wrstrp();

    if (fclose(fp) == EOF) {
        err = 1;
    }

    if (err) {
        remove(fn);
        fprintf(stderr, "I/O error on %s -- disk full?", fn);
        return 2;
    }
    return 0;
}

// write out data definitions
//
void 
wrdata(struct stabent *syms)
{
    int ndata = 0;
    struct stabent *symp;
    struct ival *ivp;
    int len, fl;

    for (symp = syms; symp; symp = symp->next) {
        if (symp->sc == EXTERN && (symp->type == SIMPLE || symp->type == VECTOR)) {
            ndata++;
        }
    }

    WRINT(ndata);

    for (symp = syms; symp; symp = symp->next) {
        if (symp->sc != EXTERN || (symp->type != SIMPLE && symp->type != VECTOR)) {
            continue;
        }
        
        wrname(symp->name);

        fl = 0;
        if (symp->type == VECTOR) {
            fl |= BIFVEC;
        }
        WRBYTE(fl);

        if (symp->type == VECTOR) {
            WRINT(symp->vecsize);
        }
        
        ndata = 0;
        for (ivp = symp->ivals.head; ivp; ivp = ivp->next) {
            ndata++;
        }
        WRINT(ndata);

        for (ivp = symp->ivals.head; ivp; ivp = ivp->next) {
            if (!ivp->isconst) {
                WRBYTE(BIFINAM);
                wrname(ivp->v.name->name);
            } else if (ivp->v.con.strlen == INTCONST) {
                WRBYTE(BIFIINT);
                WRINT(ivp->v.con.v.intcon);
            } else {
                WRBYTE(BIFISTR);
                WRINT(strpadd(ivp->v.con.v.strcon, ivp->v.con.strlen));
            }
        }
    }
}

// write out functions
//
void
wrcode(struct stabent *syms)
{
    int ncode = 0;
    struct stabent *symp;

    for (symp = syms; symp; symp = symp->next) {
        if (symp->sc == EXTERN && symp->type == FUNC) {
            ncode++;
        }
    }

    WRINT(ncode);

    for (symp = syms; symp; symp = symp->next) {
        if (symp->sc != EXTERN || symp->type != FUNC) {
            continue;
        }

        wrname(symp->name);
        wrfunc(symp);
    }
}

// write out a function
//
void
wrfunc(struct stabent *func)
{
    int pc = 0;
    struct codenode *cn;
    struct stabent *sym;
    int names = 0;
    int exidx = 0;

    // TODO hoist all the externs up into one big de-duplicated table
    for (sym = func->scope.head; sym; sym = sym->next) {
        if (sym->sc == EXTERN) {
            sym->labpc = exidx++;
        }
    }

    WRINT(exidx);
    for (sym = func->scope.head; sym; sym = sym->next) {
        if (sym->sc == EXTERN) {
            wrname(sym->name);
        }
    }

    for (cn = func->fn.head; cn; cn = cn->next) {
        if (cn->op == ONAMDEF) {
            names++;
        }
    } 

    for (cn = func->fn.head; cn; cn = cn->next, pc++) {
        if (cn->op == ONAMDEF) {
            for (sym = func->scope.head; sym; sym = sym->next) {
                if (strcmp(cn->arg.name, sym->name) == 0) {
                    break;
                }
            }

            if (sym == NULL) {
                fprintf(stderr, "undefined name '%s' in intermediate code\n", cn->arg.name);
                err = 1;
            }

            sym->labpc = pc;
        }
    } 

    WRINT(pc);

    for (cn = func->fn.head; cn; cn = cn->next, pc++) {
        WRBYTE(cn->op);
        switch (cn->op) {
        case OJMP:
        case OBZ:
            WRINT(cn->arg.target->labpc);
            break;

        case OPOPN:
        case ODUPN:
        case OENTER:
        case OLEAVE:
            WRINT(cn->arg.n);
            break;

        case OPSHCON:
            WRBYTE(cn->arg.con.strlen == INTCONST ? 0 : 1);
            if (cn->arg.con.strlen == INTCONST) {
                WRINT(cn->arg.con.v.intcon);
            } else {
                WRINT(strpadd(cn->arg.con.v.strcon, cn->arg.con.strlen));
            }
            break;

        case OPSHSYM:
            WRBYTE(cn->arg.target->sc == EXTERN ? 0 : 1);
            if (cn->arg.target->sc) {
                WRINT(cn->arg.target->labpc);
            } else if (cn->arg.target->sc != AUTO) {
                fprintf(stderr, "internal compiler error: OPSHSYM neither EXTERN nor AUTO\n");
                err = 1;
            } else {
                WRINT(cn->arg.target->stkoffs);
            }
            break;
        }
    }
}

// write a value out in a given number of bytes
//
void 
wrbytes(unsigned val, int bytes)
{
    const unsigned maxval = (1u << 8 * bytes) - 1;

    if (!err) {
        if (bytes > sizeof(unsigned)) {
            fprintf(stderr, "internal compiler error - target int size is larger than unsigned\n");
            err = 1;
            return;
        }

        if (val > maxval) {
            fprintf(stderr, "program too large");
            err = 1;
            return;
        }
    }

    while (bytes--) {
        if (fputc(val & 0xff, fp) == EOF) {
            err = 1;
        }
        val >>= 8;
    }
}

// write an identifier
//
void 
wrname(const char *name)
{
    int len = strlen(name);
    WRBYTE(len);
    wrchars(name, len);
}

// write a byte buffer 
//
void 
wrchars(const char *str, int bytes)
{
    if (fwrite(str, 1, bytes, fp) != bytes) {
        err = 1;
    }
}

// Add a string constant to the string pool
//
int 
strpadd(const char *str, int len)
{
    int newsize = strpsize ? strpsize : 16;
    char *newp;
    int offset;

    if (err) {
        return EOF;
    }

    while (strpnext + len > newsize) {
        newsize *= 2;
    }

    if (newsize > strpsize) {
        if ((newp = realloc(strpool, newsize)) == NULL) {
            if (!err) {
                fprintf(stderr, "out of memory\n");
                err = 1;
                return EOF;
            }
        }
        strpsize = newsize;
        strpool = newp;
    }

    offset = strpnext;
    memcpy(strpool + strpnext, str, len);
    strpnext += len;

    return offset;
}

// Write the string pool
//
int
wrstrp()
{
    WRINT(strpnext);
    wrchars(strpool, strpnext);
}
