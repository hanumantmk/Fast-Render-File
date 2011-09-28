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

int main (int argc, char ** argv)
{
  char * file_name = argv[1];

  int fd;
  uint32_t * mmap_base;

  struct stat sb;
  long size;

  uint16_t * vector_base;
  uint32_t * vector_header_base;
  char     * string_table_base;

  uint16_t * vec_ptr;
  int num_elements;
  uint16_t index;

  uint32_t num_rows;

  int i;
  int row;

  struct iovec iov[IOV_MAX];

  if ((fd = open(file_name, O_RDONLY)) < 0) {
    perror("error in open\n");
    exit(1);
  }

  if (fstat(fd, &sb) < 0) {
    perror("error in stat\n");
    exit(1);
  } else {
    size = sb.st_size;
  }

  if ((mmap_base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0)) < 0) {
    perror("error in mmap\n");
    exit(1);
  }

  string_table_base  = (char *)((char *)mmap_base + ntohl(*mmap_base));
  vector_header_base = (uint32_t *)((char *)mmap_base + ntohl(*(mmap_base + 1)));
  vector_base        = (uint16_t *)((char *)mmap_base + ntohl(*(mmap_base + 2)));

  num_rows = ntohl(*((uint32_t *)vector_base));

  vec_ptr = vector_base + 2;
  for (row = 0; row < num_rows; row++) {
    num_elements = ntohs(*vec_ptr);

    vec_ptr++;

    for (i = 0; i < num_elements; i++) {
      index = ntohs(*vec_ptr) * 2;
      iov[i].iov_base = string_table_base + ntohl(*(vector_header_base + index));
      iov[i].iov_len  = ntohl(*(vector_header_base + 1 + index));
      vec_ptr++;
    }
    if (writev(1, iov, i) == -1) {
      perror("error in writev\n");
      exit(1);
    }
  }

  return 0;
}
