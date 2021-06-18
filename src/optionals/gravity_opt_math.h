//
//  gravity_opt_math.h
//  gravity
//
//  Created by Marco Bambini on 14/08/2017.
//  Copyright © 2017 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_MATH__
#define __GRAVITY_MATH__

#define GRAVITY_CLASS_MATH_NAME             "Math"

#include "gravity_common.h"
#include "gravity_value.h"

GRAVITY_API void gravity_math_register (gravity_vm *vm);
GRAVITY_API void gravity_math_free (void);
GRAVITY_API bool gravity_ismath_class (gravity_class_t *c);
GRAVITY_API const char *gravity_math_name (void);

#endif
