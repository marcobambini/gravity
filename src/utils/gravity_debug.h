//
//  gravity_debug.h
//  gravity
//
//  Created by Marco Bambini on 01/04/16.
//  Copyright © 2016 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_DEBUG__
#define __GRAVITY_DEBUG__

#include "gravity_opcodes.h"

GRAVITY_API const char *opcode_constname (int n);
GRAVITY_API const char *opcode_name (opcode_t op);
GRAVITY_API const char *gravity_disassemble (gravity_vm *vm, gravity_function_t *f, const char *bcode, uint32_t blen, bool deserialize);

#endif
