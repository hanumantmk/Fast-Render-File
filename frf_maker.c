#include "frf_maker.h"
#include <dlfcn.h>
#include <assert.h>
// #define NDEBUG

static frf_maker_str2ui_t * make_p13n_lookup(json_t * p13n_json)
{
  frf_maker_str2ui_t * p13n_lookup, * p13n_node;
  p13n_lookup = NULL;

  int i;
  char * key;

  for (i = 0; i < json_array_size(p13n_json); i++) {
    key = (char *)json_string_value(json_array_get(p13n_json, i));

    p13n_node = malloc(sizeof(frf_maker_str2ui_t));
    p13n_node->str = calloc(strlen(key) + 1, 1);
    memcpy(p13n_node->str, key, strlen(key));
    p13n_node->offset = i;

    HASH_ADD_KEYPTR(hh, p13n_lookup, p13n_node->str, strlen(key), p13n_node);
  }

  return p13n_lookup;
}

static frf_maker_str2dc_t * make_dc_lookup(frf_maker_t * frf_maker, json_t * dc_json)
{
  frf_maker_str2ui_t      * p13n_node;
  frf_maker_str2dc_t      * dc_node, * dc_lookup = NULL;
  frf_maker_str2content_t * content_node;
  frf_maker_content_t     * content_temp, * alt_content_temp;

  void * iter, * alt_iter;

  const char * key, * alt_key, * alt_value;
  char * p13n_key = NULL, * default_alt = NULL;

  json_t * json_alternatives;

  json_error_t json_error;

  iter = json_object_iter(dc_json);

  while(iter) {
    key = json_object_iter_key(iter);

    if (json_unpack_ex(json_object_iter_value(iter), &json_error, 0, "{s:s, s:o, s:s}", "key", &p13n_key, "alternatives", &json_alternatives, "default", &default_alt) < 0) {
      error(1, 0, "Couldn't unpack json: %s", json_error.text);
    }

    dc_node = calloc(sizeof(frf_maker_str2dc_t), 1);
    dc_node->str = calloc(strlen(key) + 1, 1);
    memcpy(dc_node->str, key, strlen(key));

    HASH_FIND(hh, frf_maker->p13n_lookup, p13n_key, strlen(p13n_key), p13n_node);

    if (! p13n_node) {
      error(1, 0, "invalid p13n key %s", p13n_key);
    }

    dc_node->p13n_offset = p13n_node->offset;

    content_temp = calloc(sizeof(frf_maker_content_t), 1);

    dc_node->def = content_temp;

    content_temp->content = calloc(strlen(default_alt) + 1, 1);
    memcpy(content_temp->content, default_alt, strlen(default_alt));

    HASH_ADD_KEYPTR(hh, dc_lookup, key, strlen(key), dc_node);

    alt_iter = json_object_iter(json_alternatives);

    while (alt_iter) {
      alt_key   = json_object_iter_key(alt_iter);
      alt_value = json_string_value(json_object_iter_value(alt_iter));
      alt_content_temp = calloc(sizeof(frf_maker_content_t), 1);

      content_node = calloc(sizeof(frf_maker_str2content_t), 1);
      content_node->str = calloc(strlen(alt_key) + 1, 1);
      memcpy(content_node->str, alt_key, strlen(alt_key));

      content_node->content = alt_content_temp;
      alt_content_temp->content = calloc(strlen(alt_value) + 1, 1);
      memcpy(alt_content_temp->content, alt_value, strlen(alt_value));

      HASH_ADD_KEYPTR(hh, dc_node->alternatives, content_node->str, strlen(alt_key), content_node);

      alt_iter = json_object_iter_next(json_alternatives, alt_iter);
    }

    iter = json_object_iter_next(dc_json, iter);
  }

  return dc_lookup;
}

static uint32_t frf_maker_add_to_string_table(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_str2ui_t * temp, *entry;
  char short_len;
  char * buf;
  uint32_t long_len;
  uint32_t rval;

  HASH_FIND(hh, frf_maker->strings_lookup, str, len, temp);

  if (temp) {
    HASH_DELETE(hh, frf_maker->strings_lookup, temp);
    HASH_ADD_KEYPTR(hh, frf_maker->strings_lookup, temp->str, len, temp);
    rval = temp->offset;
  } else {
    temp = malloc(sizeof(frf_maker_str2ui_t));
    buf = malloc(len);
    memcpy(buf, str, len);
    temp->str = buf;
    temp->offset = frf_maker->string_table_written;
    HASH_ADD_KEYPTR(hh, frf_maker->strings_lookup, buf, len, temp);

    if (HASH_COUNT(frf_maker->strings_lookup) >= STRING_TABLE_LOOKUP_SIZE) {
      HASH_ITER(hh, frf_maker->strings_lookup, entry, temp) {
	HASH_DELETE(hh, frf_maker->strings_lookup, entry);
	free(entry->str);
	free(entry);
	break;
      }
    }

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

static frf_maker_cc_t * frf_maker_make_static_cell(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_cc_t * cc = NULL;

  if (len > 0) {
    cc = calloc(sizeof(frf_maker_cc_t), 1);
    cc->type = FRF_MAKER_CC_TYPE_STATIC;
    cc->val.offset = frf_maker_add_to_string_table(frf_maker, str, len);
  }

  return cc;
}

static frf_maker_cc_t * frf_maker_make_dt_cell(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_cc_t * cc = NULL;
  frf_transform_exp_t * t = NULL;

  if (memchr(str, '(', len)) {
    t = frf_transform_compile(frf_maker, frf_maker, str, len);
  }

  if (t) {
    cc = calloc(sizeof(frf_maker_cc_t), 1);
    cc->type = FRF_MAKER_CC_TYPE_DT;
    cc->val.exp = t;
  }

  return cc;
}

static frf_maker_cc_t * frf_maker_make_dc_cell(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_cc_t * cc = NULL;
  frf_maker_str2dc_t * temp = NULL;

  HASH_FIND(hh, frf_maker->dc_lookup, str, len, temp);

  if (temp) {
    cc = calloc(sizeof(frf_maker_cc_t), 1);
    cc->type = FRF_MAKER_CC_TYPE_DC;
    cc->val.dc = temp;
  }

  return cc;
}

static frf_maker_cc_t * frf_maker_make_p13n_cell(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_cc_t * cc = NULL;
  frf_maker_str2ui_t * temp;

  HASH_FIND(hh, frf_maker->p13n_lookup, str, len, temp);

  if (temp) {
    cc = calloc(sizeof(frf_maker_cc_t), 1);
    cc->type = FRF_MAKER_CC_TYPE_P13N;
    cc->val.p13n = temp->offset;
  }

  return cc;
}

static void frf_maker_precompile(frf_maker_t * frf_maker, frf_maker_content_t * content)
{
  char * c;

  int rc;
  int ovector[9];
  int read = 0;

  frf_maker_cc_t * cc_head = NULL, * cc_temp;

  if (! content->is_compiled) {
    c = content->content;

    while ((rc = pcre_exec(frf_maker->content_re, NULL, c, strlen(c), read, 0, ovector, 9)) > 0) {
      if ((cc_temp = frf_maker_make_static_cell(frf_maker, c + ovector[2], ovector[3] - ovector[2]))) {
	DL_APPEND(cc_head, cc_temp);
      }

      if ((cc_temp = frf_maker_make_p13n_cell(frf_maker, c + ovector[4], ovector[5] - ovector[4]))) {
	DL_APPEND(cc_head, cc_temp);
      } else if ((cc_temp = frf_maker_make_dc_cell(frf_maker, c + ovector[4], ovector[5] - ovector[4]))) {
	DL_APPEND(cc_head, cc_temp);
      } else  if ((cc_temp = frf_maker_make_dt_cell(frf_maker, c + ovector[4], ovector[5] - ovector[4]))) {
	DL_APPEND(cc_head, cc_temp);
      }

      c += ovector[1];
    }

    if ((cc_temp = frf_maker_make_static_cell(frf_maker, c, strlen(c)))) {
      DL_APPEND(cc_head, cc_temp);
    }

    content->cc = cc_head;
    content->is_compiled = 1;
  }
}

static frf_maker_cc_t * frf_maker_compile(frf_maker_t * frf_maker, frf_maker_content_t * content)
{
  frf_maker_cc_t * stack = NULL, * stack_elt, * stack_temp;
  frf_maker_cc_t * cc_temp;

  if (! content->is_compiled) {
    frf_maker_precompile(frf_maker, content);
  }

  DL_FOREACH(content->cc, cc_temp) {
    stack_temp = calloc(sizeof(frf_maker_cc_t), 1);
    stack_temp->type = cc_temp->type;
    stack_temp->val = cc_temp->val;

    if (! stack) {
      stack = stack_elt = stack_temp;
    } else {
      stack_elt->next = stack_temp;
      stack_elt = stack_temp;
    }
  }

  return stack;
}

frf_maker_t * frf_maker_new(char * content_file_name, char * output_file_name)
{
  frf_maker_t * frf_maker;

  frf_maker = calloc(sizeof(frf_maker_t), 1);
  frf_maker_init(frf_maker, content_file_name, output_file_name);

  return frf_maker;
}

int frf_maker_init(frf_maker_t * frf_maker, char * content_file_name, char * output_file_name)
{
  json_t * root;
  json_t * p13n_json = NULL, * dc_json = NULL;
  json_error_t json_error;
  char * body = NULL;
  char * dt_file = NULL;
  char buf[1024];

  pcre * content_re;
  const char * content_re_errptr = NULL;
  int content_re_err_off = 0;

  root = json_load_file(content_file_name, 0, &json_error);
  if (! root) {
    error(1, 0, "Couldn't load json: %s", json_error.text);
  }

  frf_maker->content_file_name = content_file_name;
  if (json_unpack_ex(root, &json_error, 0, "{s:s, s:o, s:o, s:s}", "body", &body, "p13n", &p13n_json, "dc", &dc_json, "dt", &dt_file) < 0) {
    error(1, 0, "Couldn't unpack json: %s", json_error.text);
  }

  content_re = pcre_compile("(.*?)%% *([^%]+?) *%%", PCRE_DOTALL, &content_re_errptr, &content_re_err_off, NULL);
  assert(content_re);
  frf_maker->content_re = content_re;

  frf_maker->sym_handle = dlopen(dt_file, RTLD_NOW);

  frf_maker->content = calloc(sizeof(frf_maker_content_t), 1);
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
  frf_maker->uniq_cells_lookup = NULL;

  frf_maker->content->content = calloc(strlen(body) + 1, 1);
  memcpy(frf_maker->content->content, body, strlen(body));
  frf_maker->p13n_lookup = make_p13n_lookup(p13n_json);
  frf_maker->dc_lookup = make_dc_lookup(frf_maker, dc_json);
  frf_maker_precompile(frf_maker, frf_maker->content);

  frf_maker->malloc_context = frf_transform_malloc_context_new();

  json_decref(root);

  return 0;
}


static uint32_t frf_maker_add_uniq_vector(frf_maker_t * frf_maker, frf_maker_cc_t * cells, int cells_cnt, frf_maker_vector_t * vector, int vector_cnt)
{
  uint32_t rval, temp;

  char * buf;

  UT_string * s;

  frf_maker_cc_t * elt;

  frf_maker_str2ui_t * uc_temp;

  utstring_new(s);

  DL_FOREACH(cells, elt) {
    switch(elt->type) {
      case FRF_MAKER_CC_TYPE_STATIC:
	utstring_printf(s, "%d.", elt->val.offset);
	break;
      default:
        utstring_printf(s, ".");
    }
  }

  HASH_FIND(hh, frf_maker->uniq_cells_lookup, utstring_body(s), utstring_len(s), uc_temp);

  if (uc_temp) {
    rval = uc_temp->offset;
  } else {
    rval = frf_maker->unique_cells_written;

    uc_temp = malloc(sizeof(frf_maker_str2ui_t));

    buf = malloc(utstring_len(s));
    memcpy(buf, utstring_body(s), utstring_len(s));

    uc_temp->str = buf;
    uc_temp->offset = rval;

    HASH_ADD_KEYPTR(hh, frf_maker->uniq_cells_lookup, buf, utstring_len(s), uc_temp);

    temp = htonl(cells_cnt);
    fwrite(&temp, 4, 1, frf_maker->unique_cells_fh);
    frf_maker->unique_cells_written += 4;

    temp = htonl(vector_cnt);
    fwrite(&temp, 4, 1, frf_maker->unique_cells_fh);
    frf_maker->unique_cells_written += 4;

    DL_FOREACH(cells, elt) {
      if (elt->type == FRF_MAKER_CC_TYPE_STATIC) {
	temp = htonl(elt->val.offset);
      } else {
	temp = htonl(0);
      }
      fwrite(&temp, 4, 1, frf_maker->unique_cells_fh);
      frf_maker->unique_cells_written += 4;
    }
  }

  utstring_free(s);

  return rval;
}

int frf_maker_finish(frf_maker_t * frf_maker)
{
  uint32_t header[4];
  char buf[1024];
  char * ofn = frf_maker->output_file_name;
  int retcode;

  header[0] = htonl(frf_maker->num_rows);
  header[1] = htonl(16);
  header[2] = htonl(16 + frf_maker->string_table_written - 1);
  header[3] = htonl(16 + frf_maker->string_table_written - 1 + frf_maker->unique_cells_written);
  fwrite(header, 4, 4, frf_maker->header_fh);

  fclose(frf_maker->string_table_fh);
  fclose(frf_maker->vector_fh);
  fclose(frf_maker->unique_cells_fh);
  fclose(frf_maker->header_fh);

  sprintf(buf, "cat %s.h %s.st %s.uc %s.v > %s", ofn, ofn, ofn, ofn, ofn);
  if ((retcode = system(buf)) == -1) {
    error(1, 0, "System failed with: %d", retcode);
  }
  sprintf(buf, "rm %s.h %s.st %s.uc %s.v", ofn, ofn, ofn, ofn);
  system(buf);

  return 0;
}

static void frf_maker_destroy_cc(frf_maker_cc_t * cc)
{
  frf_maker_cc_t * cur, * temp;

  DL_FOREACH_SAFE(cc, cur, temp) {
    DL_DELETE(cc, cur);
    free(cur);
  }
}

static void frf_maker_destroy_content(frf_maker_content_t * content)
{
  free(content->content);
  frf_maker_destroy_cc(content->cc);
  free(content);
}

static void frf_maker_destroy_str2content(frf_maker_str2content_t * str2content)
{
  frf_maker_str2content_t * entry, * temp;

  HASH_ITER(hh, str2content, entry, temp) {
    HASH_DELETE(hh, str2content, entry);
    free(entry->str);
    frf_maker_destroy_content(entry->content);
    free(entry);
  }
}

static void frf_maker_destroy_str2dc(frf_maker_str2dc_t * str2dc)
{
  frf_maker_str2dc_t * entry, * temp;

  HASH_ITER(hh, str2dc, entry, temp) {
    HASH_DELETE(hh, str2dc, entry);
    frf_maker_destroy_str2content(entry->alternatives);
    frf_maker_destroy_content(entry->def);
    free(entry->str);
    free(entry);
  }
}

static void frf_maker_destroy_str2ui(frf_maker_str2ui_t * str2ui)
{
  frf_maker_str2ui_t * entry, * temp;

  HASH_ITER(hh, str2ui, entry, temp) {
    HASH_DELETE(hh, str2ui, entry);
    free(entry->str);
    free(entry);
  }
}

void frf_maker_destroy(frf_maker_t * frf_maker)
{
  frf_maker_destroy_content(frf_maker->content);
  frf_maker_destroy_str2ui(frf_maker->strings_lookup);
  frf_maker_destroy_str2ui(frf_maker->uniq_cells_lookup);
  frf_maker_destroy_str2ui(frf_maker->p13n_lookup);
  frf_maker_destroy_str2dc(frf_maker->dc_lookup);
  pcre_free(frf_maker->content_re);
  free(frf_maker);
}

int frf_maker_add(frf_maker_t * frf_maker, char ** p13n, uint32_t * lengths)
{
  frf_maker_str2content_t  * dc_node;
  frf_maker_content_t * dc_content;

  frf_maker_cc_t * cells_head = NULL, * cells_temp, * cells_current;
  frf_maker_vector_t * vector_head = NULL, * vector_temp, * vector_current;
  uint32_t uniq_vector_offset, vector_offset;

  int cells_cnt = 0, vector_cnt = 0;

  char * dt_result;

  frf_maker_cc_t * stack_elt = frf_maker_compile(frf_maker, frf_maker->content);
  frf_maker_cc_t * stack_temp;
  frf_maker_cc_t * dc_elt, * dc_tail;

  while(stack_elt) {
    switch (stack_elt->type) {
      case FRF_MAKER_CC_TYPE_P13N:
        vector_cnt++;
        vector_temp = calloc(sizeof(frf_maker_vector_t), 1);
	
        vector_temp->offset = frf_maker_add_to_string_table(frf_maker, p13n[stack_elt->val.p13n], lengths[stack_elt->val.p13n]);

	DL_APPEND(vector_head, vector_temp);
	goto ALL_ADD;
      case FRF_MAKER_CC_TYPE_DT:
        vector_cnt++;
        vector_temp = calloc(sizeof(frf_maker_vector_t), 1);

	dt_result = frf_transform_exec(&(frf_maker->malloc_context), stack_elt->val.exp, p13n);
	
        vector_temp->offset = frf_maker_add_to_string_table(frf_maker, dt_result, strlen(dt_result));
	free(dt_result);

	DL_APPEND(vector_head, vector_temp);
	goto ALL_ADD;
ALL_ADD: case FRF_MAKER_CC_TYPE_STATIC:
        cells_cnt++;
	cells_temp = calloc(sizeof(frf_maker_cc_t), 1);
	cells_temp->type = stack_elt->type;
	cells_temp->val.offset = stack_elt->val.offset;

	DL_APPEND(cells_head, cells_temp);
	stack_temp = stack_elt;
	stack_elt = stack_elt->next;
	free(stack_temp);

        break;
      case FRF_MAKER_CC_TYPE_DC:
        HASH_FIND(hh, stack_elt->val.dc->alternatives, p13n[stack_elt->val.dc->p13n_offset], lengths[stack_elt->val.dc->p13n_offset], dc_node);

	if (dc_node) {
	  dc_content = dc_node->content;
	} else {
	  dc_content = stack_elt->val.dc->def;
	}

	dc_tail = dc_elt = frf_maker_compile(frf_maker, dc_content);

	while (dc_tail->next) {
	  dc_tail = dc_tail->next;
	}
	dc_tail->next = stack_elt->next;
	free(stack_elt);
	stack_elt = dc_elt;

	break;
      default:
        perror("Shouldn't be here");
	exit(1);
    }
  }

  uniq_vector_offset = htonl(frf_maker_add_uniq_vector(frf_maker, cells_head, cells_cnt, vector_head, vector_cnt));

  fwrite(&uniq_vector_offset, 4, 1, frf_maker->vector_fh);

  DL_FOREACH_SAFE(vector_head, vector_current, vector_temp) {
    vector_offset = htonl(vector_current->offset);
    fwrite(&vector_offset, 4, 1, frf_maker->vector_fh);

    DL_DELETE(vector_head, vector_current);
    free(vector_current);
  }

  DL_FOREACH_SAFE(cells_head, cells_current, cells_temp) {
    DL_DELETE(cells_head, cells_current);
    free(cells_current);
  }

  frf_maker->num_rows++;

  return 1;
}
