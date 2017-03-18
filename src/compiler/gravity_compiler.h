//
//  gravity_compiler.h
//  gravity
//
//  Created by Marco Bambini on 29/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_COMPILER__
#define __GRAVITY_COMPILER__

#include "gravity_delegate.h"
#include "debug_macros.h"
#include "gravity_utils.h"
#include "gravity_value.h"
#include "gravity_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

// opaque compiler data type
typedef struct gravity_compiler_t	gravity_compiler_t;

GRAVITY_DLL gravity_compiler_t	*gravity_compiler_create (gravity_delegate_t *delegate);
GRAVITY_DLL gravity_closure_t	*gravity_compiler_run (gravity_compiler_t *compiler, const char *source, size_t len, uint32_t fileid, bool is_static);
GRAVITY_DLL json_t				*gravity_compiler_serialize (gravity_compiler_t *compiler, gravity_closure_t *closure);
GRAVITY_DLL bool				gravity_compiler_serialize_infile (gravity_compiler_t *compiler, gravity_closure_t *closure, const char *path);
GRAVITY_DLL void				gravity_compiler_transfer (gravity_compiler_t *compiler, gravity_vm *vm);
GRAVITY_DLL gnode_t				*gravity_compiler_ast (gravity_compiler_t *compiler);
GRAVITY_DLL void				gravity_compiler_free (gravity_compiler_t *compiler);

#ifdef __cplusplus
}
#endif

#endif
