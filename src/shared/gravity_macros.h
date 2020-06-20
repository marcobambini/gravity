//
//  macros.h
//  gravity
//
//  Created by Marco Bambini on 24/04/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_MACROS__
#define __GRAVITY_MACROS__

#include "gravity_config.h"

#define AUTOLENGTH                          UINT32_MAX

// MARK: -
// pragma unused is not recognized by VC
#define UNUSED_PARAM(_x)                    (void)(_x)
#define UNUSED_PARAM2(_x,_y)                UNUSED_PARAM(_x),UNUSED_PARAM(_y)
#define UNUSED_PARAM3(_x,_y,_z)             UNUSED_PARAM(_x),UNUSED_PARAM(_y),UNUSED_PARAM(_z)

// MARK: -
#define VALUE_AS_OBJECT(x)                  ((x).p)
#define VALUE_AS_STRING(x)                  ((gravity_string_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_FIBER(x)                   ((gravity_fiber_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_FUNCTION(x)                ((gravity_function_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_PROPERTY(x)                ((gravity_property_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_CLOSURE(x)                 ((gravity_closure_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_CLASS(x)                   ((gravity_class_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_INSTANCE(x)                ((gravity_instance_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_LIST(x)                    ((gravity_list_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_MAP(x)                     ((gravity_map_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_RANGE(x)                   ((gravity_range_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_CSTRING(x)                 (VALUE_AS_STRING(x)->s)
#define VALUE_AS_ERROR(x)                   ((const char *)(x).p)
#define VALUE_AS_FLOAT(x)                   ((x).f)
#define VALUE_AS_INT(x)                     ((x).n)
#define VALUE_AS_BOOL(x)                    ((x).n)

// MARK: -
#if GRAVITY_USE_HIDDEN_INITIALIZERS
#define VALUE_FROM_ERROR(msg)               (gravity_value_from_error(msg))
#define VALUE_NOT_VALID                     VALUE_FROM_ERROR(NULL)
#define VALUE_FROM_OBJECT(obj)              (gravity_value_from_object(obj))
#define VALUE_FROM_STRING(_vm,_s,_len)      (gravity_string_to_value(_vm, _s, _len))
#define VALUE_FROM_CSTRING(_vm,_s)          (gravity_string_to_value(_vm, _s, AUTOLENGTH))
#define VALUE_FROM_INT(x)                   (gravity_value_from_int(x))
#define VALUE_FROM_FLOAT(x)                 (gravity_value_from_float(x))
#define VALUE_FROM_NULL                     (gravity_value_from_null())
#define VALUE_FROM_UNDEFINED                (gravity_value_from_undefined())
#define VALUE_FROM_BOOL(x)                  (gravity_value_from_bool(x))
#define VALUE_FROM_FALSE                    VALUE_FROM_BOOL(0)
#define VALUE_FROM_TRUE                     VALUE_FROM_BOOL(1)
#else
#define VALUE_FROM_ERROR(msg)               ((gravity_value_t){.isa = NULL, .p = ((gravity_object_t *)msg)})
#define VALUE_NOT_VALID                     VALUE_FROM_ERROR(NULL)
#define VALUE_FROM_OBJECT(obj)              ((gravity_value_t){.isa = ((gravity_object_t *)(obj)->isa), .p = (gravity_object_t *)(obj)})
#define VALUE_FROM_STRING(_vm,_s,_len)      (gravity_string_to_value(_vm, _s, _len))
#define VALUE_FROM_CSTRING(_vm,_s)          (gravity_string_to_value(_vm, _s, AUTOLENGTH))
#define VALUE_FROM_INT(x)                   ((gravity_value_t){.isa = gravity_class_int, .n = (x)})
#define VALUE_FROM_FLOAT(x)                 ((gravity_value_t){.isa = gravity_class_float, .f = (x)})
#define VALUE_FROM_NULL                     ((gravity_value_t){.isa = gravity_class_null, .n = 0})
#define VALUE_FROM_UNDEFINED                ((gravity_value_t){.isa = gravity_class_null, .n = 1})
#define VALUE_FROM_BOOL(x)                  ((gravity_value_t){.isa = gravity_class_bool, .n = (x)})
#define VALUE_FROM_FALSE                    VALUE_FROM_BOOL(0)
#define VALUE_FROM_TRUE                     VALUE_FROM_BOOL(1)
#endif // GRAVITY_USE_HIDDEN_INITIALIZERS

#define STATICVALUE_FROM_STRING(_v,_s,_l)   gravity_string_t __temp = {.isa = gravity_class_string, .s = (char *)_s, .len = (uint32_t)_l, }; \
                                            __temp.hash = gravity_hash_compute_buffer(__temp.s, __temp.len); \
                                            gravity_value_t _v = {.isa = gravity_class_string, .p = (gravity_object_t *)&__temp };

// MARK: -
#define VALUE_ISA_FUNCTION(v)               (v.isa == gravity_class_function)
#define VALUE_ISA_INSTANCE(v)               (v.isa == gravity_class_instance)
#define VALUE_ISA_CLOSURE(v)                (v.isa == gravity_class_closure)
#define VALUE_ISA_FIBER(v)                  (v.isa == gravity_class_fiber)
#define VALUE_ISA_CLASS(v)                  (v.isa == gravity_class_class)
#define VALUE_ISA_STRING(v)                 (v.isa == gravity_class_string)
#define VALUE_ISA_INT(v)                    (v.isa == gravity_class_int)
#define VALUE_ISA_FLOAT(v)                  (v.isa == gravity_class_float)
#define VALUE_ISA_BOOL(v)                   (v.isa == gravity_class_bool)
#define VALUE_ISA_LIST(v)                   (v.isa == gravity_class_list)
#define VALUE_ISA_MAP(v)                    (v.isa == gravity_class_map)
#define VALUE_ISA_RANGE(v)                  (v.isa == gravity_class_range)
#define VALUE_ISA_BASIC_TYPE(v)             (VALUE_ISA_STRING(v) || VALUE_ISA_INT(v) || VALUE_ISA_FLOAT(v) || VALUE_ISA_BOOL(v))
#define VALUE_ISA_NULLCLASS(v)              (v.isa == gravity_class_null)
#define VALUE_ISA_NULL(v)                   ((v.isa == gravity_class_null) && (v.n == 0))
#define VALUE_ISA_UNDEFINED(v)              ((v.isa == gravity_class_null) && (v.n == 1))
#define VALUE_ISA_CLASS(v)                  (v.isa == gravity_class_class)
#define VALUE_ISA_CALLABLE(v)               (VALUE_ISA_FUNCTION(v) || VALUE_ISA_CLASS(v) || VALUE_ISA_FIBER(v))
#define VALUE_ISA_VALID(v)                  (v.isa != NULL)
#define VALUE_ISA_NOTVALID(v)               (v.isa == NULL)
#define VALUE_ISA_ERROR(v)                  VALUE_ISA_NOTVALID(v)

// MARK: -
#define OBJECT_ISA_INT(obj)                 (obj->isa == gravity_class_int)
#define OBJECT_ISA_FLOAT(obj)               (obj->isa == gravity_class_float)
#define OBJECT_ISA_BOOL(obj)                (obj->isa == gravity_class_bool)
#define OBJECT_ISA_NULL(obj)                (obj->isa == gravity_class_null)
#define OBJECT_ISA_CLASS(obj)               (obj->isa == gravity_class_class)
#define OBJECT_ISA_FUNCTION(obj)            (obj->isa == gravity_class_function)
#define OBJECT_ISA_CLOSURE(obj)             (obj->isa == gravity_class_closure)
#define OBJECT_ISA_INSTANCE(obj)            (obj->isa == gravity_class_instance)
#define OBJECT_ISA_LIST(obj)                (obj->isa == gravity_class_list)
#define OBJECT_ISA_MAP(obj)                 (obj->isa == gravity_class_map)
#define OBJECT_ISA_STRING(obj)              (obj->isa == gravity_class_string)
#define OBJECT_ISA_UPVALUE(obj)             (obj->isa == gravity_class_upvalue)
#define OBJECT_ISA_FIBER(obj)               (obj->isa == gravity_class_fiber)
#define OBJECT_ISA_RANGE(obj)               (obj->isa == gravity_class_range)
#define OBJECT_ISA_MODULE(obj)              (obj->isa == gravity_class_module)
#define OBJECT_IS_VALID(obj)                (obj->isa != NULL)

// MARK: -
#define LIST_COUNT(v)                       (marray_size(VALUE_AS_LIST(v)->array))
#define LIST_VALUE_AT_INDEX(v, idx)         (marray_get(VALUE_AS_LIST(v)->array, idx))

// MARK: -
#define GRAVITY_JSON_FUNCTION               "function"
#define GRAVITY_JSON_CLASS                  "class"
#define GRAVITY_JSON_RANGE                  "range"
#define GRAVITY_JSON_INSTANCE               "instance"
#define GRAVITY_JSON_ENUM                   "enum"
#define GRAVITY_JSON_MAP                    "map"
#define GRAVITY_JSON_VAR                    "var"
#define GRAVITY_JSON_GETTER                 "$get"
#define GRAVITY_JSON_SETTER                 "$set"

#define GRAVITY_JSON_LABELTAG               "tag"
#define GRAVITY_JSON_LABELNAME              "name"
#define GRAVITY_JSON_LABELTYPE              "type"
#define GRAVITY_JSON_LABELVALUE             "value"
#define GRAVITY_JSON_LABELIDENTIFIER        "identifier"
#define GRAVITY_JSON_LABELPOOL              "pool"
#define GRAVITY_JSON_LABELPVALUES           "pvalues"
#define GRAVITY_JSON_LABELPNAMES            "pnames"
#define GRAVITY_JSON_LABELMETA              "meta"
#define GRAVITY_JSON_LABELBYTECODE          "bytecode"
#define GRAVITY_JSON_LABELLINENO            "lineno"
#define GRAVITY_JSON_LABELNPARAM            "nparam"
#define GRAVITY_JSON_LABELNLOCAL            "nlocal"
#define GRAVITY_JSON_LABELNTEMP             "ntemp"
#define GRAVITY_JSON_LABELNUPV              "nup"
#define GRAVITY_JSON_LABELARGS              "args"
#define GRAVITY_JSON_LABELINDEX             "index"
#define GRAVITY_JSON_LABELSUPER             "super"
#define GRAVITY_JSON_LABELNIVAR             "nivar"
#define GRAVITY_JSON_LABELSIVAR             "sivar"
#define GRAVITY_JSON_LABELPURITY            "purity"
#define GRAVITY_JSON_LABELREADONLY          "readonly"
#define GRAVITY_JSON_LABELSTORE             "store"
#define GRAVITY_JSON_LABELINIT              "init"
#define GRAVITY_JSON_LABELSTATIC            "static"
#define GRAVITY_JSON_LABELPARAMS            "params"
#define GRAVITY_JSON_LABELSTRUCT            "struct"
#define GRAVITY_JSON_LABELFROM              "from"
#define GRAVITY_JSON_LABELTO                "to"
#define GRAVITY_JSON_LABELIVAR              "ivar"

#define GRAVITY_VM_ANONYMOUS_PREFIX         "$$"

// MARK: -

#if 1
#define DEBUG_ASSERT(condition, message)    do { \
                                                if (!(condition)) { \
                                                    fprintf(stderr, "[%s:%d] Assert failed in %s(): %s\n", \
                                                    __FILE__, __LINE__, __func__, message); \
                                                    abort(); \
                                                } \
                                            } \
                                            while(0)
#else
#define DEBUG_ASSERT(condition, message)
#endif

#endif
