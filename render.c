#include "frf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <argp.h>

#define BUF_SIZE 1 << 20

const char * argp_program_version = "render 1.0";
const char * argp_program_bug_address = "jcarey@whoissunday.com";

struct arguments {
  char * args[1];

  int get_offset;
  int dump_render_size;
  int mem_dump;
  int write;

  char * seek_offset;
  char * seek_row;
  int all;
};

static struct argp_option options[] = {
  { "get_offset",       'g', 0,             OPTION_ARG_OPTIONAL, "get the offset of the record in the frf" },
  { "dump_render_size", 'd', 0,             OPTION_ARG_OPTIONAL, "dump the render size of the record" },
  { "mem_dump",         'm', 0,             OPTION_ARG_OPTIONAL, "render the record into an in memory buffer before printing" },
  { "write",            'w', 0,             OPTION_ARG_OPTIONAL, "write the record out on fd 1" },

  { NULL,                 0, 0,             OPTION_DOC,          "Choose one of the below"},
  { "seek_offset",      's', "SEEK_OFFSET", 0,                   "seek in the file to a given offset" },
  { "seek_row",         'r', "SEEK_ROW",    0,                   "seek in the file to the nth record" },
  { "all",              'a', 0,             0,                   "act on all records in the file" },
  { 0 }
};

static error_t parse_opt(int key, char * arg, struct argp_state * state) {
  struct arguments * arguments = state->input;

  switch (key) {
    case 'g':
      arguments->get_offset = 1;
      break;
    case 'd':
      arguments->dump_render_size = 1;
      break;
    case 'm':
      arguments->mem_dump = 1;
      break;
    case 's':
      arguments->seek_offset = arg;
      break;
    case 'r':
      arguments->seek_row = arg;
      break;
    case 'a':
      arguments->all = 1;
      break;
    case 'w':
      arguments->write = 1;
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num >= 1) {
	argp_usage(state);
      }
      arguments->args[state->arg_num] = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 1) {
	argp_usage(state);
      }
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static char args_doc[] = "frf_filename";
static char doc[] = "render -- a program to act on a completed frf file";
static struct argp argp = { options, parse_opt, args_doc, doc };

char * buf;

void run(frf_t * frf, struct arguments arguments)
{

  int written;
  char * buf;

  if (arguments.get_offset) {
    printf("%d\n", frf_get_offset(frf));
  }
  
  if (arguments.write) {
    frf_write(frf, 1);
  }

  if (arguments.dump_render_size) {
    printf("%d\n", frf_get_render_size(frf));
  }

  if (arguments.mem_dump) {
    written = frf_render_to_buffer(frf, buf, BUF_SIZE);
    fwrite(buf, 1, written, stdout);
  }

  return;
}

int main (int argc, char ** argv)
{
  int i, goal;

  struct arguments arguments;

  arguments.get_offset       = 0;
  arguments.dump_render_size = 0;
  arguments.mem_dump         = 0;
  arguments.write            = 0;
  arguments.seek_offset      = 0;
  arguments.seek_row         = 0;
  arguments.all              = 0;

  buf = malloc(BUF_SIZE);

  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  frf_t frf;

  frf_init(&frf, arguments.args[0]);

  if (arguments.all) {
    do {
      run(&frf, arguments);
    } while ( frf_next(&frf) );
  } else if (arguments.seek_offset) {
    frf_seek(&frf, atoi(arguments.seek_offset));
    run(&frf, arguments);
  } else if (arguments.seek_row) {
    goal = atoi(arguments.seek_row);
    for (i = 0; i < goal; i++) {
      frf_next(&frf);
    }
    run(&frf, arguments);
  }

  return 0;
}
