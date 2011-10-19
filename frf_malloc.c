#include "frf_malloc.h"
#include <stdint.h>

#define FRF_MALLOC_ALIGNMENT_SIZE (sizeof(void *) * 2)

frf_malloc_context_t * frf_malloc_context_new(size_t size)
{
  frf_malloc_context_t * c = malloc(sizeof(*c));

  if (size < FRF_MALLOC_ALIGNMENT_SIZE * 2) {
    size = FRF_MALLOC_ALIGNMENT_SIZE * 2;
  }

  c->buf = malloc(size);
  c->buf_ptr = c->buf + FRF_MALLOC_ALIGNMENT_SIZE - sizeof(size_t);
  c->size = size;
  c->end = c->buf + size;
  c->used = FRF_MALLOC_ALIGNMENT_SIZE - sizeof(size_t);

  c->next = NULL;
  c->prev = c;

  return c;
}

char * frf_strdup(frf_malloc_context_t * c, char * str)
{
  int size = strlen(str);

  char * out = frf_malloc(c, size + 1);
  memcpy(out, str, size);
  out[size] = '\0';

  return out;
}

char * frf_strndup(frf_malloc_context_t * c, char * str, size_t len)
{
  char * out = frf_malloc(c, len + 1);
  memcpy(out, str, len);
  out[len] = '\0';

  return out;
}

char * frf_transform_printf(frf_malloc_context_t * c, const char * format, ...)
{
  int len;

  va_list args;

  char * out;

  va_start(args, format);
  len = vsnprintf(NULL, 0, format, args) + 1;
  va_end(args);

  out = frf_malloc(c, len);

  va_start(args, format);
  vsnprintf(out, len, format, args);
  va_end(args);

  return out;
}

void * frf_calloc(frf_malloc_context_t * c, size_t size, int num)
{
  void * buf = frf_malloc(c, size * num);
  memset(buf, 0, size * num);

  return buf;
}

void * frf_realloc(frf_malloc_context_t * c, void * ptr, size_t size)
{
  void * buf;

  if (ptr == NULL) {
    return frf_malloc(c, size);
  }

  size_t former_size = *((size_t *)(ptr - sizeof(size_t)));

  if (former_size < size) {
    *((size_t *)(ptr - sizeof(size_t))) = size;
    return ptr;
  } else if (former_size == size) {
    return ptr;
  } else {
    buf = frf_malloc(c, size);
    memcpy(buf, ptr, former_size);
    return buf;
  }
}

void * frf_malloc(frf_malloc_context_t * c, size_t size)
{
  void * rval;
  uint32_t * size_ptr;
  frf_malloc_context_t * new_context, * tail;
  size_t new_size;

  size += sizeof(size_t);
  size += FRF_MALLOC_ALIGNMENT_SIZE - (size % FRF_MALLOC_ALIGNMENT_SIZE);

  tail = c->prev;

  rval = tail->buf_ptr;

  tail->buf_ptr += size;

  if (tail->buf_ptr > tail->end) {
    new_size = tail->size * 2;

    if (new_size < FRF_MALLOC_ALIGNMENT_SIZE + size) {
      new_size = FRF_MALLOC_ALIGNMENT_SIZE + size * 2;
    }

    new_context = malloc(sizeof(*new_context));
    assert(new_context);
    new_context->buf = malloc(new_size);
    assert(new_context->buf);
    new_context->buf_ptr = new_context->buf + FRF_MALLOC_ALIGNMENT_SIZE - sizeof(size_t);

    new_context->size = new_size;
    new_context->end = new_context->buf + new_size;
    DL_APPEND(c, new_context);

    rval = new_context->buf_ptr;
    new_context->buf_ptr += size;
  }

  c->used += size;

  size_ptr = rval;

  *size_ptr = size - sizeof(size_t); 

  // verify that we always return aligned pointers
  assert(! ((char)(rval + sizeof(size_t)) % FRF_MALLOC_ALIGNMENT_SIZE));

  return rval + sizeof(size_t);
}

frf_malloc_context_t * frf_malloc_context_reset(frf_malloc_context_t * c) {
  frf_malloc_context_t * elt, * temp, * tail;

  tail = c->prev;

  DL_FOREACH_SAFE(c, elt, temp) {
    if (tail == elt) {
      break;
    }
    free(elt->buf);
    free(elt);
  }
  tail->buf_ptr = tail->buf + FRF_MALLOC_ALIGNMENT_SIZE - sizeof(size_t);
  tail->next = NULL;
  tail->prev = tail;
  tail->used = FRF_MALLOC_ALIGNMENT_SIZE - sizeof(size_t);

  return tail;
}

void frf_malloc_context_destroy(frf_malloc_context_t * c)
{
  frf_malloc_context_t * elt, * temp;

  DL_FOREACH_SAFE(c, elt, temp) {
    DL_DELETE(c, elt);
    free(elt->buf);
    free(elt);
  }
}
