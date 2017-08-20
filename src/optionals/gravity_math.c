//
//  gravity_math.c
//  gravity
//
//  Created by Marco Bambini on 14/08/2017.
//  Copyright © 2017 CreoLabs. All rights reserved.
//
//  Based on: https://www.w3schools.com/jsref/jsref_obj_math.asp

#include <math.h>
#include <time.h>
#include <inttypes.h>
#include "gravity_vm.h"
#include "gravity_math.h"
#include "gravity_core.h"
#include "gravity_hash.h"
#include "gravity_macros.h"
#include "gravity_vmmacros.h"

#define MATH_CLASS_NAME             "Math"

#ifdef GRAVITY_ENABLE_DOUBLE
#define SIN                         sin
#define COS                         cos
#define TAN                         tan
#define ASIN                        asin
#define ACOS                        acos
#define ATAN                        atan
#define ATAN2                       atan2
#define CEIL                        ceil
#define FLOOR                       floor
#define ROUND                       round
#define LOG                         log
#define POW                         pow
#define EXP                         exp
#define SQRT                        sqrt
#define CBRT                        cbrt
#else
#define SIN                         sinf
#define COS                         cosf
#define TAN                         tanf
#define ASIN                        asinf
#define ACOS                        acosf
#define ATAN                        atanf
#define ATAN2                       atan2
#define CEIL                        ceilf
#define FLOOR                       floorf
#define ROUND                       roundf
#define LOG                         logf
#define POW                         powf
#define EXP                         expf
#define SQRT                        sqrtf
#define CBRT                        cbrtf
#endif

static gravity_class_t              *gravity_class_math = NULL;
static uint32_t                     refcount = 0;

// MARK: - Implementation -

// returns the absolute value of x
static bool math_abs (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_int_t computed_value;
        #ifdef GRAVITY_ENABLE_INT64
        computed_value = (gravity_int_t)llabs((long long)value.n);
        #else
        computed_value = (gravity_int_t)labs((long)value.n);
        #endif
        RETURN_VALUE(VALUE_FROM_INT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value;
        #ifdef GRAVITY_ENABLE_DOUBLE
        computed_value = (gravity_float_t)fabs((double)value.f);
        #else
        computed_value = (gravity_float_t)fabsf((float)value.f);
        #endif
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the arccosine of x, in radians
static bool math_acos (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ACOS((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ACOS((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the arcsine of x, in radians
static bool math_asin (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ASIN((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ASIN((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the arctangent of x as a numeric value between -PI/2 and PI/2 radians
static bool math_atan (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ATAN((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ATAN((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the arctangent of the quotient of its arguments
static bool math_atan2 (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm)
    
    if (nargs != 3) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
    
    gravity_value_t value = GET_VALUE(1);
    gravity_value_t value2 = GET_VALUE(2);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    gravity_float_t n2;
    if (VALUE_ISA_INT(value2)) n2 = (gravity_float_t)value2.n;
    else if (VALUE_ISA_FLOAT(value2)) n2 = (gravity_float_t)value2.f;
    else RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ATAN2((gravity_float_t)value.n, n2);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ATAN2((gravity_float_t)value.f, n2);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_cbrt (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)CBRT((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)CBRT((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_xrt (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 3) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    gravity_value_t base = GET_VALUE(1);
    gravity_value_t value = GET_VALUE(2);

    if (VALUE_ISA_NULL(value) || VALUE_ISA_NULL(base)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value) && VALUE_ISA_INT(base)) {
        gravity_float_t computed_value = (gravity_float_t)pow((gravity_float_t)value.n, 1.0/base.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_INT(value) && VALUE_ISA_FLOAT(base)) {
        gravity_float_t computed_value = (gravity_float_t)pow((gravity_float_t)value.n, 1.0/base.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value) && VALUE_ISA_INT(base)) {
        gravity_float_t computed_value = (gravity_float_t)pow((gravity_float_t)value.f, 1.0/base.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value) && VALUE_ISA_INT(base)) {
        gravity_float_t computed_value = (gravity_float_t)pow((gravity_float_t)value.f, 1.0/base.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns x, rounded upwards to the nearest integer
static bool math_ceil (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)CEIL((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)CEIL((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the cosine of x (x is in radians)
static bool math_cos (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)COS((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)COS((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the value of Ex
static bool math_exp (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)EXP((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)EXP((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns x, rounded downwards to the nearest integer
static bool math_floor (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)FLOOR((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)FLOOR((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static int gcf(int x, int y) {
    if (x == 0) {
        return y;
    }
    while (y != 0) {
        if (x > y) {
            x = x - y;
        }
        else {
            y = y - x;
        }
    }
    return x;
}

static bool math_gcf (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (vm, rindex)
    if (nargs < 3) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    for (int i = 1; i < nargs; ++i) {
        if (!VALUE_ISA_INT(GET_VALUE(i))) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
    }

    int gcf_value = (int)GET_VALUE(1).n;

    for (int i = 1; i < nargs-1; ++i) {
        gcf_value = gcf(gcf_value, (int)GET_VALUE(i+1).n);
    }

    RETURN_VALUE(VALUE_FROM_INT(gcf_value), rindex);
}

static int lcm(int x, int y) {
    return x*y/gcf(x,y);
}

static bool math_lcm (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (vm, rindex)
    if (nargs < 3) RETURN_ERROR("2 or more arguments expected");

    for (int i = 1; i < nargs; ++i) {
        if (!VALUE_ISA_INT(GET_VALUE(i))) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
    }

    int lcm_value = (int)GET_VALUE(1).n;

    for (int i = 1; i < nargs-1; ++i) {
        lcm_value = lcm(lcm_value, (int)GET_VALUE(i+1).n);
    }

    RETURN_VALUE(VALUE_FROM_INT(lcm_value), rindex);
}

// returns the natural logarithm (base E) of x
static bool math_log (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)LOG((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)LOG((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the base 10 logarithm of x
static bool math_log10 (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)LOG((gravity_float_t)value.n)/(gravity_float_t)LOG((gravity_float_t)10);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)LOG((gravity_float_t)value.f)/(gravity_float_t)LOG((gravity_float_t)10);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the logarithm (base x) of y
static bool math_logx (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t base = GET_VALUE(1);
    gravity_value_t value = GET_VALUE(2);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value) && VALUE_ISA_INT(base)) {
        gravity_float_t computed_value = (gravity_float_t)LOG((gravity_float_t)value.n)/(gravity_float_t)LOG((gravity_float_t)base.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_INT(value) && VALUE_ISA_FLOAT(base)) {
        gravity_float_t computed_value = (gravity_float_t)LOG((gravity_float_t)value.n)/(gravity_float_t)LOG((gravity_float_t)base.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value) && VALUE_ISA_INT(base)) {
        gravity_float_t computed_value = (gravity_float_t)LOG((gravity_float_t)value.f)/(gravity_float_t)LOG((gravity_float_t)base.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value) && VALUE_ISA_FLOAT(base)) {
        gravity_float_t computed_value = (gravity_float_t)LOG((gravity_float_t)value.f)/(gravity_float_t)LOG((gravity_float_t)base.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the number with the highest value
static bool math_max (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    gravity_float_t n = FLOAT_MIN;
    gravity_value_t result = VALUE_FROM_UNDEFINED;
    
    for (uint16_t i = 1; i<nargs; ++i) {
        gravity_value_t value = GET_VALUE(i);
        if (VALUE_ISA_INT(value)) {
            if ((gravity_float_t)value.n > n) result = value;
        } else if (VALUE_ISA_FLOAT(value)) {
            if (value.f > n) result = value;
        } else continue;
    }
    
    RETURN_VALUE(result, rindex);
}

// returns the number with the lowest value
static bool math_min (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    gravity_float_t n = FLOAT_MAX;
    gravity_value_t result = VALUE_FROM_UNDEFINED;
    
    for (uint16_t i = 1; i<nargs; ++i) {
        gravity_value_t value = GET_VALUE(i);
        if (VALUE_ISA_INT(value)) {
            if ((gravity_float_t)value.n < n) result = value;
        } else if (VALUE_ISA_FLOAT(value)) {
            if (value.f < n) result = value;
        } else continue;
    }
    
    RETURN_VALUE(result, rindex);
}

// returns the value of x to the power of y
static bool math_pow (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm)
    
    if (nargs != 3) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
    
    gravity_value_t value = GET_VALUE(1);
    gravity_value_t value2 = GET_VALUE(2);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    gravity_float_t n2;
    if (VALUE_ISA_INT(value2)) n2 = (gravity_float_t)value2.n;
    else if (VALUE_ISA_FLOAT(value2)) n2 = (gravity_float_t)value2.f;
    else RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)POW((gravity_float_t)value.n, n2);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)POW((gravity_float_t)value.f, n2);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns a random number between 0 and 1
static bool math_random (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    
    // only seed once
    static bool already_seeded = false;
    if (!already_seeded) {
        srand((unsigned)time(NULL));
        already_seeded = true;
    }
    
    int r = rand();
    RETURN_VALUE(VALUE_FROM_FLOAT((float)r / RAND_MAX), rindex);
}

// rounds x to the nearest integer
static bool math_round (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ROUND((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)ROUND((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the sine of x (x is in radians)
static bool math_sin (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)SIN((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)SIN((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the square root of x
static bool math_sqrt (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)SQRT((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)SQRT((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// returns the tangent of an angle
static bool math_tan (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }
    
    if (VALUE_ISA_INT(value)) {
        gravity_float_t computed_value = (gravity_float_t)TAN((gravity_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    if (VALUE_ISA_FLOAT(value)) {
        gravity_float_t computed_value = (gravity_float_t)TAN((gravity_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }
    
    // should be NaN
    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

// CONSTANTS
static bool math_PI (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(3.141592653589793), rindex);
}

static bool math_E (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(2.718281828459045), rindex);
}

static bool math_LN2 (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(0.6931471805599453), rindex);
}

static bool math_LN10 (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(2.302585092994046), rindex);
}

static bool math_LOG2E (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(1.4426950408889634), rindex);
}

static bool math_LOG10E (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(0.4342944819032518), rindex);
}

static bool math_SQRT2 (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(1.4142135623730951), rindex);
}

static bool math_SQRT1_2 (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(0.7071067811865476), rindex);
}

// MARK: - Internals -

//static void free_computed_property (gravity_class_t *c, const char *name) {
//    // computed properties are not registered inside VM gc so they need to be manually freed here
//    STATICVALUE_FROM_STRING(key, name, strlen(name));
//    gravity_closure_t *closure = gravity_class_lookup_closure(c, key);
//    computed_property_free(closure);
//    gravity_hash_remove(c->htable, key);
//}

static void create_optional_class (void) {
    gravity_class_math = gravity_class_new_pair(NULL, MATH_CLASS_NAME, NULL, 0, 0);
    gravity_class_t *meta = gravity_class_get_meta(gravity_class_math);
    
    gravity_class_bind(meta, "abs", NEW_CLOSURE_VALUE(math_abs));
    gravity_class_bind(meta, "acos", NEW_CLOSURE_VALUE(math_acos));
    gravity_class_bind(meta, "asin", NEW_CLOSURE_VALUE(math_asin));
    gravity_class_bind(meta, "atan", NEW_CLOSURE_VALUE(math_atan));
    gravity_class_bind(meta, "atan2", NEW_CLOSURE_VALUE(math_atan2));
    gravity_class_bind(meta, "cbrt", NEW_CLOSURE_VALUE(math_cbrt));
    gravity_class_bind(meta, "xrt", NEW_CLOSURE_VALUE(math_xrt));
    gravity_class_bind(meta, "ceil", NEW_CLOSURE_VALUE(math_ceil));
    gravity_class_bind(meta, "cos", NEW_CLOSURE_VALUE(math_cos));
    gravity_class_bind(meta, "exp", NEW_CLOSURE_VALUE(math_exp));
    gravity_class_bind(meta, "floor", NEW_CLOSURE_VALUE(math_floor));
    gravity_class_bind(meta, "gcf", NEW_CLOSURE_VALUE(math_gcf));
    gravity_class_bind(meta, "lcm", NEW_CLOSURE_VALUE(math_lcm));
    gravity_class_bind(meta, "log", NEW_CLOSURE_VALUE(math_log));
    gravity_class_bind(meta, "log10", NEW_CLOSURE_VALUE(math_log10));
    gravity_class_bind(meta, "logx", NEW_CLOSURE_VALUE(math_logx));
    gravity_class_bind(meta, "max", NEW_CLOSURE_VALUE(math_max));
    gravity_class_bind(meta, "min", NEW_CLOSURE_VALUE(math_min));
    gravity_class_bind(meta, "pow", NEW_CLOSURE_VALUE(math_pow));
    gravity_class_bind(meta, "random", NEW_CLOSURE_VALUE(math_random));
    gravity_class_bind(meta, "round", NEW_CLOSURE_VALUE(math_round));
    gravity_class_bind(meta, "sin", NEW_CLOSURE_VALUE(math_sin));
    gravity_class_bind(meta, "sqrt", NEW_CLOSURE_VALUE(math_sqrt));
    gravity_class_bind(meta, "tan", NEW_CLOSURE_VALUE(math_tan));
    
    gravity_closure_t *closure = NULL;
    closure = computed_property_create(NULL, NEW_FUNCTION(math_PI), NULL);
    gravity_class_bind(meta, "PI", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_E), NULL);
    gravity_class_bind(meta, "E", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_LN2), NULL);
    gravity_class_bind(meta, "LN2", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_LN10), NULL);
    gravity_class_bind(meta, "LN10", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_LOG2E), NULL);
    gravity_class_bind(meta, "LOG2E", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_LOG10E), NULL);
    gravity_class_bind(meta, "LOG10E", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_SQRT2), NULL);
    gravity_class_bind(meta, "SQRT2", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_SQRT1_2), NULL);
    gravity_class_bind(meta, "SQRT1_2", VALUE_FROM_OBJECT(closure));
    
    SETMETA_INITED(gravity_class_math);
}

// MARK: - Commons -

bool gravity_ismath_class (gravity_class_t *c) {
    return (c == gravity_class_math);
}

const char *gravity_math_name (void) {
    return MATH_CLASS_NAME;
}

void gravity_math_register (gravity_vm *vm) {
    if (!gravity_class_math) create_optional_class();
    ++refcount;
    
    if (!vm || gravity_vm_ismini(vm)) return;
    gravity_vm_setvalue(vm, MATH_CLASS_NAME, VALUE_FROM_OBJECT(gravity_class_math));
}

void gravity_math_free (void) {
    if (!gravity_class_math) return;
    
    --refcount;
    if (refcount) return;
    
    mem_check(false);
    gravity_class_t *meta = gravity_class_get_meta(gravity_class_math);
    computed_property_free(meta, "PI", true);
    computed_property_free(meta, "E", true);
    computed_property_free(meta, "LN2", true);
    computed_property_free(meta, "LN10", true);
    computed_property_free(meta, "LOG2E", true);
    computed_property_free(meta, "LOG10E", true);
    computed_property_free(meta, "SQRT2", true);
    computed_property_free(meta, "SQRT1_2", true);
    gravity_class_free_core(NULL, meta);
    gravity_class_free_core(NULL, gravity_class_math);
    mem_check(true);
    
    gravity_class_math = NULL;
}

