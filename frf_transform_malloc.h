#include <stdlib.h>

#ifndef FRF_TRANSFORM_MALLOC_BUF_SIZE
#define FRF_TRANSFORM_MALLOC_BUF_SIZE 1000

typedef struct frf_transform_malloc_context {
  char * buf;
  char * buf_ptr;
  size_t size;
  struct frf_transform_malloc_context * next, * prev;
} frf_transform_malloc_context_t;

void * frf_transform_malloc(frf_transform_malloc_context_t * c, size_t size);
frf_transform_malloc_context_t * frf_transform_malloc_context_new();
void frf_transform_malloc_context_destroy(frf_transform_malloc_context_t * c);
void frf_transform_malloc_context_reset(frf_transform_malloc_context_t ** c);
#endif
