%{
#include <stdio.h>
#include <string.h>
#include "frf_transform.h"
#include "frf_transform.tab.h"
%}

%option noyywrap

%%
[ \t] ;
[a-zA-Z0-9_]+ {
               yylval.str = strdup(yytext);
               return STRING;
             }

\(           { return LEFT_PAREN; }
\)           { return RIGHT_PAREN; }
,            { return COMMA; }
.;

%%
