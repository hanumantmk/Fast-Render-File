#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <error.h>
#include "utlist.h"

#ifndef FRF_MALLOC
#define FRF_MALLOC

typedef struct frf_malloc_context {
  char * buf;
  char * buf_ptr;
  char * end;
  size_t size;
  size_t used;
  struct frf_malloc_context * next, * prev;
} frf_malloc_context_t;

frf_malloc_context_t * frf_malloc_context_new(size_t size);
void frf_malloc_context_destroy(frf_malloc_context_t * c);
frf_malloc_context_t * frf_malloc_context_reset(frf_malloc_context_t * c);

void * frf_malloc(frf_malloc_context_t * c, size_t size);
void * frf_calloc(frf_malloc_context_t * c, size_t size, int num);
void * frf_realloc(frf_malloc_context_t * c, void * ptr, size_t size);

char * frf_strndup(frf_malloc_context_t * c, char * str, size_t len);
char * frf_strdup(frf_malloc_context_t * c, char * str);
char * frf_transform_printf(frf_malloc_context_t * c, const char * format, ...);
#endif
