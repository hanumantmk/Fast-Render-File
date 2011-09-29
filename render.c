#include "frf.h"

int main (int argc, char ** argv)
{
  char * file_name = argv[1];

  frf_t frf;

  frf_init(&frf, file_name);

  do {
    frf_write(&frf, 1);
  } while ( frf_next(&frf) );

  return 0;
}
