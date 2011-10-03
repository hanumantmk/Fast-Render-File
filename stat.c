#include "frf.h"
#include "uthash.h"
#include <stdlib.h>
#include <string.h>

struct frf_uniq_string {
  int id;
  int cnt;
  UT_hash_handle hh;
}; 

void print_vector_info (frf_t * frf)
{
  int small  = 0;
  int medium = 0;
  int large  = 0;

  int uniq_small  = 0;
  int uniq_medium = 0;
  int uniq_large  = 0;

  struct frf_uniq_string * param;

  struct frf_uniq_string * hash = NULL;

  uint32_t offset;

  struct iovec iov[IOV_MAX];
  int iov_cnt;
  int i;

  do {
    iov_cnt = _frf_iovec(frf, iov);

    for (i = 0; i < iov_cnt; i++) {
      offset = (uint32_t)iov[i].iov_base;

      HASH_FIND_INT(hash, &offset, param);

      if (param) {
	param->cnt++;

	if (iov[i].iov_len < (1 << 8)) {
	  small++;
	} else if (iov[i].iov_len < (1 << 16)) {
	  medium++;
	} else {
	  large++;
	}
      } else {
	param = malloc(sizeof(struct frf_uniq_string));
	param->id = offset;
	param->cnt = 0;

	HASH_ADD_INT(hash, id, param);

	if (iov[i].iov_len < (1 << 8)) {
	  small++;
	  uniq_small++;
	} else if (iov[i].iov_len < (1 << 16)) {
	  medium++;
	  uniq_medium++;
	} else {
	  large++;
	  uniq_large++;
	}
      }
    }

  } while (frf_next(frf));

  printf("\
Total Strings          %d\n\
  Small Strings        %d\n\
  Medium Strings       %d\n\
  Large Strings        %d\n\
\n\
Total Unique Strings   %d\n\
  Small Strings        %d\n\
  Medium Strings       %d\n\
  Large Strings        %d\n\
", small + medium + large, small, medium, large, uniq_small + uniq_medium + uniq_large, uniq_small, uniq_medium, uniq_large);

  return;
}

void print_file_stats(frf_t * frf)
{
  int total_size         = (char *)frf->_end - (char *)frf->_mmap_base;
  int string_table_size  = (char *)frf->_unique_cells_base - (char *)frf->_mmap_base - 16;
  int unique_cells_size  = (char *)frf->_vector_base - (char *)frf->_unique_cells_base;
  int row_data_size      = (char *)frf->_end - (char *)frf->_vector_base;

  printf("\
FRF file          %s\n\
\n\
Total Size        %d\n\
  String Table    %d\n\
  Unique Cells    %d\n\
  Row Data        %d\n\
\n\
Num Records       %d\n\
", frf->file_name, total_size, string_table_size, unique_cells_size, row_data_size, frf->num_rows);
}

int main (int argc, char ** argv)
{
  char * file_name = argv[1];

  frf_t frf;

  frf_init(&frf, file_name);


  print_file_stats(&frf);
  printf("\n");
  print_vector_info(&frf);

  return 0;
}
