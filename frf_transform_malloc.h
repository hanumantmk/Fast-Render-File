#include <stdlib.h>

#ifndef FRF_TRANSFORM_MALLOC_BUF_SIZE
#define FRF_TRANSFORM_MALLOC_BUF_SIZE 2

typedef struct frf_transform_malloc_context {
  char * buf;
  char * buf_ptr;
  size_t size;
  struct frf_transform_malloc_context * next, * prev;
} frf_transform_malloc_context_t;

frf_transform_malloc_context_t * frf_transform_malloc_context_new();
void frf_transform_malloc_context_destroy(frf_transform_malloc_context_t * c);
void frf_transform_malloc_context_reset(frf_transform_malloc_context_t ** c);

void * frf_transform_malloc(frf_transform_malloc_context_t * c, size_t size);
char * frf_transform_strndup(frf_transform_malloc_context_t * c, char * str, size_t len);
char * frf_transform_strdup(frf_transform_malloc_context_t * c, char * str);
#endif
