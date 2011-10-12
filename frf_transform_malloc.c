#include "frf_transform_malloc.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "error.h"
#include "utlist.h"

frf_transform_malloc_context_t * frf_transform_malloc_context_new()
{
  frf_transform_malloc_context_t * c = malloc(sizeof(frf_transform_malloc_context_t));

  c->buf_ptr = c->buf = malloc(FRF_TRANSFORM_MALLOC_BUF_SIZE);
  c->size = FRF_TRANSFORM_MALLOC_BUF_SIZE;

  c->next = NULL;
  c->prev = c;

  return c;
}

void * frf_transform_malloc(frf_transform_malloc_context_t * c, size_t size)
{
  void * rval;
  frf_transform_malloc_context_t * new_context, * tail;

  tail = c->prev;

  if ((tail->buf_ptr + size) > (tail->buf + tail->size)) {
    new_context = malloc(sizeof(frf_transform_malloc_context_t));
    new_context->buf_ptr = new_context->buf = malloc(tail->size * 2);
    new_context->size = tail->size * 2;
    DL_APPEND(c, new_context);
    return frf_transform_malloc(c, size);
  } else {
    rval = tail->buf_ptr;
    tail->buf_ptr += size;
    return rval;
  }
}

void frf_transform_malloc_context_reset(frf_transform_malloc_context_t ** c) {
  frf_transform_malloc_context_t * head, * elt, * temp, * tail;

  head = *c;

  tail = head->prev;

  DL_FOREACH_SAFE(head, elt, temp) {
    if (tail == elt) {
      break;
    }
    DL_DELETE(head, elt);
    free(elt->buf);
    free(elt);
  }
  tail->buf_ptr = tail->buf;

  *c = tail;
}

void frf_transform_malloc_context_destroy(frf_transform_malloc_context_t * c)
{
  frf_transform_malloc_context_t * elt, * temp;
  DL_FOREACH_SAFE(c, elt, temp) {
    DL_DELETE(c, elt);
    free(elt->buf);
    free(elt);
  }
}
