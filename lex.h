// Lexical analyzer
//
#ifndef LEX_H_
#define LEX_H_
#include <stdio.h>

enum toktyp {
    // special
    TEOF,
    TERR,

    // names/keywords
    TNAME,
    TAUTO,
    TEXTRN,
    TCASE,
    TIF,
    TELSE,
    TWHILE,
    TSWITCH,
    TGOTO,
    TRETURN,

    // braces
    TLPAREN,
    TRPAREN,
    TLBRACE,
    TRBRACE,
    TLBRACKET,
    TRBRACKET,
    TSCOLON,
    TCOMMA,

    // operators
    TPP,
    TMM,
    TMINUS,
    TNOT,
    TOR,
    TAND,
    TADDR = TAND,
    TEQ,
    TNE,
    TLT,
    TLE,
    TGT,
    TGE,
    TLSHIFT,
    TRSHIFT,
    TPLUS,
    TMOD,
    TTIMES,
    TDEREF = TTIMES,
    TDIV,
    TQUES,
    TCOLON,

    TASSIGN,


    // constants
    TINTCON,
    TSTRCON,
};

#define MAXNAM 8
#define STREOF (-1)
#define MAXSTR 256 

#define INTCONST (-1)

struct constant {
    int strlen;
    union {
        char *strcon;
        unsigned intcon;
    } v;
};

struct token {
    enum toktyp type;
    int line;
    union {
        char name[MAXNAM + 1];
        struct constant con;
    } val;
};

extern void lexinit(FILE *fp);
extern struct token *token(void);

#endif


