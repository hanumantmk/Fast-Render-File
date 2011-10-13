#include "frf.h"
#include "uthash.h"
#include "utstring.h"
#include <stdlib.h>
#include <string.h>

struct frf_uniq_string {
  int id;
  int cnt;
  UT_hash_handle hh;
}; 

struct frf_uniq_cells {
  int id;
  int cnt;
  UT_hash_handle hh;
}; 

void human_readable_size(double d, UT_string * buf)
{
  if (d > 1 << 30) {
    utstring_printf(buf, "%.2fG", d / (1 << 30));
  } else if (d > 1 << 20) {
    utstring_printf(buf, "%.2fM", d / (1 << 20));
  } else if (d > 1 << 10) {
    utstring_printf(buf, "%.2fK", d / (1 << 10));
  } else if (d > 0) {
    utstring_printf(buf, "%.0fB", d);
  } else {
    utstring_printf(buf, "error");
  }
}

void print_vector_info (frf_t * frf)
{
  int small  = 0;
  int medium = 0;
  int large  = 0;

  double bytes = 0;

  int uniq_small  = 0;
  int uniq_medium = 0;
  int uniq_large  = 0;

  double uniq_bytes = 0;
  
  double total_size = (char *)frf->_end - (char *)frf->_mmap_base;

  int uniq_c = 0;

  struct frf_uniq_string * uniq_str_param;

  struct frf_uniq_string * uniq_str_hash = NULL;

  struct frf_uniq_cells * uniq_c_param;

  struct frf_uniq_cells * uniq_c_hash = NULL;

  UT_string * human_render_size;

  uint32_t offset;

  struct iovec iov[IOV_MAX];
  int iov_cnt;
  int i;

  utstring_new(human_render_size);

  do {
    iov_cnt = _frf_iovec(frf, iov);

    offset = ntohl(*(frf->_row_ptr));

    HASH_FIND_INT(uniq_c_hash, &offset, uniq_c_param);

    if (! uniq_c_param) {
      uniq_c_param = malloc(sizeof(struct frf_uniq_cells));
      uniq_c_param->id = offset;
      uniq_c_param->cnt = 0;

      HASH_ADD_INT(uniq_c_hash, id, uniq_c_param);

      uniq_c++;
    }

    for (i = 0; i < iov_cnt; i++) {
      offset = (uint32_t)iov[i].iov_base;

      HASH_FIND_INT(uniq_str_hash, &offset, uniq_str_param);

      if (uniq_str_param) {
	uniq_str_param->cnt++;

	bytes += iov[i].iov_len;

	if (iov[i].iov_len < (1 << 8)) {
	  small++;
	} else if (iov[i].iov_len < (1 << 16)) {
	  medium++;
	} else {
	  large++;
	}
      } else {
	uniq_str_param = malloc(sizeof(struct frf_uniq_string));
	uniq_str_param->id = offset;
	uniq_str_param->cnt = 0;

	bytes += iov[i].iov_len;
	uniq_bytes += iov[i].iov_len;

	HASH_ADD_INT(uniq_str_hash, id, uniq_str_param);

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

  human_readable_size(bytes, human_render_size);

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
\n\
Total Unique Cells     %d\n\
\n\
Compression Rate       %.2fx\n\
Percent Overhead       %.2f%%\n\
\n\
Render Size            %s\n\
", small + medium + large, small, medium, large,\
   uniq_small + uniq_medium + uniq_large, uniq_small, uniq_medium, uniq_large,\
   uniq_c,\
   bytes / uniq_bytes, (100 * (total_size / uniq_bytes)) - 100,\
   utstring_body(human_render_size)\
);

  utstring_free(human_render_size);

  return;
}

void print_file_stats(frf_t * frf)
{
  UT_string * total_size;
  UT_string * string_table_size;
  UT_string * unique_cells_size;
  UT_string * row_data_size;

  utstring_new(total_size);
  utstring_new(string_table_size);
  utstring_new(unique_cells_size);
  utstring_new(row_data_size);

  human_readable_size((double)((char *)frf->_end - (char *)frf->_mmap_base), total_size);
  human_readable_size((double)((char *)frf->_unique_cells_base - (char *)frf->_mmap_base - 16), string_table_size);
  human_readable_size((double)((char *)frf->_vector_base - (char *)frf->_unique_cells_base), unique_cells_size);
  human_readable_size((double)((char *)frf->_end - (char *)frf->_vector_base), row_data_size);

  printf("\
FRF file          %s\n\
\n\
Total Size        %s\n\
  String Table    %s\n\
  Unique Cells    %s\n\
  Row Data        %s\n\
\n\
Num Records       %d\n\
", frf->file_name, utstring_body(total_size), utstring_body(string_table_size), utstring_body(unique_cells_size), utstring_body(row_data_size), frf->num_rows);

  utstring_free(total_size);
  utstring_free(string_table_size);
  utstring_free(unique_cells_size);
  utstring_free(row_data_size);
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
