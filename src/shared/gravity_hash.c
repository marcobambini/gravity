//
//  gravity_hash.c
//  gravity
//
//  Created by Marco Bambini on 23/04/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//

#include <inttypes.h>
#include "gravity_hash.h"
#include "gravity_macros.h"

#if GRAVITYHASH_ENABLE_STATS
#define INC_COLLISION(tbl)      ++tbl->ncollision
#define INC_RESIZE(tbl)         ++tbl->nresize
#else
#define INC_COLLISION(tbl)
#define INC_RESIZE(tbl)
#endif

typedef struct hash_node_s {
    uint32_t                hash;
    gravity_value_t         key;
    gravity_value_t         value;
    struct hash_node_s      *next;
} hash_node_t;

struct gravity_hash_t {
    // internals
    uint32_t                size;
    uint32_t                count;
    hash_node_t             **nodes;
    gravity_hash_compute_fn compute_fn;
    gravity_hash_isequal_fn isequal_fn;
    gravity_hash_iterate_fn free_fn;
    void                    *data;

    // stats
    #if GRAVITYHASH_ENABLE_STATS
    uint32_t                ncollision;
    uint32_t                nresize;
    #endif
};

// http://programmers.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed

/*
    Hash algorithm used in Gravity Hash Table is DJB2 which does a pretty good job with string keys and it is fast.
    Original algorithm is: http://www.cse.yorku.ca/~oz/hash.html

    DJBX33A (Daniel J. Bernstein, Times 33 with Addition)

    This is Daniel J. Bernstein's popular `times 33' hash function as
    posted by him years ago on comp.lang.c. It basically uses a function
    like ``hash(i) = hash(i-1)     33 + str[i]''. This is one of the best
    known hash functions for strings. Because it is both computed very
    fast and distributes very well.

    Why 33? (<< 5 in the code)
    The magic of number 33, i.e. why it works better than many other
    constants, prime or not, has never been adequately explained by
    anyone. So I try an explanation: if one experimentally tests all
    multipliers between 1 and 256 (as RSE did now) one detects that even
    numbers are not useable at all. The remaining 128 odd numbers
    (except for the number 1) work more or less all equally well. They
    all distribute in an acceptable way and this way fill a hash table
    with an average percent of approx. 86%.

    If one compares the Chi^2 values of the variants, the number 33 not
    even has the best value. But the number 33 and a few other equally
    good numbers like 17, 31, 63, 127 and 129 have nevertheless a great
    advantage to the remaining numbers in the large set of possible
    multipliers: their multiply operation can be replaced by a faster
    operation based on just one shift plus either a single addition
    or subtraction operation. And because a hash function has to both
    distribute good _and_ has to be very fast to compute, those few
    numbers should be preferred and seems to be the reason why Daniel J.
    Bernstein also preferred it.

    Why 5381?
    1. odd number
    2. prime number
    3. deficient number (https://en.wikipedia.org/wiki/Deficient_number)
    4. 001/010/100/000/101 b

    Practically any good multiplier works. I think you're worrying about
    the fact that 31c + d doesn't cover any reasonable range of hash values
    if c and d are between 0 and 255. That's why, when I discovered the 33 hash
    function and started using it in my compressors, I started with a hash value
    of 5381. I think you'll find that this does just as well as a 261 multiplier.

    Note that the starting value of the hash (5381) makes no difference for strings
    of equal lengths, but will play a role in generating different hash values for
    strings of different lengths.
 */

#define HASH_SEED_VALUE                     5381
#define ROT32(x, y)                         ((x << y) | (x >> (32 - y)))
#define COMPUTE_HASH(tbl,key,hash)          register uint32_t hash = murmur3_32(key, len, HASH_SEED_VALUE); hash = hash % tbl->size
#define COMPUTE_HASH_NOMODULO(key,hash)     register uint32_t hash = murmur3_32(key, len, HASH_SEED_VALUE)
#define RECOMPUTE_HASH(tbl,key,hash)        hash = murmur3_32(key, len, HASH_SEED_VALUE); hash = hash % tbl->size

static inline uint32_t murmur3_32 (const char *key, uint32_t len, uint32_t seed) {
    static const uint32_t c1 = 0xcc9e2d51;
    static const uint32_t c2 = 0x1b873593;
    static const uint32_t r1 = 15;
    static const uint32_t r2 = 13;
    static const uint32_t m = 5;
    static const uint32_t n = 0xe6546b64;

    uint32_t hash = seed;

    const int nblocks = len / 4;
    const uint32_t *blocks = (const uint32_t *) key;
    for (int i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = ROT32(k, r1);
        k *= c2;

        hash ^= k;
        hash = ROT32(hash, r2) * m + n;
    }

    const uint8_t *tail = (const uint8_t *) (key + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3:
            k1 ^= tail[2] << 16;
        case 2:
            k1 ^= tail[1] << 8;
        case 1:
            k1 ^= tail[0];

            k1 *= c1;
            k1 = ROT32(k1, r1);
            k1 *= c2;
            hash ^= k1;
    }

    hash ^= len;
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);

    return hash;
}

static void table_dump (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused (hashtable, data)
    const char *k = ((gravity_string_t *)key.p)->s;
    printf("%-20s=>\t",k);
    gravity_value_dump(NULL, value, NULL, 0);
}

// MARK: -

gravity_hash_t *gravity_hash_create (uint32_t size, gravity_hash_compute_fn compute, gravity_hash_isequal_fn isequal, gravity_hash_iterate_fn free_fn, void *data) {
    if ((!compute) || (!isequal)) return NULL;
    if (size < GRAVITYHASH_DEFAULT_SIZE) size = GRAVITYHASH_DEFAULT_SIZE;

    gravity_hash_t *hashtable = (gravity_hash_t *)mem_alloc(NULL, sizeof(gravity_hash_t));
    if (!hashtable) return NULL;
    if (!(hashtable->nodes = mem_calloc(NULL, size, sizeof(hash_node_t*)))) {mem_free(hashtable); return NULL;}

    hashtable->compute_fn = compute;
    hashtable->isequal_fn = isequal;
    hashtable->free_fn = free_fn;
    hashtable->data = data;
    hashtable->size = size;
    return hashtable;
}

void gravity_hash_free (gravity_hash_t *hashtable) {
    if (!hashtable) return;
    gravity_hash_iterate_fn free_fn = hashtable->free_fn;

    for (uint32_t n = 0; n < hashtable->size; ++n) {
        hash_node_t *node = hashtable->nodes[n];
        hashtable->nodes[n] = NULL;
        while (node) {
            if (free_fn) free_fn(hashtable, node->key, node->value, hashtable->data);
            hash_node_t *old_node = node;
            node = node->next;
            mem_free(old_node);
        }
    }
    mem_free(hashtable->nodes);
    mem_free(hashtable);
    hashtable = NULL;
}

uint32_t gravity_hash_memsize (gravity_hash_t *hashtable) {
    uint32_t size = sizeof(gravity_hash_t);
    size += hashtable->size * sizeof(hash_node_t);
    return size;
}

bool gravity_hash_isempty (gravity_hash_t *hashtable) {
    return (hashtable->count == 0);
}

static inline int gravity_hash_resize (gravity_hash_t *hashtable) {
    uint32_t size = (hashtable->size * 2);
    gravity_hash_t newtbl = {
        .size = size,
        .count = 0,
        .isequal_fn = hashtable->isequal_fn,
        .compute_fn = hashtable->compute_fn
    };
    if (!(newtbl.nodes = mem_calloc(NULL, size, sizeof(hash_node_t*)))) return -1;

    hash_node_t *node, *next;
    for (uint32_t n = 0; n < hashtable->size; ++n) {
        for (node = hashtable->nodes[n]; node; node = next) {
            next = node->next;
            gravity_hash_insert(&newtbl, node->key, node->value);
            // temporary disable free callback registered in hashtable
            // because both key and values are reused in the new table
            gravity_hash_iterate_fn free_fn = hashtable->free_fn;
            hashtable->free_fn = NULL;
            gravity_hash_remove(hashtable, node->key);
            hashtable->free_fn = free_fn;
        }
    }

    mem_free(hashtable->nodes);
    hashtable->size = newtbl.size;
    hashtable->count = newtbl.count;
    hashtable->nodes = newtbl.nodes;
    INC_RESIZE(hashtable);

    return 0;
}

bool gravity_hash_remove (gravity_hash_t *hashtable, gravity_value_t key) {
    register uint32_t hash = hashtable->compute_fn(key);
    register uint32_t position = hash % hashtable->size;

    gravity_hash_iterate_fn free_fn = hashtable->free_fn;
    hash_node_t *node = hashtable->nodes[position];
    hash_node_t *prevnode = NULL;
    while (node) {
        if ((node->hash == hash) && (hashtable->isequal_fn(key, node->key))) {
            if (free_fn) free_fn(hashtable, node->key, node->value, hashtable->data);
            if (prevnode) prevnode->next = node->next;
            else hashtable->nodes[position] = node->next;
            mem_free(node);
            hashtable->count--;
            return true;
        }

        prevnode = node;
        node = node->next;
    }

    return false;
}

bool gravity_hash_insert (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value) {
    if (hashtable->count >= GRAVITYHASH_MAXENTRIES) return false;
    
    register uint32_t hash = hashtable->compute_fn(key);
    register uint32_t position = hash % hashtable->size;

    hash_node_t *node = hashtable->nodes[position];
    if (node) INC_COLLISION(hashtable);

    // check if the key is already in the table
    while (node) {
        if ((node->hash == hash) && (hashtable->isequal_fn(key, node->key))) {
            node->value = value;
            return false;
        }
        node = node->next;
    }

    // resize table if the threshold is exceeded
    // default threshold is: <table size> * <load factor GRAVITYHASH_THRESHOLD>
    if (hashtable->count >= hashtable->size * GRAVITYHASH_THRESHOLD) {
        if (gravity_hash_resize(hashtable) == -1) return false;
        // recompute position here because hashtable->size has changed!
        position = hash % hashtable->size;
    }

    // allocate new entry and set new data
    if (!(node = mem_alloc(NULL, sizeof(hash_node_t)))) return false;
    node->key = key;
    node->hash = hash;
    node->value = value;
    node->next = hashtable->nodes[position];
    hashtable->nodes[position] = node;
    ++hashtable->count;

    return true;
}

gravity_value_t *gravity_hash_lookup (gravity_hash_t *hashtable, gravity_value_t key) {
    register uint32_t hash = hashtable->compute_fn(key);
    register uint32_t position = hash % hashtable->size;

    hash_node_t *node = hashtable->nodes[position];
    while (node) {
        if ((node->hash == hash) && (hashtable->isequal_fn(key, node->key))) return &node->value;
        node = node->next;
    }

    return NULL;
}

gravity_value_t *gravity_hash_lookup_cstring (gravity_hash_t *hashtable, const char *ckey) {
    STATICVALUE_FROM_STRING(key, ckey, strlen(ckey));
    return gravity_hash_lookup(hashtable, key);
}

uint32_t gravity_hash_count (gravity_hash_t *hashtable) {
    return hashtable->count;
}

uint32_t gravity_hash_compute_buffer (const char *key, uint32_t len) {
    return murmur3_32(key, len, HASH_SEED_VALUE);
}

uint32_t gravity_hash_compute_int (gravity_int_t n) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%" PRId64, n);
    return murmur3_32(buffer, (uint32_t)strlen(buffer), HASH_SEED_VALUE);
}

uint32_t gravity_hash_compute_float (gravity_float_t f) {
    char buffer[24];
    // was %g but we don't like scientific notation nor the missing .0 in case of float number with no decimals
    snprintf(buffer, sizeof(buffer), "%f", f);
    return murmur3_32(buffer, (uint32_t)strlen(buffer), HASH_SEED_VALUE);
}

void gravity_hash_stat (gravity_hash_t *hashtable) {
    #if GRAVITYHASH_ENABLE_STATS
    printf("==============\n");
    printf("Collision: %d\n", hashtable->ncollision);
    printf("Resize: %d\n", hashtable->nresize);
    printf("Size: %d\n", hashtable->size);
    printf("Count: %d\n", hashtable->count);
    printf("==============\n");
    #endif
}

void gravity_hash_transform (gravity_hash_t *hashtable, gravity_hash_transform_fn transform, void *data) {
    if ((!hashtable) || (!transform)) return;

    for (uint32_t i=0; i<hashtable->size; ++i) {
        hash_node_t *node = hashtable->nodes[i];
        if (!node) continue;

        while (node) {
            transform(hashtable, node->key, &node->value, data);
            node = node->next;
        }
    }
}

void gravity_hash_iterate (gravity_hash_t *hashtable, gravity_hash_iterate_fn iterate, void *data) {
    if ((!hashtable) || (!iterate)) return;

    for (uint32_t i=0; i<hashtable->size; ++i) {
        hash_node_t *node = hashtable->nodes[i];
        if (!node) continue;

        while (node) {
            iterate(hashtable, node->key, node->value, data);
            node = node->next;
        }
    }
}

void gravity_hash_iterate2 (gravity_hash_t *hashtable, gravity_hash_iterate2_fn iterate, void *data1, void *data2) {
    if ((!hashtable) || (!iterate)) return;

    for (uint32_t i=0; i<hashtable->size; ++i) {
        hash_node_t *node = hashtable->nodes[i];
        if (!node) continue;

        while (node) {
            iterate(hashtable, node->key, node->value, data1, data2);
            node = node->next;
        }
    }
}

void gravity_hash_iterate3 (gravity_hash_t *hashtable, gravity_hash_iterate3_fn iterate, void *data1, void *data2, void *data3) {
    if ((!hashtable) || (!iterate)) return;
    
    for (uint32_t i=0; i<hashtable->size; ++i) {
        hash_node_t *node = hashtable->nodes[i];
        if (!node) continue;
        
        while (node) {
            iterate(hashtable, node->key, node->value, data1, data2, data3);
            node = node->next;
        }
    }
}

void gravity_hash_dump (gravity_hash_t *hashtable) {
    gravity_hash_iterate(hashtable, table_dump, NULL);
}

void gravity_hash_append (gravity_hash_t *hashtable1, gravity_hash_t *hashtable2) {
    for (uint32_t i=0; i<hashtable2->size; ++i) {
        hash_node_t *node = hashtable2->nodes[i];
        if (!node) continue;

        while (node) {
            gravity_hash_insert(hashtable1, node->key, node->value);
            node = node->next;
        }
    }
}

void gravity_hash_resetfree (gravity_hash_t *hashtable) {
    hashtable->free_fn = NULL;
}

bool gravity_hash_compare (gravity_hash_t *hashtable1, gravity_hash_t *hashtable2, gravity_hash_compare_fn compare, void *data) {
    if (hashtable1->count != hashtable2->count) return false;
    if (!compare) return false;

    // 1. allocate arrays of keys and values
    gravity_value_r keys1; gravity_value_r values1;
    gravity_value_r keys2; gravity_value_r values2;
    marray_init(keys1); marray_init(values1);
    marray_init(keys2); marray_init(values2);
    marray_resize(gravity_value_t, keys1, hashtable1->count + MARRAY_DEFAULT_SIZE);
    marray_resize(gravity_value_t, keys2, hashtable1->count + MARRAY_DEFAULT_SIZE);
    marray_resize(gravity_value_t, values1, hashtable1->count + MARRAY_DEFAULT_SIZE);
    marray_resize(gravity_value_t, values2, hashtable1->count + MARRAY_DEFAULT_SIZE);

    // 2. build arrays of keys and values for hashtable1
    for (uint32_t i=0; i<hashtable1->size; ++i) {
        hash_node_t *node = hashtable1->nodes[i];
        if (!node) continue;
        while (node) {
            marray_push(gravity_value_t, keys1, node->key);
            marray_push(gravity_value_t, values1, node->value);
            node = node->next;
        }
    }

    // 3. build arrays of keys and values for hashtable2
    for (uint32_t i=0; i<hashtable2->size; ++i) {
        hash_node_t *node = hashtable2->nodes[i];
        if (!node) continue;
        while (node) {
            marray_push(gravity_value_t, keys2, node->key);
            marray_push(gravity_value_t, values2, node->value);
            node = node->next;
        }
    }

    // sanity check
    bool result = false;
    uint32_t count = (uint32_t)marray_size(keys1);
    if (count != (uint32_t)marray_size(keys2)) goto cleanup;

    // 4. compare keys and values
    for (uint32_t i=0; i<count; ++i) {
        if (!compare(marray_get(keys1, i), marray_get(keys2, i), data)) goto cleanup;
        if (!compare(marray_get(values1, i), marray_get(values2, i), data)) goto cleanup;
    }

    result = true;

cleanup:
    marray_destroy(keys1);
    marray_destroy(keys2);
    marray_destroy(values1);
    marray_destroy(values2);

    return result;
}
