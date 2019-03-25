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

const char *opcode_constname (int n);
const char *opcode_name (opcode_t op);
const char *gravity_disassemble (gravity_vm *vm, gravity_function_t *f, const char *bcode, uint32_t blen, bool deserialize);

#endif
