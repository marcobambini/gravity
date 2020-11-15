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
#include "gravity_opt_math.h"
#else
#define GRAVITY_MATH_REGISTER(_vm)
#define GRAVITY_MATH_FREE()
#define GRAVITY_MATH_NAME()                 NULL
#define GRAVITY_ISMATH_CLASS(_c)            false
#endif

#ifndef GRAVITY_INCLUDE_JSON
#define GRAVITY_INCLUDE_JSON
#endif

#ifdef GRAVITY_INCLUDE_JSON
#define GRAVITY_JSON_REGISTER(_vm)          gravity_json_register(_vm)
#define GRAVITY_JSON_FREE()                 gravity_json_free()
#define GRAVITY_JSON_NAME()                 gravity_json_name()
#define GRAVITY_ISJSON_CLASS(_c)            gravity_isjson_class(_c)
#include "gravity_opt_json.h"
#else
#define GRAVITY_JSON_REGISTER(_vm)
#define GRAVITY_JSON_FREE()
#define GRAVITY_JSON_NAME()                 NULL
#define GRAVITY_ISJSON_CLASS(_c)            false
#endif

#ifndef GRAVITY_INCLUDE_ENV
#define GRAVITY_INCLUDE_ENV
#endif

#ifdef GRAVITY_INCLUDE_ENV
#define GRAVITY_ENV_REGISTER(_vm)           gravity_env_register(_vm)
#define GRAVITY_ENV_FREE()                  gravity_env_free()
#define GRAVITY_ENV_NAME()                  gravity_env_name()
#define GRAVITY_ISENV_CLASS(_c)             gravity_isenv_class(_c)
#include "gravity_opt_env.h"
#else
#define GRAVITY_ENV_REGISTER(_vm)
#define GRAVITY_ENV_FREE()
#define GRAVITY_ENV_NAME()                  NULL
#define GRAVITY_ISENV_CLASS(_c)             false
#endif

#ifndef GRAVITY_INCLUDE_FILE
#define GRAVITY_INCLUDE_FILE
#endif

#ifdef GRAVITY_INCLUDE_FILE
#define GRAVITY_FILE_REGISTER(_vm)           gravity_file_register(_vm)
#define GRAVITY_FILE_FREE()                  gravity_file_free()
#define GRAVITY_FILE_NAME()                  gravity_file_name()
#define GRAVITY_ISFILE_CLASS(_c)             gravity_isfile_class(_c)
#include "gravity_opt_file.h"
#else
#define GRAVITY_FILE_REGISTER(_vm)
#define GRAVITY_FILE_FREE()
#define GRAVITY_FILE_NAME()                  NULL
#define GRAVITY_ISFILE_CLASS(_c)             false
#endif

#ifdef _MSC_VER
#define INLINE								__inline
#else
#define INLINE								inline
#endif

INLINE static const char **gravity_optional_identifiers(void) {
    static const char *list[] = {
        #ifdef GRAVITY_INCLUDE_MATH
        GRAVITY_CLASS_MATH_NAME,
        #endif
        #ifdef GRAVITY_INCLUDE_ENV
        GRAVITY_CLASS_ENV_NAME,
        #endif
        #ifdef GRAVITY_INCLUDE_JSON
        GRAVITY_CLASS_JSON_NAME,
        #endif
        #ifdef GRAVITY_INCLUDE_FILE
        GRAVITY_CLASS_FILE_NAME,
        #endif
        NULL};
    return list;
}

#endif
