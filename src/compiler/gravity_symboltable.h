//
//  gravity_symboltable.h
//  gravity
//
//  Created by Marco Bambini on 12/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_SYMTABLE__
#define __GRAVITY_SYMTABLE__

#include "debug_macros.h"
#include "gravity_ast.h"

#ifndef GRAVITY_SYMBOLTABLE_DEFINED
#define GRAVITY_SYMBOLTABLE_DEFINED
typedef struct symboltable_t    symboltable_t;
#endif

typedef enum {
    SYMTABLE_TAG_GLOBAL = 0,
    SYMTABLE_TAG_FUNC = 1,
    SYMTABLE_TAG_CLASS = 2,
    SYMTABLE_TAG_MODULE = 3,
    SYMTABLE_TAG_ENUM = 4
} symtable_tag;

symboltable_t   *symboltable_create (symtable_tag tag);
gnode_t         *symboltable_lookup (symboltable_t *table, const char *identifier);
gnode_t         *symboltable_global_lookup (symboltable_t *table, const char *identifier);
bool            symboltable_insert (symboltable_t *table, const char *identifier, gnode_t *node);
uint32_t        symboltable_count (symboltable_t *table, uint32_t index);
symtable_tag    symboltable_tag (symboltable_t *table);
uint16_t        symboltable_setivar (symboltable_t *table, bool is_static);

void            symboltable_enter_scope (symboltable_t *table);
uint32_t        symboltable_exit_scope (symboltable_t *table, uint32_t *nlevel);
uint32_t        symboltable_local_index (symboltable_t *table);
void            symboltable_free (symboltable_t *table);
void            symboltable_dump (symboltable_t *table);

void            *symboltable_hash_atindex (symboltable_t *table, size_t n);

#endif
