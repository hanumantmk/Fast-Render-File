#ifndef FRF_MAKER_DEF
#define FRF_MAKER_DEF
#include <string.h>
#include <jansson.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pcre.h>
#include "uthash.h"
#include "utlist.h"
#include "utstring.h"
#include <stdio.h>
#include <error.h>

#include "frf_transform.h"

enum FRF_MAKER_CC_TYPE {
  FRF_MAKER_CC_TYPE_STATIC,
  FRF_MAKER_CC_TYPE_P13N,
  FRF_MAKER_CC_TYPE_DC,
  FRF_MAKER_CC_TYPE_DT
};

typedef struct frf_maker_callback {
  void (* static_cb)(struct frf_maker_callback *, char *, size_t);
  void (* tag_cb)   (struct frf_maker_callback *, char **, size_t *);
  frf_malloc_context_t * malloc_context;
  void * data;
} frf_maker_callback_t;

typedef struct frf_maker_ui2ui {
  uint32_t * key;
  uint32_t offset;

  UT_hash_handle hh;
} frf_maker_ui2ui_t ;

typedef struct frf_maker_str2str {
  char * str;
  char * val;

  UT_hash_handle hh;
} frf_maker_str2str_t;

typedef struct frf_maker_str2ui {
  char * str;
  uint32_t offset;

  UT_hash_handle hh;
} frf_maker_str2ui_t;

typedef struct frf_maker_vector {
  uint32_t offset;

  struct frf_maker_vector * next, * prev;
} frf_maker_vector_t;

struct frf_maker_str2dc;

typedef struct frf_maker_cc {
  enum FRF_MAKER_CC_TYPE type;
  union {
    uint32_t p13n;
    uint32_t offset;
    struct frf_transform_exp * exp;
    struct frf_maker_str2dc * dc;
  } val;

  int value_index;

  struct frf_maker_cc * next, * prev;
} frf_maker_cc_t;

typedef struct frf_maker_str2cc {
  char * str;
  frf_maker_cc_t * cc;

  UT_hash_handle hh;
} frf_maker_str2cc_t;

typedef struct frf_maker_content {
  int is_compiled;
  char * content;
  frf_maker_cc_t * cc;
} frf_maker_content_t;

typedef struct frf_maker_str2content {
  char * str;
  frf_maker_content_t * content;

  UT_hash_handle hh;
} frf_maker_str2content_t;

typedef struct frf_maker_str2dc {
  char * str;

  uint32_t p13n_offset;
  frf_maker_str2content_t * alternatives;
  frf_maker_content_t * def;

  UT_hash_handle hh;
} frf_maker_str2dc_t;

typedef struct frf_maker {
  char * content_file_name;
  char * output_file_name;

  frf_maker_content_t * content;
  frf_maker_str2ui_t * p13n_lookup;
  frf_maker_str2dc_t * dc_lookup;

  frf_maker_str2ui_t * strings_lookup;
  frf_maker_ui2ui_t * uniq_cells_lookup;
  frf_maker_str2str_t * macro_lookup;

  frf_maker_str2cc_t * tag_lookup;
  int num_uniq_tags;

  pcre * content_re;

  frf_malloc_context_t * malloc_context;
  frf_malloc_context_t * str_malloc_context;
  size_t str_malloc_context_size;

  frf_maker_callback_t * callback;

  void * sym_handle;

  FILE * string_table_fh;
  uint32_t string_table_written;
  FILE * unique_cells_fh;
  uint32_t unique_cells_written;
  FILE * vector_fh;
  uint32_t vector_written;
  FILE * header_fh;

  uint32_t num_rows;
} frf_maker_t;

int frf_maker_init(frf_maker_t * frf_maker, char * content_file_name, char * output_file_name);
frf_maker_t * frf_maker_new(char * content_file_name, char * output_file_name);
int frf_maker_add(frf_maker_t * frf_maker, char ** p13n);
int frf_maker_finish(frf_maker_t * frf_maker);
void frf_maker_destroy(frf_maker_t * frf_maker);
#endif
