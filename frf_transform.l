%{
#include <stdio.h>
#include <string.h>
#include "frf_transform.h"
#include "frf_transform.tab.h"
%}

%option noyywrap

%%
[ \t] ;

L?\"(\\.|[^\\"])*\" { yylval.str = strdup(yytext + 1); yylval.str[strlen(yylval.str) - 1] = '\0';
return STRING; }

[a-zA-Z0-9_]+ { yylval.str = strdup(yytext); return IDENT; }

\(           { return LEFT_PAREN; }
\)           { return RIGHT_PAREN; }
,            { return COMMA; }
.;

%%
