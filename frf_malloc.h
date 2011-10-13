#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <error.h>
#include "utlist.h"

#ifndef FRF_TRANSFORM_MALLOC_BUF_SIZE
#define FRF_TRANSFORM_MALLOC_BUF_SIZE 2

typedef struct frf_malloc_context {
  char * buf;
  char * buf_ptr;
  size_t size;
  struct frf_malloc_context * next, * prev;
} frf_malloc_context_t;

frf_malloc_context_t * frf_malloc_context_new();
void frf_malloc_context_destroy(frf_malloc_context_t * c);
void frf_malloc_context_reset(frf_malloc_context_t ** c);

void * frf_malloc(frf_malloc_context_t * c, size_t size);
char * frf_strndup(frf_malloc_context_t * c, char * str, size_t len);
char * frf_strdup(frf_malloc_context_t * c, char * str);
char * frf_transform_printf(frf_malloc_context_t * c, const char * format, ...);
#endif
