#include "frf_maker.h"
#include <assert.h>
// #define NDEBUG

frf_maker_p13n_t * make_p13n_lookup(json_t * p13n_json)
{
  frf_maker_p13n_t * p13n_lookup, * p13n_node;
  p13n_lookup = NULL;

  int i;
  char * key;

  for (i = 0; i < json_array_size(p13n_json); i++) {
    key = (char *)json_string_value(json_array_get(p13n_json, i));

    p13n_node = malloc(sizeof(frf_maker_p13n_t));
    p13n_node->key = key;
    p13n_node->offset = i;

    HASH_ADD_KEYPTR(hh, p13n_lookup, key, strlen(key), p13n_node);
  }

  return p13n_lookup;
}

static inline uint32_t frf_maker_add_to_string_table(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_str2ui_t * temp;
  char short_len;
  char * buf;
  uint32_t long_len;
  uint32_t rval;

  HASH_FIND(hh, frf_maker->strings_lookup, str, len, temp);

  if (temp) {
    rval = temp->offset;
  } else {
    temp = malloc(sizeof(frf_maker_str2ui_t));
    buf = malloc(len);
    memcpy(buf, str, len);
    temp->str = buf;
    temp->offset = frf_maker->string_table_written;
    HASH_ADD_KEYPTR(hh, frf_maker->strings_lookup, buf, len, temp);

    rval = frf_maker->string_table_written;
    if (len < 1 << 7) {
      short_len = (char)len;
      fwrite(&short_len, 1, 1, frf_maker->string_table_fh);
      fwrite(buf, 1, len, frf_maker->string_table_fh);
      frf_maker->string_table_written += 1 + len;
    } else {
      long_len = htonl(len | (1 << 31));
      fwrite(&long_len, 4, 1, frf_maker->string_table_fh);
      fwrite(buf, 1, len, frf_maker->string_table_fh);
      frf_maker->string_table_written += 4 + len;
    }
  }

  return rval;
}

void frf_maker_compile(frf_maker_t * frf_maker, frf_maker_content_t * content)
{
  char * c;

  frf_maker_p13n_t * p13n_temp;

  int rc;
  int ovector[9];
  int read = 0;
  int size;

  frf_maker_cc_t * cc_head = NULL, * cc_tail, * cc_temp;

  assert(content->is_compiled == 0);

  c = content->content;

  const char * content_re_errptr = NULL;
  int content_re_err_off = 0;
  pcre * content_re = pcre_compile("(.*?)%% *([^% ]+) *%%", PCRE_DOTALL, &content_re_errptr, &content_re_err_off, NULL);
  assert(content_re);

  while ((rc = pcre_exec(content_re, NULL, c, strlen(c), read, 0, ovector, sizeof(ovector))) > 0) {
    size = ovector[3] - ovector[2];
    if (size > 0) {
      cc_temp = malloc(sizeof(frf_maker_cc_t));
      cc_temp->type = FRF_MAKER_CC_TYPE_STATIC;
      cc_temp->val.offset = frf_maker_add_to_string_table(frf_maker, c + ovector[2], size);

      if (cc_head) {
	cc_tail->next = cc_temp;
	cc_tail = cc_tail->next;
      } else {
	cc_head = cc_tail = cc_temp;
      }
    }

    size = ovector[5] - ovector[4];

    HASH_FIND(hh, frf_maker->content->p13n_lookup, c + ovector[4], size, p13n_temp);

    if (p13n_temp) {
      cc_temp = malloc(sizeof(frf_maker_cc_t));
      cc_temp->type = FRF_MAKER_CC_TYPE_P13N;
      cc_temp->val.p13n = p13n_temp->offset;

      if (cc_head) {
	cc_tail->next = cc_temp;
	cc_tail = cc_tail->next;
      } else {
	cc_head = cc_tail = cc_temp;
      }
    }

    c += ovector[1];
  }

  size = strlen(c);
  if (size > 0) {
    cc_temp = malloc(sizeof(frf_maker_cc_t));
    cc_temp->type = FRF_MAKER_CC_TYPE_STATIC;
    cc_temp->val.offset = frf_maker_add_to_string_table(frf_maker, c, size);

    if (cc_head) {
      cc_tail->next = cc_temp;
      cc_tail = cc_tail->next;
    } else {
      cc_head = cc_tail = cc_temp;
    }
  }

  content->cc = cc_head;
  content->is_compiled = 1;
}

int frf_maker_init(frf_maker_t * frf_maker, char * content_file_name, char * output_file_name)
{
  json_t * root;
  json_t * p13n_json = NULL;
  json_error_t json_error;
  char * body = NULL;
  char buf[1024];

  root = json_load_file(content_file_name, 0, &json_error);
  if (! root) {
    error(1, 0, "Couldn't load json: %s", json_error.text);
  }

  frf_maker->content_file_name = content_file_name;
  if (json_unpack_ex(root, &json_error, 0, "{s:s, s:o}", "body", &body, "p13n", &p13n_json) < 0) {
    error(1, 0, "Couldn't unpack json: %s", json_error.text);
  }

  frf_maker->content = malloc(sizeof(frf_maker_content_t));
  frf_maker->num_rows = 0;

  frf_maker->output_file_name = output_file_name;

  sprintf(buf, "%s.st", output_file_name);
  frf_maker->string_table_fh = fopen(buf, "w");
  frf_maker->string_table_written = 1;

  sprintf(buf, "%s.uc", output_file_name);
  frf_maker->unique_cells_fh = fopen(buf, "w");
  frf_maker->unique_cells_written = 0;

  sprintf(buf, "%s.v", output_file_name);
  frf_maker->vector_fh = fopen(buf, "w");

  sprintf(buf, "%s.h", output_file_name);
  frf_maker->header_fh = fopen(buf, "w");

  frf_maker->strings_lookup = NULL;

  frf_maker->content->is_compiled = 0;
  frf_maker->content->content = body;
  frf_maker->content->p13n_lookup = make_p13n_lookup(p13n_json);
  frf_maker_compile(frf_maker, frf_maker->content);

  return 0;
}


static inline uint32_t frf_maker_add_uniq_vector(frf_maker_t * frf_maker, frf_maker_cc_t * cells, int cells_cnt, frf_maker_vector_t * vector, int vector_cnt)
{
  uint32_t rval, temp;

  rval = frf_maker->unique_cells_written;

  temp = htonl(cells_cnt);
  fwrite(&temp, 4, 1, frf_maker->unique_cells_fh);
  frf_maker->unique_cells_written += 4;

  temp = htonl(vector_cnt);
  fwrite(&temp, 4, 1, frf_maker->unique_cells_fh);
  frf_maker->unique_cells_written += 4;

  while(cells) {
    if (cells->type == FRF_MAKER_CC_TYPE_STATIC) {
      temp = htonl(cells->val.offset);
    } else {
      temp = 0;
    }
    fwrite(&temp, 4, 1, frf_maker->unique_cells_fh);
    frf_maker->unique_cells_written += 4;
    cells = cells->next;
  }

  return rval;
}

int frf_maker_finish(frf_maker_t * frf_maker)
{
  uint32_t header[4];
  char buf[1024];
  char * ofn = frf_maker->output_file_name;

  header[0] = htonl(frf_maker->num_rows);
  header[1] = htonl(16);
  header[2] = htonl(16 + frf_maker->string_table_written - 1);
  header[3] = htonl(16 + frf_maker->string_table_written - 1 + frf_maker->unique_cells_written);
  fwrite(header, 4, 4, frf_maker->header_fh);

  fflush(frf_maker->string_table_fh);
  fflush(frf_maker->vector_fh);
  fflush(frf_maker->unique_cells_fh);
  fflush(frf_maker->header_fh);

  sprintf(buf, "cat %s.h %s.st %s.uc %s.v > %s", ofn, ofn, ofn, ofn, ofn);
  system(buf);
  sprintf(buf, "rm %s.h %s.st %s.uc %s.v", ofn, ofn, ofn, ofn);
  system(buf);

  return 0;
}

int frf_maker_add(frf_maker_t * frf_maker, char ** p13n, uint32_t * lengths)
{
  frf_maker_cc_t * stack = frf_maker->content->cc;
  frf_maker_cc_t * cells_head = NULL, * cells_temp, * cells_current;
  frf_maker_vector_t * vector_head = NULL, * vector_temp, *vector_current;
  uint32_t uniq_vector_offset, vector_offset;

  int cells_cnt = 0, vector_cnt = 0;

  while (stack) {
    switch (stack->type) {
      case FRF_MAKER_CC_TYPE_P13N:
        vector_cnt++;
        vector_temp = malloc(sizeof(frf_maker_vector_t));
	
        vector_temp->offset = frf_maker_add_to_string_table(frf_maker, p13n[stack->val.p13n], lengths[stack->val.p13n]);
	vector_temp->next = NULL;

        if (vector_head) {
	  vector_current->next = vector_temp;
	  vector_current = vector_temp;
	} else {
	  vector_head = vector_current = vector_temp;
	}
	// deliberate fallthrough
      case FRF_MAKER_CC_TYPE_STATIC:
        cells_cnt++;
	cells_temp = malloc(sizeof(frf_maker_cc_t));
	cells_temp->type = stack->type;
	cells_temp->val.offset = stack->val.offset;
	cells_temp->next = NULL;
        if (cells_head) {
	  cells_current->next = cells_temp;
	  cells_current = cells_temp;
	} else {
	  cells_head = cells_current = cells_temp;
	}
        break;
      default:
        perror("Shouldn't be here");
	exit(1);
    }

    stack = stack->next;
  }

  uniq_vector_offset = htonl(frf_maker_add_uniq_vector(frf_maker, cells_head, cells_cnt, vector_head, vector_cnt));

  fwrite(&uniq_vector_offset, 4, 1, frf_maker->vector_fh);

  vector_temp = vector_head;
  while(vector_temp) {
    vector_offset = htonl(vector_temp->offset);
    fwrite(&vector_offset, 4, 1, frf_maker->vector_fh);
    vector_temp = vector_temp->next;
  }

  while(vector_head) {
    vector_temp = vector_head;
    vector_head = vector_head->next;
    free(vector_temp);
  }

  while(cells_head) {
    cells_temp = cells_head;
    cells_head = cells_head->next;
    free(cells_temp);
  }

  frf_maker->num_rows++;

  return 1;
}
