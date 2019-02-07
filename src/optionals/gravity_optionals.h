//
//  gravity_optionals.h
//  gravity
//
//  Created by Marco Bambini on 14/08/2017.
//  Copyright © 2017 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_OPTIONALS__
#define __GRAVITY_OPTIONALS__

#ifndef GRAVITY_INCLUDE_MATH
#define GRAVITY_INCLUDE_MATH
#endif

#ifdef GRAVITY_INCLUDE_MATH
#define GRAVITY_MATH_REGISTER(_vm)          gravity_math_register(_vm)
#define GRAVITY_MATH_FREE()                 gravity_math_free()
#define GRAVITY_MATH_NAME()                 gravity_math_name()
#define GRAVITY_ISMATH_CLASS(_c)            gravity_ismath_class(_c)
#include "gravity_math.h"
#else
#define GRAVITY_MATH_REGISTER(_vm)
#define GRAVITY_MATH_FREE()
#define GRAVITY_MATH_NAME()                 NULL
#define GRAVITY_ISMATH_CLASS(_c)            false
#endif

#ifndef GRAVITY_INCLUDE_ENV
#define GRAVITY_INCLUDE_ENV
#endif

#ifdef GRAVITY_INCLUDE_ENV
#include "gravity_env.h"
#define GRAVITY_ENV_REGISTER(_vm) gravity_env_register(_vm)
#define GRAVITY_ENV_FREE() gravity_math_free()
#define GRAVITY_ENV_NAME() ENV_CLASS_NAME
#else
#define GRAVITY_ENV_REGISTER(_vm)
#define GRAVITY_ENV_FREE()
#define GRAVITY_ENV_NAME() NULL
#endif

#endif
