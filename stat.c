#include "frf.h"
#include <stdlib.h>
#include <string.h>

int main (int argc, char ** argv)
{
  char * file_name = argv[1];

  frf_t frf;

  frf_init(&frf, file_name);

  int total_size         = (char *)frf._end - (char *)frf._mmap_base;
  int string_table_size  = (char *)frf._vector_header_base - (char *)frf._mmap_base - 12;
  int vector_header_size = (char *)frf._vector_base - (char *)frf._vector_header_base;
  int row_data_size      = (char *)frf._end - (char *)frf._vector_base;

  printf("\
FRF file          %s\n\
Total Size        %d\n\
  String Table    %d\n\
  Vector Header   %d\n\
  Row Data        %d\n\
Num Records       %d\n\
", file_name, total_size, string_table_size, vector_header_size, row_data_size, frf.num_rows);

  return 0;
}
