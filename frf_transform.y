%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <dlfcn.h>
#include "frf_transform.h"
#include "frf_malloc.h"
#include "frf_maker.h"

extern int yylex();
extern int yyparse();
extern int yy_scan_bytes(const char * str, size_t size);
extern void yy_delete_buffer(int buffer);

void yyerror(char * s);

static frf_maker_t * maker;
static frf_transform_exp_t * root;

%}

%union {
  char * str;
  frf_transform_func_t * f;
  frf_transform_arg_t * args;
  frf_transform_exp_t * exp;
}

%token LEFT_PAREN RIGHT_PAREN COMMA
%token <str> STRING
%type <args> list
%type <f> func
%type <exp> expression top

%%
top:                    { root = NULL; }
           | expression { root = $1; }
	   ;

expression:
	     STRING {
	       frf_transform_exp_t * a = calloc(sizeof(frf_transform_exp_t), 1);

	       frf_maker_str2ui_t * t = NULL;

	       HASH_FIND(hh, maker->p13n_lookup, $1, strlen($1), t);

	       if (t) {
		 a->val.p = calloc(sizeof(frf_transform_p13n_t), 1);
		 a->val.p->offset = t->offset;
		 a->val.p->name = $1;
		 a->type = EXP_TYPE_P13N;
	       } else {
		 a->val.s = $1;
		 a->type = EXP_TYPE_STRING;
	       }

	       $$ = a;
	     }
	   | func {
	     frf_transform_exp_t * a = calloc(sizeof(frf_transform_exp_t), 1);
	     a->val.f = $1;
	     a->type = EXP_TYPE_FUNC;
	     $$ = a;
	   }
	   ;

func:
	     STRING LEFT_PAREN list RIGHT_PAREN {
	       char buf[300];
	       frf_transform_func_t * f = calloc(sizeof(frf_transform_func_t), 1);
	       f->name = $1;
	       f->args = $3;

	       sprintf(buf, "FRF_TRANSFORM_%s", $1);
	       f->f = dlsym(maker->sym_handle, buf);
	       if (! f->f) {
		 error(1, 0, "No symbol: %s found in symbol table", buf);
	       }

	       int i = 0;
	       frf_transform_arg_t * a;
	       DL_FOREACH(f->args, a) {
		 i++;
	       }
	       f->argc = i;

	       $$ = f;
	     }
           ;

list:
	     list COMMA expression {
	       frf_transform_arg_t * a = $1, * t;
	       frf_transform_exp_t * e = $3;
	       t = calloc(sizeof(frf_transform_arg_t), 1);
	       t->exp = e;
	       DL_APPEND(a, t);
	       $$ = a;
	     }
	   | expression {
	     frf_transform_exp_t * e = $1;
	     frf_transform_arg_t * h = NULL, * l;
	     l = calloc(sizeof(frf_transform_arg_t), 1);
	     l->exp = e;
	     DL_APPEND(h, l);
	     $$ = h;
	   }
	   ;
%%

void frf_transform_pp_expression(frf_transform_exp_t * e)
{
  frf_transform_arg_t * a;
  int i = 0;

  if (e->type == EXP_TYPE_P13N) {
    printf("%s", e->val.p->name);
  } else if (e->type == EXP_TYPE_STRING) {
    printf("%s", e->val.s);
  } else {
    printf("%s(", e->val.f->name);
    DL_FOREACH(e->val.f->args, a) {
      frf_transform_pp_expression(a->exp);

      if (++i < e->val.f->argc) {
	printf(",");
      }
    }
    printf(")");
  }
}

void frf_transform_pp(frf_transform_exp_t * e)
{
  frf_transform_pp_expression(e);
  printf("\n");
}

char * frf_transform_exec_expression(frf_malloc_context_t * c, frf_transform_exp_t * e, char ** p13n)
{
  int i;
  char * buf[100];
  char * temp;
  char * rval = NULL;
  frf_transform_arg_t * arg;
  switch (e->type) {
    case EXP_TYPE_STRING:
      rval = e->val.s;
      break;
    case EXP_TYPE_P13N:
      rval = p13n[e->val.p->offset];
      break;
    case EXP_TYPE_FUNC:
      i = 0;
      DL_FOREACH(e->val.f->args, arg) {
	temp = buf[i++] = frf_transform_exec_expression(c, arg->exp, p13n);

	if (temp == NULL) {
	  return NULL;
	}
      }
      rval = e->val.f->f(c, i, buf);
      break;
    default:
      error(1, 0, "We shouldn't be here with: %d", e->type);
  }

  return rval;
}

char * frf_transform_exec(frf_malloc_context_t ** c, frf_transform_exp_t * e, char ** p13n)
{
  frf_malloc_context_reset(c);
  char * rval = frf_transform_exec_expression(*c, e, p13n);

  if (rval) {
    return strdup(frf_transform_exec_expression(*c, e, p13n));
  } else {
    return NULL;
  }
}

frf_transform_exp_t * frf_transform_compile(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  int bs;

  maker = frf_maker;
  root = NULL;

  bs = yy_scan_bytes(str, len);
  yyparse();
  yy_delete_buffer(bs);

  return root;
}

void yyerror(char * s)
{
  error(1, 0, "Error: %s\n", s);
}

