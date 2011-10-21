#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "frf_transform.h"
#include "utstring.h"

char * FRF_TRANSFORM_trunc(frf_malloc_context_t * c, int argc, char ** argv)
{
  int len, size;

  if (argc != 2) {
    return NULL;
  }

  len = strlen(argv[0]);
  size = atoi(argv[1]);

  if (size > len) {
    size = len;
  }

  char * buf = frf_malloc(c, size + 1);
  memcpy(buf, argv[0], size);

  buf[size] = '\0';

  return buf;
}

char * FRF_TRANSFORM_add(frf_malloc_context_t * c, int argc, char ** argv)
{
  int i, acc = 0;

  for (i = 0; i < argc; i++) {
    acc += atoi(argv[i]);
  }

  return frf_printf(c, "%d", acc);
}

char * FRF_TRANSFORM_join(frf_malloc_context_t * c, int argc, char ** argv)
{
  int i;
  char * join_str = argv[0];
  char * out;

  UT_string * s;
  utstring_new(s);

  for (i = 1; i < argc - 1; i++) {
    utstring_bincpy(s, argv[i], strlen(argv[i]));
    utstring_bincpy(s, join_str, strlen(join_str));
  }
  utstring_bincpy(s, argv[i], strlen(argv[i]));

  out = frf_strndup(c, utstring_body(s), utstring_len(s));

  utstring_free(s);

  return out;
}
