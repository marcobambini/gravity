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

typedef struct symboltable_t	symboltable_t;

symboltable_t	*symboltable_create (bool is_enum);
gnode_t			*symboltable_lookup (symboltable_t *table, const char *identifier);
gnode_t			*symboltable_global_lookup (symboltable_t *table, const char *identifier);
bool			symboltable_insert (symboltable_t *table, const char *identifier, gnode_t *node);
uint32_t		symboltable_count (symboltable_t *table, uint32_t index);

void			symboltable_enter_scope (symboltable_t *table);
uint32_t		symboltable_exit_scope (symboltable_t *table, uint32_t *nlevel);
uint32_t		symboltable_local_index (symboltable_t *table);
void			symboltable_free (symboltable_t *table);
void			symboltable_dump (symboltable_t *table);

#endif
