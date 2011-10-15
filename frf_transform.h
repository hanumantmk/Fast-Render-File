#include <stdint.h>
#include "frf_malloc.h"

#ifndef FRF_TRANSFORM_DEFINE
#define FRF_TRANSFORM_DEFINE

struct frf_maker;

enum EXP_TYPE {
  EXP_TYPE_P13N,
  EXP_TYPE_STRING,
  EXP_TYPE_FUNC
};

typedef struct frf_transform_exp {
  enum EXP_TYPE type;

  union {
    char * s;
    struct frf_transform_p13n * p;
    struct frf_transform_func * f;
  } val;
} frf_transform_exp_t;

typedef struct frf_transform_arg {
  frf_transform_exp_t * exp;

  struct frf_transform_arg * next, * prev;
} frf_transform_arg_t;

typedef struct frf_transform_func {
  frf_transform_arg_t * args;
  int argc;
  char * (*f)(frf_malloc_context_t *, int, char **);
  char * name;
} frf_transform_func_t;

typedef struct frf_transform_p13n {
  char * name;
  uint32_t offset;
} frf_transform_p13n_t;

frf_transform_exp_t * frf_transform_compile(struct frf_maker * frf_maker, char * str, uint32_t len);
char * frf_transform_exec(frf_malloc_context_t * c, frf_transform_exp_t * e, char ** p13n);
void frf_transform_destroy(frf_transform_exp_t * e);
void frf_transform_pp(frf_transform_exp_t * e);

#endif
