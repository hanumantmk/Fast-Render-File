#include "frf_maker.h"
#include <dlfcn.h>
#include <assert.h>

#define FRF_MAKER_MAIN_BUFFER_SIZE (1 << 10)
#define FRF_MAKER_STR_BUFFER_SIZE  (1 << 16)

#ifndef NDEBUG
#define ASSERT_INDEX(index) assert((index) & (1<<31) || (index) <= (frf_maker->string_table_written));
#else
#define ASSERT_INDEX(index) 0
#endif

union uint32_t2char {
  uint32_t ui;
  char chars[4];
};

static frf_maker_str2str_t * make_macro_lookup(json_t * macros)
{
  frf_maker_str2str_t * macro_lookup = NULL, * macro_node;

  const char * key, * value;

  void * iter = json_object_iter(macros);

  while(iter) {
    key = json_object_iter_key(iter);
    value = json_string_value(json_object_iter_value(iter));

    macro_node = malloc(sizeof(*macro_node));
    macro_node->str = strdup(key);
    macro_node->val = strdup(value);

    HASH_ADD_KEYPTR(hh, macro_lookup, macro_node->str, strlen(key), macro_node);

    iter = json_object_iter_next(macros, iter);
  }

  return macro_lookup;
}

static frf_maker_str2ui_t * make_p13n_lookup(json_t * p13n_json)
{
  frf_maker_str2ui_t * p13n_lookup, * p13n_node;
  p13n_lookup = NULL;

  int i;
  char * key;

  for (i = 0; i < json_array_size(p13n_json); i++) {
    key = (char *)json_string_value(json_array_get(p13n_json, i));

    p13n_node = malloc(sizeof(*p13n_node));
    p13n_node->str = strndup(key, strlen(key));
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

    dc_node = calloc(sizeof(*dc_node), 1);
    dc_node->str = strndup(key, strlen(key));

    HASH_FIND(hh, frf_maker->p13n_lookup, p13n_key, strlen(p13n_key), p13n_node);

    if (! p13n_node) {
      error(1, 0, "invalid p13n key %s", p13n_key);
    }

    dc_node->p13n_offset = p13n_node->offset;

    content_temp = calloc(sizeof(*content_temp), 1);

    dc_node->def = content_temp;

    content_temp->content = strndup(default_alt, strlen(default_alt));

    HASH_ADD_KEYPTR(hh, dc_lookup, key, strlen(key), dc_node);

    alt_iter = json_object_iter(json_alternatives);

    while (alt_iter) {
      alt_key   = json_object_iter_key(alt_iter);
      alt_value = json_string_value(json_object_iter_value(alt_iter));
      alt_content_temp = calloc(sizeof(*alt_content_temp), 1);

      content_node = calloc(sizeof(*content_node), 1);
      content_node->str = strndup(alt_key, strlen(alt_key));

      content_node->content = alt_content_temp;
      alt_content_temp->content = strndup(alt_value, strlen(alt_value));

      HASH_ADD_KEYPTR(hh, dc_node->alternatives, content_node->str, strlen(alt_key), content_node);

      alt_iter = json_object_iter_next(json_alternatives, alt_iter);
    }

    iter = json_object_iter_next(dc_json, iter);
  }

  return dc_lookup;
}

#undef uthash_malloc
#undef uthash_free
#define uthash_malloc(sz) frf_malloc(frf_maker->str_malloc_context, sz)
#define uthash_free(ptr, sz) 0

static uint32_t frf_maker_add_to_string_table(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_str2ui_t * temp;
  char short_len;
  char * buf;
  uint32_t long_len;
  uint32_t rval;
  int i;

  union uint32_t2char converter = { 0 };

  if (len < 5) {
    for (i = 0; i < len; i++) {
      converter.chars[(4 - len) + i] = str[len - i - 1];
    }

    converter.ui |= (1 << 31);

    return converter.ui;
  }

  if (frf_maker->str_malloc_context->used > frf_maker->str_malloc_context_size) {
    frf_maker->str_malloc_context = frf_malloc_context_reset(frf_maker->str_malloc_context);
    frf_maker->strings_lookup = NULL;
  }

  HASH_FIND(hh, frf_maker->strings_lookup, str, len, temp);

  if (temp) {
    rval = temp->offset;
  } else {
    temp = frf_malloc(frf_maker->str_malloc_context, sizeof(*temp));
    temp->str = buf = frf_strndup(frf_maker->str_malloc_context, str, len);
    temp->offset = frf_maker->string_table_written;
    HASH_ADD_KEYPTR(hh, frf_maker->strings_lookup, buf, len, temp);

    rval = frf_maker->string_table_written;
    if (len < (1 << 7)) {
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

#undef uthash_malloc
#undef uthash_free
#define uthash_malloc(sz) malloc(sz)
#define uthash_free(ptr, sz) free(ptr)

static frf_maker_cc_t * frf_maker_make_static_cell(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_cc_t * cc = NULL;

  if (len > 0) {
    cc = calloc(sizeof(*cc), 1);
    cc->type = FRF_MAKER_CC_TYPE_STATIC;
    cc->val.offset = frf_maker_add_to_string_table(frf_maker, str, len);
    ASSERT_INDEX(cc->val.offset);
  }

  return cc;
}

static frf_maker_cc_t * frf_maker_make_dt_cell(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_cc_t * cc = NULL;
  frf_transform_exp_t * t = NULL;

  if (memchr(str, '(', len)) {
    t = frf_transform_compile(frf_maker, str, len);
  }

  if (t) {
    cc = calloc(sizeof(*cc), 1);
    cc->type = FRF_MAKER_CC_TYPE_DT;
    cc->val.exp = t;
    cc->value_index = frf_maker->num_uniq_tags;
  }

  return cc;
}

static frf_maker_cc_t * frf_maker_make_dc_cell(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_cc_t * cc = NULL;
  frf_maker_str2dc_t * temp = NULL;

  HASH_FIND(hh, frf_maker->dc_lookup, str, len, temp);

  if (temp) {
    cc = calloc(sizeof(*cc), 1);
    cc->type = FRF_MAKER_CC_TYPE_DC;
    cc->val.dc = temp;
    cc->value_index = frf_maker->num_uniq_tags;
  }

  return cc;
}

static frf_maker_cc_t * frf_maker_make_p13n_cell(frf_maker_t * frf_maker, char * str, uint32_t len)
{
  frf_maker_cc_t * cc = NULL;
  frf_maker_str2ui_t * temp;

  HASH_FIND(hh, frf_maker->p13n_lookup, str, len, temp);

  if (temp) {
    cc = calloc(sizeof(*cc), 1);
    cc->type = FRF_MAKER_CC_TYPE_P13N;
    cc->val.p13n = temp->offset;
    cc->value_index = frf_maker->num_uniq_tags;
  }

  return cc;
}

static void frf_maker_precompile(frf_maker_t * frf_maker, frf_maker_content_t * content)
{
  char * c;

  int rc;
  int ovector[9];
  int read = 0;

  char * key, * mkey;
  int key_len, mkey_len;

  frf_maker_str2str_t * macro_node;
  frf_maker_str2cc_t * tag_node;

  frf_maker_cc_t * cc_head = NULL, * cc_temp;

  if (! content->is_compiled) {
    c = content->content;

    while ((rc = pcre_exec(frf_maker->content_re, NULL, c, strlen(c), read, 0, ovector, 9)) > 0) {
      if ((cc_temp = frf_maker_make_static_cell(frf_maker, c + ovector[2], ovector[3] - ovector[2]))) {
	DL_APPEND(cc_head, cc_temp);
      }

      key     = c + ovector[4];
      key_len = ovector[5] - ovector[4];

      HASH_FIND(hh, frf_maker->tag_lookup, key, key_len, tag_node);
      if (tag_node) {
	cc_temp = malloc(sizeof(*cc_temp));
	*cc_temp = *(tag_node->cc);
	DL_APPEND(cc_head, cc_temp);
      } else {
	HASH_FIND(hh, frf_maker->macro_lookup, key, key_len, macro_node);
	if (macro_node) {
	  mkey = macro_node->val;
	  mkey_len = strlen(mkey);
	} else {
	  mkey = key;
	  mkey_len = key_len;
	}

	cc_temp = NULL;

	if ((cc_temp = frf_maker_make_p13n_cell(frf_maker, mkey, mkey_len))) {
	  DL_APPEND(cc_head, cc_temp);
	} else if ((cc_temp = frf_maker_make_dc_cell(frf_maker, mkey, mkey_len))) {
	  DL_APPEND(cc_head, cc_temp);
	} else if ((cc_temp = frf_maker_make_dt_cell(frf_maker, mkey, mkey_len))) {
	  DL_APPEND(cc_head, cc_temp);
	}

	if (cc_temp) {
	  frf_maker->num_uniq_tags++;

	  tag_node = malloc(sizeof(*tag_node));
	  tag_node->str = strdup(key);
	  tag_node->cc = malloc(sizeof(*(tag_node->cc)));
	  *(tag_node->cc) = *cc_temp;
	  HASH_ADD_KEYPTR(hh, frf_maker->tag_lookup, tag_node->str, key_len, tag_node);
	}
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
    stack_temp = frf_malloc(frf_maker->malloc_context, sizeof(*stack_temp));
    *stack_temp = *cc_temp;
    stack_temp->prev = stack_temp->next = NULL;

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

  frf_maker = calloc(sizeof(*frf_maker), 1);
  frf_maker_init(frf_maker, content_file_name, output_file_name);

  return frf_maker;
}

int frf_maker_init(frf_maker_t * frf_maker, char * content_file_name, char * output_file_name)
{
  json_t * root;
  json_t * p13n_json = NULL, * dc_json = NULL, * macros = NULL;
  json_error_t json_error;
  char * body = NULL;
  char * dt_file = NULL;

  UT_string *str;

  char * env_buffer_size; 
  size_t buffer_size;

  pcre * content_re;
  const char * content_re_errptr = NULL;
  int content_re_err_off = 0;

  utstring_new(str);

  root = json_load_file(content_file_name, 0, &json_error);
  if (! root) {
    error(1, 0, "Couldn't load json: %s", json_error.text);
  }

  frf_maker->content_file_name = content_file_name;
  if (json_unpack_ex(root, &json_error, 0, "{s:s, s:o, s:o, s:s, s:o}", "body", &body, "p13n", &p13n_json, "dc", &dc_json, "dt", &dt_file, "macros", &macros) < 0) {
    error(1, 0, "Couldn't unpack json: %s", json_error.text);
  }

  content_re = pcre_compile("(.*?)%% *([^%]+?) *%%", PCRE_DOTALL, &content_re_errptr, &content_re_err_off, NULL);
  assert(content_re);
  frf_maker->content_re = content_re;

  frf_maker->sym_handle = dlopen(dt_file, RTLD_NOW);

  frf_maker->malloc_context = frf_malloc_context_new(FRF_MAKER_MAIN_BUFFER_SIZE);

  env_buffer_size = getenv("FRF_MAKER_BUFFER_SIZE");

  if (env_buffer_size) {
    buffer_size = atoi(env_buffer_size);
  } else {
    buffer_size = FRF_MAKER_STR_BUFFER_SIZE;
  }
  frf_maker->str_malloc_context_size = buffer_size * 0.9;
  frf_maker->str_malloc_context = frf_malloc_context_new(buffer_size);

  frf_maker->tag_lookup = NULL;
  frf_maker->num_uniq_tags = 0;

  frf_maker->content = calloc(sizeof(*(frf_maker->content)), 1);
  frf_maker->num_rows = 0;

  frf_maker->output_file_name = output_file_name;

  utstring_printf(str, "%s.st", output_file_name);
  frf_maker->string_table_fh = fopen(utstring_body(str), "w");
  frf_maker->string_table_written = 1;

  utstring_clear(str);
  utstring_printf(str, "%s.uc", output_file_name);
  frf_maker->unique_cells_fh = fopen(utstring_body(str), "w");
  frf_maker->unique_cells_written = 0;

  utstring_clear(str);
  utstring_printf(str, "%s.v", output_file_name);
  frf_maker->vector_fh = fopen(utstring_body(str), "w");
  frf_maker->vector_written = 0;

  utstring_clear(str);
  utstring_printf(str, "%s.h", output_file_name);
  frf_maker->header_fh = fopen(utstring_body(str), "w");

  frf_maker->strings_lookup = NULL;
  frf_maker->uniq_cells_lookup = NULL;

  frf_maker->content->content = strndup(body, strlen(body));
  frf_maker->p13n_lookup = make_p13n_lookup(p13n_json);
  frf_maker->dc_lookup = make_dc_lookup(frf_maker, dc_json);
  frf_maker->macro_lookup = make_macro_lookup(macros);
  frf_maker_precompile(frf_maker, frf_maker->content);

  utstring_free(str);

  json_decref(root);

  return 0;
}

static uint32_t frf_maker_add_uniq_vector(frf_maker_t * frf_maker, frf_maker_cc_t * cells, int cells_cnt, frf_maker_vector_t * vector, int vector_cnt)
{
  uint32_t rval, temp;

  uint32_t * key;
  uint32_t key_len;

  int i = 0;

  frf_maker_cc_t * elt;

  frf_maker_ui2ui_t * uc_temp = NULL;

  key_len = cells_cnt * sizeof(uint32_t);

  key = frf_malloc(frf_maker->malloc_context, key_len);

  DL_FOREACH(cells, elt) {
    switch(elt->type) {
      case FRF_MAKER_CC_TYPE_STATIC:
        key[i] = elt->val.offset;
	break;
      default:
        key[i] = 0;
    }
    i++;
  }

  HASH_FIND(hh, frf_maker->uniq_cells_lookup, key, key_len, uc_temp);

  if (uc_temp) {
    rval = uc_temp->offset;
  } else {
    rval = frf_maker->unique_cells_written;

    uc_temp = malloc(sizeof(*uc_temp));

    uc_temp->key = malloc(key_len);
    memcpy(uc_temp->key, key, key_len);
    uc_temp->offset = rval;

    HASH_ADD_KEYPTR(hh, frf_maker->uniq_cells_lookup, uc_temp->key, key_len, uc_temp);

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
      ASSERT_INDEX(ntohl(temp));
      fwrite(&temp, 4, 1, frf_maker->unique_cells_fh);
      frf_maker->unique_cells_written += 4;
    }
  }

  return rval;
}

int frf_maker_finish(frf_maker_t * frf_maker)
{
  uint32_t header[4];

  UT_string * str;

  char * ofn = frf_maker->output_file_name;
  int retcode;

  utstring_new(str);

  header[0] = htonl(frf_maker->num_rows);
  header[1] = htonl(16);
  header[2] = htonl(16 + frf_maker->string_table_written - 1);
  header[3] = htonl(16 + frf_maker->string_table_written - 1 + frf_maker->unique_cells_written);
  fwrite(header, 4, 4, frf_maker->header_fh);

  fclose(frf_maker->string_table_fh);
  fclose(frf_maker->vector_fh);
  fclose(frf_maker->unique_cells_fh);
  fclose(frf_maker->header_fh);

  utstring_printf(str, "cat %s.h %s.st %s.uc %s.v > %s", ofn, ofn, ofn, ofn, ofn);
  if ((retcode = system(utstring_body(str))) == -1) {
    error(1, 0, "System failed with: %d", retcode);
  }
  utstring_clear(str);
  utstring_printf(str, "rm %s.h %s.st %s.uc %s.v", ofn, ofn, ofn, ofn);
  system(utstring_body(str));

  utstring_free(str);

  return 0;
}

static void frf_maker_destroy_tag_lookup(frf_maker_str2cc_t * str2cc)
{
  frf_maker_str2cc_t * entry, * temp;

  HASH_ITER(hh, str2cc, entry, temp) {
    HASH_DELETE(hh, str2cc, entry);
    free(entry->str);

    if (entry->cc->type == FRF_MAKER_CC_TYPE_DT) {
      frf_transform_destroy(entry->cc->val.exp);
    }

    free(entry->cc);
    free(entry);
  }
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

static void frf_maker_destroy_str2str(frf_maker_str2str_t * str2str)
{
  frf_maker_str2str_t * entry, * temp;

  HASH_ITER(hh, str2str, entry, temp) {
    HASH_DELETE(hh, str2str, entry);
    free(entry->str);
    free(entry->val);
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

static void frf_maker_destroy_ui2ui(frf_maker_ui2ui_t * ui2ui)
{
  frf_maker_ui2ui_t * entry, * temp;

  HASH_ITER(hh, ui2ui, entry, temp) {
    HASH_DELETE(hh, ui2ui, entry);
    free(entry->key);
    free(entry);
  }
}

void frf_maker_destroy(frf_maker_t * frf_maker)
{
  frf_maker_destroy_content(frf_maker->content);
  frf_maker_destroy_ui2ui(frf_maker->uniq_cells_lookup);
  frf_maker_destroy_str2ui(frf_maker->p13n_lookup);
  frf_maker_destroy_str2dc(frf_maker->dc_lookup);
  frf_maker_destroy_str2str(frf_maker->macro_lookup);
  frf_maker_destroy_tag_lookup(frf_maker->tag_lookup);
  frf_malloc_context_destroy(frf_maker->malloc_context);
  frf_malloc_context_destroy(frf_maker->str_malloc_context);
  pcre_free(frf_maker->content_re);
  free(frf_maker);
}

int frf_maker_add(frf_maker_t * frf_maker, char ** p13n)
{
  frf_maker_str2content_t  * dc_node;
  frf_maker_content_t * dc_content;

  frf_maker_cc_t * cells_head = NULL, * cells_temp;
  frf_maker_vector_t * vector_head = NULL, * vector_temp, * vector_current;
  uint32_t uniq_vector_offset, vector_offset, result;

  uint32_t * results;

  int cells_cnt = 0, vector_cnt = 0;

  int rval = frf_maker->vector_written;

  char * dt_result;

  frf_malloc_context_t * c;

  c = frf_maker->malloc_context = frf_malloc_context_reset(frf_maker->malloc_context);

  frf_maker_cc_t * stack_elt = frf_maker_compile(frf_maker, frf_maker->content);
  frf_maker_cc_t * dc_elt, * dc_tail;

  results = frf_calloc(c, sizeof(uint32_t), frf_maker->num_uniq_tags);

  while(stack_elt) {
    if (stack_elt->type == FRF_MAKER_CC_TYPE_DC) {
      HASH_FIND(hh, stack_elt->val.dc->alternatives, p13n[stack_elt->val.dc->p13n_offset], strlen(p13n[stack_elt->val.dc->p13n_offset]), dc_node);

      if (dc_node) {
	dc_content = dc_node->content;
      } else {
	dc_content = stack_elt->val.dc->def;
      }

      int old_num_uniq_tags = frf_maker->num_uniq_tags;

      dc_tail = dc_elt = frf_maker_compile(frf_maker, dc_content);

      if (old_num_uniq_tags != frf_maker->num_uniq_tags) {
	results = frf_realloc(c, results, sizeof(uint32_t) * frf_maker->num_uniq_tags);
	memset((char *)results + (sizeof(uint32_t) * old_num_uniq_tags), 0, (frf_maker->num_uniq_tags - old_num_uniq_tags) * sizeof(uint32_t));
      }

      while (dc_tail->next) {
	dc_tail = dc_tail->next;
      }
      dc_tail->next = stack_elt->next;
      stack_elt = dc_elt;
    } else {
      if (stack_elt->type == FRF_MAKER_CC_TYPE_STATIC) {
	result = stack_elt->val.offset;
      } else if (results[stack_elt->value_index]) {
	result = results[stack_elt->value_index];
      } else {
	switch (stack_elt->type) {
	  case FRF_MAKER_CC_TYPE_P13N:
	    result = frf_maker_add_to_string_table(frf_maker, p13n[stack_elt->val.p13n], strlen(p13n[stack_elt->val.p13n]));
	    break;
	  case FRF_MAKER_CC_TYPE_DT:
	    dt_result = frf_transform_exec(c, stack_elt->val.exp, p13n);

	    result = frf_maker_add_to_string_table(frf_maker, dt_result, strlen(dt_result));
	    break;
	  default:
	    error(1, 0, "Shouldn't be here with %d\n", stack_elt->type);
	}

	results[stack_elt->value_index] = result;
      }
      ASSERT_INDEX(result);

      cells_cnt++;
      cells_temp = frf_malloc(c, sizeof(*cells_temp));
      cells_temp->type = stack_elt->type;

      DL_APPEND(cells_head, cells_temp);

      if (stack_elt->type == FRF_MAKER_CC_TYPE_P13N || stack_elt->type == FRF_MAKER_CC_TYPE_DT) {
        vector_cnt++;
        vector_temp = frf_malloc(c, sizeof(*vector_temp));
	
        vector_temp->offset = result;

	DL_APPEND(vector_head, vector_temp);
      } else {
	cells_temp->val.offset = result;
      }

      stack_elt = stack_elt->next;
    }
  }

  uniq_vector_offset = htonl(frf_maker_add_uniq_vector(frf_maker, cells_head, cells_cnt, vector_head, vector_cnt));

  fwrite(&uniq_vector_offset, 4, 1, frf_maker->vector_fh);

  DL_FOREACH(vector_head, vector_current) {
    ASSERT_INDEX(vector_current->offset);

    vector_offset = htonl(vector_current->offset);
    fwrite(&vector_offset, 4, 1, frf_maker->vector_fh);
  }

  frf_maker->vector_written += 4 * (vector_cnt + 1);

  frf_maker->num_rows++;

  return rval;
}
