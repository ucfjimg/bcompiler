%{
#include "b.h"
#include "lex.h"

#include <string.h>

static int lineno = 1;

extern err(int code, int line, const char *fmt, ...);

static const char unterm[] = "unterminated string or character constant";

static void strstart(void);
static void strapp(void);
static int  mkstrcon(void);
static int  mkcharcon(void);
static int  mkintcon(void);
static int  mkname(void);

static const char *toknam(int tok);
%}

%option noyywrap

%x COMMENT
%x STRING
%x CHARCON

namstrt                 [a-zA-Z_]
name                    [a-zA-Z0-9_]

%%

"\n"                    lineno++; 
[ \t]*                  ;

"/*"                    BEGIN(COMMENT);
<COMMENT>"*/"           BEGIN(INITIAL); 
<COMMENT>\n             lineno++;
<COMMENT>.              ;

\"                      { 
                            strstart();
                            BEGIN(STRING); 
                        }
<STRING>{
    \n                  {
                            err(__LINE__,lineno, unterm);
                            lineno++;
                        }

    <<EOF>>             {
                            err(__LINE__,lineno, unterm);
                        }
                        
    ([^\*\"]|\*\")      strapp(); 
    \"                  {
                            BEGIN(INITIAL); 
                            return mkstrcon();
                        }

    .                   strapp(); 
}

\'                      { 
                            strstart();
                            BEGIN(CHARCON); 
                        }
<CHARCON>{
    \n                  {
                            err(__LINE__,lineno, unterm);
                            lineno++;
                        }

    <<EOF>>             {
                            err(__LINE__,lineno, unterm);
                        }
                        
    ([^\*\']|\*\')      strapp(); 
    \'                  {
                            BEGIN(INITIAL); 
                            return mkcharcon();
                        }
    .                   strapp(); 
}


[0-9]+                  return mkintcon();

"auto"                  return TAUTO;
"extrn"                 return TEXTRN;
"case"                  return TCASE;
"if"                    return TIF;
"else"                  return TELSE;
"while"                 return TWHILE;
"switch"                return TSWITCH;
"goto"                  return TGOTO;
"return"                return TRETURN;

{namstrt}{name}*        return mkname();

"("                     return TLPAREN;
")"                     return TRPAREN;
"{"                     return TLBRACE;
"}"                     return TRBRACE;
"["                     return TLBRACKET;
"]"                     return TRBRACKET;
";"                     return TSCOLON;
","                     return TCOMMA;

"++"                    return TPP;
"--"                    return TMM;
"-"                     return TMINUS;
"!"                     return TNOT;
"|"                     return TOR;
"&"                     return TAND;
"=="                    return TEQ;
"!="                    return TNE;
"<"                     return TLT;
"<="                    return TLE;
">"                     return TGT;
">="                    return TGE;
"<<"                    return TLSHIFT;
">>"                    return TRSHIFT;
"+"                     return TPLUS;
"*"                     return TTIMES;
"%"                     return TMOD;
"/"                     return TDIV;
"?"                     return TQUES;
":"                     return TCOLON;

"="                     return TASSIGN;
"=&"                    return TAAND;
"=|"                    return TAOR;
"==="                   return TAEQ;
"=!="                   return TANE;
"=<"                    return TALT;
"=<="                   return TALE;
"=>"                    return TAGT;
"=>="                   return TAGE;
"=<<"                   return TALSHIFT;
"=>>"                   return TARSHIFT;
"=%"                    return TAMOD;
"=*"                    return TATIMES;
"=/"                    return TADIV;
"=-"                    return TAMINUS;
"=+"                    return TAPLUS;

<<EOF>>                 return TEOF;

.                       { err(__LINE__,lineno, "invalid character '%c' ignored.", yytext[0]); }
%%

static char str[MAXSTR+2];
static unsigned intcon = 0;
static char name[MAXNAM+1];

static int stridx = 0;
static int lastesc = 0;

// decode the given character as the second half of a '*x' escape
// sequence
//
static char unescape(char ch)
{
    switch (ch) {
    case '0': return '\0';
    case 'e': return STREOF;
    case '(': return '{';
    case ')': return '}';
    case 't': return '\t';
    case 'n': return '\n';
    case '*':
    case '=':
    case '\'': return ch;

    default:
        err(__LINE__, lineno, "invalid escape '*%c'.", ch);
        return ch;
    }
}

// start collecting a new string or character constant
//
void strstart(void)
{
    stridx = 0;
    lastesc = 0;
}

// append yytext to the string buffer, decoding escape
// sequences
//
void strapp(void)
{
    char *p, ch;

    for (p = yytext; (ch = *p++) != '\0';) {
        if (lastesc) {
            ch = unescape(ch);
        } else if ((lastesc = ch == '*') != 0) {
            continue;
        }
        
        if (stridx <= MAXSTR) {
            str[stridx++] = ch;
        }
    }
}

// finish up a string constant
//
int mkstrcon(void)
{
    if (stridx > MAXSTR) {
        err(__LINE__,lineno, "string constant too long (max %d).", MAXSTR);
        stridx = MAXSTR;
    }

    str[stridx] = STREOF;
    str[stridx+1] = '\0';

    return TSTRCON;
}

// finish up a character constant
//
int mkcharcon(void)
{
    char *p, ch;

    if (stridx > INTSIZE) {
        err(__LINE__,lineno, "too many characters in character constant (max %d).", INTSIZE);
        stridx = INTSIZE;
    }

    str[stridx] = '\0';

    intcon = 0;    
    for (p = str; (ch = *p++) != '\0';) {
        intcon = (intcon << 8) | (ch & 0xff);
    }

    return TINTCON;
}

// collect the name in yytext and return a name token
//
int mkname(void)
{
    if (strlen(yytext) > MAXNAM) {
        yytext[MAXNAM] = '\0';
        err(__LINE__,lineno, "identifier too long -- truncated to %s.", yytext);
    }
    strcpy(name, yytext);

    return TNAME;
}

// collect an integer constant
//
int mkintcon(void)
{
    char *p, ch;

    // if the constant starts with a '0', it's octal; else decimal
    int base = (yytext[0] == '0') ? 8 : 10;    
    
    intcon = 0;
    for (p = yytext; (ch = *p++) != '\0';) {
        intcon = (intcon * base) + (ch - '0');
    }
    
    return TINTCON;
}

// initialize the lexer
//
void lexinit(FILE *fp)
{
    yyin = fp;
}

// get a token
//
struct token *token(void)
{
    static struct token tok;

    switch (tok.type = yylex()) {
    case TINTCON:
        tok.val.con.strlen = INTCONST;
        tok.val.con.v.intcon = intcon;
//        fprintf(stderr, "TINTCON %u\n", intcon);
        break;

    case TSTRCON:
        tok.val.con.strlen = strlen(str);
        tok.val.con.v.strcon = strdup(str);
//        fprintf(stderr, "TSTRCON %.*s\n", tok.val.con.strlen, str);
        break;

    case TNAME:
        strcpy(tok.val.name, name);
//        fprintf(stderr, "TNAME %s\n", name);
        break;

    default:
//        fprintf(stderr, "%s\n", toknam(tok.type));
        break;
    }
    
    tok.line = lineno;

    return &tok;
}

const char *toknam(int tok)
{
    switch (tok) {
    case TEOF: return "TEOF";
    case TERR: return "TERR";
    case TNAME: return "TNAME";
    case TAUTO: return "TAUTO";
    case TEXTRN: return "TEXTRN";
    case TCASE: return "TCASE";
    case TIF: return "TIF";
    case TELSE: return "TELSE";
    case TWHILE: return "TWHILE";
    case TSWITCH: return "TSWITCH";
    case TGOTO: return "TGOTO";
    case TRETURN: return "TRETURN";
    case TLPAREN: return "TLPAREN";
    case TRPAREN: return "TRPAREN";
    case TLBRACE: return "TLBRACE";
    case TRBRACE: return "TRBRACE";
    case TLBRACKET: return "TLBRACKET";
    case TRBRACKET: return "TRBRACKET";
    case TSCOLON: return "TSCOLON";
    case TCOMMA: return "TCOMMA";
    case TPP: return "TPP";
    case TMM: return "TMM";
    case TMINUS: return "TMINUS";
    case TNOT: return "TNOT";
    case TOR: return "TOR";
    case TAND: return "TAND";
    case TEQ: return "TEQ";
    case TNE: return "TNE";
    case TLT: return "TLT";
    case TLE: return "TLE";
    case TGT: return "TGT";
    case TGE: return "TGE";
    case TLSHIFT: return "TLSHIFT";
    case TRSHIFT: return "TRSHIFT";
    case TPLUS: return "TPLUS";
    case TMOD: return "TMOD";
    case TTIMES: return "TTIMES";
    case TDIV: return "TDIV";
    case TQUES: return "TQUES";
    case TCOLON: return "TCOLON";
    case TASSIGN: return "TASSIGN";
    case TAAND: return "TAAND";
    case TAOR: return "TAOR";
    case TAEQ: return "TAEQ";
    case TANE: return "TANE";
    case TALT: return "TALT";
    case TALE: return "TALE";
    case TAGT: return "TAGT";
    case TAGE: return "TAGE";
    case TALSHIFT: return "TALSHIFT";
    case TARSHIFT: return "TARSHIFT";
    case TAMOD: return "TAMOD";
    case TATIMES: return "TATIMES";
    case TADIV: return "TADIV";
    case TAMINUS: return "TAMINUS";
    case TAPLUS: return "TAPLUS";
    case TINTCON: return "TINTCON";
    case TSTRCON: return "TSTRCON";
    }
    return "???";
}
