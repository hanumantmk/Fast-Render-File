#include "frf_maker.h"
#include <string.h>
#include <stdio.h>

int main (int argc, char ** argv)
{
  frf_maker_t * frf_maker;

  char * start, * end;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  char * p13n[100];

  int i;

  FILE * fp;

  frf_maker = frf_maker_new(argv[1], argv[2]);

  fp = fopen(argv[3], "r");

  while ((read = (getline(&line, &len, fp))) != -1) {
    line[read - 1] = '\0';
    start = line;
    i = 0;

    while ((end = strchr(start, '\t')) != NULL) {
      p13n[i++] = start;
      *end = '\0';
      start = end + 1;
    }
    p13n[i] = start;

    frf_maker_add(frf_maker, p13n);
  }

  free(line);

  frf_maker_finish(frf_maker);

  frf_maker_destroy(frf_maker);

  return 0;
}
