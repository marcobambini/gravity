//
//  GravitySwiftCBridging.h
//  
//
//  Created by v.prusakov on 6/2/22.
//

#ifndef Header_h
#define Header_h

#include "gravity_macros.h"
#include "../runtime/gravity_vmmacros.h"

// MARK: - Gravity Macros Bridging -

// Swift cant use macro function. We write bridging instead

static inline gravity_string_t* gravity_cast_value_as_string(gravity_value_t value) {
    return VALUE_AS_STRING(value);
}

static inline gravity_fiber_t* gravity_cast_value_as_fiber(gravity_value_t value) {
    return VALUE_AS_FIBER(value);
}

static inline gravity_function_t* gravity_cast_value_as_func(gravity_value_t value) {
    return VALUE_AS_FUNCTION(value);
}

static inline gravity_closure_t* gravity_cast_value_as_closure(gravity_value_t value) {
    return VALUE_AS_CLOSURE(value);
}

static inline gravity_class_t* gravity_cast_value_as_class(gravity_value_t value) {
    return VALUE_AS_CLASS(value);
}

static inline gravity_instance_t* gravity_cast_value_as_instance(gravity_value_t value) {
    return VALUE_AS_INSTANCE(value);
}

static inline gravity_list_t* gravity_cast_value_as_list(gravity_value_t value) {
    return VALUE_AS_LIST(value);
}

static inline gravity_map_t* gravity_cast_value_as_map(gravity_value_t value) {
    return VALUE_AS_MAP(value);
}

static inline gravity_range_t* gravity_cast_value_as_range(gravity_value_t value) {
    return VALUE_AS_RANGE(value);
}

static inline const char * gravity_cast_value_as_cString(gravity_value_t value) {
    return VALUE_AS_CSTRING(value);
}

static inline const char * gravity_cast_value_as_error(gravity_value_t value) {
    return VALUE_AS_ERROR(value);
}

static inline gravity_float_t gravity_cast_value_as_float(gravity_value_t value) {
    return VALUE_AS_FLOAT(value);
}

static inline gravity_int_t gravity_cast_value_as_int(gravity_value_t value) {
    return VALUE_AS_INT(value);
}

static inline bool gravity_cast_value_as_bool(gravity_value_t value) {
    return VALUE_AS_BOOL(value);
}

static inline gravity_object_t * gravity_cast_value_as_object(gravity_value_t value) {
    return VALUE_AS_OBJECT(value);
}

// MARK: - Value is a

static inline bool gravity_value_isa_func(gravity_value_t value) {
    return VALUE_ISA_FUNCTION(value);
}

static inline bool gravity_value_isa_instance(gravity_value_t value) {
    return VALUE_ISA_INSTANCE(value);
}

static inline bool gravity_value_isa_closure(gravity_value_t value) {
    return VALUE_ISA_CLOSURE(value);
}

static inline bool gravity_value_isa_fiber(gravity_value_t value) {
    return VALUE_ISA_FIBER(value);
}

static inline bool gravity_value_isa_class(gravity_value_t value) {
    return VALUE_ISA_CLASS(value);
}

static inline bool gravity_value_isa_string(gravity_value_t value) {
    return VALUE_ISA_STRING(value);
}

static inline bool gravity_value_isa_int(gravity_value_t value) {
    return VALUE_ISA_INT(value);
}

static inline bool gravity_value_isa_float(gravity_value_t value) {
    return VALUE_ISA_FLOAT(value);
}

static inline bool gravity_value_isa_bool(gravity_value_t value) {
    return VALUE_ISA_BOOL(value);
}

static inline bool gravity_value_isa_list(gravity_value_t value) {
    return VALUE_ISA_LIST(value);
}

static inline bool gravity_value_isa_map(gravity_value_t value) {
    return VALUE_ISA_MAP(value);
}

static inline bool gravity_value_isa_range(gravity_value_t value) {
    return VALUE_ISA_RANGE(value);
}

static inline bool gravity_value_isa_null(gravity_value_t value) {
    return VALUE_ISA_NULL(value);
}

static inline bool gravity_value_isa_nullclass(gravity_value_t value) {
    return VALUE_ISA_NULLCLASS(value);
}

static inline bool gravity_value_isa_callable(gravity_value_t value) {
    return VALUE_ISA_CALLABLE(value);
}

static inline bool gravity_value_isa_basic_type(gravity_value_t value) {
    return VALUE_ISA_BASIC_TYPE(value);
}

static inline bool gravity_value_isa_undefined(gravity_value_t value) {
    return VALUE_ISA_UNDEFINED(value);
}

static inline bool gravity_value_isa_valid(gravity_value_t value) {
    return VALUE_ISA_VALID(value);
}

static inline bool gravity_value_isa_notvalid(gravity_value_t value) {
    return VALUE_ISA_NOTVALID(value);
}

static inline bool gravity_value_isa_error(gravity_value_t value) {
    return VALUE_ISA_ERROR(value);
}

// MARK: Object is a

static inline bool gravity_object_isa_func(gravity_object_t * object) {
    return OBJECT_ISA_FUNCTION(object);
}

static inline bool gravity_object_isa_instance(gravity_object_t * object) {
    return OBJECT_ISA_INSTANCE(object);
}

static inline bool gravity_object_isa_closure(gravity_object_t * object) {
    return OBJECT_ISA_CLOSURE(object);
}

static inline bool gravity_object_isa_fiber(gravity_object_t * object) {
    return OBJECT_ISA_FIBER(object);
}

static inline bool gravity_object_isa_string(gravity_object_t * object) {
    return OBJECT_ISA_STRING(object);
}

static inline bool gravity_object_isa_int(gravity_object_t * object) {
    return OBJECT_ISA_INT(object);
}

static inline bool gravity_object_isa_float(gravity_object_t * object) {
    return OBJECT_ISA_FLOAT(object);
}

static inline bool gravity_object_isa_bool(gravity_object_t * object) {
    return OBJECT_ISA_BOOL(object);
}

static inline bool gravity_object_isa_list(gravity_object_t * object) {
    return OBJECT_ISA_LIST(object);
}

static inline bool gravity_object_isa_map(gravity_object_t * object) {
    return OBJECT_ISA_MAP(object);
}

static inline bool gravity_object_isa_range(gravity_object_t * object) {
    return OBJECT_ISA_RANGE(object);
}

static inline bool gravity_object_isa_class(gravity_object_t * object) {
    return OBJECT_ISA_CLASS(object);
}

static inline bool gravity_object_isa_upvalue(gravity_object_t * object) {
    return OBJECT_ISA_UPVALUE(object);
}

static inline bool gravity_object_is_valid(gravity_object_t * object) {
    return OBJECT_IS_VALID(object);
}

static inline bool gravity_object_isa_module(gravity_object_t * object) {
    return OBJECT_ISA_MODULE(object);
}

static inline gravity_int_t gravity_list_count(gravity_value_t value) {
    return LIST_COUNT(value);
}

static inline gravity_value_t gravity_list_get_value_at_index(gravity_value_t value, int index) {
    return LIST_VALUE_AT_INDEX(value, index);
}

static inline void gravity_list_push_value(gravity_list_t* list, gravity_value_t value) {
    marray_push(gravity_value_t, list->array, value);
}

// MARK: Internal

static inline gravity_function_t * new_function(void * fptr) {
    return NEW_FUNCTION(fptr);
}

static inline gravity_value_t new_closure_value(gravity_c_internal fptr) {
    return NEW_CLOSURE_VALUE(fptr);
}

static inline gravity_value_t gravity_get_args_value_at_index(gravity_value_t *args, int index) {
    return GET_VALUE(index);
}

// MARK: Function Return

static inline bool gravity_return_no_value() {
    RETURN_NOVALUE();
}

static inline bool gravity_return_value(gravity_vm *vm, gravity_value_t value, int rindex) {
    RETURN_VALUE(value, rindex);
}

static inline bool gravity_return_error_for_rindex(gravity_vm *vm, int rindex, const char *msg) {
    gravity_fiber_seterror(gravity_vm_fiber(vm), msg);
    gravity_vm_setslot(vm, VALUE_FROM_NULL, rindex);
    return false;
}

static inline bool gravity_return_error(gravity_vm *vm, const char *msg) {
    gravity_fiber_seterror(gravity_vm_fiber(vm), msg);
    return false;
}

static inline bool gravity_return_closure(gravity_vm *vm, gravity_value_t value, int index) {
    RETURN_CLOSURE(value, index);
}

#endif /* Header_h */
