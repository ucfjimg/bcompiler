// Lexical analyzer
//
#ifndef LEX_H_
#define LEX_H_

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

struct token {
    enum toktyp type;
    int line;
    union {
        char name[MAXNAM + 1];
        unsigned intcon;
        struct {
            char *bytes;
            int len;
        } strcon;
    } val;
};

extern struct token *token(void);

#endif


