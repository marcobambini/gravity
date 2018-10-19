//
//  gravity_codegen.h
//  gravity
//
//  Created by Marco Bambini on 09/10/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_CODEGEN__
#define __GRAVITY_CODEGEN__

#include "gravity_ast.h"
#include "gravity_value.h"
#include "gravity_delegate.h"

gravity_function_t *gravity_codegen(gnode_t *node, gravity_delegate_t *delegate, gravity_vm *vm, bool add_debug);

#endif
