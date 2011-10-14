#include "frf_maker.h"
#include <string.h>
#include <stdio.h>
#include <cdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "utstring.h"

int main (int argc, char ** argv)
{
  frf_maker_t * frf_maker;

  char * start, * end;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  char * p13n[100];

  UT_string * str, * tempfile;

  struct cdb_make cdbm;
  int cdb_fd;
  char * key;
  int value;
  uint32_t klen, vlen;
  int p13n_pk;

  int i;

  FILE * fp;

  frf_maker = frf_maker_new(argv[1], argv[2]);

  fp = fopen(argv[3], "r");

  if (argc == 5) {
    utstring_new(tempfile);
    utstring_new(str);

    utstring_printf(tempfile, "%s.cdb.temp", argv[2]);
    cdb_fd = open(utstring_body(tempfile), O_RDWR | O_CREAT);
    cdb_make_start(&cdbm, cdb_fd);
    p13n_pk = atoi(argv[4]);
  }

  while ((read = (getline(&line, &len, fp))) != -1) {
    line[read - 1] = '\0';
    start = line;
    i = 0;

    while ((end = strchr(start, '\t')) != NULL) {
      p13n[i] = start;
      *end = '\0';

      if (argc == 5 && i == p13n_pk) {
	key = start;
      }

      start = end + 1;
      i++;
    }
    p13n[i] = start;

    value = frf_maker_add(frf_maker, p13n);

    if (argc == 5) {
      utstring_clear(str);
      utstring_printf(str, "%d", value);
      klen = strlen(key);
      vlen = strlen(utstring_body(str));
      cdb_make_add(&cdbm, key, klen, utstring_body(str), vlen);
    }
  }

  free(line);

  frf_maker_finish(frf_maker);

  if (argc == 5) {
    cdb_make_finish(&cdbm);
    utstring_clear(str);
    utstring_printf(str, "%s.cdb", argv[2]);
    rename(utstring_body(tempfile), utstring_body(str));

    utstring_free(tempfile);
    utstring_free(str);
  }

  frf_maker_destroy(frf_maker);

  return 0;
}
