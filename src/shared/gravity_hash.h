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

#define GRAVITYHASH_ENABLE_STATS	1						// if 0 then stats are not enabled
#define GRAVITYHASH_DEFAULT_SIZE	32						// default hash table size (used if 0 is passed in gravity_hash_create)
#define GRAVITYHASH_THRESHOLD		0.75					// threshold used to decide when re-hash the table

typedef struct 		gravity_hash_t	gravity_hash_t;			// opaque hash table struct

// CALLBACK functions
typedef uint32_t	(*gravity_hash_compute_fn) (gravity_value_t key);
typedef bool		(*gravity_hash_isequal_fn) (gravity_value_t v1, gravity_value_t v2);
typedef void  		(*gravity_hash_iterate_fn) (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data);
typedef void  		(*gravity_hash_iterate2_fn) (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data1, void *data2);
typedef void		(*gravity_hash_transform_fn) (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t *value, void *data);

// PUBLIC functions
gravity_hash_t		*gravity_hash_create (uint32_t size, gravity_hash_compute_fn compute, gravity_hash_isequal_fn isequal, gravity_hash_iterate_fn free, void *data);
void				gravity_hash_free (gravity_hash_t *hashtable);
bool				gravity_hash_isempty (gravity_hash_t *hashtable);
bool				gravity_hash_remove  (gravity_hash_t *hashtable, gravity_value_t key);
bool				gravity_hash_insert (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value);
gravity_value_t		*gravity_hash_lookup (gravity_hash_t *hashtable, gravity_value_t key);

uint32_t			gravity_hash_memsize (gravity_hash_t *hashtable);
uint32_t			gravity_hash_count (gravity_hash_t *hashtable);
uint32_t			gravity_hash_compute_buffer (const char *key, uint32_t len);
uint32_t			gravity_hash_compute_int (gravity_int_t n);
uint32_t			gravity_hash_compute_float (gravity_float_t f);
void				gravity_hash_stat (gravity_hash_t *hashtable);
void				gravity_hash_iterate (gravity_hash_t *hashtable, gravity_hash_iterate_fn iterate, void *data);
void				gravity_hash_iterate2 (gravity_hash_t *hashtable, gravity_hash_iterate2_fn iterate, void *data1, void *data2);
void				gravity_hash_transform (gravity_hash_t *hashtable, gravity_hash_transform_fn iterate, void *data);
void				gravity_hash_dump (gravity_hash_t *hashtable);
void				gravity_hash_append (gravity_hash_t *hashtable1, gravity_hash_t *hashtable2);
void				gravity_hash_resetfree (gravity_hash_t *hashtable);

#endif
