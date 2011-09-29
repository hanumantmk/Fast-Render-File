#include "frf.h"
#include <stdlib.h>
#include <string.h>

int main (int argc, char ** argv)
{
  char * file_name = argv[1];

  frf_t frf;

  frf_init(&frf, file_name);

  int i, goal;

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
    } else if (strcmp(argv[2], "seek") == 0) {
      goal = atoi(argv[3]);

      frf_seek(&frf, goal);

      frf_write(&frf, 1);
    }
  }

  return 0;
}
