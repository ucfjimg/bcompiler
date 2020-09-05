#ifndef B_H_
#define B_H_

#include "lex.h"

#define INTSIZE 4

enum storclas {
    NEW,
    EXTERN,
    AUTO,
    INTERNAL,
};

enum objtype {
    SIMPLE,
    VECTOR,
    FUNC,
    LABEL,
};

struct codenode;
struct ival;

struct codefrag {
    struct codenode *head, *tail;
};

struct ivallist {
    struct ival *head, *tail;
};

struct stablist {
    struct stabent *head, *tail;
};

struct stabent {
    struct stabent *next;           // next in chain
    struct stabent *nextData;       // next in data decl's
    char name[MAXNAM + 1];          // symbol name
    enum storclas sc;               // storage class
    enum objtype type;              // what is it?
    struct codefrag fn;             // if a FUNC, the code
    struct codenode *cptr;          // if a LABEL, a pointer to where in the code
    int stkoffs;                    // if AUTO, offset onto stack
    int vecsize;                    // if VECTOR, the size
    struct ivallist ivals;          // if SIMPLE or VECTOR, initializers
    struct stablist scope;          // if this is a FUNC, the local symbols
    int forward;                    // only one thing can be forward defined: a goto label
    int labpc;                      // if label, where does it point?
};

struct ival {
    struct ival *next;              // next in chain
    int isconst;                    // is it a constant? else name
    union {
        struct constant con;        // if constant, the value
        struct stabent *name;       // else the symbol
    } v; 
};

enum codeop {
    ONAMDEF,                        // define a name for an address
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
    OENTER,                         // push display pointer, set display pointer to current 
                                    // stack, and reserve 'n' stack slots
    OLEAVE,                         // pop 'n' stack slots, pop dp  
    ORET,                           // return 
    OAVINIT,                        // initialize the auto vector at offset 'n'

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
    OCASE,                          // disc /case/ disc if not taken
                                    // disc /case/      if taken
};

struct codenode {
    struct codenode *next;
    int pc;
    enum codeop op;
    union {
        char name[MAXNAM+1];
        struct ival ival;
        unsigned n;
        struct stabent *target;
        struct constant con;
        struct {
            unsigned disc;
            struct stabent *label;
        } caseval;
    } arg;
};

#endif


