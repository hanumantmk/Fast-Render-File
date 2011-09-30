#define _GNU_SOURCE

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <limits.h>

typedef struct frf {
  int        num_rows;

  uint32_t * _vector_base;
  uint32_t * _vector_header_base;
  char     * _string_table_base;
  uint32_t * _mmap_base;

  uint32_t * _row_ptr;
  uint32_t * _end;
} frf_t;

int frf_init(frf_t * frf, char * file_name);

ssize_t frf_write(frf_t * frf, int fd);

int frf_next(frf_t * frf);

uint32_t frf_get_offset(frf_t * frf);

int frf_seek(frf_t * frf, uint32_t offset);
