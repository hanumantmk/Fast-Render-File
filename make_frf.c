#include "frf_maker.h"
#include <string.h>
#include <stdio.h>
#include <cdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <argp.h>
#include "utstring.h"

const char * argp_program_version = "make_frf 1.0";
const char * argp_program_bug_address = "jcarey@whoissunday.com";

struct arguments {
  char * content;
  char * output;
  char * p13n;

  char * make_cdb;
};

static struct argp_option options[] = {
  { "content",  'c', "CONTENT_FILE", 0,                   "the content file to template with" },
  { "output",   'o', "OUTPUT_fILE",  0,                   "the output filename for the frf" },
  { "p13n",     'p', "P13N_FILE",    0,                   "the output p13n file" },
  { "make_cdb", 'm', "KEY_OFFSET",   OPTION_ARG_OPTIONAL, "make a cdb off of the given field from p13n" },
  { 0 }
};

static error_t parse_opt(int key, char * arg, struct argp_state * state) {
  struct arguments * arguments = state->input;

  switch (key) {
    case 'c':
      arguments->content = arg;
      break;
    case 'o':
      arguments->output = arg;
      break;
    case 'p':
      arguments->p13n = arg;
      break;
    case 'm':
      arguments->make_cdb = arg;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static char args_doc[] = "";
static char doc[] = "make_frf -- a program to create frf files";
static struct argp argp = { options, parse_opt, args_doc, doc };

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

  struct arguments arguments = { 0 };
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  frf_maker = frf_maker_new(arguments.content, arguments.output);

  fp = fopen(arguments.p13n, "r");

  if (arguments.make_cdb) {
    utstring_new(tempfile);
    utstring_new(str);

    utstring_printf(tempfile, "%s.cdb.temp", arguments.output);
    cdb_fd = open(utstring_body(tempfile), O_RDWR | O_CREAT);
    cdb_make_start(&cdbm, cdb_fd);
    p13n_pk = atoi(arguments.make_cdb);
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

    if (arguments.make_cdb) {
      utstring_clear(str);
      utstring_printf(str, "%d", value);
      klen = strlen(key);
      vlen = strlen(utstring_body(str));
      cdb_make_add(&cdbm, key, klen, utstring_body(str), vlen);
    }
  }

  free(line);

  frf_maker_finish(frf_maker);

  if (arguments.make_cdb) {
    cdb_make_finish(&cdbm);
    utstring_clear(str);
    utstring_printf(str, "%s.cdb", arguments.output);
    rename(utstring_body(tempfile), utstring_body(str));

    utstring_free(tempfile);
    utstring_free(str);
  }

  frf_maker_destroy(frf_maker);

  return 0;
}
