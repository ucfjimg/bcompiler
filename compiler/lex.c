#include "lex.h"

#include "b.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern err(int sl, int line, const char *fmt, ...);

int line = 1;
static int currch;
static FILE *fp;

struct keyword {
    char *name;
    enum toktyp type;
};

static struct keyword keywords[] = {
    { "auto",   TAUTO },
    { "extrn",  TEXTRN },
    { "case",   TCASE },
    { "if",     TIF },
    { "else",   TELSE },
    { "while",  TWHILE },
    { "switch", TSWITCH },
    { "goto",   TGOTO },
    { "return", TRETURN },
};
static const int nkeyword = sizeof(keywords) / sizeof(keywords[0]);

static enum toktyp binop(void);
static void name(struct token *t);
static void intcon(struct token *t);
static void charcon(struct token *t);
static void strcon(struct token *t);
static void punc(struct token *t);
static void assigns(struct token *t);
static int escape(int ch);

static int advskipws(void);
static int skipws(void);
static void advance(void);
static void advraw(void);

static int get(void);
static void unget(int ch);

// initialize the lexer
//
void
lexinit(FILE *fin)
{
    fp = fin;
    advance();
}

// get a token
//
struct token *
token(void)
{
    static struct token tok;

    skipws();

    do {
        skipws();
        tok.line = line;

        if (currch == EOF) {
            tok.type = TEOF;
        } else if (isalpha(currch) || currch == '_') {
            name(&tok);
        } else if (isdigit(currch)) {
            intcon(&tok);
        } else if (currch == '\'') {
            charcon(&tok);
        } else if (currch == '"') {
            strcon(&tok);
        } else {
            punc(&tok);
        }
    } while (tok.type == TERR);

    return &tok;
}
     
// Parse a name, checking the keyword table for reserved words
// as well.
//
void 
name(struct token *t)
{
    int i = 0;
    struct keyword *kw;

    while (isalnum(currch) || currch == '_') {
        if (i < MAXNAM) {
            t->val.name[i] = currch;
        }
        i++;
        advraw();
    }

    if (i <= MAXNAM) {
        t->val.name[i] = 0;
    } else {
        err(__LINE__,line, "name too long");
    }

    t->type = TNAME;

    for (kw = keywords; kw < keywords + nkeyword; kw++) {
        if (strcmp(kw->name, t->val.name) == 0) {
            t->type = kw->type;
            break;
        }    
    }
}

// Parse an integer constant
// 
void
intcon(struct token *t)
{
    int base = (currch == '0') ? 8 : 10;
    unsigned val = 0;

    while (isdigit(currch)) {
        val = val * base + currch - '0';
        advraw();
    }

    t->type = TINTCON;
    t->val.con.strlen = INTCONST;
    t->val.con.v.intcon = val;
}

// Parse a character constant
//
void 
charcon(struct token *t)
{
    int i;
    unsigned val = 0;

    advraw();

    for (i = 0; i < INTSIZE; i++, advraw()) {
        if (currch == EOF) {
            err(__LINE__,line, "unterminated char constant");
            break;
        }

        if (currch == '\'') {
            if (i == 0) {
                err(__LINE__,line, "empty char constant");
            }
            break;
        }

        if (currch == '*') {
            advraw();
            currch = escape(currch);
        }

        val = (val << 8) | (currch & 0xff);
    }

    if (i == INTSIZE && currch != '\'') {
        err(__LINE__,line, "unterminated char constant");
        return;
    }

    advskipws();

    t->type = TINTCON;
    t->val.con.strlen = INTCONST;
    t->val.con.v.intcon = val;
}

// parse a string constant
//
void 
strcon(struct token *t)
{
    static char buf[MAXSTR];
    int idx = 0;
    int here = line;

    for(advraw();; advraw()) {
        if (currch == EOF) {
            err(__LINE__,here, "unterminated string constant");
            break;
        }

        if (currch == '"') {
            if (idx < MAXSTR) {
                buf[idx] = STREOF;
            }
            idx++;
            advskipws();
            break;
        }

        if (currch == '*') {
            advraw();
            currch = escape(currch);
        }

        if (idx < MAXSTR) {
            buf[idx] = currch;
        }
        idx++;
    }

    if (idx > MAXSTR) {
        err(__LINE__,here, "string constant too long (max %d).", MAXSTR);
    }

    t->type = TSTRCON;
    t->val.con.strlen = idx;
    t->val.con.v.strcon = malloc(idx);
    memcpy(t->val.con.v.strcon, buf, idx);
}

// Parse operators and other punctuation
//
void
punc(struct token *tok)
{
    switch (currch) {
    case '\'':
        charcon(tok);
        return;

    case '"':
        strcon(tok);
        break;

    case '+':
        advraw();
        tok->type = TPLUS;
        if (currch == '+') {
            advraw();
            tok->type = TPP;
        }
        break;

    case '-':
        advraw();
        tok->type = TMINUS;
        if (currch == '-') {
            advraw();
            tok->type = TMM;
        }
        break;

    case '!':
        advraw();
        tok->type = TNOT;
        if (currch == '=') {
            advraw();
            tok->type = TNE;
        }
        break;

    case '|':
        advraw();
        tok->type = TOR;
        break;

    case '&':
        advraw();
        tok->type = TAND;
        break;

    case '<':
        advraw();
        tok->type = TLT;
        if (currch == '=') {
            advraw();
            tok->type = TLE;
        } else if (currch == '<') {
            advraw();
            tok->type = TLSHIFT;
        }
        break;

    case '>':
        advraw();
        tok->type = TGT;
        if (currch == '=') {
            advraw();
            tok->type = TGE;
        } else if (currch == '>') {
            advraw();
            tok->type = TRSHIFT;
        }
        break;

    case '%':
        advraw();
        tok->type = TMOD;
        break;

    case '*':
        advraw();
        tok->type = TTIMES;
        break;

    case '/':
        advraw();
        tok->type = TDIV;
        break;

    case '?': 
        advraw();
        tok->type = TQUES;
        break;

    case ':': 
        advraw();
        tok->type = TCOLON;
        break;

    case '(': 
        advraw();
        tok->type = TLPAREN;
        break;

    case ')': 
        advraw();
        tok->type = TRPAREN;
        break;

    case '[': 
        advraw();
        tok->type = TLBRACKET;
        break;

    case ']': 
        advraw();
        tok->type = TRBRACKET;
        break;

    case '{': 
        advraw();
        tok->type = TLBRACE;
        break;

    case '}': 
        advraw();
        tok->type = TRBRACE;
        break;

    case ',': 
        advraw();
        tok->type = TCOMMA;
        break;

    case ';': 
        advraw();
        tok->type = TSCOLON;
        break;

    case '=':
        advraw();
        tok->type = TASSIGN;
        assigns(tok);
        break;

    default:
        tok->type = TERR;
        err(__LINE__,line, "invalid character '%c' in source", currch);
        advraw();
        break;
    }
}

// Check for =x style assignment operators
//
void 
assigns(struct token *tok)
{
    switch (currch) {
    case '=':
        tok->type = TEQ;
        advraw();
        if (currch == '=') {
            tok->type = TAEQ;
            advraw();
        }
        break;

    case '!':
        advraw();
        if (currch == '=') {
            tok->type = TANE;
            advraw();
        } else {
            err(__LINE__,line, "'=!' is not a valid token");
        }
        break;

    case '<':
        tok->type = TALT;
        advraw();
        if (currch == '=') {
            tok->type = TALE;
            advraw();
        } else if (currch == '<') {
            tok->type = TALSHIFT;
            advraw();
        }
        break;

    case '>':
        tok->type = TAGT;
        advraw();
        if (currch == '=') {
            tok->type = TAGE;
            advraw();
        } else if (currch == '<') {
            tok->type = TARSHIFT;
            advraw();
        }
        break;

    case '+':
        tok->type = TAPLUS;
        advraw();
        break;

    case '-':
        tok->type = TAMINUS;
        advraw();
        break;

    case '|':
        tok->type = TAOR;
        advraw();
        break;

    case '&':
        tok->type = TAAND;
        advraw();
        break;

    case '%':
        tok->type = TAMOD;
        advraw();
        break;

    case '*':
        tok->type = TATIMES;
        advraw();
        break;

    case '/':
        tok->type = TADIV;
        advraw();
        break;

    }
}

// translate an escape sequence into the character it
// represents.
//
int 
escape(int ch)
{
    switch (ch) {
    case '0': return 0;
    case 'e': return STREOF;
    case '(': return '{';
    case ')': return '}';
    case 't': return '\t';
    case 'n': return '\n';
    case '*': case '=': case '\'':
        return ch;

    default:
        err(__LINE__,line, "invalid escape '%c'", ch);
        break;
    }

    return ch;
}

// advance to the next character and consume any whitespace
// after it, landing on the first non-whitespace character
// and returning it
//
int
advskipws(void)
{
    advance();
    return skipws();
}

// skip white space, including comments, and return the next
// non white space character.
//
int 
skipws(void)
{
    while (isspace(currch)) {
        advance();
    }
    return currch;
}

// Advance to the next character, replacing comments w/ a single space.
//
void 
advance()
{
    int here = line;
    int peekch;
    int star;

    if ((currch = get()) != '/') {
        return;
    }

    if ((peekch = get()) != '*') {
        unget(peekch);
        return;
    }
    
    for(star = 0;;) {
        if ((currch = get()) == EOF) {
            err(__LINE__,here, "unterminated comment.");
            break;
        }

        if (star && currch == '/') {
            currch = ' ';
            break;
        }

        star = currch == '*';
    }
}

// Advance to the next character, ignoring comments
//
void
advraw()
{
    currch = get();
}

// get a raw character, counting lines
//
int 
get(void)
{
    int ch = fgetc(fp);
    if (ch == '\n') {
        line++;
    }
    return ch;
}

// put back a raw character, counting lines
//
void 
unget(int ch)
{
    if (ch == '\n') {
        line--;
    }
    ungetc(ch, fp);
}

