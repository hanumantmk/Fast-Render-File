#include "frf_malloc.h"

frf_malloc_context_t * frf_malloc_context_new(size_t size)
{
  frf_malloc_context_t * c = malloc(sizeof(frf_malloc_context_t));

  c->buf_ptr = c->buf = malloc(size);
  c->size = size;
  c->end = c->buf + size;
  c->used = 0;

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
  len = vsnprintf(NULL, 0, format, args);
  va_end(args);

  out = frf_malloc(c, len + 1);

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

void * frf_malloc(frf_malloc_context_t * c, size_t size)
{
  void * rval;
  frf_malloc_context_t * new_context, * tail;
  size_t new_size;

  tail = c->prev;

  rval = tail->buf_ptr;

  tail->buf_ptr += size;

  if (tail->buf_ptr > tail->end) {
    new_size = tail->size * 2;

    if (new_size < size) {
      new_size = size * 2;
    }
    new_context = malloc(sizeof(frf_malloc_context_t));
    new_context->buf_ptr = new_context->buf = malloc(new_size);
    new_context->size = new_size;
    new_context->end = new_context->buf_ptr + new_size;
    DL_APPEND(c, new_context);

    rval = new_context->buf_ptr;
  }

  c->used += size;

  return rval;
}

frf_malloc_context_t * frf_malloc_context_reset(frf_malloc_context_t * c) {
  frf_malloc_context_t * head, * elt, * temp, * tail;

  head = c;

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
  tail->next = NULL;
  tail->prev = tail;
  tail->used = 0;

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
