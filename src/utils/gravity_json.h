#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef __GRAVITY_JSON_SERIALIZER__
#define __GRAVITY_JSON_SERIALIZER__

// MARK: JSON serializer -

typedef enum
{
    json_opt_none           =   0x00,
    json_opt_need_comma     =   0x01,
    json_opt_prettify       =   0x02,
    json_opt_no_maptype     =   0x04,
    json_opt_no_undef       =   0x08,
    json_opt_unused_1       =   0x10,
    json_opt_unused_2       =   0x20,
    json_opt_unused_3       =   0x40,
    json_opt_unused_4       =   0x80,
    json_opt_unused_5       =   0x100,
} json_opt_mask;

typedef struct json_t    json_t;
json_t      *json_new (void);
void        json_free (json_t *json);

void        json_begin_object (json_t *json, const char *key);
void        json_end_object (json_t *json);
void        json_begin_array (json_t *json, const char *key);
void        json_end_array (json_t *json);
void        json_add_cstring (json_t *json, const char *key, const char *value);
void        json_add_string (json_t *json, const char *key, const char *value, size_t len);
void        json_add_int (json_t *json, const char *key, int64_t value);
void        json_add_double (json_t *json, const char *key, double value);
void        json_add_bool (json_t *json, const char *key, bool value);
void        json_add_null (json_t *json, const char *key);
void        json_set_label (json_t *json, const char *key);
const char  *json_get_label (json_t *json, const char *key);

char        *json_buffer (json_t *json, size_t *len);
bool        json_write_file (json_t *json, const char *path);

uint32_t    json_get_options (json_t *json);
void        json_set_option (json_t *json, json_opt_mask option_value);
void        json_clear_option (json_t *json, json_opt_mask option_value);
bool        json_option_isset (json_t *json, json_opt_mask option_value);

#endif

// MARK: - JSON Parser -
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

#ifndef _JSON_H
#define _JSON_H

#ifndef json_char
   #define json_char char
#endif

#ifndef json_int_t
   #ifndef _MSC_VER
      #define json_int_t int64_t
   #else
      #define json_int_t __int64
   #endif
#endif

#include <stdlib.h>

#ifdef __cplusplus

   #include <string.h>

   extern "C"
   {

#endif

typedef struct
{
   unsigned long max_memory;
   int settings;

   /* Custom allocator support (leave null to use malloc/free)
    */

   void * (* memory_alloc) (size_t, int zero, void * user_data);
   void (* memory_free) (void *, void * user_data);

   void * user_data;  /* will be passed to mem_alloc and mem_free */

   size_t value_extra;  /* how much extra space to allocate for values? */

} json_settings;

#define json_enable_comments  0x01

typedef enum
{
   json_none,
   json_object,
   json_array,
   json_integer,
   json_double,
   json_string,
   json_boolean,
   json_null

} json_type;

extern const struct _json_value json_value_none;

typedef struct _json_object_entry
{
    json_char * name;
    unsigned int name_length;

    struct _json_value * value;

} json_object_entry;

typedef struct _json_value
{
   struct _json_value * parent;

   json_type type;

   union
   {
      int boolean;
      json_int_t integer;
      double dbl;

      struct
      {
         unsigned int length;
         json_char * ptr; /* null terminated */

      } string;

      struct
      {
         unsigned int length;

         json_object_entry * values;

         #if defined(__cplusplus) && __cplusplus >= 201103L
         decltype(values) begin () const
         {  return values;
         }
         decltype(values) end () const
         {  return values + length;
         }
         #endif

      } object;

      struct
      {
         unsigned int length;
         struct _json_value ** values;

         #if defined(__cplusplus) && __cplusplus >= 201103L
         decltype(values) begin () const
         {  return values;
         }
         decltype(values) end () const
         {  return values + length;
         }
         #endif

      } array;

   } u;

   union
   {
      struct _json_value * next_alloc;
      void * object_mem;

   } _reserved;

   #ifdef JSON_TRACK_SOURCE

      /* Location of the value in the source JSON
       */
      unsigned int line, col;

   #endif


   /* Some C++ operator sugar */

   #ifdef __cplusplus

      public:

         inline _json_value ()
         {  memset (this, 0, sizeof (_json_value));
         }

         inline const struct _json_value &operator [] (int index) const
         {
            if (type != json_array || index < 0
                     || ((unsigned int) index) >= u.array.length)
            {
               return json_value_none;
            }

            return *u.array.values [index];
         }

         inline const struct _json_value &operator [] (const char * index) const
         {
            if (type != json_object)
               return json_value_none;

            for (unsigned int i = 0; i < u.object.length; ++ i)
               if (!strcmp (u.object.values [i].name, index))
                  return *u.object.values [i].value;

            return json_value_none;
         }

         inline operator const char * () const
         {
            switch (type)
            {
               case json_string:
                  return u.string.ptr;

               default:
                  return "";
            };
         }

         inline operator json_int_t () const
         {
            switch (type)
            {
               case json_integer:
                  return u.integer;

               case json_double:
                  return (json_int_t) u.dbl;

               default:
                  return 0;
            };
         }

         inline operator bool () const
         {
            if (type != json_boolean)
               return false;

            return u.boolean != 0;
         }

         inline operator double () const
         {
            switch (type)
            {
               case json_integer:
                  return (double) u.integer;

               case json_double:
                  return u.dbl;

               default:
                  return 0;
            };
         }

   #endif

} json_value;

#define EMPTY_SETTINGS_STRUCT    {0,0,0,0,0,0}
#define EMPTY_STATE_STRUCT        {0,0,0,EMPTY_SETTINGS_STRUCT,0,0,0,0}

json_value * json_parse (const json_char * json,
                         size_t length);

#define json_error_max 128
json_value * json_parse_ex (json_settings * settings,
                            const json_char * json,
                            size_t length,
                            char * error);

void json_value_free (json_value *);


/* Not usually necessary, unless you used a custom mem_alloc and now want to
 * use a custom mem_free.
 */
void json_value_free_ex (json_settings * settings,
                         json_value *);


#ifdef __cplusplus
   } /* extern "C" */
#endif

#endif
