/* vim: set et ts=3 sw=3 sts=3 ft=c:
 *
 * Copyright (C) 2012, 2013, 2014 James McLaughlin et al.  All rights reserved.
 * https://github.com/udp/json-parser
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gravity_json.h"
#include "gravity_utils.h"
#include "gravity_memory.h"
#include "gravity_array.h"
#include "gravity_config.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>

#ifdef _MSC_VER
   #ifndef _CRT_SECURE_NO_WARNINGS
      #define _CRT_SECURE_NO_WARNINGS
   #endif
#endif

// MARK: - JSON Serializer -
// Written by Marco Bambini

#define JSON_MINSIZE            4096
#define JSON_NEWLINE            "\n"
#define JSON_NEWLINE_CHAR       '\n'
#define JSON_PRETTYLINE         "    "
#define JSON_PRETTYSIZE         4
#define JSON_WRITE_COLUMN       json_write_raw(json, ":", 1, false, false)
#define JSON_WRITE_COMMA        json_write_raw(json, ",", 1, false, false);

#define JSON_POP_CTX(j)         marray_pop(j->context)
#define JSON_PUSH_CTX(j,x)      marray_push(JSON_CONTEXT, j->context, x);
#define JSON_CURR_CTX(j)        marray_last(j->context)

#define JSON_ESCAPE(c)          do {        \
                                    new_buffer[j] = '\\';     \
                                    new_buffer[j+1] = (c);    \
                                    j+=2;                     \
                                } while(0);                   \

typedef enum {
    JSON_CONTEXT_ROOT = 0,
    JSON_CONTEXT_OBJECT = 1,
    JSON_CONTEXT_ARRAY = 2
} JSON_CONTEXT;
typedef marray_t(JSON_CONTEXT)    JSON_CONTEXT_R;

// MARK: -

struct json_t {
    char            *buffer;
    size_t          blen;
    size_t          bused;
    
    const char      *label;
    uint32_t        options;
    JSON_CONTEXT_R  context;
};

json_t *json_new (void) {
    json_t *json = mem_alloc(NULL, sizeof(json_t));
    assert(json);

    json->buffer = mem_alloc(NULL, JSON_MINSIZE);
    assert(json->buffer);
    
    json->blen = JSON_MINSIZE;
    json->bused = 0;
    json->options = json_opt_none;
    marray_init(json->context);
    marray_push(JSON_CONTEXT, json->context, JSON_CONTEXT_ROOT);
    
    return json;
}

void json_free (json_t *json) {
    JSON_CONTEXT context = marray_pop(json->context);
    if (context != JSON_CONTEXT_ROOT) assert(0);
    
    marray_destroy(json->context);
    mem_free(json->buffer);
    mem_free(json);
}

// MARK: - Private

static void json_write_raw (json_t *json, const char *buffer, size_t len, bool escape, bool is_pretty) {
    // pretty output disabled in this version
    is_pretty = false;
    
    bool      pretty_mask = json_option_isset(json, json_opt_prettify);
    uint32_t  ident_count = ((uint32_t) marray_size(json->context)) - 1;
    size_t    prettylen = (is_pretty && pretty_mask) ? (ident_count * JSON_PRETTYSIZE) : 0;
    size_t    escapelen = (escape) ? 2 : 0;

    // check buffer reallocation
    size_t reqlen = json->bused + len + prettylen + escapelen + JSON_MINSIZE;
    if (reqlen >= json->blen) {
        json->buffer = mem_realloc(NULL, json->buffer, json->blen + reqlen);
        assert(json->buffer);
        json->blen += reqlen;
    }

    if (is_pretty && pretty_mask) {
        for (uint32_t i=0; i<ident_count; ++i) {
            memcpy(json->buffer+json->bused, JSON_PRETTYLINE, JSON_PRETTYSIZE);
            json->bused += JSON_PRETTYSIZE;
        }
    }

    if (escape) {
        memcpy(json->buffer+json->bused, "\"", 1);
        json->bused += 1;
    }

    memcpy(json->buffer+json->bused, buffer, len);
    json->bused += len;

    if (escape) {
        memcpy(json->buffer+json->bused, "\"", 1);
        json->bused += 1;
    }
}

static void json_write_escaped (json_t *json, const char *buffer, size_t len, bool escape, bool is_pretty) {
    if (!len) {
        json_write_raw(json, "", 0, escape, is_pretty);
        return;
    }

    char *new_buffer = mem_alloc(NULL, len*2);
    size_t j = 0;
    assert(new_buffer);

    for (size_t i=0; i<len; ++i) {
        char c = buffer[i];
        switch (c) {
            case '"' : JSON_ESCAPE ('\"');  continue;
            case '\\': JSON_ESCAPE ('\\');  continue;
            case '\b': JSON_ESCAPE ('b');   continue;
            case '\f': JSON_ESCAPE ('f');   continue;
            case '\n': JSON_ESCAPE ('n');   continue;
            case '\r': JSON_ESCAPE ('r');   continue;
            case '\t': JSON_ESCAPE ('t');   continue;

            default: new_buffer[j] = c; ++j;break;
        };
    }

    json_write_raw(json, new_buffer, j, escape, is_pretty);
    mem_free(new_buffer);
}

static void json_check_comma (json_t *json) {
    if (json_option_isset(json, json_opt_need_comma)) {
        JSON_WRITE_COMMA;
    } else {
        json_set_option(json, json_opt_need_comma);
    }
}

// MARK: - Public

void json_begin_object (json_t *json, const char *key) {
    json_check_comma(json);
    
    // ignore given key if not inside an object
    if (JSON_CURR_CTX(json) != JSON_CONTEXT_OBJECT) {
        key = NULL;
    }

    if (key) {
        json_write_raw (json, key, strlen(key), true, true);
        JSON_WRITE_COLUMN;
    }
    
    JSON_PUSH_CTX(json, JSON_CONTEXT_OBJECT);
    json_write_raw(json, "{", 1, false, (key == NULL));
    
    json_clear_option(json, json_opt_need_comma);
}

void json_end_object (json_t *json) {
    JSON_POP_CTX(json);
    
    json_set_option(json, json_opt_need_comma);
    json_write_raw(json, "}", 1, false, true);
}

void json_begin_array (json_t *json, const char *key) {
    json_check_comma(json);
    
    // ignore given key if not inside an object
    if (JSON_CURR_CTX(json) != JSON_CONTEXT_OBJECT) {
        key = NULL;
    }
    
    if (key) {
        json_write_raw (json, key, strlen(key), true, true);
        JSON_WRITE_COLUMN;
    }

    JSON_PUSH_CTX(json, JSON_CONTEXT_ARRAY);
    json_write_raw(json, "[", 1, false, (key == NULL));
    
    json_clear_option(json, json_opt_need_comma);
}

void json_end_array (json_t *json) {
    JSON_POP_CTX(json);
    
    json_set_option(json, json_opt_need_comma);
    json_write_raw(json, "]", 1, false, true);
}

void json_add_string (json_t *json, const char *key, const char *value, size_t len) {
    json_check_comma(json);
    
    if (!value) {
        json_add_null(json, key);
        return;
    }

    if (key) {
        json_write_raw (json, key, strlen(key), true, true);
        JSON_WRITE_COLUMN;
    }

    // check if string value needs to be escaped
    bool write_escaped = false;
    for (size_t i=0; i<len; ++i) {
        if (value[i] == '"') {write_escaped = true; break;}
    }
    if (len == 0) write_escaped = true;

    if (write_escaped)
        json_write_escaped(json, value, len, true, (key == NULL));
    else
        json_write_raw(json, value, len, true, (key == NULL));
}

void json_add_cstring (json_t *json, const char *key, const char *value) {
    json_add_string(json, key, value, (value) ? strlen(value) : 0);
}

void json_add_int (json_t *json, const char *key, int64_t value) {
    json_check_comma(json);
    
    char buffer[512];
    size_t len = snprintf(buffer, sizeof(buffer), "%" PRId64, value);

    if (key) {
        json_write_raw (json, key, strlen(key), true, true);
        JSON_WRITE_COLUMN;
    }
    
    json_write_raw(json, buffer, len, false, (key == NULL));
}

void json_add_double (json_t *json, const char *key, double value) {
    json_check_comma(json);
    
    char buffer[512];
    // was %g but we don't like scientific notation nor the missing .0 in case of float number with no decimals
    size_t len = snprintf(buffer, sizeof(buffer), "%f", value);

    if (key) {
        json_write_raw (json, key, strlen(key), true, true);
        JSON_WRITE_COLUMN;
    }
    
    json_write_raw(json, buffer, len, false, (key == NULL));
}

void json_add_bool (json_t *json, const char *key, bool bvalue) {
    json_check_comma(json);
    
    const char *value = (bvalue) ? "true" : "false";

    if (key) {
        json_write_raw (json, key, strlen(key), true, true);
        JSON_WRITE_COLUMN;
    }
    
    json_write_raw(json, value, strlen(value), false, (key == NULL));
}

void json_add_null (json_t *json, const char *key) {
    json_check_comma (json);
    
    if (key) {
        json_write_raw (json, key, strlen(key), true, true);
        JSON_WRITE_COLUMN;
    }
    
    json_write_raw(json, "null", 4, false, (key == NULL));
}

void json_set_label (json_t *json, const char *key) {
    if (JSON_CURR_CTX(json) != JSON_CONTEXT_OBJECT) return;
    json->label = key;
}

const char *json_get_label (json_t *json, const char *key) {
    if (JSON_CURR_CTX(json) != JSON_CONTEXT_OBJECT) return NULL;
    
    if (json->label) {
        const char *result = json->label;
        json->label = NULL;
        return result;
    }
    
    if (key) return key;
    assert(0);
	return NULL;
}

// MARK: - Buffer

char *json_buffer (json_t *json, size_t *len) {
    assert(json->buffer);
    if (len) *len = json->bused;
    return json->buffer;
}

bool json_write_file (json_t *json, const char *path) {
    return file_write(path, json->buffer, json->bused);
}

// MARK: - Options

uint32_t json_get_options (json_t *json) {
    return json->options;
}

void json_set_option (json_t *json, json_opt_mask option_value) {
    json->options |= option_value;
}

bool json_option_isset (json_t *json, json_opt_mask option_value) {
    return (json->options & option_value);
}

void json_clear_option (json_t *json, json_opt_mask option_value) {
    json->options &= ~option_value;
}

#undef JSON_MINSIZE
#undef JSON_NEWLINE
#undef JSON_NEWLINE_CHAR
#undef JSON_PRETTYLINE
#undef JSON_PRETTYSIZE
#undef JSON_WRITE_SEP
#undef JSON_TERM_FIELD

// MARK: - JSON Parser -
// Written by https://github.com/udp/json-parser

const struct _json_value json_value_none;
typedef unsigned int json_uchar;

static unsigned char hex_value (json_char c)
{
   if (isdigit(c))
      return c - '0';

   switch (c) {
      case 'a': case 'A': return 0x0A;
      case 'b': case 'B': return 0x0B;
      case 'c': case 'C': return 0x0C;
      case 'd': case 'D': return 0x0D;
      case 'e': case 'E': return 0x0E;
      case 'f': case 'F': return 0x0F;
      default: return 0xFF;
   }
}

typedef struct
{
   unsigned long used_memory;

   unsigned int uint_max;
   unsigned long ulong_max;

   json_settings settings;
   int first_pass;

   const json_char * ptr;
   unsigned int cur_line, cur_col;

} json_state;

static void * default_alloc (size_t size, int zero, void * user_data)
{
    #pragma unused(zero, user_data)
    return mem_alloc(NULL, size);
    //return zero ? calloc (1, size) : malloc (size);
}

static void default_free (void * ptr, void * user_data)
{
    #pragma unused(user_data)
    mem_free(ptr);
    //free (ptr);
}

static void * json_alloc (json_state * state, unsigned long size, int zero)
{
   if ((state->ulong_max - state->used_memory) < size)
      return 0;

   if (state->settings.max_memory
         && (state->used_memory += size) > state->settings.max_memory)
   {
      return 0;
   }

   return state->settings.memory_alloc (size, zero, state->settings.user_data);
}

static int new_value (json_state * state,
                      json_value ** top, json_value ** root, json_value ** alloc,
                      json_type type)
{
   json_value * value;
   int values_size;

   if (!state->first_pass)
   {
      value = *top = *alloc;
      *alloc = (*alloc)->_reserved.next_alloc;

      if (!*root)
         *root = value;

      switch (value->type)
      {
         case json_array:

            if (value->u.array.length == 0)
               break;

            if (! (value->u.array.values = (json_value **) json_alloc
               (state, value->u.array.length * sizeof (json_value *), 0)) )
            {
               return 0;
            }

            value->u.array.length = 0;
            break;

         case json_object:

            if (value->u.object.length == 0)
               break;

            values_size = sizeof (*value->u.object.values) * value->u.object.length;

            if (! (value->u.object.values = (json_object_entry *) json_alloc
                  (state, values_size + ((unsigned long) value->u.object.values), 0)) )
            {
               return 0;
            }

            value->_reserved.object_mem = (*(char **) &value->u.object.values) + values_size;

            value->u.object.length = 0;
            break;

         case json_string:

            if (! (value->u.string.ptr = (json_char *) json_alloc
               (state, (value->u.string.length + 1) * sizeof (json_char), 0)) )
            {
               return 0;
            }

            value->u.string.length = 0;
            break;

         default:
            break;
      };

      return 1;
   }

   if (! (value = (json_value *) json_alloc
         (state, (unsigned long)(sizeof (json_value) + state->settings.value_extra), 1)))
   {
      return 0;
   }

   if (!*root)
      *root = value;

   value->type = type;
   value->parent = *top;

   #ifdef JSON_TRACK_SOURCE
      value->line = state->cur_line;
      value->col = state->cur_col;
   #endif

   if (*alloc)
      (*alloc)->_reserved.next_alloc = value;

   *alloc = *top = value;

   return 1;
}

#define whitespace \
   case '\n': ++ state.cur_line;  state.cur_col = 0; \
   case ' ': case '\t': case '\r'

#define string_add(b)  \
   do { if (!state.first_pass) string [string_length] = b;  ++ string_length; } while (0);

#define line_and_col \
   state.cur_line, state.cur_col

static const long
   flag_next             = 1 << 0,
   flag_reproc           = 1 << 1,
   flag_need_comma       = 1 << 2,
   flag_seek_value       = 1 << 3,
   flag_escaped          = 1 << 4,
   flag_string           = 1 << 5,
   flag_need_colon       = 1 << 6,
   flag_done             = 1 << 7,
   flag_num_negative     = 1 << 8,
   flag_num_zero         = 1 << 9,
   flag_num_e            = 1 << 10,
   flag_num_e_got_sign   = 1 << 11,
   flag_num_e_negative   = 1 << 12,
   flag_line_comment     = 1 << 13,
   flag_block_comment    = 1 << 14;

json_value * json_parse_ex (json_settings * settings,
                            const json_char * json,
                            size_t length,
                            char * error_buf)
{
   json_char error [json_error_max];
   const json_char * end;
   json_value *top, *root = NULL, * alloc = 0;
   json_state state = EMPTY_STATE_STRUCT;
   long flags;
   long num_digits = 0, num_e = 0;
   json_int_t num_fraction = 0;

   /* Skip UTF-8 BOM
    */
   if (length >= 3 && ((unsigned char) json [0]) == 0xEF
                   && ((unsigned char) json [1]) == 0xBB
                   && ((unsigned char) json [2]) == 0xBF)
   {
      json += 3;
      length -= 3;
   }

   error[0] = '\0';
   end = (json + length);

   memcpy (&state.settings, settings, sizeof (json_settings));

   if (!state.settings.memory_alloc)
      state.settings.memory_alloc = default_alloc;

   if (!state.settings.memory_free)
      state.settings.memory_free = default_free;

   memset (&state.uint_max, 0xFF, sizeof (state.uint_max));
   memset (&state.ulong_max, 0xFF, sizeof (state.ulong_max));

   state.uint_max -= 8; /* limit of how much can be added before next check */
   state.ulong_max -= 8;

   for (state.first_pass = 1; state.first_pass >= 0; -- state.first_pass)
   {
      json_uchar uchar;
      unsigned char uc_b1, uc_b2, uc_b3, uc_b4;
      json_char * string = 0;
      unsigned int string_length = 0;

      top = root = 0;
      flags = flag_seek_value;

      state.cur_line = 1;

      for (state.ptr = json ;; ++ state.ptr)
      {
         json_char b = (state.ptr == end ? 0 : *state.ptr);

         if (flags & flag_string)
         {
            if (!b)
            {  sprintf (error, "Unexpected EOF in string (at %d:%d)", line_and_col);
               goto e_failed;
            }

            if (string_length > state.uint_max)
               goto e_overflow;

            if (flags & flag_escaped)
            {
               flags &= ~ flag_escaped;

               switch (b)
               {
                  case 'b':  string_add ('\b');  break;
                  case 'f':  string_add ('\f');  break;
                  case 'n':  string_add ('\n');  break;
                  case 'r':  string_add ('\r');  break;
                  case 't':  string_add ('\t');  break;
                  case 'u':

                    if (end - state.ptr < 4 ||
                        (uc_b1 = hex_value (*++ state.ptr)) == 0xFF ||
                        (uc_b2 = hex_value (*++ state.ptr)) == 0xFF ||
                        (uc_b3 = hex_value (*++ state.ptr)) == 0xFF ||
                        (uc_b4 = hex_value (*++ state.ptr)) == 0xFF)
                    {
                        sprintf (error, "Invalid character value `%c` (at %d:%d)", b, line_and_col);
                        goto e_failed;
                    }

                    uc_b1 = (uc_b1 << 4) | uc_b2;
                    uc_b2 = (uc_b3 << 4) | uc_b4;
                    uchar = (uc_b1 << 8) | uc_b2;

                    if ((uchar & 0xF800) == 0xD800) {
                        json_uchar uchar2;

                        if (end - state.ptr < 6 || (*++ state.ptr) != '\\' || (*++ state.ptr) != 'u' ||
                            (uc_b1 = hex_value (*++ state.ptr)) == 0xFF ||
                            (uc_b2 = hex_value (*++ state.ptr)) == 0xFF ||
                            (uc_b3 = hex_value (*++ state.ptr)) == 0xFF ||
                            (uc_b4 = hex_value (*++ state.ptr)) == 0xFF)
                        {
                            sprintf (error, "Invalid character value `%c` (at %d:%d)", b, line_and_col);
                            goto e_failed;
                        }

                        uc_b1 = (uc_b1 << 4) | uc_b2;
                        uc_b2 = (uc_b3 << 4) | uc_b4;
                        uchar2 = (uc_b1 << 8) | uc_b2;

                        uchar = 0x010000 | ((uchar & 0x3FF) << 10) | (uchar2 & 0x3FF);
                    }

                    if (sizeof (json_char) >= sizeof (json_uchar) || (uchar <= 0x7F))
                    {
                       string_add ((json_char) uchar);
                       break;
                    }

                    if (uchar <= 0x7FF)
                    {
                        if (state.first_pass)
                           string_length += 2;
                        else
                        {  string [string_length ++] = 0xC0 | (uchar >> 6);
                           string [string_length ++] = 0x80 | (uchar & 0x3F);
                        }

                        break;
                    }

                    if (uchar <= 0xFFFF) {
                        if (state.first_pass)
                           string_length += 3;
                        else
                        {  string [string_length ++] = 0xE0 | (uchar >> 12);
                           string [string_length ++] = 0x80 | ((uchar >> 6) & 0x3F);
                           string [string_length ++] = 0x80 | (uchar & 0x3F);
                        }

                        break;
                    }

                    if (state.first_pass)
                       string_length += 4;
                    else
                    {  string [string_length ++] = 0xF0 | (uchar >> 18);
                       string [string_length ++] = 0x80 | ((uchar >> 12) & 0x3F);
                       string [string_length ++] = 0x80 | ((uchar >> 6) & 0x3F);
                       string [string_length ++] = 0x80 | (uchar & 0x3F);
                    }

                    break;

                  default:
                     string_add (b);
               };

               continue;
            }

            if (b == '\\')
            {
               flags |= flag_escaped;
               continue;
            }

            if (b == '"')
            {
               if (!state.first_pass)
                  string [string_length] = 0;

               flags &= ~ flag_string;
               string = 0;

               switch (top->type)
               {
                  case json_string:

                     top->u.string.length = string_length;
                     flags |= flag_next;

                     break;

                  case json_object:

                     if (state.first_pass)
                        (*(json_char **) &top->u.object.values) += string_length + 1;
                     else
                     {
                        top->u.object.values [top->u.object.length].name
                           = (json_char *) top->_reserved.object_mem;

                        top->u.object.values [top->u.object.length].name_length
                           = string_length;

                        (*(json_char **) &top->_reserved.object_mem) += string_length + 1;
                     }

                     flags |= flag_seek_value | flag_need_colon;
                     continue;

                  default:
                     break;
               };
            }
            else
            {
               string_add (b);
               continue;
            }
         }

         if (state.settings.settings & json_enable_comments)
         {
            if (flags & (flag_line_comment | flag_block_comment))
            {
               if (flags & flag_line_comment)
               {
                  if (b == '\r' || b == '\n' || !b)
                  {
                     flags &= ~ flag_line_comment;
                     -- state.ptr;  /* so null can be reproc'd */
                  }

                  continue;
               }

               if (flags & flag_block_comment)
               {
                  if (!b)
                  {  sprintf (error, "%d:%d: Unexpected EOF in block comment", line_and_col);
                     goto e_failed;
                  }

                  if (b == '*' && state.ptr < (end - 1) && state.ptr [1] == '/')
                  {
                     flags &= ~ flag_block_comment;
                     ++ state.ptr;  /* skip closing sequence */
                  }

                  continue;
               }
            }
            else if (b == '/')
            {
               if (! (flags & (flag_seek_value | flag_done)) && top->type != json_object)
               {  sprintf (error, "%d:%d: Comment not allowed here", line_and_col);
                  goto e_failed;
               }

               if (++ state.ptr == end)
               {  sprintf (error, "%d:%d: EOF unexpected", line_and_col);
                  goto e_failed;
               }

               switch (b = *state.ptr)
               {
                  case '/':
                     flags |= flag_line_comment;
                     continue;

                  case '*':
                     flags |= flag_block_comment;
                     continue;

                  default:
                     sprintf (error, "%d:%d: Unexpected `%c` in comment opening sequence", line_and_col, b);
                     goto e_failed;
               };
            }
         }

         if (flags & flag_done)
         {
            if (!b)
               break;

            switch (b)
            {
               whitespace:
                  continue;

               default:

                  sprintf (error, "%d:%d: Trailing garbage: `%c`",
                           state.cur_line, state.cur_col, b);

                  goto e_failed;
            };
         }

         if (flags & flag_seek_value)
         {
            switch (b)
            {
               whitespace:
                  continue;

               case ']':

                  if (top && top->type == json_array)
                     flags = (flags & ~ (flag_need_comma | flag_seek_value)) | flag_next;
                  else
                  {  sprintf (error, "%d:%d: Unexpected ]", line_and_col);
                     goto e_failed;
                  }

                  break;

               default:

                  if (flags & flag_need_comma)
                  {
                     if (b == ',')
                     {  flags &= ~ flag_need_comma;
                        continue;
                     }
                     else
                     {
                        sprintf (error, "%d:%d: Expected , before %c",
                                 state.cur_line, state.cur_col, b);

                        goto e_failed;
                     }
                  }

                  if (flags & flag_need_colon)
                  {
                     if (b == ':')
                     {  flags &= ~ flag_need_colon;
                        continue;
                     }
                     else
                     {
                        sprintf (error, "%d:%d: Expected : before %c",
                                 state.cur_line, state.cur_col, b);

                        goto e_failed;
                     }
                  }

                  flags &= ~ flag_seek_value;

                  switch (b)
                  {
                     case '{':

                        if (!new_value (&state, &top, &root, &alloc, json_object))
                           goto e_alloc_failure;

                        continue;

                     case '[':

                        if (!new_value (&state, &top, &root, &alloc, json_array))
                           goto e_alloc_failure;

                        flags |= flag_seek_value;
                        continue;

                     case '"':

                        if (!new_value (&state, &top, &root, &alloc, json_string))
                           goto e_alloc_failure;

                        flags |= flag_string;

                        string = top->u.string.ptr;
                        string_length = 0;

                        continue;

                     case 't':

                        if ((end - state.ptr) < 3 || *(++ state.ptr) != 'r' ||
                            *(++ state.ptr) != 'u' || *(++ state.ptr) != 'e')
                        {
                           goto e_unknown_value;
                        }

                        if (!new_value (&state, &top, &root, &alloc, json_boolean))
                           goto e_alloc_failure;

                        top->u.boolean = 1;

                        flags |= flag_next;
                        break;

                     case 'f':

                        if ((end - state.ptr) < 4 || *(++ state.ptr) != 'a' ||
                            *(++ state.ptr) != 'l' || *(++ state.ptr) != 's' ||
                            *(++ state.ptr) != 'e')
                        {
                           goto e_unknown_value;
                        }

                        if (!new_value (&state, &top, &root, &alloc, json_boolean))
                           goto e_alloc_failure;

                        flags |= flag_next;
                        break;

                     case 'n':

                        if ((end - state.ptr) < 3 || *(++ state.ptr) != 'u' ||
                            *(++ state.ptr) != 'l' || *(++ state.ptr) != 'l')
                        {
                           goto e_unknown_value;
                        }

                        if (!new_value (&state, &top, &root, &alloc, json_null))
                           goto e_alloc_failure;

                        flags |= flag_next;
                        break;

                     default:

                        if (isdigit (b) || b == '-')
                        {
                           if (!new_value (&state, &top, &root, &alloc, json_integer))
                              goto e_alloc_failure;

                           if (!state.first_pass)
                           {
                              while (isdigit (b) || b == '+' || b == '-'
                                        || b == 'e' || b == 'E' || b == '.')
                              {
                                 if ( (++ state.ptr) == end)
                                 {
                                    b = 0;
                                    break;
                                 }

                                 b = *state.ptr;
                              }

                              flags |= flag_next | flag_reproc;
                              break;
                           }

                           flags &= ~ (flag_num_negative | flag_num_e |
                                        flag_num_e_got_sign | flag_num_e_negative |
                                           flag_num_zero);

                           num_digits = 0;
                           num_fraction = 0;
                           num_e = 0;

                           if (b != '-')
                           {
                              flags |= flag_reproc;
                              break;
                           }

                           flags |= flag_num_negative;
                           continue;
                        }
                        else
                        {  sprintf (error, "%d:%d: Unexpected %c when seeking value", line_and_col, b);
                           goto e_failed;
                        }
                  };
            };
         }
         else
         {
            switch (top->type)
            {
            case json_object:

               switch (b)
               {
                  whitespace:
                     continue;

                  case '"':

                     if (flags & flag_need_comma)
                     {  sprintf (error, "%d:%d: Expected , before \"", line_and_col);
                        goto e_failed;
                     }

                     flags |= flag_string;

                     string = (json_char *) top->_reserved.object_mem;
                     string_length = 0;

                     break;

                  case '}':

                     flags = (flags & ~ flag_need_comma) | flag_next;
                     break;

                  case ',':

                     if (flags & flag_need_comma)
                     {
                        flags &= ~ flag_need_comma;
                        break;
                     }

                  default:
                     sprintf (error, "%d:%d: Unexpected `%c` in object", line_and_col, b);
                     goto e_failed;
               };

               break;

            case json_integer:
            case json_double:

               if (isdigit (b))
               {
                  ++ num_digits;

                  if (top->type == json_integer || flags & flag_num_e)
                  {
                     if (! (flags & flag_num_e))
                     {
                        if (flags & flag_num_zero)
                        {  sprintf (error, "%d:%d: Unexpected `0` before `%c`", line_and_col, b);
                           goto e_failed;
                        }

                        if (num_digits == 1 && b == '0')
                           flags |= flag_num_zero;
                     }
                     else
                     {
                        flags |= flag_num_e_got_sign;
                        num_e = (num_e * 10) + (b - '0');
                        continue;
                     }

                     top->u.integer = (top->u.integer * 10) + (b - '0');
                     continue;
                  }

                  num_fraction = (num_fraction * 10) + (b - '0');
                  continue;
               }

               if (b == '+' || b == '-')
               {
                  if ( (flags & flag_num_e) && !(flags & flag_num_e_got_sign))
                  {
                     flags |= flag_num_e_got_sign;

                     if (b == '-')
                        flags |= flag_num_e_negative;

                     continue;
                  }
               }
               else if (b == '.' && top->type == json_integer)
               {
                  if (!num_digits)
                  {  sprintf (error, "%d:%d: Expected digit before `.`", line_and_col);
                     goto e_failed;
                  }

                  top->type = json_double;
                  top->u.dbl = (double) top->u.integer;

                  num_digits = 0;
                  continue;
               }

               if (! (flags & flag_num_e))
               {
                  if (top->type == json_double)
                  {
                     if (!num_digits)
                     {  sprintf (error, "%d:%d: Expected digit after `.`", line_and_col);
                        goto e_failed;
                     }

                     top->u.dbl += ((double) num_fraction) / (pow (10.0, (double) num_digits));
                  }

                  if (b == 'e' || b == 'E')
                  {
                     flags |= flag_num_e;

                     if (top->type == json_integer)
                     {
                        top->type = json_double;
                        top->u.dbl = (double) top->u.integer;
                     }

                     num_digits = 0;
                     flags &= ~ flag_num_zero;

                     continue;
                  }
               }
               else
               {
                  if (!num_digits)
                  {  sprintf (error, "%d:%d: Expected digit after `e`", line_and_col);
                     goto e_failed;
                  }

                  top->u.dbl *= pow (10.0, (double)
                      (flags & flag_num_e_negative ? - num_e : num_e));
               }

               if (flags & flag_num_negative)
               {
                  if (top->type == json_integer)
                     top->u.integer = - top->u.integer;
                  else
                     top->u.dbl = - top->u.dbl;
               }

               flags |= flag_next | flag_reproc;
               break;

            default:
               break;
            };
         }

         if (flags & flag_reproc)
         {
            flags &= ~ flag_reproc;
            -- state.ptr;
         }

         if (flags & flag_next)
         {
            flags = (flags & ~ flag_next) | flag_need_comma;

            if (!top->parent)
            {
               /* root value done */

               flags |= flag_done;
               continue;
            }

            if (top->parent->type == json_array)
               flags |= flag_seek_value;

            if (!state.first_pass)
            {
               json_value * parent = top->parent;

               switch (parent->type)
               {
                  case json_object:

                     parent->u.object.values
                        [parent->u.object.length].value = top;

                     break;

                  case json_array:

                     parent->u.array.values
                           [parent->u.array.length] = top;

                     break;

                  default:
                     break;
               };
            }

            if ( (++ top->parent->u.array.length) > state.uint_max)
               goto e_overflow;

            top = top->parent;

            continue;
         }
      }

      alloc = root;
   }

   return root;

e_unknown_value:

   sprintf (error, "%d:%d: Unknown value", line_and_col);
   goto e_failed;

e_alloc_failure:

   strcpy (error, "Memory allocation failure");
   goto e_failed;

e_overflow:

   sprintf (error, "%d:%d: Too long (caught overflow)", line_and_col);
   goto e_failed;

e_failed:

   if (error_buf)
   {
      if (*error)
         strcpy (error_buf, error);
      else
         strcpy (error_buf, "Unknown error");
   }

   if (state.first_pass)
      alloc = root;

   while (alloc)
   {
      top = alloc->_reserved.next_alloc;
      state.settings.memory_free (alloc, state.settings.user_data);
      alloc = top;
   }

   if (!state.first_pass)
      json_value_free_ex (&state.settings, root);

   return 0;
}

json_value * json_parse (const json_char * json, size_t length)
{
   json_settings settings = EMPTY_SETTINGS_STRUCT;
   return json_parse_ex (&settings, json, length, 0);
}

void json_value_free_ex (json_settings * settings, json_value * value)
{
   json_value * cur_value;

   if (!value)
      return;

   value->parent = 0;

   while (value)
   {
      switch (value->type)
      {
         case json_array:

            if (!value->u.array.length)
            {
               if (value->u.array.values) settings->memory_free (value->u.array.values, settings->user_data);
               break;
            }

            value = value->u.array.values [-- value->u.array.length];
            continue;

         case json_object:

            if (!value->u.object.length)
            {
               settings->memory_free (value->u.object.values, settings->user_data);
               break;
            }

            value = value->u.object.values [-- value->u.object.length].value;
            continue;

         case json_string:

            settings->memory_free (value->u.string.ptr, settings->user_data);
            break;

         default:
            break;
      };

      cur_value = value;
      value = value->parent;
      settings->memory_free (cur_value, settings->user_data);
   }
}

void json_value_free (json_value * value)
{
   json_settings settings = EMPTY_SETTINGS_STRUCT;
   settings.memory_free = default_free;
   json_value_free_ex (&settings, value);
}
