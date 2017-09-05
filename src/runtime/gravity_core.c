//
//  gravity_core.c
//  gravity
//
//  Created by Marco Bambini on 10/01/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//

#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "gravity_core.h"
#include "gravity_hash.h"
#include "gravity_value.h"
#include "gravity_debug.h"
#include "gravity_opcodes.h"
#include "gravity_macros.h"
#include "gravity_memory.h"
#include "gravity_vmmacros.h"

// Gravity base classes (the isa pointer in each object).
// Null and Undefined points to same class (Null) and they
// differs from the n field inside gravity_value_t.
// n == 0 means NULL while n == 1 means UNDEFINED so I can
// reuse the same methods for both.
//	
// Intrinsic datatypes are:
// - Int
// - Float
// - Boolean
// - String
// For these classes 4 conveniente conversion methods are provided.

// How internal conversion works
//
// Conversion is driven by the v1 class, so v2 is usually converter to v1 class
// and if the result is not as expected (very likely in complex expression) then the user
// is invited to explicitly cast values to the desired types.
// If a propert conversion function is not found then a runtime error is raised.

// Special note about Integer class
//
// Integer not always drives conversion based on v1 class
// that's because we are trying to fix the common case where
// an integer is added to a float.
// Without the smart check an operation like:
// 1 + 2.3 will result in 3
// So the first check is about v2 class (v1 is known) and if v2 class is float
// then v1 is converted to float and the propert operator_float_* function is called.

// Special note about Integer class
//
// Bitshift Operators does not make any sense for floating point values
// as pointed out here: http://www.cs.umd.edu/class/sum2003/cmsc311/Notes/BitOp/bitshift.html
// a trick could be to use a pointer to an int to actually manipulate
// floating point value. Since this is more a trick then a real solution
// I decided to cast v2 to integer without any extra check.
// Only operator_float_bit* functions are affected by this trick.

// Special note about Null class
//
// Every value in gravity is initialized to Null
// and can partecipate in math operations.
// This class should be defined in a way to do be
// less dangerous as possibile and a Null value should
// be interpreted as a zero number (where possibile).

static bool core_inited = false;		// initialize global classes just once
static uint32_t refcount = 0;			// protect deallocation of global classes

// boxed
gravity_class_t *gravity_class_int;
gravity_class_t *gravity_class_float;
gravity_class_t *gravity_class_bool;
gravity_class_t	*gravity_class_null;
// objects
gravity_class_t *gravity_class_string;
gravity_class_t *gravity_class_object;
gravity_class_t *gravity_class_function;
gravity_class_t *gravity_class_closure;
gravity_class_t *gravity_class_fiber;
gravity_class_t *gravity_class_class;
gravity_class_t *gravity_class_instance;
gravity_class_t *gravity_class_module;
gravity_class_t *gravity_class_list;
gravity_class_t *gravity_class_map;
gravity_class_t *gravity_class_range;
gravity_class_t *gravity_class_upvalue;
gravity_class_t *gravity_class_system;

// MARK: - Utils -
static void map_keys_array (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused (hashtable, value)
    gravity_list_t *list = (gravity_list_t *)data;
    marray_push(gravity_value_t, list->array, key);
}

// MARK: - Conversions -

static gravity_value_t convert_string2number (gravity_string_t *string, bool float_preferred) {
	// empty string case
	if (string->len == 0) return (float_preferred) ? VALUE_FROM_FLOAT(0.0) : VALUE_FROM_INT(0);
	
	register const char *s = string->s;
	register uint32_t len = string->len;
	register int32_t sign = 1;
	
	// check sign first
	if ((s[0] == '-') || (s[0] == '+')) {
		if (s[0] == '-') sign = -1;
		++s; --len;
	}
	
	// check special HEX, OCT, BIN cases
	if ((s[0] == '0') && (len > 2)) {
		int64_t n = 0;
		
		int c = toupper(s[1]);
		if (c == 'B') n = number_from_bin(&s[2], len-2);
		else if (c == 'O') n = number_from_oct(&s[2], len-2);
		else if (c == 'X') n = number_from_hex(s, len);
		if (sign == -1) n = -n;
		return (float_preferred) ? VALUE_FROM_FLOAT((gravity_float_t)n) : VALUE_FROM_INT((gravity_int_t)n);
	}
	
	// default case
	return (float_preferred) ? VALUE_FROM_FLOAT(strtod(string->s, NULL)) : VALUE_FROM_INT(strtoll(string->s, NULL, 0));
}

static bool convert_object_int (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_value_t v = convert_value2int(vm, GET_VALUE(0));
	if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Int.");
	RETURN_VALUE(v, rindex);
}

static bool convert_object_float (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_value_t v = convert_value2float(vm, GET_VALUE(0));
	if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Float.");
	RETURN_VALUE(v, rindex);
}

static bool convert_object_bool (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_value_t v = convert_value2bool(vm, GET_VALUE(0));
	if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Bool.");
	RETURN_VALUE(v, rindex);
}

static bool convert_object_string (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_value_t v = convert_value2string(vm, GET_VALUE(0));
	if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to String.");
	RETURN_VALUE(v, rindex);
}

static inline gravity_value_t convert_map2string (gravity_vm *vm, gravity_map_t *map) {
    // allocate initial memory to a 512 buffer
    uint32_t len = 512;
    char *buffer = mem_alloc(len+1);
    buffer[0] = '[';
    uint32_t pos = 1;
    
    // get keys list
    uint32_t count = gravity_hash_count(map->hash);
    gravity_list_t *list = gravity_list_new(vm, count);
    gravity_hash_iterate(map->hash, map_keys_array, (void *)list);
    
    count = (uint32_t) marray_size(list->array);
    for (uint32_t i=0; i<count; ++i) {
        gravity_value_t key = marray_get(list->array, i);
        gravity_value_t *v = gravity_hash_lookup(map->hash, key);
        gravity_value_t value = (v) ? *v : VALUE_FROM_NULL;
        
        gravity_string_t *svalue;
        gravity_string_t *skey;
        
        if (!VALUE_ISA_STRING(key)) key = convert_value2string(vm, key);
        skey = (VALUE_ISA_STRING(key)) ? VALUE_AS_STRING(key) : NULL;
        
        if (VALUE_ISA_MAP(value) && (VALUE_AS_MAP(value) == map)) {
            svalue = NULL;
        } else {
            gravity_value_t value2 = convert_value2string(vm, value);
            svalue = VALUE_ISA_VALID(value2) ? VALUE_AS_STRING(value2) : NULL;
        }
        
        // KEY
        char *s1 = (skey) ? skey->s : "N/A";
        uint32_t len1 = (skey) ? skey->len : 3;
        
        // VALUE
        char *s2 = (svalue) ? svalue->s : "N/A";
        uint32_t len2 = (svalue) ? svalue->len : 3;
        
        // check if buffer needs to be reallocated
        if (len1 + len2 + pos + 4 > len) {
            len = (len1 + len2 + pos + 4) + len;
            buffer = mem_realloc(buffer, len);
        }
        
        // copy key string to new buffer
        memcpy(buffer+pos, s1, len1);
        pos += len1;
        
        // copy ':' key/value separator
        memcpy(buffer+pos, ":", 1);
        pos += 1;
        
        // copy value string to new buffer
        memcpy(buffer+pos, s2, len2);
        pos += len2;
        
        // add entries separator
        if (i+1 < count) {
            memcpy(buffer+pos, ",", 1);
            pos += 1;
        }
    }
    
    // Write latest ] character
    memcpy(buffer+pos, "]", 1);
    buffer[++pos] = 0;
    
    gravity_value_t result = VALUE_FROM_STRING(vm, buffer, pos);
    mem_free(buffer);
    return result;
}

static inline gravity_value_t convert_list2string (gravity_vm *vm, gravity_list_t *list) {
	// allocate initial memory to a 512 buffer
	uint32_t len = 512;
	char *buffer = mem_alloc(len+1);
	buffer[0] = '[';
	uint32_t pos = 1;
	
	// loop to perform string concat
	uint32_t count = (uint32_t) marray_size(list->array);
	for (uint32_t i=0; i<count; ++i) {
		gravity_value_t value = marray_get(list->array, i);
        gravity_string_t *string;
        if (VALUE_ISA_LIST(value) && (VALUE_AS_LIST(value) == list)) {
            string = NULL;
        } else {
			gravity_value_t value2 = convert_value2string(vm, value);
            string = VALUE_ISA_VALID(value2) ? VALUE_AS_STRING(value2) : NULL;
        }
		
		char *s1 = (string) ? string->s : "N/A";
		uint32_t len1 = (string) ? string->len : 3;
		
		// check if buffer needs to be reallocated
		if (len1+pos+2 > len) {
			len = (len1+pos+2) + len;
			buffer = mem_realloc(buffer, len);
		}
		
		// copy string to new buffer
		memcpy(buffer+pos, s1, len1);
		pos += len1;
		
		// add separator
		if (i+1 < count) {
			memcpy(buffer+pos, ",", 1);
			pos += 1;
		}
	}
	
	// Write latest ] character
	memcpy(buffer+pos, "]", 1);
	buffer[++pos] = 0;
	
	gravity_value_t result = VALUE_FROM_STRING(vm, buffer, pos);
	mem_free(buffer);
	return result;
}

inline gravity_value_t convert_value2int (gravity_vm *vm, gravity_value_t v) {
	if (VALUE_ISA_INT(v)) return v;
	
	// handle conversion for basic classes
	if (VALUE_ISA_FLOAT(v)) return VALUE_FROM_INT((gravity_int_t)v.f);
	if (VALUE_ISA_BOOL(v)) return VALUE_FROM_INT(v.n);
	if (VALUE_ISA_NULL(v)) return VALUE_FROM_INT(0);
	if (VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_INT(0);
	if (VALUE_ISA_STRING(v)) {return convert_string2number(VALUE_AS_STRING(v), false);}
	
	// check if class implements the Int method
	gravity_closure_t *closure = gravity_vm_fastlookup(vm, gravity_value_getclass(v), GRAVITY_INT_INDEX);
	
	// sanity check (and break recursion)
	if ((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_int)) ||
        gravity_vm_getclosure(vm) == closure) return VALUE_FROM_ERROR(NULL);
	
	// execute closure and return its value
	if (gravity_vm_runclosure(vm, closure, v, NULL, 0)) return gravity_vm_result(vm);
	
	return VALUE_FROM_ERROR(NULL);
}

inline gravity_value_t convert_value2float (gravity_vm *vm, gravity_value_t v) {
	if (VALUE_ISA_FLOAT(v)) return v;
	
	// handle conversion for basic classes
	if (VALUE_ISA_INT(v)) return VALUE_FROM_FLOAT((gravity_float_t)v.n);
	if (VALUE_ISA_BOOL(v)) return VALUE_FROM_FLOAT(v.n);
	if (VALUE_ISA_NULL(v)) return VALUE_FROM_FLOAT(0);
	if (VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_FLOAT(0);
	if (VALUE_ISA_STRING(v)) {return convert_string2number(VALUE_AS_STRING(v), true);}
	
	// check if class implements the Float method
	gravity_closure_t *closure = gravity_vm_fastlookup(vm, gravity_value_getclass(v), GRAVITY_FLOAT_INDEX);
	
	// sanity check (and break recursion)
    if ((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_float)) ||
        gravity_vm_getclosure(vm) == closure) return VALUE_FROM_ERROR(NULL);
	
	// execute closure and return its value
	if (gravity_vm_runclosure(vm, closure, v, NULL, 0)) return gravity_vm_result(vm);
	
	return VALUE_FROM_ERROR(NULL);
}

inline gravity_value_t convert_value2bool (gravity_vm *vm, gravity_value_t v) {
	if (VALUE_ISA_BOOL(v)) return v;
	
	// handle conversion for basic classes
	if (VALUE_ISA_INT(v)) return VALUE_FROM_BOOL(v.n != 0);
	if (VALUE_ISA_FLOAT(v)) return VALUE_FROM_BOOL(v.f != 0.0);
	if (VALUE_ISA_NULL(v)) return VALUE_FROM_FALSE;
	if (VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_FALSE;
	if (VALUE_ISA_STRING(v)) {
		gravity_string_t *string = VALUE_AS_STRING(v);
		if (string->len == 0) return VALUE_FROM_FALSE;
		return VALUE_FROM_BOOL((strcmp(string->s, "false") != 0));
	}
	
	// check if class implements the Bool method
	gravity_closure_t *closure = gravity_vm_fastlookup(vm, gravity_value_getclass(v), GRAVITY_BOOL_INDEX);
	
	// sanity check (and break recursion)
    if ((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_bool)) ||
        gravity_vm_getclosure(vm) == closure) return VALUE_FROM_BOOL(1);
	
	// execute closure and return its value
	if (gravity_vm_runclosure(vm, closure, v, NULL, 0)) return gravity_vm_result(vm);
	
	return VALUE_FROM_ERROR(NULL);
}

inline gravity_value_t convert_value2string (gravity_vm *vm, gravity_value_t v) {
	if (VALUE_ISA_STRING(v)) return v;
	
	// handle conversion for basic classes
	if (VALUE_ISA_INT(v)) {
		char buffer[512];
		#if GRAVITY_ENABLE_INT64
		snprintf(buffer, sizeof(buffer), "%" PRId64, v.n);
		#else
		snprintf(buffer, sizeof(buffer), "%d", v.n);
		#endif
		return VALUE_FROM_CSTRING(vm, buffer);
		
	}
	if (VALUE_ISA_BOOL(v)) return VALUE_FROM_CSTRING(vm, (v.n) ? "true" : "false");
	if (VALUE_ISA_NULL(v)) return VALUE_FROM_CSTRING(vm, "null");
	if (VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_CSTRING(vm, "undefined");
	if (VALUE_ISA_FLOAT(v)) {
		char buffer[512];
		snprintf(buffer, sizeof(buffer), "%f", v.f);
		return VALUE_FROM_CSTRING(vm, buffer);
	}
	
	if (VALUE_ISA_CLASS(v)) {
		const char *identifier = (VALUE_AS_CLASS(v)->identifier);
		if (!identifier) identifier = "anonymous class";
		return VALUE_FROM_CSTRING(vm, identifier);
	}
	
	if (VALUE_ISA_FUNCTION(v)) {
		const char *identifier = (VALUE_AS_FUNCTION(v)->identifier);
		if (!identifier) identifier = "anonymous func";
		return VALUE_FROM_CSTRING(vm, identifier);
	}
	
	if (VALUE_ISA_CLOSURE(v)) {
		const char *identifier = (VALUE_AS_CLOSURE(v)->f->identifier);
		if (!identifier) identifier = "anonymous func";
		return VALUE_FROM_CSTRING(vm, identifier);
	}
	
	if (VALUE_ISA_LIST(v)) {
		gravity_list_t *list = VALUE_AS_LIST(v);
		return convert_list2string(vm, list);
	}
	
	if (VALUE_ISA_MAP(v)) {
		gravity_map_t *map = VALUE_AS_MAP(v);
		return convert_map2string(vm, map);
	}
	
	// check if class implements the String method (avoiding infinte loop by checking for convert_object_string)
	gravity_closure_t *closure = gravity_vm_fastlookup(vm, gravity_value_getclass(v), GRAVITY_STRING_INDEX);
	
	// sanity check (and break recursion)
    if ((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_string)) || gravity_vm_getclosure(vm) == closure) {
        if (VALUE_ISA_INSTANCE(v)) {
            gravity_instance_t *instance = VALUE_AS_INSTANCE(v);
            if (instance->xdata) {
                gravity_delegate_t *delegate = gravity_vm_delegate(vm);
                if (delegate->bridge_string) {
                    uint32_t len = 0;
                    const char *s = delegate->bridge_string(vm, instance->xdata, &len);
                    if (s) return VALUE_FROM_STRING(vm, s, len);
                }
            }
        }
        return VALUE_FROM_ERROR(NULL);
    }
	
	// execute closure and return its value
	if (gravity_vm_runclosure(vm, closure, v, NULL, 0)) return gravity_vm_result(vm);
	
	return VALUE_FROM_ERROR(NULL);
}

// MARK: - Object Class -

static bool object_class (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_class_t *c = gravity_value_getclass(GET_VALUE(0));
	RETURN_VALUE(VALUE_FROM_OBJECT(c), rindex);
}

static bool object_internal_size (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_int_t size = gravity_value_size(vm, GET_VALUE(0));
	if (size == 0) size = sizeof(gravity_value_t);
	RETURN_VALUE(VALUE_FROM_INT(size), rindex);
}

static bool object_isa (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	gravity_class_t	*c1 = gravity_value_getclass(GET_VALUE(0));
	gravity_class_t *c2 = VALUE_AS_CLASS(GET_VALUE(1));
	
	while (c1 != c2 && c1->superclass != NULL) {
		c1 = c1->superclass;
	}
	
	RETURN_VALUE(VALUE_FROM_BOOL(c1 == c2), rindex);
}

static bool object_eqq (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    
    gravity_value_t v1 = GET_VALUE(0);
    gravity_value_t v2 = GET_VALUE(1);
    
    // compare class first
    if (gravity_value_getclass(v1) != gravity_value_getclass(v2))
        RETURN_VALUE(VALUE_FROM_FALSE, rindex);
    
    // then compare value
    RETURN_VALUE(VALUE_FROM_BOOL(gravity_value_vm_equals(vm, v1, v2)), rindex);
}

static bool object_neqq (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    object_eqq(vm, args, nargs, rindex);
    gravity_value_t value = GET_VALUE(rindex);
    if (VALUE_ISA_BOOL(value)) {
         RETURN_VALUE(VALUE_FROM_BOOL(!VALUE_AS_BOOL(value)), rindex);
    }
    return true;
}

static bool object_cmp (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	if (gravity_value_vm_equals(vm, GET_VALUE(0), GET_VALUE(1))) RETURN_VALUE(VALUE_FROM_INT(0), rindex);
	RETURN_VALUE(VALUE_FROM_INT(1), rindex);
}

static bool object_not (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	// !obj
	// if obj is NULL then result is true
	// everything else must be false
	RETURN_VALUE(VALUE_FROM_BOOL(VALUE_ISA_NULLCLASS(GET_VALUE(0))), rindex);
}

static bool object_real_load (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex, bool is_super) {
	#pragma unused(vm, nargs)
	
	// if there is a possibility that gravity_vm_runclosure is called then it is MANDATORY to save arguments before the call
	gravity_value_t target = GET_VALUE(0);
	gravity_value_t key = GET_VALUE(1);
	
	// check if meta class needs to be initialized (it means if it contains valued static ivars)
	// meta classes must be inited somewhere, this problem does not exist with instances since object creation itself trigger a class init
	if (VALUE_ISA_CLASS(target)) {
		gravity_class_t *c = VALUE_AS_CLASS(target);
		gravity_class_t *meta = gravity_class_get_meta(c);
		if (!meta->is_inited) {
			meta->is_inited = true;
			gravity_closure_t *closure = gravity_class_lookup_constructor(meta, 0);
            if (closure) {
                if (!gravity_vm_runclosure(vm, closure, VALUE_FROM_OBJECT(meta), NULL, 0)) return false;
            }
		}
	}
    
    // sanity check for super operator
    if (is_super && !VALUE_ISA_CLASS(target)) {
        RETURN_ERROR("Unable to lookup super for non class object");
    }
	
	// retrieve class and process key
	gravity_class_t *c = (is_super) ? VALUE_AS_CLASS(target) : gravity_value_getclass(target);
	gravity_instance_t *instance = VALUE_ISA_INSTANCE(target) ? VALUE_AS_INSTANCE(target) : NULL;
	
	// key is an int its an optimization for faster loading of ivar
	if (VALUE_ISA_INT(key)) {
        // sanity check
        uint32_t nivar = c->nivars;
        uint32_t nindex = (uint32_t)key.n;
        if (nindex >= nivar) RETURN_ERROR("Out of bounds ivar index.");
        
		if (instance) RETURN_VALUE(instance->ivars[nindex], rindex);	// instance case
		RETURN_VALUE(c->ivars[nindex], rindex);                         // class case
	}
	
	// key must be a string in this version
	if (!VALUE_ISA_STRING(key)) {
		RETURN_ERROR("Unable to lookup non string value into class %s", c->identifier);
	}
	
	// lookup key in class c
	gravity_object_t *obj = (gravity_object_t *)gravity_class_lookup(c, key);
    if (!obj) {
			// not explicitly declared so check for dynamic property in bridge case
			gravity_delegate_t *delegate = gravity_vm_delegate(vm);
			if ((instance) && (instance->xdata) && (delegate) && (delegate->bridge_getundef)) {
				if (delegate->bridge_getundef(vm, instance->xdata, target, VALUE_AS_CSTRING(key), rindex)) return true;
			}
		}
	if (!obj) goto execute_notfound;
	
	gravity_closure_t *closure;
	if (OBJECT_ISA_CLOSURE(obj)) {
		closure = (gravity_closure_t *)obj;
		if (!closure || !closure->f) goto execute_notfound;
		
		// execute optimized default getter
		if (FUNCTION_ISA_SPECIAL(closure->f)) {
			if (FUNCTION_ISA_DEFAULT_GETTER(closure->f)) {
                // sanity check
                uint32_t nivar = c->nivars;
                uint32_t nindex = closure->f->index;
                if (nindex >= nivar) RETURN_ERROR("Out of bounds ivar index.");
                
				if (instance) RETURN_VALUE(instance->ivars[closure->f->index], rindex);
				RETURN_VALUE(c->ivars[closure->f->index], rindex);
			}
			
			if (FUNCTION_ISA_GETTER(closure->f)) {
				// returns a function to be executed using the return false trick
				RETURN_CLOSURE(VALUE_FROM_OBJECT((gravity_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_GETTER]), rindex);
			}
			goto execute_notfound;
		}
	}
	
	RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);
	
execute_notfound:
	// in case of not found error return the notfound function to be executed (MANDATORY)
	closure = (gravity_closure_t *)gravity_class_lookup(c, gravity_vm_keyindex(vm, GRAVITY_NOTFOUND_INDEX));
	RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
}

static bool object_loads (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return object_real_load(vm, args, nargs, rindex, true);
}

static bool object_load (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return object_real_load(vm, args, nargs, rindex, false);
}

static bool object_store (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs, rindex)
	
	// if there is a possibility that gravity_vm_runfunc is called then it is MANDATORY to save arguments before the call
	gravity_value_t target = GET_VALUE(0);
	gravity_value_t key = GET_VALUE(1);
	gravity_value_t value = GET_VALUE(2);
	
	// check if meta class needs to be initialized (it means if it contains valued static ivars)
	// meta classes must be inited somewhere, this problem does not exist with classes since object creation itself trigger a class init
	if (VALUE_ISA_CLASS(target)) {
		gravity_class_t *c = VALUE_AS_CLASS(target);
		gravity_class_t *meta = gravity_class_get_meta(c);
		if (!meta->is_inited) {
			meta->is_inited = true;
			gravity_closure_t *closure = gravity_class_lookup_constructor(meta, 0);
            if (closure) {
                if (!gravity_vm_runclosure(vm, closure, VALUE_FROM_OBJECT(meta), NULL, 0)) return false;
            }
		}
	}
	
	// retrieve class and process key
	gravity_class_t *c = gravity_value_getclass(target);
	gravity_instance_t *instance = VALUE_ISA_INSTANCE(target) ? VALUE_AS_INSTANCE(target) : NULL;
	
	// key is an int its an optimization for faster loading of ivar
	if (VALUE_ISA_INT(key)) {
        // sanity check
        uint32_t nivar = c->nivars;
        uint32_t nindex = (uint32_t)key.n;
        if (nindex >= nivar) RETURN_ERROR("Out of bounds ivar index.");
        
		if (instance) instance->ivars[nindex] = value;
		else c->ivars[nindex] = value;
		RETURN_NOVALUE();
	}
	
	// key must be a string in this version
	if (!VALUE_ISA_STRING(key)) {
		RETURN_ERROR("Unable to lookup non string value into class %s", c->identifier);
	}
	
	// lookup key in class c
	gravity_object_t *obj = gravity_class_lookup(c, key);
    if (!obj) {
			// not explicitly declared so check for dynamic property in bridge case
			gravity_delegate_t *delegate = gravity_vm_delegate(vm);
			if ((instance) && (instance->xdata) && (delegate) && (delegate->bridge_setundef)) {
				if (delegate->bridge_setundef(vm, instance->xdata, target, VALUE_AS_CSTRING(key), value)) RETURN_NOVALUE();
			}
		}
	if (!obj) goto execute_notfound;
	
	gravity_closure_t *closure;
	if (OBJECT_ISA_CLOSURE(obj)) {
		closure = (gravity_closure_t *)obj;
		if (!closure || !closure->f) goto execute_notfound;
		
		// check for special functions case
		if (FUNCTION_ISA_SPECIAL(closure->f)) {
			// execute optimized default setter
			if (FUNCTION_ISA_DEFAULT_SETTER(closure->f)) {
                // sanity check
                uint32_t nivar = c->nivars;
                uint32_t nindex = closure->f->index;
                if (nindex >= nivar) RETURN_ERROR("Out of bounds ivar index.");
                
                if (instance) instance->ivars[nindex] = value;
                else c->ivars[nindex] = value;
                
				RETURN_NOVALUE();
			}
			
			if (FUNCTION_ISA_SETTER(closure->f)) {
				// returns a function to be executed using the return false trick
				RETURN_CLOSURE(VALUE_FROM_OBJECT((gravity_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_SETTER]), rindex);
			}
			goto execute_notfound;
		}
	}
	
	RETURN_NOVALUE();
	
execute_notfound:
	// in case of not found error return the notfound function to be executed (MANDATORY)
	closure = (gravity_closure_t *)gravity_class_lookup(c, gravity_vm_keyindex(vm, GRAVITY_NOTFOUND_INDEX));
	RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
}

static bool object_notfound (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(nargs,rindex)
	gravity_class_t *c = gravity_value_getclass(GET_VALUE(0));
	gravity_value_t key = GET_VALUE(1); // vm_getslot(vm, rindex);
    RETURN_ERROR("Unable to find %s into class %s", VALUE_ISA_STRING(key) ? VALUE_AS_CSTRING(key) : "N/A", c->identifier);
}

static bool object_bind (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(rindex)
	
	// sanity check first
	if (nargs < 3) RETURN_ERROR("Incorrect number of arguments.");
	if (!VALUE_ISA_STRING(GET_VALUE(1))) RETURN_ERROR("First argument must be a String.");
	if (!VALUE_ISA_CLOSURE(GET_VALUE(2))) RETURN_ERROR("Second argument must be a Closure.");
	
	gravity_object_t *object = NULL;
	if (VALUE_ISA_INSTANCE(GET_VALUE(0)) || VALUE_ISA_CLASS(GET_VALUE(0))) {
		object = VALUE_AS_OBJECT(GET_VALUE(0));
	} else {
		RETURN_ERROR("bind method can be applied only to instances or classes.");
	}
	
	gravity_string_t *key = VALUE_AS_STRING(GET_VALUE(1));
	gravity_class_t *c = gravity_value_getclass(GET_VALUE(0));
	
	// in this version core classes are shared among all VM instances and
	// this could be an issue in case of bound methods so it would be probably
	// a good idea to play safe and forbid bind on core classes
	if (gravity_iscore_class(c)) {
		RETURN_ERROR("Unable to bind method to a Gravity core class.");
	}
	
	// check if instance has already an anonymous class added to its hierarchy
	if (string_casencmp(c->identifier, GRAVITY_VM_ANONYMOUS_PREFIX, strlen(GRAVITY_VM_ANONYMOUS_PREFIX) != 0)) {
		// no super anonymous class found so create a new one, set its super as c, and add it to the hierarchy
		char *name = gravity_vm_anonymous(vm);
		gravity_class_t *anon = gravity_class_new_pair(NULL, name, c, 0, 0);
		gravity_class_t *anon_meta = gravity_class_get_meta(anon);
		object->objclass = anon;
		c = anon;
		
		// store anonymous class (and its meta) into VM special GC stack
		// manually retains anonymous class that will retain its bound functions
		gravity_gc_push(vm, (gravity_object_t *)anon);
		gravity_gc_push(vm, (gravity_object_t *)anon_meta);
	}
	
	// add closure to anonymous class
	gravity_class_bind(c, key->s, GET_VALUE(2));
	RETURN_NOVALUE();
}

static bool object_unbind (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(rindex)
	
	// sanity check first
	if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
	if (!VALUE_ISA_STRING(GET_VALUE(1))) RETURN_ERROR("Argument must be a String.");
	
	// remove key/value from hash table
	gravity_class_t *c = gravity_value_getclass(GET_VALUE(0));
	gravity_hash_remove(c->htable, GET_VALUE(1));
	
	RETURN_NOVALUE();
}

static bool object_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_ERROR("Forbidden Object execution.");
}

// MARK: - List Class -

static bool list_count (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
	RETURN_VALUE(VALUE_FROM_INT(marray_size(list->array)), rindex);
}

static bool list_contains (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
	gravity_value_t element = GET_VALUE(1);
	gravity_value_t result = VALUE_FROM_FALSE;
 
	register uint32_t count = (uint32_t)marray_size(list->array);
	register gravity_int_t i = 0;
 
	while (i < count) {
		if (gravity_value_vm_equals(vm, marray_get(list->array, i), element)) {
			result = VALUE_FROM_TRUE;
			break;
		 }
		++i;
	 }
 
	RETURN_VALUE(result, rindex);
}

static bool list_indexOf (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    gravity_value_t element = GET_VALUE(1);
    gravity_value_t result = VALUE_FROM_INT(-1);
    
    register uint32_t count = (uint32_t)marray_size(list->array);
    register gravity_int_t i = 0;
    
    while (i < count) {
        if (gravity_value_vm_equals(vm, marray_get(list->array, i), element)) {
            result = VALUE_FROM_INT(i);
            break;
        }
        ++i;
	 }
 
	RETURN_VALUE(result, rindex);
 }

static bool list_loadat (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_list_t	*list = VALUE_AS_LIST(GET_VALUE(0));
	gravity_value_t value = GET_VALUE(1);
	if (!VALUE_ISA_INT(value)) RETURN_ERROR("An integer index is required to access a list item.");
	
	register int32_t index = (int32_t)VALUE_AS_INT(value);
	register uint32_t count = (uint32_t)marray_size(list->array);
	
	if (index < 0) index = count + index;
	if ((index < 0) || ((uint32_t)index >= count)) RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, count-1);
	
	RETURN_VALUE(marray_get(list->array, index), rindex);
}

static bool list_storeat (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs, rindex)
	gravity_list_t	*list = VALUE_AS_LIST(GET_VALUE(0));
	gravity_value_t idxvalue = GET_VALUE(1);
	gravity_value_t value = GET_VALUE(2);
	if (!VALUE_ISA_INT(idxvalue)) RETURN_ERROR("An integer index is required to access a list item.");
	
	register int32_t index = (int32_t)VALUE_AS_INT(idxvalue);
	register uint32_t count = (uint32_t)marray_size(list->array);
	
	if (index < 0) index = count + index;
	if (index < 0) RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, count-1);
	if ((uint32_t)index >= count) {
		// handle list resizing here
		marray_resize(gravity_value_t, list->array, index-count);
        if (!list->array.p) RETURN_ERROR("Not enough memory to resize List.");
		marray_nset(list->array, index+1);
		for (int32_t i=count; i<index; ++i) {
			marray_set(list->array, i, VALUE_FROM_NULL);
		}
		marray_set(list->array, index, value);
	}
		
	marray_set(list->array, index, value);
	RETURN_NOVALUE();
}

static bool list_push (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(nargs)
	gravity_list_t	*list = VALUE_AS_LIST(GET_VALUE(0));
	gravity_value_t value = GET_VALUE(1);
	marray_push(gravity_value_t, list->array, value);
	RETURN_VALUE(VALUE_FROM_INT(marray_size(list->array)), rindex);
}

static bool list_pop (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(nargs)
	gravity_list_t	*list = VALUE_AS_LIST(GET_VALUE(0));
	size_t count = marray_size(list->array);
	if (count < 1) RETURN_ERROR("Unable to pop a value from an empty list.");
	gravity_value_t value = marray_pop(list->array);
	RETURN_VALUE(value, rindex);
}
	
static bool list_remove (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    gravity_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    if (!VALUE_ISA_INT(GET_VALUE(1))) RETURN_ERROR("Parameter must be of type Int.");
    
    gravity_int_t index = VALUE_AS_INT(GET_VALUE(1));
    size_t count = marray_size(list->array);
    if ((index < 0) || (index >= count)) RETURN_ERROR("Out of bounds index.");
    
    // remove an item means move others down
    memmove(&list->array.p[index], &list->array.p[index+1], ((count-1)-index) * sizeof(gravity_value_t));
    list->array.n -= 1;
    
    RETURN_VALUE(GET_VALUE(0), rindex);
}

static bool list_iterator (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_list_t	*list = VALUE_AS_LIST(GET_VALUE(0));
	
	// check for empty list first
	register uint32_t count = (uint32_t)marray_size(list->array);
	if (count == 0) RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	
	// check for start of iteration
	if (VALUE_ISA_NULL(GET_VALUE(1))) RETURN_VALUE(VALUE_FROM_INT(0), rindex);
	
	// extract value
	gravity_value_t value = GET_VALUE(1);
	
	// check error condition
	if (!VALUE_ISA_INT(value)) RETURN_ERROR("Iterator expects a numeric value here.");
	
	// compute new value
	gravity_int_t n = value.n;
	if (n+1 < count) {
		++n;
	} else {
		RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	}
	
	// return new iterator
	RETURN_VALUE(VALUE_FROM_INT(n), rindex);
}

static bool list_iterator_next (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_list_t	*list = VALUE_AS_LIST(GET_VALUE(0));
	register int32_t index = (int32_t)VALUE_AS_INT(GET_VALUE(1));
	RETURN_VALUE(marray_get(list->array, index), rindex);
}

static bool list_loop (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
	if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");
	
	gravity_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));	// closure to execute
	gravity_value_t value = GET_VALUE(0);							// self parameter
	gravity_list_t *list = VALUE_AS_LIST(value);
	register gravity_int_t n = marray_size(list->array);			// times to execute the loop
	register gravity_int_t i = 0;
	
	nanotime_t t1 = nanotime();
	while (i < n) {
        if (!gravity_vm_runclosure(vm, closure, value, &marray_get(list->array, i), 1)) return false;
		++i;
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool list_join (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	gravity_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
	const char *sep = NULL;
	if ((nargs > 1) && VALUE_ISA_STRING(GET_VALUE(1))) sep = VALUE_AS_CSTRING(GET_VALUE(1));
	
	// create a new empty buffer
	uint32_t alloc = (uint32_t) (marray_size(list->array) * 64);
    if (alloc > MAX_MEMORY_BLOCK) RETURN_ERROR("Maximum memory block size reached (max %d, requested %d).", MAX_MEMORY_BLOCK, alloc);
    
	uint32_t len = 0;
	uint32_t seplen = (sep) ? VALUE_AS_STRING(GET_VALUE(1))->len : 0;
	char *_buffer = mem_alloc(alloc);
	if (!_buffer) RETURN_ERROR("Not enought memory to allocate a buffer for the join operation.");
	
	register gravity_int_t n = marray_size(list->array);
	register gravity_int_t i = 0;
	
	// traverse list and append each item
	while (i < n) {
		gravity_value_t value = convert_value2string(vm, marray_get(list->array, i));
        if (VALUE_ISA_ERROR(value)) {
            mem_free(_buffer);
            RETURN_VALUE(value, rindex);
        }
		
        // compute string to appen
		const char *s2 = VALUE_AS_STRING(value)->s;
		uint32_t req = VALUE_AS_STRING(value)->len;
		uint32_t free_mem = alloc - len;
		
		// check if buffer needs to be reallocated
		if (free_mem < req + seplen) {
            uint64_t to_alloc = alloc + (req + seplen) * 2 + 4096;
            
            // sanity check
            if (to_alloc > MAX_MEMORY_BLOCK) {
                mem_free(_buffer);
                RETURN_ERROR("Maximum memory block size reached (max %d, requested %llu).", MAX_MEMORY_BLOCK, to_alloc);
            }
            
			_buffer = mem_realloc(_buffer, (uint32_t)to_alloc);
            if (!_buffer) {
                RETURN_ERROR("Not enought memory to re-allocate a buffer for the join operation.");
            }
			alloc = (uint32_t)to_alloc;
		}
		
		// copy s2 to into buffer
		memcpy(_buffer+len, s2, req);
		len += req;
		
		// check for separator string
		if (i+1 < n && seplen) {
			memcpy(_buffer+len, sep, seplen);
			len += seplen;
		}
		
		++i;
	}
	
	gravity_string_t *result = gravity_string_new(vm, _buffer, len, alloc);
	RETURN_VALUE(VALUE_FROM_OBJECT(result), rindex);
}

static bool list_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if ((nargs != 2) || (!VALUE_ISA_INT(GET_VALUE(1)))) RETURN_ERROR("An Int value is expected as argument of List allocate.");
    
    uint32_t n = (uint32_t)VALUE_AS_INT(GET_VALUE(1));
    gravity_list_t *list = gravity_list_new(vm, n);
    if (!list) RETURN_ERROR("Maximum List allocation size reached (%d).", MAX_ALLOCATION);
    
    for (uint32_t i=0; i<n; ++i) marray_push(gravity_value_t, list->array, VALUE_FROM_NULL);
    
    RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
}

// MARK: - Map Class -

static bool map_count (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
	RETURN_VALUE(VALUE_FROM_INT(gravity_hash_count(map->hash)), rindex);
}

static bool map_keys (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
	uint32_t count = gravity_hash_count(map->hash);
	
	gravity_list_t *list = gravity_list_new(vm, count);
	gravity_hash_iterate(map->hash, map_keys_array, (void *)list);
	RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
}

static bool map_loadat (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
	gravity_value_t key = GET_VALUE(1);
	if (VALUE_ISA_NOTVALID(key)) RETURN_ERROR("Invalid map key.");
	
    #ifdef GRAVITY_MAP_DOTSUGAR
    gravity_object_t *obj = (gravity_object_t *)gravity_class_lookup(gravity_class_map, key);
    if (obj) {
        if (OBJECT_ISA_CLOSURE(obj)) {
            gravity_closure_t *closure = (gravity_closure_t *)obj;
            if (closure && closure->f) {
                // execute optimized default getter
                if (FUNCTION_ISA_SPECIAL(closure->f)) {
                    if (FUNCTION_ISA_GETTER(closure->f)) {
                        // returns a function to be executed using the return false trick
                        RETURN_CLOSURE(VALUE_FROM_OBJECT((gravity_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_GETTER]), rindex);
                    }
                }
            }
        }
        RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);
    }
    #endif
    
	gravity_value_t *value = gravity_hash_lookup(map->hash, key);
	RETURN_VALUE((value) ? *value : VALUE_FROM_NULL, rindex);
}

static bool map_haskey (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    gravity_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    gravity_value_t key = GET_VALUE(1);
    
    gravity_value_t *value = gravity_hash_lookup(map->hash, key);
    RETURN_VALUE((value) ? VALUE_FROM_TRUE : VALUE_FROM_FALSE, rindex);
}

static bool map_storeat (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs, rindex)
	gravity_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
	gravity_value_t key = GET_VALUE(1);
	gravity_value_t value = GET_VALUE(2);
	
//    #ifdef GRAVITY_MAP_DOTSUGAR
//    gravity_object_t *obj = (gravity_object_t *)gravity_class_lookup(gravity_class_map, key);
//    if (obj) RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);
//    #endif
	
	gravity_hash_insert(map->hash, key, value);
	RETURN_NOVALUE();
}

static bool map_remove (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
	gravity_value_t key = GET_VALUE(1);
	
	bool existed = gravity_hash_remove(map->hash, key);
	RETURN_VALUE(VALUE_FROM_BOOL(existed), rindex);
}

static bool map_loop (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
	if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");
	
	gravity_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));	// closure to execute
	gravity_value_t value = GET_VALUE(0);							// self parameter
	gravity_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
	register gravity_int_t n = gravity_hash_count(map->hash);		// times to execute the loop
	register gravity_int_t i = 0;
	
	// build keys array
	gravity_list_t *list = gravity_list_new(vm, (uint32_t)n);
	gravity_hash_iterate(map->hash, map_keys_array, (void *)list);
	
	nanotime_t t1 = nanotime();
	while (i < n) {
        if (!gravity_vm_runclosure(vm, closure, value, &marray_get(list->array, i), 1)) return false;
		++i;
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

// MARK: - Range Class -

static bool range_count (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_range_t *range = VALUE_AS_RANGE(GET_VALUE(0));
	gravity_int_t count = (range->to > range->from) ? (range->to - range->from) : (range->from - range->to);
	RETURN_VALUE(VALUE_FROM_INT(count+1), rindex);
}

static bool range_loop (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
	if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");
	
	gravity_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));	// closure to execute
	gravity_value_t value = GET_VALUE(0);							// self parameter
	gravity_range_t *range = VALUE_AS_RANGE(value);
	bool is_forward = range->from < range->to;
	
	nanotime_t t1 = nanotime();
	if (is_forward) {
		register gravity_int_t n = range->to;
		register gravity_int_t i = range->from;
		while (i <= n) {
            if (!gravity_vm_runclosure(vm, closure, value, &VALUE_FROM_INT(i), 1)) return false;
			++i;
		}
	} else {
		register gravity_int_t n = range->from;			// 5...1
		register gravity_int_t i = range->to;
		while (n >= i) {
            if (!gravity_vm_runclosure(vm, closure, value, &VALUE_FROM_INT(n), 1)) return false;
			--n;
		}
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool range_iterator (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_range_t *range = VALUE_AS_RANGE(GET_VALUE(0));
	
	// check for empty range first
	if (range->from == range->to) RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	
	// check for start of iteration
	if (VALUE_ISA_NULL(GET_VALUE(1))) RETURN_VALUE(VALUE_FROM_INT(range->from), rindex);
	
	// extract value
	gravity_value_t value = GET_VALUE(1);
	
	// check error condition
	if (!VALUE_ISA_INT(value)) RETURN_ERROR("Iterator expects a numeric value here.");
	
	// compute new value
	gravity_int_t n = value.n;
	if (range->from < range->to) {
		++n;
		if (n > range->to) RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	} else {
		--n;
		if (n < range->to) RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	}
	
	// return new iterator
	RETURN_VALUE(VALUE_FROM_INT(n), rindex);
}

static bool range_iterator_next (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	RETURN_VALUE(GET_VALUE(1), rindex);
}

static bool range_contains (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_range_t *range = VALUE_AS_RANGE(GET_VALUE(0));
	gravity_value_t value = GET_VALUE(1);
	
	// check error condition
	if (!VALUE_ISA_INT(value)) RETURN_ERROR("A numeric value is expected.");
	
	RETURN_VALUE(VALUE_FROM_BOOL((value.n >= range->from) && (value.n <= range->to)), rindex);
}

static bool range_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if ((nargs != 3) || !VALUE_ISA_INT(GET_VALUE(1)) || !VALUE_ISA_INT(GET_VALUE(2))) RETURN_ERROR("Two Int values are expected as argument of Range creation.");
    
    uint32_t n1 = (uint32_t)VALUE_AS_INT(GET_VALUE(1));
    uint32_t n2 = (uint32_t)VALUE_AS_INT(GET_VALUE(2));
    
    gravity_range_t *range = gravity_range_new(vm, n1, n2, true);
    RETURN_VALUE(VALUE_FROM_OBJECT(range), rindex);
}

// MARK: - Class Class -

static bool class_name (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_class_t *c = (GET_VALUE(0).p);
	RETURN_VALUE(VALUE_FROM_CSTRING(vm, c->identifier), rindex);
}

static bool class_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (VALUE_ISA_CLASS(GET_VALUE(0))) {
        gravity_class_t *c = (gravity_class_t *)GET_VALUE(0).p;
        if (gravity_iscore_class(c)) {
            STATICVALUE_FROM_STRING(exec_key, GRAVITY_INTERNAL_EXEC_NAME, strlen(GRAVITY_INTERNAL_EXEC_NAME));
            gravity_closure_t *closure = (gravity_closure_t *)gravity_class_lookup_closure(c, exec_key);
            if (closure) RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
        }
    }
    
	// if 1st argument is not a class that means that this execution is part of a inner classes chained init
	// so retrieve class from callable object (that I am sure it is the right class)
	// this is more an hack than an elegation solution, I really hope to find out a better way
	if (!VALUE_ISA_CLASS(GET_VALUE(0))) args[0] = *(args-1);
	
	// retrieve class (with sanity check)
	if (!VALUE_ISA_CLASS(GET_VALUE(0))) RETURN_ERROR("Unable to execute non class object.");
	gravity_class_t *c = (gravity_class_t *)GET_VALUE(0).p;
	
	// perform alloc (then check for init)
	gravity_instance_t *instance = gravity_instance_new(vm, c);
	
	// if is inner class then ivar 0 is reserved for a reference to its outer class
	if (c->has_outer) gravity_instance_setivar(instance, 0, gravity_vm_getslot(vm, 0));
	
	// check for constructor function (-1 because self implicit parameter does not count)
	gravity_closure_t *closure = (gravity_closure_t *)gravity_class_lookup_constructor(c, nargs-1);
	
	// replace first parameter (self) to newly allocated instance
	args[0] = VALUE_FROM_OBJECT(instance);
	
	// if constructor found in this class then executes it
	if (closure) RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
	
	// no closure found (means no constructor found in this class)
	gravity_delegate_t *delegate = gravity_vm_delegate(vm);
	if (c->xdata && delegate && delegate->bridge_initinstance) {
		// even if no closure is found try to execute the default bridge init instance (if class is bridged)
		if (nargs != 1) RETURN_ERROR("No init with %d parameters found in class %s", nargs-1, c->identifier);
		delegate->bridge_initinstance(vm, c->xdata, instance, args, nargs);
	}
	
	// in any case set destination register to newly allocated instance
	RETURN_VALUE(VALUE_FROM_OBJECT(instance), rindex);
}

// MARK: - Closure Class -

static bool closure_disassemble (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(nargs)
	
	gravity_closure_t *closure = (gravity_closure_t *)(GET_VALUE(0).p);
	if (closure->f->tag != EXEC_TYPE_NATIVE) RETURN_VALUE(VALUE_FROM_NULL, rindex);
	
	const char *buffer = gravity_disassemble((const char *)closure->f->bytecode, closure->f->ninsts, false);
	if (!buffer) RETURN_VALUE(VALUE_FROM_NULL, rindex);
	
	RETURN_VALUE(gravity_string_to_value(vm, buffer, AUTOLENGTH), rindex);
}

static bool closure_apply (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	if (nargs != 3) RETURN_ERROR("Two arguments are needed by the apply function.");
	if (!VALUE_ISA_LIST(GET_VALUE(2))) RETURN_ERROR("A list of arguments is required in the apply function.");
	
	gravity_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(0));
	gravity_value_t self_value = GET_VALUE(1);
	gravity_list_t *list = VALUE_AS_LIST(GET_VALUE(2));
	
    if (!gravity_vm_runclosure(vm, closure, self_value, list->array.p, (uint16_t)marray_size(list->array))) return false;
	gravity_value_t result = gravity_vm_result(vm);
	
	RETURN_VALUE(result, rindex);
}

// MARK: - Float Class -

static bool operator_float_add (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_FLOAT(v1);
	INTERNAL_CONVERT_FLOAT(v2);
	RETURN_VALUE(VALUE_FROM_FLOAT(v1.f + v2.f), rindex);
}

static bool operator_float_sub (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_FLOAT(v1);
	INTERNAL_CONVERT_FLOAT(v2);
	RETURN_VALUE(VALUE_FROM_FLOAT(v1.f - v2.f), rindex);
}

static bool operator_float_div (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_FLOAT(v1);
	INTERNAL_CONVERT_FLOAT(v2);
	
	if (v2.f == 0.0) RETURN_ERROR("Division by 0 error.");
	RETURN_VALUE(VALUE_FROM_FLOAT(v1.f / v2.f), rindex);
}

static bool operator_float_mul (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_FLOAT(v1);
	INTERNAL_CONVERT_FLOAT(v2);
	RETURN_VALUE(VALUE_FROM_FLOAT(v1.f * v2.f), rindex);
}

static bool operator_float_rem (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_FLOAT(v1);
	INTERNAL_CONVERT_FLOAT(v2);
	
	// compute floating point modulus
	#if GRAVITY_ENABLE_DOUBLE
	RETURN_VALUE(VALUE_FROM_FLOAT(remainder(v1.f, v2.f)), rindex);
	#else
	RETURN_VALUE(VALUE_FROM_FLOAT(remainderf(v1.f, v2.f)), rindex);
	#endif
}

static bool operator_float_and (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	INTERNAL_CONVERT_BOOL(v2);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_float_or (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	INTERNAL_CONVERT_BOOL(v2);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_float_neg (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	RETURN_VALUE(VALUE_FROM_FLOAT(-GET_VALUE(0).f), rindex);
}

static bool operator_float_not (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	RETURN_VALUE(VALUE_FROM_BOOL(!GET_VALUE(0).f), rindex);
}

static bool operator_float_cmp (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_FLOAT(v2);
	if (v1.f == v2.f) RETURN_VALUE(VALUE_FROM_INT(0), rindex);
	if (v1.f > v2.f) RETURN_VALUE(VALUE_FROM_INT(1), rindex);
	RETURN_VALUE(VALUE_FROM_INT(-1), rindex);
}

static bool function_float_round (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	#if GRAVITY_ENABLE_DOUBLE
	RETURN_VALUE(VALUE_FROM_FLOAT(round(GET_VALUE(0).f)), rindex);
	#else
	RETURN_VALUE(VALUE_FROM_FLOAT(roundf(GET_VALUE(0).f)), rindex);
	#endif
}

static bool function_float_floor (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	#if GRAVITY_ENABLE_DOUBLE
	RETURN_VALUE(VALUE_FROM_FLOAT(floor(GET_VALUE(0).f)), rindex);
	#else
	RETURN_VALUE(VALUE_FROM_FLOAT(floorf(GET_VALUE(0).f)), rindex);
	#endif
}

static bool function_float_ceil (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	#if GRAVITY_ENABLE_DOUBLE
	RETURN_VALUE(VALUE_FROM_FLOAT(ceil(GET_VALUE(0).f)), rindex);
	#else
	RETURN_VALUE(VALUE_FROM_FLOAT(ceilf(GET_VALUE(0).f)), rindex);
	#endif
}

static bool float_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("A single argument is expected in Float casting.");
    
    gravity_value_t v = convert_value2float(vm, GET_VALUE(1));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Float.");
    RETURN_VALUE(v, rindex);
}

static bool float_degrees (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	// Convert the float from radians to degrees
	RETURN_VALUE(VALUE_FROM_FLOAT(GET_VALUE(0).f*180/3.141592653589793), rindex);
}

static bool float_radians (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	// Convert the float from degrees to radians
	RETURN_VALUE(VALUE_FROM_FLOAT(GET_VALUE(0).f*3.141592653589793/180), rindex);
}

// MARK: - Int Class -

// binary operators
static bool operator_int_add (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (nargs)
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_INT(v2);
	RETURN_VALUE(VALUE_FROM_INT(v1.n + v2.n), rindex);
}

static bool operator_int_sub (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (nargs)
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_INT(v2);
	RETURN_VALUE(VALUE_FROM_INT(v1.n - v2.n), rindex);
}

static bool operator_int_div (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (nargs)
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_INT(v2);
	
	if (v2.n == 0) RETURN_ERROR("Division by 0 error.");
	RETURN_VALUE(VALUE_FROM_INT(v1.n / v2.n), rindex);
}

static bool operator_int_mul (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (nargs)
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_INT(v2);
	RETURN_VALUE(VALUE_FROM_INT(v1.n * v2.n), rindex);
}

static bool operator_int_rem (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (nargs)
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_INT(v2);
	
	if (v2.n == 0) RETURN_ERROR("Reminder by 0 error.");
	RETURN_VALUE(VALUE_FROM_INT(v1.n % v2.n), rindex);
}

static bool operator_int_and (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (nargs)
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	INTERNAL_CONVERT_BOOL(v2);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_int_or (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (nargs)
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	INTERNAL_CONVERT_BOOL(v2);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_int_neg (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	RETURN_VALUE(VALUE_FROM_INT(-GET_VALUE(0).n), rindex);
}

static bool operator_int_not (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	RETURN_VALUE(VALUE_FROM_BOOL(!GET_VALUE(0).n), rindex);
}

static bool operator_int_cmp (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	if (VALUE_ISA_FLOAT(args[1])) return operator_float_cmp(vm, args, nargs, rindex);
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_INT(v2);
	if (v1.n == v2.n) RETURN_VALUE(VALUE_FROM_INT(0), rindex);
	if (v1.n > v2.n) RETURN_VALUE(VALUE_FROM_INT(1), rindex);
	RETURN_VALUE(VALUE_FROM_INT(-1), rindex);
}

static bool int_loop (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
	if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");
	
	gravity_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));	// closure to execute
	gravity_value_t value = GET_VALUE(0);							// self parameter
	register gravity_int_t n = value.n;								// times to execute the loop
	register gravity_int_t i = 0;
	
	nanotime_t t1 = nanotime();
	while (i < n) {
        if (!gravity_vm_runclosure(vm, closure, value, &VALUE_FROM_INT(i), 1)) return false;
		++i;
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool int_random (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args)
	if (nargs != 3) RETURN_ERROR("Int.random() expects 2 integer arguments");

	if (!VALUE_ISA_INT(GET_VALUE(1)) || !VALUE_ISA_INT(GET_VALUE(2))) RETURN_ERROR("Int.random() arguments must be integers");

	gravity_int_t num1 = VALUE_AS_INT(GET_VALUE(1));
	gravity_int_t num2 = VALUE_AS_INT(GET_VALUE(2));

	// Only Seed once
	static bool already_seeded = false;
	if (!already_seeded) {
		srand((unsigned)time(NULL));
		already_seeded = true;
	}

	int r;
	// if num1 is lower, consider it min, otherwise, num2 is min
	if (num1 < num2) {
		// returns a random integer between num1 and num2 inclusive
		r = (int)((rand() % (num2 - num1 + 1)) + num1);
	}
	else if (num1 > num2) {
		r = (int)((rand() % (num1 - num2 + 1)) + num2);
	}
	else {
		r = (int)num1;
	}
	RETURN_VALUE(VALUE_FROM_INT(r), rindex);
}

static bool int_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("A single argument is expected in Int casting.");
    
    gravity_value_t v = convert_value2int(vm, GET_VALUE(1));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Int.");
    RETURN_VALUE(v, rindex);
}

static bool int_degrees (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	// Convert the int from radians to degrees
	RETURN_VALUE(VALUE_FROM_FLOAT(GET_VALUE(0).n*180/3.141592653589793), rindex);
}

static bool int_radians (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	// Convert the int from degrees to radians
	RETURN_VALUE(VALUE_FROM_FLOAT(GET_VALUE(0).n*3.141592653589793/180), rindex);
}

// MARK: - Bool Class -

static bool operator_bool_add (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return operator_int_add(vm, args, nargs, rindex);
}

static bool operator_bool_sub (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return operator_int_sub(vm, args, nargs, rindex);
}

static bool operator_bool_div (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return operator_int_div(vm, args, nargs, rindex);
}

static bool operator_bool_mul (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return operator_int_mul(vm, args, nargs, rindex);
}

static bool operator_bool_rem (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return operator_int_rem(vm, args, nargs, rindex);
}

static bool operator_bool_and (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_bool_or (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_bool_bitor (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)

	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n | v2.n), rindex);
}

static bool operator_bool_bitand (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)

	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n & v2.n), rindex);
}

static bool operator_bool_bitxor (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)

	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n ^ v2.n), rindex);
}

static bool operator_bool_cmp (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return operator_int_cmp(vm, args, nargs, rindex);
}

// unary operators
static bool operator_bool_neg (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	RETURN_VALUE(VALUE_FROM_INT(-GET_VALUE(0).n), rindex);
}

static bool operator_bool_not (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	RETURN_VALUE(VALUE_FROM_INT(!GET_VALUE(0).n), rindex);
}

static bool bool_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("A single argument is expected in Bool casting.");
    
    gravity_value_t v = convert_value2bool(vm, GET_VALUE(1));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Bool.");
    RETURN_VALUE(v, rindex);
}

// MARK: - String Class -

static bool operator_string_add (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_STRING(v2);
	
	gravity_string_t *s1 = VALUE_AS_STRING(v1);
	gravity_string_t *s2 = VALUE_AS_STRING(v2);
	
	uint32_t len = s1->len + s2->len;
	char buffer[4096];
	char *s = NULL;
	
	// check if I can save an allocation
	if (len+1<sizeof(buffer)) s = buffer;
	else s = mem_alloc(len+1);
	
	if (!s) return false;
	memcpy(s, s1->s, s1->len);
	memcpy(s+s1->len, s2->s, s2->len);
	
	gravity_value_t v = VALUE_FROM_STRING(vm, s, len);
	if (s != NULL && s != buffer) mem_free(s);
	
	RETURN_VALUE(v, rindex);
}

static bool operator_string_sub (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_STRING(v2);
	
	gravity_string_t *s1 = VALUE_AS_STRING(v1);
	gravity_string_t *s2 = VALUE_AS_STRING(v2);
	
	// subtract s2 from s1
	char *found = string_strnstr(s1->s, s2->s, (size_t)s1->len);
	if (!found) RETURN_VALUE(VALUE_FROM_STRING(vm, s1->s, s1->len), rindex);
	
	// substring found
	// now check if entire substring must be considered
	uint32_t flen = (uint32_t)strlen(found);
	if (flen == s2->len) RETURN_VALUE(VALUE_FROM_STRING(vm, s1->s, (uint32_t)(found - s1->s)), rindex);
	// sanity check for malformed strings
    if (flen < s2->len) RETURN_ERROR("Malformed string.");
    
	// substring found but cannot be entirely considered
	uint32_t alloc = MAXNUM(s1->len + s2->len +1, DEFAULT_MINSTRING_SIZE);
	char *s = mem_alloc(alloc);
	
	uint32_t seek = (uint32_t)(found - s1->s);
	uint32_t len = seek + (flen - s2->len);
	memcpy(s, s1->s, seek);
	memcpy(s+seek, found+s2->len, flen - s2->len);
	
	gravity_string_t *string = gravity_string_new(vm, s, len, alloc);
	RETURN_VALUE(VALUE_FROM_OBJECT(string), rindex);
}

static bool operator_string_and (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	INTERNAL_CONVERT_BOOL(v2);
	
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_string_or (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_BOOL(v1);
	INTERNAL_CONVERT_BOOL(v2);
	
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_string_neg (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_1VARIABLE(v1, 0);
	
	// reverse the string
	gravity_string_t *s1 = VALUE_AS_STRING(v1);
	char *s = (char *)string_ndup(s1->s, s1->len);
	if (!utf8_reverse(s)) RETURN_ERROR("Unable to reverse a malformed string.");
	
	gravity_string_t *string = gravity_string_new(vm, s, s1->len, s1->len);
	RETURN_VALUE(VALUE_FROM_OBJECT(string), rindex);
}

static bool operator_string_cmp (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_2VARIABLES(v1, v2, 0, 1);
	INTERNAL_CONVERT_STRING(v2);
	
	gravity_string_t *s1 = VALUE_AS_STRING(v1);
	gravity_string_t *s2 = VALUE_AS_STRING(v2);
	
	RETURN_VALUE(VALUE_FROM_INT(strcmp(s1->s, s2->s)), rindex);
}

static bool string_length (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	
	DECLARE_1VARIABLE(v1, 0);
	gravity_string_t *s1 = VALUE_AS_STRING(v1);
	
	RETURN_VALUE(VALUE_FROM_INT(s1->len), rindex);
}

static bool string_index (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm)

	if ((nargs != 2) || (!VALUE_ISA_STRING(GET_VALUE(1)))) {
		RETURN_ERROR("String.index() expects a string as an argument");
	}
	
	gravity_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));
	gravity_string_t *str_to_index = VALUE_AS_STRING(GET_VALUE(1));

	// search for the string
	char *ptr = string_strnstr(main_str->s, str_to_index->s, (size_t)main_str->len);

	// if it doesn't exist, return null
	if (ptr == NULL) {
		RETURN_VALUE(VALUE_FROM_NULL, rindex);
	}
	// otherwise, return the difference, which is the index that the string starts at
	else {
		RETURN_VALUE(VALUE_FROM_INT(ptr - main_str->s), rindex);
	}
}

static bool string_count (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm)

	if ((nargs != 2) || (!VALUE_ISA_STRING(GET_VALUE(1)))) {
		RETURN_ERROR("String.count() expects a string as an argument");
	}
	
	gravity_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));
	gravity_string_t *str_to_count = VALUE_AS_STRING(GET_VALUE(1));

	int j = 0;
	int count = 0;

	// iterate through whole string
	for (int i = 0; i < main_str->len; ++i) {
		if (main_str->s[i] == str_to_count->s[j]) {
			// if the characters match and we are on the last character of the search
			// string, then we have found a match
			if (j == str_to_count->len - 1) {
				++count;
				j = 0;
				continue;
			}
		}
		// reset if it isn't a match
		else {
			j = 0;
			continue;
		}
		// move forward in the search string if we found a match but we aren't
		// finished checking all the characters of the search string yet
		++j;
	}

	RETURN_VALUE(VALUE_FROM_INT(count), rindex);
}


static bool string_repeat (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	if ((nargs != 2) || (!VALUE_ISA_INT(GET_VALUE(1)))) {
		RETURN_ERROR("String.repeat() expects an integer argument");
	}
	
	gravity_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));
	gravity_int_t times_to_repeat = VALUE_AS_INT(GET_VALUE(1));
	if (times_to_repeat < 1 || times_to_repeat > MAX_ALLOCATION) {
		RETURN_ERROR("String.repeat() expects a value >= 1 and < %d", MAX_ALLOCATION);
	}

	// figure out the size of the array we need to make to hold the new string
	uint32_t new_size = (uint32_t)(main_str->len * times_to_repeat);
	char *new_str = mem_alloc(new_size+1);
	if (!new_str) RETURN_ERROR("Unable to allocate a String so big (%d)", new_size);
	
    uint32_t seek = 0;
	for (uint32_t i = 0; i < times_to_repeat; ++i) {
        memcpy(new_str+seek, main_str->s, main_str->len);
        seek += main_str->len;
	}

    gravity_string_t *s = gravity_string_new(vm, new_str, new_size, new_size);
	RETURN_VALUE(VALUE_FROM_OBJECT(s), rindex);
}

static bool string_upper (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	gravity_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));

	char *ret = mem_alloc(main_str->len + 1);
	if (!ret) RETURN_ERROR("Unable to allocate a String so big (%d)", main_str->len);
	strcpy(ret, main_str->s);

	// if no arguments passed, change the whole string to uppercase
	if (nargs == 1) {
		for (int i = 0; i <= main_str->len; ++i) {
		 ret[i] = toupper(ret[i]);
		}
	}
	// otherwise, evaluate all the arguments
	else {
		for (int i = 1; i < nargs; ++i) {
			gravity_value_t value = GET_VALUE(i);
			if (VALUE_ISA_INT(value)) {
				int32_t index = (int32_t)VALUE_AS_INT(value);

				if (index < 0) index = main_str->len + index;
				if ((index < 0) || ((uint32_t)index >= main_str->len)) {
					mem_free(ret);
					RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, main_str->len - 1);
				}

				ret[index] = toupper(ret[index]);
			}
			else {
				mem_free(ret);
				RETURN_ERROR("upper() expects either no arguments, or integer arguments.");
			}
		}
	}

	gravity_string_t *s = gravity_string_new(vm, ret, main_str->len, 0);
	RETURN_VALUE(VALUE_FROM_OBJECT(s), rindex);
}

static bool string_lower (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	gravity_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));

	char *ret = mem_alloc(main_str->len + 1);
	if (!ret) RETURN_ERROR("Unable to allocate a String so big (%d)", main_str->len);
	strcpy(ret, main_str->s);

	// if no arguments passed, change the whole string to lowercase
	if (nargs == 1) {
		for (int i = 0; i <= main_str->len; ++i) {
		 ret[i] = tolower(ret[i]);
		}
	}
	// otherwise, evaluate all the arguments
	else {
		for (int i = 1; i < nargs; ++i) {
			gravity_value_t value = GET_VALUE(i);
			if (VALUE_ISA_INT(value)) {
				int32_t index = (int32_t)VALUE_AS_INT(value);

				if (index < 0) index = main_str->len + index;
				if ((index < 0) || ((uint32_t)index >= main_str->len))  {
					mem_free(ret);
					RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, main_str->len - 1);
				}

				ret[index] = tolower(ret[index]);
			}
			else {
				mem_free(ret);
				RETURN_ERROR("lower() expects either no arguments, or integer arguments.");
			}
		}
	}

	gravity_string_t *s = gravity_string_new(vm, ret, main_str->len, 0);
	RETURN_VALUE(VALUE_FROM_OBJECT(s), rindex);
}

static bool string_loadat (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(nargs)
	gravity_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
	gravity_value_t value = GET_VALUE(1);

	int32_t first_index;
	int32_t second_index;

	if (VALUE_ISA_INT(value)) {
		first_index = (int32_t)VALUE_AS_INT(value);
		second_index = first_index;
	}
	else if (VALUE_ISA_RANGE(value)) {
		gravity_range_t *range = VALUE_AS_RANGE(value);
		first_index = (int32_t)range->from;
		second_index = (int32_t)range->to;
	}
	else {
		RETURN_ERROR("An integer index or index range is required to access string items.");
	}

	if (first_index < 0) first_index = string->len + first_index;
	if ((first_index < 0) || ((uint32_t)first_index >= string->len)) RETURN_ERROR("Out of bounds error: first_index %d beyond bounds 0...%d", first_index, string->len-1);

	if (second_index < 0) second_index = string->len + second_index;
	if ((second_index < 0) || ((uint32_t)second_index >= string->len)) RETURN_ERROR("Out of bounds error: second_index %d beyond bounds 0...%d", second_index, string->len-1);

	uint32_t substr_len = first_index < second_index ? second_index - first_index + 1 : first_index - second_index + 1;

	bool is_forward = first_index <= second_index;
	if (!is_forward) {
		char *original = mem_alloc(string->len);
		if (!original) RETURN_ERROR("Unable to allocate a String so big (%d)", string->len);
		// without copying it, we would be modifying the original string
		strncpy((char *)original, string->s, string->len);
		uint32_t original_len = (uint32_t) string->len;
		
		// Reverse the string, and reverse the indices
		first_index = original_len - first_index -1;

		// reverse the String
		int i = original_len - 1;
		int j = 0;
		char c;
		while (i > j) {
			c = original[i];
			original[i] = original[j];
			original[j] = c;
			--i;
			++j;
		}
        
        gravity_value_t s = VALUE_FROM_STRING(vm, original + first_index, substr_len);
        mem_free(original);
        
		RETURN_VALUE(s, rindex);
	}
    RETURN_VALUE(VALUE_FROM_STRING(vm, string->s + first_index, substr_len), rindex);
}

static bool string_storeat (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs, rindex)
	
	gravity_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
	gravity_value_t idxvalue = GET_VALUE(1);
	if (!VALUE_ISA_INT(idxvalue)) RETURN_ERROR("An integer index is required to access a string item.");
	if (!VALUE_ISA_STRING(GET_VALUE(2))) RETURN_ERROR("A string needs to be assigned to a string index");

	gravity_string_t *value = VALUE_AS_STRING(GET_VALUE(2));
	register int32_t index = (int32_t)VALUE_AS_INT(idxvalue);
	
	if (index < 0) index = string->len + index;
	if (index < 0 || index >= string->len) RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, string->len-1);
	if (index+value->len - 1 >= string->len) RETURN_ERROR("Out of bounds error: End of inserted string exceeds the length of the initial string");

	// this code is not UTF-8 safe
	for (int i = index; i < index+value->len; ++i) {
		string->s[i] = value->s[i-index];
	}
	
	// characters inside string changed so we need to re-compute hash
	string->hash = gravity_hash_compute_buffer((const char *)string->s, string->len);
	
	RETURN_NOVALUE();
}

static bool string_split (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	// sanity check
	if ((nargs != 2) || (!VALUE_ISA_STRING(GET_VALUE(1)))) RETURN_ERROR("String.split() expects 1 string separator.");
	
	// setup arguments
	gravity_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
	gravity_string_t *substr = VALUE_AS_STRING(GET_VALUE(1));
	const char *sep = substr->s;
	uint32_t seplen = substr->len;
	
	// initialize the list to have a size of 0
	gravity_list_t *list = gravity_list_new(vm, 0);
	
	// split loop
	char *original = string->s;
    uint32_t slen = string->len;
	while (1) {
		char *p = string_strnstr(original, sep, (size_t)slen);
		if (p == NULL) {
            if (marray_size(list->array) == 0) slen = 0;
			marray_push(gravity_value_t, list->array, VALUE_FROM_STRING(vm, original, string->len - slen));
			break;
		}
		marray_push(gravity_value_t, list->array, VALUE_FROM_STRING(vm, original, (uint32_t)(p-original)));
		
        // update pointer and slen
		original = p + seplen;
        slen = (uint32_t)(original - string->s);        
	}
	RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
}

static bool string_loop (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
	if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");

	gravity_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));	// closure to execute
	gravity_value_t value = GET_VALUE(0);							// self parameter
	gravity_string_t *string = VALUE_AS_STRING(value);
	char *str = string->s;
	register gravity_int_t n = string->len;  // Times to execute the loop
	register gravity_int_t i = 0;

	nanotime_t t1 = nanotime();
	while (i < n) {
		gravity_value_t v_str = VALUE_FROM_STRING(vm, str + i, 1);
		if (!gravity_vm_runclosure(vm, closure, value, &v_str, 1)) return false;
		++i;
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool string_iterator (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_string_t *string = VALUE_AS_STRING(GET_VALUE(0));

	// check for empty string first
	if (string->len == 0) RETURN_VALUE(VALUE_FROM_FALSE, rindex);

	// check for start of iteration
	if (VALUE_ISA_NULL(GET_VALUE(1))) RETURN_VALUE(VALUE_FROM_INT(0), rindex);

	// extract value
	gravity_value_t value = GET_VALUE(1);

	// check error condition
	if (!VALUE_ISA_INT(value)) RETURN_ERROR("Iterator expects a numeric value here.");

	// compute new value
	gravity_int_t n = value.n;
	if (n+1 < string->len) {
		++n;
	} else {
		RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	}

	// return new iterator
	RETURN_VALUE(VALUE_FROM_INT(n), rindex);
}

static bool string_iterator_next (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm, nargs)
	gravity_string_t	*string = VALUE_AS_STRING(GET_VALUE(0));
	register int32_t index = (int32_t)VALUE_AS_INT(GET_VALUE(1));
	RETURN_VALUE(VALUE_FROM_STRING(vm, string->s + index, 1), rindex);
}

static bool string_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("A single argument is expected in String casting.");
    
    gravity_value_t v = convert_value2string(vm, GET_VALUE(1));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to String.");
    RETURN_VALUE(v, rindex);
}

// MARK: - Fiber Class -

static bool fiber_create (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(nargs)
	
	if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("A function is expected as argument to Fiber.create.");
	
	gravity_fiber_t *fiber = gravity_fiber_new(vm, VALUE_AS_CLOSURE(GET_VALUE(1)), 0, 0);
	RETURN_VALUE(VALUE_FROM_OBJECT(fiber), rindex);
}

static bool fiber_run (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex, bool is_trying) {
	#pragma unused(nargs, rindex)
	
	gravity_fiber_t *fiber = VALUE_AS_FIBER(GET_VALUE(0));
	if (fiber->caller != NULL) RETURN_ERROR("Fiber has already been called.");
	
	// remember who ran the fiber
	fiber->caller = gravity_vm_fiber(vm);
	
	// set trying flag
	fiber->trying = is_trying;
	
	// switch currently running fiber
	gravity_vm_setfiber(vm, fiber);
	
	RETURN_FIBER();
}

static bool fiber_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return fiber_run(vm, args, nargs, rindex, false);
}

static bool fiber_try (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return fiber_run(vm, args, nargs, rindex, true);
}

static bool fiber_yield (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args, nargs, rindex)
	
    // set rindex slot to NULL in order to falsify the if closure check performed by the VM
    gravity_vm_setslot(vm, VALUE_FROM_NULL, rindex);
    
    // get currently executed fiber
	gravity_fiber_t *fiber = gravity_vm_fiber(vm);
    
    // in no caller then this is just a NOP
    if (fiber->caller) {
		gravity_vm_setfiber(vm, fiber->caller);
	
		// unhook this fiber from the one that called it
		fiber->caller = NULL;
		fiber->trying = false;
	
		RETURN_FIBER();
    } else {
        RETURN_NOVALUE();
    }
}

static bool fiber_status (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(nargs)
		
	gravity_fiber_t *fiber = VALUE_AS_FIBER(GET_VALUE(0));
	RETURN_VALUE(VALUE_FROM_BOOL(fiber->nframes == 0 || fiber->error), rindex);
}

static bool fiber_abort (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	gravity_value_t msg = (nargs > 0) ? GET_VALUE(1) : VALUE_FROM_NULL;
	if (!VALUE_ISA_STRING(msg)) RETURN_ERROR("Fiber.abort expects a string as argument.");
	
	gravity_string_t *s = VALUE_AS_STRING(msg);
	RETURN_ERROR("%.*s", s->len, s->s);
}

// MARK: - Null Class -

static bool operator_null_add (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,nargs)
	// NULL + v2 = v2
	RETURN_VALUE(args[1], rindex);
}

static bool operator_null_sub (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	// NULL - v2 should be computed as -v2 in my opinion (since NULL is interpreted as zero)
	args[0] = VALUE_FROM_INT(0);
	return operator_int_sub(vm, args, nargs, rindex);
}

static bool operator_null_div (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,args,nargs)
	// NULL / v2 = 0
	RETURN_VALUE(VALUE_FROM_INT(0), rindex);
}

static bool operator_null_mul (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,args,nargs)
	// NULL * v2 = 0
	RETURN_VALUE(VALUE_FROM_INT(0), rindex);
}

static bool operator_null_rem (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,args,nargs)
	// NULL % v2 = 0
	RETURN_VALUE(VALUE_FROM_INT(0), rindex);
}

static bool operator_null_and (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,args,nargs)
	RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
}

static bool operator_null_or (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,nargs)
	
	DECLARE_1VARIABLE(v2, 1);
	INTERNAL_CONVERT_BOOL(v2);
	RETURN_VALUE(VALUE_FROM_BOOL(0 || v2.n), rindex);
}

// unary operators
static bool operator_null_neg (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,args,nargs)
	RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
}

static bool operator_null_not (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,args,nargs)
	// !null = true in all the tested programming languages
	RETURN_VALUE(VALUE_FROM_BOOL(1), rindex);
}

#if GRAVITY_NULL_SILENT
static bool operator_null_silent (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,args,nargs)
	RETURN_VALUE(VALUE_FROM_NULL, rindex);
}
#endif

static bool operator_null_cmp (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(vm,nargs)
	if (VALUE_ISA_UNDEFINED(args[0])) {
		// undefined case (undefined is equal ONLY to undefined)
		if (VALUE_ISA_UNDEFINED(args[1])) RETURN_VALUE(VALUE_FROM_BOOL(1), rindex);
		RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
	}
	
	args[0] = VALUE_FROM_INT(0);
	return operator_int_cmp(vm, args, nargs, rindex);
}

static bool null_exec (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_NULL, rindex);
}

// MARK: - System -

static bool system_nanotime (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused(args,nargs)
	nanotime_t t = nanotime();
	RETURN_VALUE(VALUE_FROM_INT(t), rindex);
}

static bool system_realprint (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex, bool cr) {
	#pragma unused (rindex)
	for (uint16_t i=1; i<nargs; ++i) {
		gravity_value_t v = GET_VALUE(i);
		INTERNAL_CONVERT_STRING(v);
		gravity_string_t *s = VALUE_AS_STRING(v);
		printf("%.*s", s->len, s->s);
	}
	if (cr) printf("\n");
	RETURN_NOVALUE();
}

static bool system_put (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return system_realprint(vm, args, nargs, rindex, false);
}
	
static bool system_print (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	return system_realprint(vm, args, nargs, rindex, true);
}

static bool system_get (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (args, nargs)
	gravity_value_t key = GET_VALUE(1);
	if (!VALUE_ISA_STRING(key)) RETURN_VALUE(VALUE_FROM_NULL, rindex);
	RETURN_VALUE(gravity_vm_get(vm, VALUE_AS_CSTRING(key)), rindex);
}

static bool system_set (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (nargs, rindex)
	gravity_value_t key = GET_VALUE(1);
	gravity_value_t value = GET_VALUE(2);
	if (!VALUE_ISA_STRING(key)) RETURN_NOVALUE();
	
	bool result = gravity_vm_set(vm, VALUE_AS_CSTRING(key), value);
	if (!result) RETURN_ERROR("Unable to apply System setting.");
	RETURN_NOVALUE();
}

static bool system_exit (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
	#pragma unused (vm, nargs, rindex)
	int n = (VALUE_ISA_INT(GET_VALUE(1))) ? (int)GET_VALUE(1).n : 0;
	exit(n);
	RETURN_NOVALUE();
}


// MARK: - CORE -

gravity_closure_t *computed_property_create (gravity_vm *vm, gravity_function_t *getter_func, gravity_function_t *setter_func) {
	gravity_closure_t *getter_closure = (getter_func) ? gravity_closure_new(vm, getter_func) : NULL;
	gravity_closure_t *setter_closure = (setter_func) ? gravity_closure_new(vm, setter_func) : NULL;
	gravity_function_t *f = gravity_function_new_special(vm, NULL, GRAVITY_COMPUTED_INDEX, getter_closure, setter_closure);
	return gravity_closure_new(vm, f);
}

void computed_property_free (gravity_class_t *c, const char *name, bool remove_flag) {
    STATICVALUE_FROM_STRING(key, name, strlen(name));
    gravity_closure_t *closure = gravity_class_lookup_closure(c, key);
    assert(closure);
    
    gravity_closure_t *getter = (gravity_closure_t *)closure->f->special[0];
    gravity_closure_t *setter = (closure->f->special[0] != closure->f->special[1]) ? (gravity_closure_t *)closure->f->special[1] : NULL;
    if (getter) {
        gravity_function_t *f = getter->f;
        gravity_closure_free(NULL, getter);
        gravity_function_free(NULL, f);
    }
    if (setter) {
        gravity_function_t *f = setter->f;
        gravity_closure_free(NULL, setter);
        gravity_function_free(NULL, f);
    }
    
    if (closure->f) gravity_function_free(NULL, closure->f);
    gravity_closure_free(NULL, closure);
    
    if (remove_flag) gravity_hash_remove(c->htable, key);
}

static void gravity_core_init (void) {
	// this function must be executed ONCE
	if (core_inited) return;
	core_inited = true;
	
	mem_check(false);
	
	// Creation order here is very important
	// for example in a earlier version the intrinsic classes
	// were created before the Function class
	// so when the isa pointer was set to gravity_class_function
	// it resulted in a NULL pointer
	
	// Object and Class are special classes
	// Object has no superclass (so the lookup loop knows when to finish)
	// Class has Object as its superclass
	// Any other class without an explicit superclass automatically has Object as its super
	// Both Object and Class classes has Class set as metaclass
	// Any other class created with gravity_class_new_pair has "class meta" as its metaclass
	
	//		CORE CLASS DIAGRAM:
	//
	//		---->	means class's superclass
	//		====>	means class's metaclass
	//
	//
	//           +--------------------+    +=========+
	//           |                    |    ||       ||
	//           v                    |    \/       ||
	//		+--------------+     +--------------+   ||
	//		|    Object    | ==> |     Class    |====+
	//		+--------------+     +--------------+
	//             ^                    ^
	//             |                    |
	//		+--------------+     +--------------+
	//		|     Base     | ==> |   Base meta  |
	//		+--------------+     +--------------+
	//             ^                    ^
	//             |                    |
	//		+--------------+     +--------------+
	//		|   Subclass   | ==> |Subclass meta |
	//		+--------------+     +--------------+
	
	// Create classes first and then bind methods
	// A class without a superclass in a subclass of Object.
	
	// every class without a super will have OBJECT as its superclass
	gravity_class_object = gravity_class_new_single(NULL, GRAVITY_CLASS_OBJECT_NAME, 0);
	gravity_class_class = gravity_class_new_single(NULL, GRAVITY_CLASS_CLASS_NAME, 0);
	gravity_class_setsuper(gravity_class_class, gravity_class_object);
	
	// manually set meta class and isa pointer for classes created without the gravity_class_new_pair
	// when gravity_class_new_single was called gravity_class_class was NULL so the isa pointer must be reset
	gravity_class_object->objclass = gravity_class_class; gravity_class_object->isa = gravity_class_class;
	gravity_class_class->objclass = gravity_class_class; gravity_class_class->isa = gravity_class_class;
	
	// NULL in gravity_class_new_pair and NEW_FUNCTION macro because I do not want them to be in the GC
	gravity_class_function = gravity_class_new_pair(NULL, GRAVITY_CLASS_FUNCTION_NAME, NULL, 0, 0);
	gravity_class_fiber = gravity_class_new_pair(NULL, GRAVITY_CLASS_FIBER_NAME, NULL, 0, 0);
	gravity_class_instance = gravity_class_new_pair(NULL, GRAVITY_CLASS_INSTANCE_NAME, NULL, 0, 0);
	gravity_class_closure = gravity_class_new_pair(NULL, GRAVITY_CLASS_CLOSURE_NAME, NULL, 0, 0);
	gravity_class_upvalue = gravity_class_new_pair(NULL, GRAVITY_CLASS_UPVALUE_NAME, NULL, 0, 0);
	gravity_class_module = NULL;
	
	// create intrinsic classes (intrinsic datatypes are: Int, Float, Bool, Null, String, List, Map and Range)
	gravity_class_int = gravity_class_new_pair(NULL, GRAVITY_CLASS_INT_NAME, NULL, 0, 0);
	gravity_class_float = gravity_class_new_pair(NULL, GRAVITY_CLASS_FLOAT_NAME, NULL, 0, 0);
	gravity_class_bool = gravity_class_new_pair(NULL, GRAVITY_CLASS_BOOL_NAME, NULL, 0, 0);
	gravity_class_null = gravity_class_new_pair(NULL, GRAVITY_CLASS_NULL_NAME, NULL, 0, 0);
	gravity_class_string = gravity_class_new_pair(NULL, GRAVITY_CLASS_STRING_NAME, NULL, 0, 0);
	gravity_class_list = gravity_class_new_pair(NULL, GRAVITY_CLASS_LIST_NAME, NULL, 0, 0);
	gravity_class_map = gravity_class_new_pair(NULL, GRAVITY_CLASS_MAP_NAME, NULL, 0, 0);
	gravity_class_range = gravity_class_new_pair(NULL, GRAVITY_CLASS_RANGE_NAME, NULL, 0, 0);
	
	// OBJECT CLASS
	gravity_class_bind(gravity_class_object, GRAVITY_CLASS_CLASS_NAME, NEW_CLOSURE_VALUE(object_class));
	gravity_class_bind(gravity_class_object, GRAVITY_OPERATOR_ISA_NAME, NEW_CLOSURE_VALUE(object_isa));
	gravity_class_bind(gravity_class_object, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(object_cmp));
    gravity_class_bind(gravity_class_object, GRAVITY_OPERATOR_EQQ_NAME, NEW_CLOSURE_VALUE(object_eqq));
    gravity_class_bind(gravity_class_object, GRAVITY_OPERATOR_NEQQ_NAME, NEW_CLOSURE_VALUE(object_neqq));
	gravity_class_bind(gravity_class_object, GRAVITY_CLASS_INT_NAME, NEW_CLOSURE_VALUE(convert_object_int));
	gravity_class_bind(gravity_class_object, GRAVITY_CLASS_FLOAT_NAME, NEW_CLOSURE_VALUE(convert_object_float));
	gravity_class_bind(gravity_class_object, GRAVITY_CLASS_BOOL_NAME, NEW_CLOSURE_VALUE(convert_object_bool));
	gravity_class_bind(gravity_class_object, GRAVITY_CLASS_STRING_NAME, NEW_CLOSURE_VALUE(convert_object_string));
	gravity_class_bind(gravity_class_object, GRAVITY_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(object_load));
	gravity_class_bind(gravity_class_object, GRAVITY_INTERNAL_LOADS_NAME, NEW_CLOSURE_VALUE(object_loads));
	gravity_class_bind(gravity_class_object, GRAVITY_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(object_store));
	gravity_class_bind(gravity_class_object, GRAVITY_INTERNAL_NOTFOUND_NAME, NEW_CLOSURE_VALUE(object_notfound));
	gravity_class_bind(gravity_class_object, "_size", NEW_CLOSURE_VALUE(object_internal_size));
	gravity_class_bind(gravity_class_object, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(object_not));
	gravity_class_bind(gravity_class_object, "bind", NEW_CLOSURE_VALUE(object_bind));
	gravity_class_bind(gravity_class_object, "unbind", NEW_CLOSURE_VALUE(object_unbind));
    gravity_class_bind(gravity_class_object, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(object_exec));
    
    // NULL CLASS
    // Meta
    gravity_class_t *null_meta = gravity_class_get_meta(gravity_class_null);
    gravity_class_bind(null_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(null_exec));
	
	// CLASS CLASS
	gravity_class_bind(gravity_class_class, "name", NEW_CLOSURE_VALUE(class_name));
	gravity_class_bind(gravity_class_class, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(class_exec));
	
	// CLOSURE CLASS
	gravity_class_bind(gravity_class_closure, "disassemble", NEW_CLOSURE_VALUE(closure_disassemble));
	gravity_class_bind(gravity_class_closure, "apply", NEW_CLOSURE_VALUE(closure_apply));
	
	// LIST CLASS
    gravity_closure_t *closure = computed_property_create(NULL, NEW_FUNCTION(list_count), NULL);
	gravity_class_bind(gravity_class_list, "count", VALUE_FROM_OBJECT(closure));
	gravity_class_bind(gravity_class_list, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(list_iterator));
	gravity_class_bind(gravity_class_list, ITERATOR_NEXT_FUNCTION, NEW_CLOSURE_VALUE(list_iterator_next));
	gravity_class_bind(gravity_class_list, GRAVITY_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(list_loadat));
	gravity_class_bind(gravity_class_list, GRAVITY_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(list_storeat));
	gravity_class_bind(gravity_class_list, GRAVITY_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(list_loop));
	gravity_class_bind(gravity_class_list, "join", NEW_CLOSURE_VALUE(list_join));
	gravity_class_bind(gravity_class_list, "push", NEW_CLOSURE_VALUE(list_push));
	gravity_class_bind(gravity_class_list, "pop", NEW_CLOSURE_VALUE(list_pop));
	gravity_class_bind(gravity_class_list, "contains", NEW_CLOSURE_VALUE(list_contains));
    gravity_class_bind(gravity_class_list, "remove", NEW_CLOSURE_VALUE(list_remove));
    gravity_class_bind(gravity_class_list, "indexOf", NEW_CLOSURE_VALUE(list_indexOf));
    // Meta
    gravity_class_t *list_meta = gravity_class_get_meta(gravity_class_list);
    gravity_class_bind(list_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(list_exec));
	
	// MAP CLASS
	gravity_class_bind(gravity_class_map, "keys", NEW_CLOSURE_VALUE(map_keys));
	gravity_class_bind(gravity_class_map, "remove", NEW_CLOSURE_VALUE(map_remove));
    closure = computed_property_create(NULL, NEW_FUNCTION(map_count), NULL);
	gravity_class_bind(gravity_class_map, "count", VALUE_FROM_OBJECT(closure));
	gravity_class_bind(gravity_class_map, GRAVITY_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(map_loop));
	gravity_class_bind(gravity_class_map, GRAVITY_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(map_loadat));
	gravity_class_bind(gravity_class_map, GRAVITY_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(map_storeat));
    gravity_class_bind(gravity_class_map, "hasKey", NEW_CLOSURE_VALUE(map_haskey));
	#if GRAVITY_MAP_DOTSUGAR
	gravity_class_bind(gravity_class_map, GRAVITY_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(map_loadat));
	gravity_class_bind(gravity_class_map, GRAVITY_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(map_storeat));
	#endif
	
	// RANGE CLASS
    closure = computed_property_create(NULL, NEW_FUNCTION(range_count), NULL);
	gravity_class_bind(gravity_class_range, "count", VALUE_FROM_OBJECT(closure));
	gravity_class_bind(gravity_class_range, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(range_iterator));
	gravity_class_bind(gravity_class_range, ITERATOR_NEXT_FUNCTION, NEW_CLOSURE_VALUE(range_iterator_next));
	gravity_class_bind(gravity_class_range, "contains", NEW_CLOSURE_VALUE(range_contains));
	gravity_class_bind(gravity_class_range, GRAVITY_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(range_loop));
    // Meta
    gravity_class_t *range_meta = gravity_class_get_meta(gravity_class_range);
    gravity_class_bind(range_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(range_exec));
	
	// INT CLASS
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_int_add));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_int_sub));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_int_div));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_int_mul));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_int_rem));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_int_and));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_int_or));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_int_cmp));
//	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_BITOR_NAME, NEW_CLOSURE_VALUE(operator_int_bitor));
//	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_BITAND_NAME, NEW_CLOSURE_VALUE(operator_int_bitand));
//	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_BITXOR_NAME, NEW_CLOSURE_VALUE(operator_int_bitxor));
//	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_BITLS_NAME, NEW_CLOSURE_VALUE(operator_int_bitls));
//	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_BITRS_NAME, NEW_CLOSURE_VALUE(operator_int_bitrs));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_int_neg));
	gravity_class_bind(gravity_class_int, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_int_not));
	gravity_class_bind(gravity_class_int, GRAVITY_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(int_loop));
    closure = computed_property_create(NULL, NEW_FUNCTION(int_radians), NULL);
	gravity_class_bind(gravity_class_int, "radians", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(int_degrees), NULL);
	gravity_class_bind(gravity_class_int, "degrees", VALUE_FROM_OBJECT(closure));
	// Meta
	gravity_class_t *int_meta = gravity_class_get_meta(gravity_class_int);
	gravity_class_bind(int_meta, "random", NEW_CLOSURE_VALUE(int_random));
    gravity_class_bind(int_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(int_exec));
	
	// FLOAT CLASS
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_float_add));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_float_sub));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_float_div));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_float_mul));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_float_rem));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_float_and));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_float_or));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_float_cmp));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_float_neg));
	gravity_class_bind(gravity_class_float, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_float_not));
	gravity_class_bind(gravity_class_float, "round", NEW_CLOSURE_VALUE(function_float_round));
	gravity_class_bind(gravity_class_float, "floor", NEW_CLOSURE_VALUE(function_float_floor));
	gravity_class_bind(gravity_class_float, "ceil", NEW_CLOSURE_VALUE(function_float_ceil));
    closure = computed_property_create(NULL, NEW_FUNCTION(float_radians), NULL);
	gravity_class_bind(gravity_class_float, "radians", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(float_degrees), NULL);
	gravity_class_bind(gravity_class_float, "degrees", VALUE_FROM_OBJECT(closure));
    // Meta
    gravity_class_t *float_meta = gravity_class_get_meta(gravity_class_float);
    gravity_class_bind(float_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(float_exec));
	
	// BOOL CLASS
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_bool_add));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_bool_sub));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_bool_div));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_bool_mul));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_bool_rem));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_bool_and));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_bool_or));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_BOR_NAME, NEW_CLOSURE_VALUE(operator_bool_bitor));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_BAND_NAME, NEW_CLOSURE_VALUE(operator_bool_bitand));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_BXOR_NAME, NEW_CLOSURE_VALUE(operator_bool_bitxor));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_bool_cmp));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_bool_neg));
	gravity_class_bind(gravity_class_bool, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_bool_not));
    // Meta
    gravity_class_t *bool_meta = gravity_class_get_meta(gravity_class_bool);
    gravity_class_bind(bool_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(bool_exec));
	
	// STRING CLASS
	gravity_class_bind(gravity_class_string, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_string_add));
	gravity_class_bind(gravity_class_string, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_string_sub));
	gravity_class_bind(gravity_class_string, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_string_and));
	gravity_class_bind(gravity_class_string, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_string_or));
	gravity_class_bind(gravity_class_string, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_string_cmp));
	gravity_class_bind(gravity_class_string, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_string_neg));
	gravity_class_bind(gravity_class_string, GRAVITY_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(string_loadat));
	gravity_class_bind(gravity_class_string, GRAVITY_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(string_storeat));
    closure = computed_property_create(NULL, NEW_FUNCTION(string_length), NULL);
	gravity_class_bind(gravity_class_string, "length", VALUE_FROM_OBJECT(closure));
	gravity_class_bind(gravity_class_string, "index", NEW_CLOSURE_VALUE(string_index));
	gravity_class_bind(gravity_class_string, "count", NEW_CLOSURE_VALUE(string_count));
	gravity_class_bind(gravity_class_string, "repeat", NEW_CLOSURE_VALUE(string_repeat));
	gravity_class_bind(gravity_class_string, "upper", NEW_CLOSURE_VALUE(string_upper));
	gravity_class_bind(gravity_class_string, "lower", NEW_CLOSURE_VALUE(string_lower));
	gravity_class_bind(gravity_class_string, "split", NEW_CLOSURE_VALUE(string_split));
	gravity_class_bind(gravity_class_string, "loop", NEW_CLOSURE_VALUE(string_loop));
	gravity_class_bind(gravity_class_string, "iterate", NEW_CLOSURE_VALUE(string_iterator));
	gravity_class_bind(gravity_class_string, "next", NEW_CLOSURE_VALUE(string_iterator_next));
    // Meta
    gravity_class_t *string_meta = gravity_class_get_meta(gravity_class_string);
    gravity_class_bind(string_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(string_exec));

	// FIBER CLASS
	gravity_class_t *fiber_meta = gravity_class_get_meta(gravity_class_fiber);
	gravity_class_bind(fiber_meta, "create", NEW_CLOSURE_VALUE(fiber_create));
	gravity_class_bind(gravity_class_fiber, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(fiber_exec));
	gravity_class_bind(gravity_class_fiber, "try", NEW_CLOSURE_VALUE(fiber_try));
	gravity_class_bind(fiber_meta, "yield", NEW_CLOSURE_VALUE(fiber_yield));
	gravity_class_bind(gravity_class_fiber, "status", NEW_CLOSURE_VALUE(fiber_status));
	gravity_class_bind(fiber_meta, "abort", NEW_CLOSURE_VALUE(fiber_abort));
	
	// BASIC OPERATIONS added also to NULL CLASS (and UNDEFINED since they points to the same class)
	// this is required because every variable is initialized by default to NULL
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_null_add));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_null_sub));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_null_div));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_null_mul));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_null_rem));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_null_and));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_null_or));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_null_cmp));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_null_neg));
	gravity_class_bind(gravity_class_null, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_null_not));
	#if GRAVITY_NULL_SILENT
	gravity_class_bind(gravity_class_null, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
	gravity_class_bind(gravity_class_null, GRAVITY_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
	gravity_class_bind(gravity_class_null, GRAVITY_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
	gravity_class_bind(gravity_class_null, GRAVITY_INTERNAL_NOTFOUND_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
	#endif
	
	// SYSTEM class
	gravity_class_system = gravity_class_new_pair(NULL, GRAVITY_CLASS_SYSTEM_NAME, NULL, 0, 0);
	gravity_class_t *system_meta = gravity_class_get_meta(gravity_class_system);
	gravity_class_bind(system_meta, GRAVITY_SYSTEM_NANOTIME_NAME, NEW_CLOSURE_VALUE(system_nanotime));
	gravity_class_bind(system_meta, GRAVITY_SYSTEM_PRINT_NAME, NEW_CLOSURE_VALUE(system_print));
	gravity_class_bind(system_meta, GRAVITY_SYSTEM_PUT_NAME, NEW_CLOSURE_VALUE(system_put));
	gravity_class_bind(system_meta, "exit", NEW_CLOSURE_VALUE(system_exit));
	
    closure = computed_property_create(NULL, NEW_FUNCTION(system_get), NEW_FUNCTION(system_set));
	gravity_value_t value = VALUE_FROM_OBJECT(closure);
	gravity_class_bind(system_meta, "gcenabled", value);
	gravity_class_bind(system_meta, "gcminthreshold", value);
	gravity_class_bind(system_meta, "gcthreshold", value);
	gravity_class_bind(system_meta, "gcratio", value);
	
	// INIT META
	SETMETA_INITED(gravity_class_int);
	SETMETA_INITED(gravity_class_float);
	SETMETA_INITED(gravity_class_bool);
	SETMETA_INITED(gravity_class_null);
	SETMETA_INITED(gravity_class_string);
	SETMETA_INITED(gravity_class_object);
	SETMETA_INITED(gravity_class_function);
	SETMETA_INITED(gravity_class_closure);
	SETMETA_INITED(gravity_class_fiber);
	SETMETA_INITED(gravity_class_class);
	SETMETA_INITED(gravity_class_instance);
	SETMETA_INITED(gravity_class_list);
	SETMETA_INITED(gravity_class_map);
	SETMETA_INITED(gravity_class_range);
	SETMETA_INITED(gravity_class_upvalue);
	SETMETA_INITED(gravity_class_system);
	//SETMETA_INITED(gravity_class_module);
	
	mem_check(true);
}

void gravity_core_free (void) {
    // free optionals first
    gravity_opt_free();
    
	if (!core_inited) return;
	
	// check if others VM are still running
	if (--refcount != 0) return;
	
	// this function should never be called
	// it is just called when we need to internally check for memory leaks
	
    // computed properties are not registered inside VM gc so they need to be manually freed here
	mem_check(false);
    computed_property_free(gravity_class_list, "count", true);
    computed_property_free(gravity_class_map, "count", true);
    computed_property_free(gravity_class_range, "count", true);
    computed_property_free(gravity_class_string, "length", true);
    computed_property_free(gravity_class_int, "radians", true);
    computed_property_free(gravity_class_int, "degrees", true);
    computed_property_free(gravity_class_float, "radians", true);
    computed_property_free(gravity_class_float, "degrees", true);
    gravity_class_t *system_meta = gravity_class_get_meta(gravity_class_system);
    computed_property_free(system_meta, "gcenabled", true);
        
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_int));
	gravity_class_free_core(NULL, gravity_class_int);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_float));
	gravity_class_free_core(NULL, gravity_class_float);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_bool));
	gravity_class_free_core(NULL, gravity_class_bool);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_string));
	gravity_class_free_core(NULL, gravity_class_string);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_null));
	gravity_class_free_core(NULL, gravity_class_null);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_function));
	gravity_class_free_core(NULL, gravity_class_function);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_closure));
	gravity_class_free_core(NULL, gravity_class_closure);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_fiber));
	gravity_class_free_core(NULL, gravity_class_fiber);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_instance));
	gravity_class_free_core(NULL, gravity_class_instance);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_list));
	gravity_class_free_core(NULL, gravity_class_list);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_map));
	gravity_class_free_core(NULL, gravity_class_map);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_range));
	gravity_class_free_core(NULL, gravity_class_range);
	gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_upvalue));
	gravity_class_free_core(NULL, gravity_class_upvalue);
	
	// before freeing the meta class we need to remove entries with duplicated functions
	{STATICVALUE_FROM_STRING(key, "gcminthreshold", strlen("gcminthreshold")); gravity_hash_remove(system_meta->htable, key);}
	{STATICVALUE_FROM_STRING(key, "gcthreshold", strlen("gcthreshold")); gravity_hash_remove(system_meta->htable, key);}
	{STATICVALUE_FROM_STRING(key, "gcratio", strlen("gcratio")); gravity_hash_remove(system_meta->htable, key);}
	gravity_class_free_core(NULL, system_meta);
	gravity_class_free_core(NULL, gravity_class_system);
	
	// object must be the last class to be freed
	gravity_class_free_core(NULL, gravity_class_class);
	gravity_class_free_core(NULL, gravity_class_object);
	mem_check(true);
	
	gravity_class_int = NULL;
	gravity_class_float = NULL;
	gravity_class_bool = NULL;
	gravity_class_string = NULL;
	gravity_class_object = NULL;
	gravity_class_null = NULL;
	gravity_class_function = NULL;
	gravity_class_closure = NULL;
	gravity_class_fiber = NULL;
	gravity_class_class = NULL;
	gravity_class_instance = NULL;
	gravity_class_list = NULL;
	gravity_class_map = NULL;
	gravity_class_range = NULL;
	gravity_class_upvalue = NULL;
	gravity_class_system = NULL;
	gravity_class_module = NULL;
	
	core_inited = false;
}

const char **gravity_core_identifiers (void) {
	static const char *list[] = {GRAVITY_CLASS_OBJECT_NAME, GRAVITY_CLASS_CLASS_NAME, GRAVITY_CLASS_BOOL_NAME, GRAVITY_CLASS_NULL_NAME,
		GRAVITY_CLASS_INT_NAME, GRAVITY_CLASS_FLOAT_NAME, GRAVITY_CLASS_FUNCTION_NAME, GRAVITY_CLASS_FIBER_NAME, GRAVITY_CLASS_STRING_NAME,
		GRAVITY_CLASS_INSTANCE_NAME, GRAVITY_CLASS_LIST_NAME, GRAVITY_CLASS_MAP_NAME, GRAVITY_CLASS_RANGE_NAME, GRAVITY_CLASS_SYSTEM_NAME,
		GRAVITY_CLASS_CLOSURE_NAME, GRAVITY_CLASS_UPVALUE_NAME, NULL};
    return list;
}

void gravity_core_register (gravity_vm *vm) {
	gravity_core_init();
    gravity_opt_register(vm);
	++refcount;
	if (!vm) return;
	
	// register core classes inside VM
	if (gravity_vm_ismini(vm)) return;
	gravity_vm_setvalue(vm, GRAVITY_CLASS_OBJECT_NAME, VALUE_FROM_OBJECT(gravity_class_object));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_CLASS_NAME, VALUE_FROM_OBJECT(gravity_class_class));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_BOOL_NAME, VALUE_FROM_OBJECT(gravity_class_bool));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_NULL_NAME, VALUE_FROM_OBJECT(gravity_class_null));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_INT_NAME, VALUE_FROM_OBJECT(gravity_class_int));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_FLOAT_NAME, VALUE_FROM_OBJECT(gravity_class_float));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_FUNCTION_NAME, VALUE_FROM_OBJECT(gravity_class_function));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_CLOSURE_NAME, VALUE_FROM_OBJECT(gravity_class_closure));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_FIBER_NAME, VALUE_FROM_OBJECT(gravity_class_fiber));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_STRING_NAME, VALUE_FROM_OBJECT(gravity_class_string));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_INSTANCE_NAME, VALUE_FROM_OBJECT(gravity_class_instance));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_LIST_NAME, VALUE_FROM_OBJECT(gravity_class_list));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_MAP_NAME, VALUE_FROM_OBJECT(gravity_class_map));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_RANGE_NAME, VALUE_FROM_OBJECT(gravity_class_range));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_UPVALUE_NAME, VALUE_FROM_OBJECT(gravity_class_upvalue));
	gravity_vm_setvalue(vm, GRAVITY_CLASS_SYSTEM_NAME, VALUE_FROM_OBJECT(gravity_class_system));
}

gravity_class_t *gravity_core_class_from_name (const char *name) {
    if (name) {
        if (string_cmp(name, GRAVITY_CLASS_OBJECT_NAME) == 0) return gravity_class_object;
        if (string_cmp(name, GRAVITY_CLASS_CLASS_NAME) == 0) return gravity_class_class;
        if (string_cmp(name, GRAVITY_CLASS_BOOL_NAME) == 0) return gravity_class_bool;
        if (string_cmp(name, GRAVITY_CLASS_NULL_NAME) == 0) return gravity_class_null;
        if (string_cmp(name, GRAVITY_CLASS_INT_NAME) == 0) return gravity_class_int;
        if (string_cmp(name, GRAVITY_CLASS_FLOAT_NAME) == 0) return gravity_class_float;
        if (string_cmp(name, GRAVITY_CLASS_FUNCTION_NAME) == 0) return gravity_class_function;
        if (string_cmp(name, GRAVITY_CLASS_CLOSURE_NAME) == 0) return gravity_class_closure;
        if (string_cmp(name, GRAVITY_CLASS_FIBER_NAME) == 0) return gravity_class_fiber;
        if (string_cmp(name, GRAVITY_CLASS_STRING_NAME) == 0) return gravity_class_string;
        if (string_cmp(name, GRAVITY_CLASS_INSTANCE_NAME) == 0) return gravity_class_instance;
        if (string_cmp(name, GRAVITY_CLASS_LIST_NAME) == 0) return gravity_class_list;
        if (string_cmp(name, GRAVITY_CLASS_MAP_NAME) == 0) return gravity_class_map;
        if (string_cmp(name, GRAVITY_CLASS_RANGE_NAME) == 0) return gravity_class_range;
        if (string_cmp(name, GRAVITY_CLASS_UPVALUE_NAME) == 0) return gravity_class_upvalue;
        if (string_cmp(name, GRAVITY_CLASS_SYSTEM_NAME) == 0) return gravity_class_system;
    }
    return NULL;
}

bool gravity_iscore_class (gravity_class_t *c) {
	// first check if it is a class
	if ((c == gravity_class_object) || (c == gravity_class_class) || (c == gravity_class_bool) ||
			(c == gravity_class_null) || (c == gravity_class_int) || (c == gravity_class_float) ||
			(c == gravity_class_function) || (c == gravity_class_fiber) || (c == gravity_class_string) ||
			(c == gravity_class_instance) || (c == gravity_class_list) || (c == gravity_class_map) ||
			(c == gravity_class_range) || (c == gravity_class_system) || (c == gravity_class_closure) ||
		(c == gravity_class_upvalue)) return true;
	
	// if class check is false then check for meta
	return ((c == gravity_class_get_meta(gravity_class_object)) || (c == gravity_class_get_meta(gravity_class_class)) ||
			(c == gravity_class_get_meta(gravity_class_bool)) || (c == gravity_class_get_meta(gravity_class_null)) ||
			(c == gravity_class_get_meta(gravity_class_int)) || (c == gravity_class_get_meta(gravity_class_float)) ||
			(c == gravity_class_get_meta(gravity_class_function)) || (c == gravity_class_get_meta(gravity_class_fiber)) ||
			(c == gravity_class_get_meta(gravity_class_string)) || (c == gravity_class_get_meta(gravity_class_instance)) ||
			(c == gravity_class_get_meta(gravity_class_list)) || (c == gravity_class_get_meta(gravity_class_map)) ||
			(c == gravity_class_get_meta(gravity_class_range)) || (c == gravity_class_get_meta(gravity_class_system)) ||
			(c == gravity_class_get_meta(gravity_class_closure)) || (c == gravity_class_get_meta(gravity_class_upvalue)));
}
