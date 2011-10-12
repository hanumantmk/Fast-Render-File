#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "frf_transform.h"

char * FRF_TRANSFORM_trunc(frf_transform_malloc_context_t * c, int argc, char ** argv)
{
  int len, size;

  len = strlen(argv[0]);
  size = atoi(argv[1]);

  if (size > len) {
    len = size;
  }

  char * buf = frf_transform_malloc(c, len + 1);
  memcpy(buf, argv[0], len + 1);

  buf[size] = '\0';

  return buf;
}

char * FRF_TRANSFORM_add(frf_transform_malloc_context_t * c, int argc, char ** argv)
{
  int i, acc = 0;
  char buf[100];

  for (i = 0; i < argc; i++) {
    acc += atoi(argv[i]);
  }

  sprintf(buf, "%d", acc);

  return frf_transform_strdup(c, buf);
}
