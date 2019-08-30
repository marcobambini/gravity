//
//  gravity_hash.h
//  gravity
//
//  Created by Marco Bambini on 23/04/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_HASH__
#define __GRAVITY_HASH__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "gravity_value.h"

#define GRAVITYHASH_ENABLE_STATS    1               // if 0 then stats are not enabled
#define GRAVITYHASH_DEFAULT_SIZE    32              // default hash table size (used if 0 is passed in gravity_hash_create)
#define GRAVITYHASH_THRESHOLD       0.75            // threshold used to decide when re-hash the table
#define GRAVITYHASH_MAXENTRIES      1073741824      // please don't put more than 1 billion values in my hash table (2^30)

#ifndef GRAVITY_HASH_DEFINED
#define GRAVITY_HASH_DEFINED
typedef struct         gravity_hash_t    gravity_hash_t;    // opaque hash table struct
#endif

#ifdef __cplusplus
extern "C" {
#endif

// CALLBACK functions
typedef uint32_t    (*gravity_hash_compute_fn) (gravity_value_t key);
typedef bool        (*gravity_hash_isequal_fn) (gravity_value_t v1, gravity_value_t v2);
typedef void        (*gravity_hash_iterate_fn) (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data);
typedef void        (*gravity_hash_iterate2_fn) (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data1, void *data2);
typedef void        (*gravity_hash_iterate3_fn) (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data1, void *data2, void *data3);
typedef void        (*gravity_hash_transform_fn) (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t *value, void *data);
typedef bool        (*gravity_hash_compare_fn) (gravity_value_t value1, gravity_value_t value2, void *data);

// PUBLIC functions
GRAVITY_API gravity_hash_t  *gravity_hash_create (uint32_t size, gravity_hash_compute_fn compute, gravity_hash_isequal_fn isequal, gravity_hash_iterate_fn free, void *data);
GRAVITY_API void            gravity_hash_free (gravity_hash_t *hashtable);
GRAVITY_API bool            gravity_hash_isempty (gravity_hash_t *hashtable);
GRAVITY_API bool            gravity_hash_remove  (gravity_hash_t *hashtable, gravity_value_t key);
GRAVITY_API bool            gravity_hash_insert (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value);
GRAVITY_API gravity_value_t *gravity_hash_lookup (gravity_hash_t *hashtable, gravity_value_t key);
GRAVITY_API gravity_value_t *gravity_hash_lookup_cstring (gravity_hash_t *hashtable, const char *key);

GRAVITY_API uint32_t        gravity_hash_memsize (gravity_hash_t *hashtable);
GRAVITY_API uint32_t        gravity_hash_count (gravity_hash_t *hashtable);
GRAVITY_API uint32_t        gravity_hash_compute_buffer (const char *key, uint32_t len);
GRAVITY_API uint32_t        gravity_hash_compute_int (gravity_int_t n);
GRAVITY_API uint32_t        gravity_hash_compute_float (gravity_float_t f);
GRAVITY_API void            gravity_hash_stat (gravity_hash_t *hashtable);
GRAVITY_API void            gravity_hash_iterate (gravity_hash_t *hashtable, gravity_hash_iterate_fn iterate, void *data);
GRAVITY_API void            gravity_hash_iterate2 (gravity_hash_t *hashtable, gravity_hash_iterate2_fn iterate, void *data1, void *data2);
GRAVITY_API void            gravity_hash_iterate3 (gravity_hash_t *hashtable, gravity_hash_iterate3_fn iterate, void *data1, void *data2, void *data3);
GRAVITY_API void            gravity_hash_transform (gravity_hash_t *hashtable, gravity_hash_transform_fn iterate, void *data);
GRAVITY_API void            gravity_hash_dump (gravity_hash_t *hashtable);
GRAVITY_API void            gravity_hash_append (gravity_hash_t *hashtable1, gravity_hash_t *hashtable2);
GRAVITY_API void            gravity_hash_resetfree (gravity_hash_t *hashtable);

GRAVITY_API bool            gravity_hash_compare (gravity_hash_t *hashtable1, gravity_hash_t *hashtable2, gravity_hash_compare_fn compare, void *data);

#ifdef __cplusplus
}
#endif

#endif
