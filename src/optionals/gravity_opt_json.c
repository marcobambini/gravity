//
//  gravity_opt_json.c
//  gravity
//
//  Created by Marco Bambini on 20/08/2019.
//  Copyright © 2019 CreoLabs. All rights reserved.
//

#include <inttypes.h>
#include "gravity_vm.h"
#include "gravity_core.h"
#include "gravity_hash.h"
#include "gravity_json.h"
#include "gravity_utils.h"
#include "gravity_macros.h"
#include "gravity_opcodes.h"
#include "gravity_vmmacros.h"
#include "gravity_opt_json.h"

#define GRAVITY_JSON_STRINGIFY_NAME     "stringify"
#define GRAVITY_JSON_PARSE_NAME         "parse"
#define STATIC_BUFFER_SIZE              8192

static gravity_class_t                  *gravity_class_json = NULL;
static uint32_t                         refcount = 0;

// MARK: - Implementation -

static bool JSON_stringify (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    
    // extract value
    gravity_value_t value = GET_VALUE(1);
    
    // special case for string because it can be huge (and must be quoted)
    if (VALUE_ISA_STRING(value)) {
        const int nchars = 5;
        const char *v = VALUE_AS_STRING(value)->s;
        size_t vlen = VALUE_AS_STRING(value)->len;

        // string must be quoted
        if (vlen < 4096-nchars) {
            char vbuffer2[4096];
            vlen = snprintf(vbuffer2, sizeof(vbuffer2), "\"%s\"", v);
            RETURN_VALUE(VALUE_FROM_STRING(vm, vbuffer2, (uint32_t)vlen), rindex);
        } else {
            char *vbuffer2 = mem_alloc(NULL, vlen + nchars);
            vlen = snprintf(vbuffer2, vlen + nchars, "\"%s\"", v);
            RETURN_VALUE(VALUE_FROM_OBJECT(gravity_string_new(vm, vbuffer2, (uint32_t)vlen, 0)), rindex);
        }
    }
    
    // primitive cases supported by JSON (true, false, null, number)
    char vbuffer[512];
    const char *v = NULL;
    
    if (VALUE_ISA_NULL(value) || (VALUE_ISA_UNDEFINED(value))) v = "null";
    // was %g but we don't like scientific notation nor the missing .0 in case of float number with no decimals
    else if (VALUE_ISA_FLOAT(value)) {snprintf(vbuffer, sizeof(vbuffer), "%f", value.f); v = vbuffer;}
    else if (VALUE_ISA_BOOL(value)) v = (value.n) ? "true" : "false";
    else if (VALUE_ISA_INT(value)) {
        #if GRAVITY_ENABLE_INT64
        snprintf(vbuffer, sizeof(vbuffer), "%" PRId64 "", value.n);
        #else
        snprintf(vbuffer, sizeof(vbuffer), "%d", value.n);
        #endif
        v = vbuffer;
    }
    if (v) RETURN_VALUE(VALUE_FROM_CSTRING(vm, v), rindex);
    
    // more complex object case (list, map, class, closure, instance/object)
    json_t *json = json_new();
    json_set_option(json, json_opt_no_maptype);
    json_set_option(json, json_opt_no_undef);
    json_set_option(json, json_opt_prettify);
    gravity_value_serialize(NULL, value, json);
    
    size_t len = 0;
    const char *jbuffer = json_buffer(json, &len);
    const char *buffer = string_ndup(jbuffer, len);
    
    gravity_string_t *s = gravity_string_new(vm, (char *)buffer, (uint32_t)len, 0);
    json_free(json);
    
    RETURN_VALUE(VALUE_FROM_OBJECT(s), rindex);
}

static gravity_value_t JSON_value (gravity_vm *vm, json_value *json) {
    switch (json->type) {
        case json_none:
        case json_null:
            return VALUE_FROM_NULL;
            
        case json_object: {
            gravity_object_t *obj = gravity_object_deserialize(vm, json);
            gravity_value_t objv = (obj) ? VALUE_FROM_OBJECT(obj) : VALUE_FROM_NULL;
            return objv;
        }
            
        case json_array: {
            unsigned int length = json->u.array.length;
            gravity_list_t *list = gravity_list_new(vm, length);
            for (unsigned int i = 0; i < length; ++i) {
                gravity_value_t value = JSON_value(vm, json->u.array.values[i]);
                marray_push(gravity_value_t, list->array, value);
            }
            return VALUE_FROM_OBJECT(list);
        }
            
        case json_integer:
            return VALUE_FROM_INT(json->u.integer);
            
        case json_double:
            return VALUE_FROM_FLOAT(json->u.dbl);
            
        case json_string:
            return VALUE_FROM_STRING(vm, json->u.string.ptr, json->u.string.length);
            
        case json_boolean:
            return VALUE_FROM_BOOL(json->u.boolean);
    }
    
    return VALUE_FROM_NULL;
}

static bool JSON_parse (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    
    // value to parse
    gravity_value_t value = GET_VALUE(1);
    if (!VALUE_ISA_STRING(value)) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    
    gravity_string_t *string = VALUE_AS_STRING(value);
    json_value *json = json_parse(string->s, string->len);
    if (!json) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    
    RETURN_VALUE(JSON_value(vm, json), rindex);
}

//static bool JSON_begin_object (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
//
//    return false;
//}
//
//static bool json_end_object (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
//
//}
//
//static bool json_begin_array (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
//
//}
//
//static bool json_end_array (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
//
//}
//
//static bool json_add_object (gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
//
//}

// MARK: - Internals -

static void create_optional_class (void) {
    gravity_class_json = gravity_class_new_pair(NULL, GRAVITY_CLASS_JSON_NAME, NULL, 0, 0);
    gravity_class_t *json_meta = gravity_class_get_meta(gravity_class_json);
    
    //gravity_class_bind(json_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(JSON_exec));
    gravity_class_bind(json_meta, GRAVITY_JSON_STRINGIFY_NAME, NEW_CLOSURE_VALUE(JSON_stringify));
    gravity_class_bind(json_meta, GRAVITY_JSON_PARSE_NAME, NEW_CLOSURE_VALUE(JSON_parse));
    //gravity_class_bind(gravity_class_json, "begin", NEW_CLOSURE_VALUE(JSON_begin_object));
    
    SETMETA_INITED(gravity_class_json);
}

// MARK: - Commons -

bool gravity_isjson_class (gravity_class_t *c) {
    return (c == gravity_class_json);
}

const char *gravity_json_name (void) {
    return GRAVITY_CLASS_JSON_NAME;
}

void gravity_json_register (gravity_vm *vm) {
    if (!gravity_class_json) create_optional_class();
    ++refcount;
    
    if (!vm || gravity_vm_ismini(vm)) return;
    gravity_vm_setvalue(vm, GRAVITY_CLASS_JSON_NAME, VALUE_FROM_OBJECT(gravity_class_json));
}

void gravity_json_free (void) {
    if (!gravity_class_json) return;
    if (--refcount) return;
    
    gravity_class_free_core(NULL, gravity_class_get_meta(gravity_class_json));
    gravity_class_free_core(NULL, gravity_class_json);
    
    gravity_class_json = NULL;
}
