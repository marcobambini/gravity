//
//  gravity_symboltable.c
//  gravity
//
//  Created by Marco Bambini on 12/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_symboltable.h"
#include "gravity_macros.h"
#include "gravity_array.h"
#include "gravity_hash.h"

// symbol table implementation using a stack of hash tables
typedef marray_t(gravity_hash_t*)       ghash_r;

#define scope_stack_init(r)             marray_init(*r)
#define scope_stack_size(r)             marray_size(*r)
#define scope_stack_get(r,n)            marray_get(*r,n)
#define scope_stack_free(r)             marray_destroy(*r)
#define scope_stack_push(r,hash)        marray_push(gravity_hash_t*,*r,hash)
#define scope_stack_pop(r)              (marray_size(*r) ? marray_pop(*r) : NULL)

#define VALUE_FROM_NODE(x)              ((gravity_value_t){.isa = NULL, .p = (gravity_object_t *)(x)})
#define VALUE_AS_NODE(x)                ((gnode_t *)VALUE_AS_OBJECT(x))

// MARK: -

struct symboltable_t {
    ghash_r         *stack;
    uint16_t        count1;     // used for local var
    uint16_t        count2;     // used for ivar
    uint16_t        count3;     // used for static ivar
    uint16_t        unused;
    symtable_tag    tag;
};

// MARK: -

static void check_upvalue_inscope (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(hashtable, key)

    uint32_t index = *(uint32_t *)data;
    gnode_t *node = VALUE_AS_NODE(value);
    if (NODE_ISA(node, NODE_VARIABLE)) {
        gnode_var_t *var = (gnode_var_t *)node;
        if (var->upvalue) {
            if ((index == UINT32_MAX) || (var->index < index)) *(uint32_t *)data = var->index;
        }
    }
}

// MARK: -

static void symboltable_hash_free (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(hashtable, value, data)
    // free key only because node is usually retained by other objects and freed in other points
    gravity_value_free(NULL, key);
}

static void symboltable_keyvalue_free (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(hashtable, data)
    // in enum nodes are retained by the symboltable
    gnode_t *node = VALUE_AS_NODE(value);
    gnode_free(node);
    gravity_value_free(NULL, key);
}

symboltable_t *symboltable_create (symtable_tag tag) {
    bool is_enum = (tag == SYMTABLE_TAG_ENUM);
    
    symboltable_t    *table = mem_alloc(NULL, sizeof(symboltable_t));
    gravity_hash_t    *hash = gravity_hash_create(0, gravity_value_hash, gravity_value_equals,
                                                (is_enum) ? symboltable_keyvalue_free : symboltable_hash_free, NULL);
    if (!table) return NULL;

    // init symbol table
    table->tag = tag;
    table->count1 = 0;
    table->count2 = 0;
    table->count3 = 0;
    table->stack = mem_alloc(NULL, sizeof(ghash_r));
    scope_stack_init(table->stack);
    scope_stack_push(table->stack, hash);

    return table;
}

void symboltable_free (symboltable_t *table) {
    size_t i, n = scope_stack_size(table->stack);

    for (i=0; i<n; ++i) {
        gravity_hash_t *h = scope_stack_get(table->stack, i);
        gravity_hash_free(h);
    }

    if (table->stack) {
        scope_stack_free(table->stack);
        mem_free(table->stack);
    }

    mem_free(table);
}

bool symboltable_insert (symboltable_t *table, const char *identifier, gnode_t *node) {
    if (!identifier) return false;

    size_t            n = scope_stack_size(table->stack);
    gravity_hash_t    *h = scope_stack_get(table->stack, n-1);

    // insert node with key identifier into hash table (and check if already exists in current scope)
    gravity_value_t key = VALUE_FROM_CSTRING(NULL, identifier);
    if (gravity_hash_lookup(h, key) != NULL) {
        gravity_value_free(NULL, key);
        return false;
    }
    gravity_hash_insert(h, key, VALUE_FROM_NODE(node));

    ++table->count1;
    return true;
}

gnode_t *symboltable_lookup (symboltable_t *table, const char *identifier) {
    STATICVALUE_FROM_STRING(key, identifier, strlen(identifier));

    size_t n = scope_stack_size(table->stack);
    for (int i=(int)n-1; i>=0; --i) {
        gravity_hash_t *h = scope_stack_get(table->stack, i);
        gravity_value_t *v = gravity_hash_lookup(h, key);
        if (v) return VALUE_AS_NODE(*v);
    }

    return NULL;
}

uint32_t symboltable_count (symboltable_t *table, uint32_t index) {
    gravity_hash_t *h = scope_stack_get(table->stack, index);
    return gravity_hash_count(h);
}

symtable_tag symboltable_tag (symboltable_t *table) {
    return table->tag;
}

uint16_t symboltable_setivar (symboltable_t *table, bool is_static) {
    return ((is_static) ? table->count3++ : table->count2++);
}

// MARK: -

gnode_t *symboltable_global_lookup (symboltable_t *table, const char *identifier) {
    gravity_hash_t *h = scope_stack_get(table->stack, 0);
    STATICVALUE_FROM_STRING(key, identifier, strlen(identifier));

    gravity_value_t *v = gravity_hash_lookup(h, key);
    return (v) ? VALUE_AS_NODE(*v) : NULL;
}

void symboltable_enter_scope (symboltable_t *table) {
    gravity_hash_t *h = gravity_hash_create(0, gravity_value_hash, gravity_value_equals, symboltable_hash_free, NULL);
    scope_stack_push(table->stack, h);
}

uint32_t symboltable_local_index (symboltable_t *table) {
    return table->count1 - 1;
}

uint32_t symboltable_exit_scope (symboltable_t *table, uint32_t *nlevel) {
    gravity_hash_t *h = (gravity_hash_t *)scope_stack_pop(table->stack);
    if (nlevel) {
        *nlevel = UINT32_MAX;
        gravity_hash_iterate(h, check_upvalue_inscope, (void *)nlevel);
    }
    gravity_hash_free(h);
    return table->count1;
}

void symboltable_dump (symboltable_t *table) {
    size_t n = scope_stack_size(table->stack);

    for (int i=(int)n-1; i>=0; --i) {
        gravity_hash_t *h = (gravity_hash_t *)scope_stack_get(table->stack, i);
        gravity_hash_dump(h);
    }
}

void *symboltable_hash_atindex (symboltable_t *table, size_t n) {
    size_t count = scope_stack_size(table->stack);
    if (count <= n) return NULL;
    return (void *) scope_stack_get(table->stack, n);
}
