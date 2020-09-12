%{
#include "ast.h"
#include "parser.h"
#include "jfl-parser.h"

#include <stdlib.h>
#include <stdarg.h>

extern void yyerror(const char *, ...);

static char* make_str(char *s);
%}

digit  [0-9]
letter [a-zA-Z]
white  [ \t\r\n]

%%

"//".*   { /* comment */ }

{white}+ { /* skip white space */ }

-?{digit}+ { yylval.intcon = strtol(yytext, NULL, 10); return NUM; }


\"(\\.|[^"\\])*\"  { yylval.str = make_str(yytext); return STR; }

"let"    { return LET; }
"as"     { return AS; }
"fun"    { return FUN; }
"end"    { return END; }
"if"     { return IF; }
"then"   { return THEN; }
"else"   { return ELSE; }
"+"      { return '+'; }
"-"      { return '-'; }
"*"      { return '*'; }
"/"      { return '/'; }
";"      { return ';'; }
"="      { return '='; }
"<"      { return '<'; }
">"      { return '>'; }
"<="     { return LE; }
">="     { return GE; }
"("      { return '('; }
")"      { return ')'; }
"["      { return '['; }
"]"      { return ']'; }
"{"      { return '{'; }
"}"      { return '}'; }
":"      { return ':'; }
"`"      { return '`'; }
","      { return ','; }


({letter}|"_")({letter}|{digit}|"_")*  { yylval.id = strdup(yytext); return IDENTIFIER; } 

.        { yyerror("invalid character `%c' in source", yytext[0]); }
%%

int yywrap() {
  return 1;
}

void yyerror(const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  fprintf(stderr, "\n");
  va_end(va);
  exit(1);
}

/* encode a string */
char* make_str(char *str) {
  /* NB we have at least "" */
  char *s = str + 1;
  s[strlen(s) - 1] = '\0';
  char *d = s;

  while (*s) {
    if (*s == '\\') {
      s++;
      if (!*s) {
        break;
      }

      switch (*s) {
      case 'n':
        *d = '\n';
        break;

      case 't':
        *d = '\t';
        break;
      }

      d++, s++;
    } else {
      *d++ = *s++;
    }   
  }

  *d = '\0';

  return strdup(str + 1);
  
}

