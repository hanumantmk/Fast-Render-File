#define _GNU_SOURCE

#include "frf.h"

int frf_init(frf_t * frf, char * file_name)
{
  uint32_t * mmap_base;
  int fd;

  struct stat sb;
  long size;

  if ((fd = open(file_name, O_RDONLY)) < 0) {
    return 1;
  }

  if (fstat(fd, &sb) < 0) {
    return 1;
  } else {
    size = sb.st_size;
  }

  if ((mmap_base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0)) < 0) {
    return 1;
  }

  frf->_end = (uint16_t *)(((char *)mmap_base) + size);

  frf->_mmap_base = mmap_base;
  frf->_string_table_base  = (char *)((char *)mmap_base + ntohl(*mmap_base));
  frf->_vector_header_base = (uint32_t *)((char *)mmap_base + ntohl(*(mmap_base + 1)));
  frf->_vector_base        = (uint16_t *)((char *)mmap_base + ntohl(*(mmap_base + 2)));

  frf->num_rows = ntohl(*((uint32_t *)(frf->_vector_base)));

  frf->row_ptr = frf->_vector_base + 2;

  return 0;
}

ssize_t frf_write(frf_t * frf, int fd)
{
  char     * string_table_base  = frf->_string_table_base;
  uint32_t * vector_header_base = frf->_vector_header_base;
  uint16_t * vector_base        = frf->_vector_base;

  uint16_t * vec_ptr = frf->row_ptr;

  if (vec_ptr >= frf->_end) {
    return -1;
  }

  int i;
  
  int writev_return;

  int num_elements;

  uint16_t index;

  struct iovec iov[IOV_MAX];

  num_elements = ntohs(*vec_ptr);

  vec_ptr++;

  for (i = 0; i < num_elements; i++) {
    index = ntohs(*vec_ptr) * 2;
    iov[i].iov_base = string_table_base + ntohl(*(vector_header_base + index));
    iov[i].iov_len  = ntohl(*(vector_header_base + 1 + index));
    vec_ptr++;
  }

  return writev_return = writev(1, iov, i);
}

int frf_next(frf_t * frf)
{
  int num_elements;

  num_elements = ntohs(*(frf->row_ptr));

  if ((frf->row_ptr += num_elements + 1) <= frf->_end) {
    return 1;
  } else {
    return 0;
  }
}
