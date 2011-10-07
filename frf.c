#include "frf.h"

#define FRF_HIGH_BIT   (1 << 7)
#define FRF_LARGE_MASK (1 << 31)

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

  frf->file_name = file_name;

  frf->_end = (uint32_t *)(((char *)mmap_base) + size);

  frf->_mmap_base         = mmap_base;

  frf->num_rows           = ntohl(*((uint32_t *)(frf->_mmap_base)));
  frf->_string_table_base = (char *)((char *)mmap_base + ntohl(*(mmap_base + 1)));
  frf->_unique_cells_base = (uint32_t *)((char *)mmap_base + ntohl(*(mmap_base + 2)));
  frf->_vector_base       = (uint32_t *)((char *)mmap_base + ntohl(*(mmap_base + 3)));

  frf->_row_ptr = frf->_vector_base;

  return 0;
}

frf_t * frf_new(char * file_name)
{
  frf_t * frf = malloc(sizeof(frf_t));

  if (frf_init(frf, file_name)) {
    free(frf);
    return NULL;

  }

  return frf;
}

void frf_destroy(frf_t * frf)
{
  free(frf);
  return;
}

int _frf_iovec(frf_t * frf, struct iovec * iov)
{
  char     * string_table_base = frf->_string_table_base;
  uint32_t * unique_cells_base = frf->_unique_cells_base;

  uint32_t * vec_ptr = frf->_row_ptr;
  uint32_t * cell_ptr;

  if (vec_ptr >= frf->_end) {
    return -1;
  }

  int i;
  
  uint32_t unique_cell_offset;

  int num_elements;

  uint32_t index;
  char len;

  unique_cell_offset = ntohl(*vec_ptr);

  cell_ptr = (uint32_t *)((char *)unique_cells_base + unique_cell_offset);

  num_elements = ntohl(*cell_ptr);

  cell_ptr += 2;

  vec_ptr++;

  for (i = 0; i < num_elements; i++) {
    index = ntohl(*cell_ptr);

    if (! index) {
      index = ntohl(*vec_ptr);
      vec_ptr++;
    }
    index--;

    len = *(string_table_base + index);

    if (len & FRF_HIGH_BIT) {
      iov[i].iov_len  = ntohl(*((uint32_t *)(string_table_base + index))) - FRF_LARGE_MASK;
      iov[i].iov_base = string_table_base + index + 4;
    } else {
      iov[i].iov_len  = *(string_table_base + index);
      iov[i].iov_base = string_table_base + index + 1;
    }

    cell_ptr++;
  }

  return i;
}

ssize_t frf_write(frf_t * frf, int fd)
{
  struct iovec iov[IOV_MAX];

  int iov_cnt = _frf_iovec(frf, iov);

  return writev(fd, iov, iov_cnt);
}

int frf_next(frf_t * frf)
{
  int to_skip;

  to_skip = ntohl(*((uint32_t *)((char *)frf->_unique_cells_base + ntohl(*(frf->_row_ptr))) + 1)) + 1;

  if ((frf->_row_ptr += to_skip) <= frf->_end) {
    return 1;
  } else {
    return 0;
  }
}

uint32_t frf_get_offset(frf_t * frf)
{
  return (char *)(frf->_row_ptr) - (char *)(frf->_mmap_base);
}

int frf_get_render_size(frf_t * frf)
{
  struct iovec iov[IOV_MAX];

  int iov_cnt = _frf_iovec(frf, iov);
  int i;

  int acc = 0;

  for (i = 0; i < iov_cnt; i++) {
    acc += iov[i].iov_len;
  }

  return acc;
}

int frf_seek(frf_t * frf, uint32_t offset)
{
  if ((frf->_row_ptr = ((uint32_t *)(((char *)frf->_mmap_base) + offset))) >= frf->_end) {
    return -1;
  } else {
    return 0;
  }
}

int frf_render_to_buffer(frf_t * frf, char * buf, int buf_size)
{
  struct iovec iov[IOV_MAX];

  int iov_cnt = _frf_iovec(frf, iov);
  int i;
  int written = 0;
  int len;
  char * buf_ptr = buf;

  for (i = 0; i < iov_cnt; i++) {
    len = iov[i].iov_len;

    if (written + len > buf_size) {
      return -1;
    }

    memcpy(buf_ptr, iov[i].iov_base, len);
    buf_ptr += len;
    written += len;
  }

  return written;
}
