#include "frf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 1 << 20

int main (int argc, char ** argv)
{
  char * file_name = argv[1];

  frf_t frf;

  frf_init(&frf, file_name);

  int i, goal;

  int written;
  char * buf;

  if (argc == 2) {
      do {
	frf_write(&frf, 1);
      } while ( frf_next(&frf) );
  } else {
    if (strcmp(argv[2], "get_offset") == 0) {
      goal = atoi(argv[3]);

      for (i = 0; i < goal; i++) {
	frf_next(&frf);
      }

      printf("%d\n", frf_get_offset(&frf));
    } else if (strcmp(argv[2], "dump_offsets") == 0) {
      do {
	printf("%d\n", frf_get_offset(&frf));
      } while ( frf_next(&frf) );
    } else if (strcmp(argv[2], "dump_render_size") == 0) {
      do {
	printf("%d\n", frf_get_render_size(&frf));
      } while ( frf_next(&frf) );
    } else if (strcmp(argv[2], "mem_dump") == 0) {
      buf = malloc(BUF_SIZE);
      do {
	written = frf_render_to_buffer(&frf, buf, BUF_SIZE);
	fwrite(buf, 1, written, stdout);
      } while ( frf_next(&frf) );
    } else if (strcmp(argv[2], "seek") == 0) {
      goal = atoi(argv[3]);

      frf_seek(&frf, goal);

      frf_write(&frf, 1);
    }
  }

  return 0;
}
