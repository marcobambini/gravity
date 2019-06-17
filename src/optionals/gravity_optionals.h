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


#ifndef GRAVITY_INCLUDE_HTTP
#define GRAVITY_INCLUDE_HTTP
#endif

#ifdef GRAVITY_INCLUDE_HTTP
#define GRAVITY_HTTP_REGISTER(_vm)          gravity_http_register(_vm)
#define GRAVITY_HTTP_FREE()                 gravity_http_free()
#define GRAVITY_HTTP_NAME()                 gravity_http_name()
#define GRAVITY_ISHTTP_CLASS(_c)            gravity_ishttp_class(_c)
#include "gravity_http.h"
#else
#define GRAVITY_HTTP_REGISTER(_vm)
#define GRAVITY_HTTP_FREE()
#define GRAVITY_HTTP_NAME()                 NULL
#define GRAVITY_ISHTTP_CLASS(_c)            false
#endif

#ifndef GRAVITY_INCLUDE_ENV
#define GRAVITY_INCLUDE_ENV
#endif

#ifdef GRAVITY_INCLUDE_ENV
#define GRAVITY_ENV_REGISTER(_vm)           gravity_env_register(_vm)
#define GRAVITY_ENV_FREE()                  gravity_env_free()
#define GRAVITY_ENV_NAME()                  gravity_env_name()
#define GRAVITY_ISENV_CLASS(_c)             gravity_isenv_class(_c)
#include "gravity_env.h"
#else
#define GRAVITY_ENV_REGISTER(_vm)
#define GRAVITY_ENV_FREE()
#define GRAVITY_ENV_NAME()                  NULL
#define GRAVITY_ISENV_CLASS(_c)             false
#endif

#endif
