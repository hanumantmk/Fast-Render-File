#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

typedef struct frf {
  char     * file_name;
  int        num_rows;

  uint32_t * _mmap_base;
  char     * _string_table_base;
  uint32_t * _unique_cells_base;
  uint32_t * _vector_base;

  uint32_t * _row_ptr;
  uint32_t * _end;
} frf_t;

int frf_init(frf_t * frf, char * file_name);

ssize_t frf_write(frf_t * frf, int fd);

int frf_next(frf_t * frf);

uint32_t frf_get_offset(frf_t * frf);

int frf_seek(frf_t * frf, uint32_t offset);

int _frf_iovec(frf_t * frf, struct iovec * iov);

int frf_render_to_buffer(frf_t * frf, char * buf, int buf_size);

int frf_get_render_size(frf_t * frf);

frf_t * frf_new(char * file_name);

void frf_destroy(frf_t * frf);
