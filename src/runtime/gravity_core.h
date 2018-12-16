//
//  gravity_core.h
//  gravity
//
//  Created by Marco Bambini on 10/01/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_CORE__
#define __GRAVITY_CORE__

#include "gravity_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

// core functions
GRAVITY_API void gravity_core_init (void);
GRAVITY_API void gravity_core_register (gravity_vm *vm);
GRAVITY_API bool gravity_iscore_class (gravity_class_t *c);
GRAVITY_API void gravity_core_free (void);
GRAVITY_API const char **gravity_core_identifiers (void);
GRAVITY_API gravity_class_t *gravity_core_class_from_name (const char *name);

// conversion functions
gravity_value_t convert_value2int (gravity_vm *vm, gravity_value_t v);
gravity_value_t convert_value2float (gravity_vm *vm, gravity_value_t v);
gravity_value_t convert_value2bool (gravity_vm *vm, gravity_value_t v);
gravity_value_t convert_value2string (gravity_vm *vm, gravity_value_t v);

// internal functions
gravity_closure_t *computed_property_create (gravity_vm *vm, gravity_function_t *getter_func, gravity_function_t *setter_func);
void computed_property_free (gravity_class_t *c, const char *name, bool remove_flag);

#ifdef __cplusplus
}
#endif

#endif
