//
//  gravity_value.c
//  gravity
//
//  Created by Marco Bambini on 11/12/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include <inttypes.h>
#include "gravity_hash.h"
#include "gravity_core.h"
#include "gravity_value.h"
#include "gravity_utils.h"
#include "gravity_memory.h"
#include "gravity_macros.h"
#include "gravity_opcodes.h"
#include "gravity_vmmacros.h"

                                                // mark object visited to avoid infinite loop
#define SET_OBJECT_VISITED_FLAG(_obj, _flag)    (((gravity_object_t *)_obj)->gc.visited = _flag)

// MARK: -

static void gravity_function_special_serialize (gravity_function_t *f, const char *key, json_t *json);
static gravity_map_t *gravity_map_deserialize (gravity_vm *vm, json_value *json);

static void gravity_hash_serialize (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(table)
    json_t *json = (json_t *)data;
    
    if (VALUE_ISA_CLOSURE(value)) value = VALUE_FROM_OBJECT(VALUE_AS_CLOSURE(value)->f);

    if (VALUE_ISA_FUNCTION(value)) {
        gravity_function_t *f = VALUE_AS_FUNCTION(value);
        if (f->tag == EXEC_TYPE_SPECIAL) gravity_function_special_serialize(f, VALUE_AS_CSTRING(key), json);
        else {
            // there was an issue here due to the fact that when a subclass needs to use a $init from a superclass
            // internally it has a unique name (key) but f->identifier continue to be called $init
            // without this fix the subclass would continue to have 2 or more $init functions
            gravity_string_t *s = VALUE_AS_STRING(key);
            bool is_super_function = ((s->len > 5) && (string_casencmp(s->s, CLASS_INTERNAL_INIT_NAME, 5) == 0));
            const char *saved = f->identifier;
            if (is_super_function) f->identifier = s->s;
            gravity_function_serialize(f, json);
            if (is_super_function) f->identifier = saved;
        }
    }
    else if (VALUE_ISA_CLASS(value)) {
        gravity_class_serialize(VALUE_AS_CLASS(value), json);
    }
    else
        assert(0);
}

void gravity_hash_keyvaluefree (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(table)
    gravity_vm *vm = (gravity_vm *)data;
    gravity_value_free(vm, key);
    gravity_value_free(vm, value);
}

void gravity_hash_keyfree (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(table, value)
    gravity_vm *vm = (gravity_vm *)data;
    gravity_value_free(vm, key);
}

void gravity_hash_finteralfree (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(table, key, data)
    if (gravity_value_isobject(value)) {
        gravity_object_t *obj = VALUE_AS_OBJECT(value);
        if (OBJECT_ISA_CLOSURE(obj)) {
            gravity_closure_t *closure = (gravity_closure_t *)obj;
            if (closure->f && closure->f->tag == EXEC_TYPE_INTERNAL) gravity_function_free(NULL, closure->f);
        }
    }
}

void gravity_hash_valuefree (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data) {
    #pragma unused(table, key)
    gravity_vm *vm = (gravity_vm *)data;
    gravity_value_free(vm, value);
}

static void gravity_hash_internalsize (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data1, void *data2) {
    #pragma unused(table)
    uint32_t    *size = (uint32_t *)data1;
    gravity_vm    *vm = (gravity_vm *)data2;
    *size = gravity_value_size(vm, key);
    *size += gravity_value_size(vm, value);
}

static void gravity_hash_gray (gravity_hash_t *table, gravity_value_t key, gravity_value_t value, void *data1) {
    #pragma unused(table)
    gravity_vm *vm = (gravity_vm *)data1;
    gravity_gray_value(vm, key);
    gravity_gray_value(vm, value);
}

// MARK: -

gravity_module_t *gravity_module_new (gravity_vm *vm, const char *identifier) {
    gravity_module_t *m = (gravity_module_t *)mem_alloc(NULL, sizeof(gravity_module_t));
    assert(m);

    m->isa = gravity_class_module;
    m->identifier = string_dup(identifier);
    m->htable = gravity_hash_create(0, gravity_value_hash, gravity_value_equals, gravity_hash_keyvaluefree, (void*)vm);

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*)m);
    return m;
}

void gravity_module_free (gravity_vm *vm, gravity_module_t *m) {
    #pragma unused(vm)

    if (m->identifier) mem_free(m->identifier);
    gravity_hash_free(m->htable);
    mem_free(m);
}

uint32_t gravity_module_size (gravity_vm *vm, gravity_module_t *m) {
    SET_OBJECT_VISITED_FLAG(m, true);
    
    uint32_t hash_size = 0;
    gravity_hash_iterate2(m->htable, gravity_hash_internalsize, (void*)&hash_size, (void*)vm);
    uint32_t module_size = (sizeof(gravity_module_t)) + string_size(m->identifier) + hash_size + gravity_hash_memsize(m->htable);
    
    SET_OBJECT_VISITED_FLAG(m, false);
    return module_size;
}

void gravity_module_blacken (gravity_vm *vm, gravity_module_t *m) {
    gravity_vm_memupdate(vm, gravity_module_size(vm, m));
    gravity_hash_iterate(m->htable, gravity_hash_gray, (void*)vm);
}

// MARK: -

void gravity_class_bind (gravity_class_t *c, const char *key, gravity_value_t value) {
    if (VALUE_ISA_CLASS(value)) {
        // set has_outer when bind a class inside another class
        gravity_class_t *obj = VALUE_AS_CLASS(value);
        obj->has_outer = true;
    }
    gravity_hash_insert(c->htable, VALUE_FROM_CSTRING(NULL, key), value);
}

gravity_class_t *gravity_class_getsuper (gravity_class_t *c) {
    return c->superclass;
}

bool gravity_class_grow (gravity_class_t *c, uint32_t n) {
    if (c->ivars) mem_free(c->ivars);
    if (c->nivars + n >= MAX_IVARS) return false;
    c->nivars += n;
    c->ivars = (gravity_value_t *)mem_alloc(NULL, c->nivars * sizeof(gravity_value_t));
    for (uint32_t i=0; i<c->nivars; ++i) c->ivars[i] = VALUE_FROM_NULL;
    return true;
}

bool gravity_class_setsuper (gravity_class_t *baseclass, gravity_class_t *superclass) {
    if (!superclass) return true;
    baseclass->superclass = superclass;

    // check meta class first
    gravity_class_t *supermeta = (superclass) ? gravity_class_get_meta(superclass) : NULL;
    uint32_t n1 = (supermeta) ? supermeta->nivars : 0;
    if (n1) if (!gravity_class_grow (gravity_class_get_meta(baseclass), n1)) return false;

    // then check real class
    uint32_t n2 = (superclass) ? superclass->nivars : 0;
    if (n2) if (!gravity_class_grow (baseclass, n2)) return false;

    return true;
}

bool gravity_class_setsuper_extern (gravity_class_t *baseclass, const char *identifier) {
    if (identifier) baseclass->superlook = string_dup(identifier);
    return true;
}

gravity_class_t *gravity_class_new_single (gravity_vm *vm, const char *identifier, uint32_t nivar) {
    gravity_class_t *c = (gravity_class_t *)mem_alloc(NULL, sizeof(gravity_class_t));
    assert(c);

    c->isa = gravity_class_class;
    c->identifier = string_dup(identifier);
    c->superclass = NULL;
    c->nivars = nivar;
    c->htable = gravity_hash_create(0, gravity_value_hash, gravity_value_equals, gravity_hash_keyfree, NULL);
    if (nivar) {
        c->ivars = (gravity_value_t *)mem_alloc(NULL, nivar * sizeof(gravity_value_t));
        for (uint32_t i=0; i<nivar; ++i) c->ivars[i] = VALUE_FROM_NULL;
    }

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) c);
    return c;
}

gravity_class_t *gravity_class_new_pair (gravity_vm *vm, const char *identifier, gravity_class_t *superclass, uint32_t nivar, uint32_t nsvar) {
    // each class must have a valid identifier
    if (!identifier) return NULL;

    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s meta", identifier);
    
    // ivar count/grow is managed by gravity_class_setsuper
    gravity_class_t *meta = gravity_class_new_single(vm, buffer, nsvar);
    meta->objclass = gravity_class_object;
    gravity_class_setsuper(meta, gravity_class_class);

    gravity_class_t *c = gravity_class_new_single(vm, identifier, nivar);
    c->objclass = meta;

    // a class without a superclass in a subclass of Object
    gravity_class_setsuper(c, (superclass) ? superclass : gravity_class_object);

    return c;
}

gravity_class_t *gravity_class_get_meta (gravity_class_t *c) {
    // meta classes have objclass set to class object
    if (c->objclass == gravity_class_object) return c;
    return c->objclass;
}

bool gravity_class_is_meta (gravity_class_t *c) {
    // meta classes have objclass set to class object
    return (c->objclass == gravity_class_object);
}

bool gravity_class_is_anon (gravity_class_t *c) {
    return (string_casencmp(c->identifier, GRAVITY_VM_ANONYMOUS_PREFIX, strlen(GRAVITY_VM_ANONYMOUS_PREFIX)) == 0);
}

uint32_t gravity_class_count_ivars (gravity_class_t *c) {
    return (uint32_t)c->nivars;
}

int16_t gravity_class_add_ivar (gravity_class_t *c, const char *identifier) {
    #pragma unused(identifier)
    // TODO: add identifier in array (for easier debugging)
    ++c->nivars;
    return c->nivars-1; // its a C array so index is 0 based
}

void gravity_class_dump (gravity_class_t *c) {
    gravity_hash_dump(c->htable);
}

void gravity_class_setxdata (gravity_class_t *c, void *xdata) {
    c->xdata = xdata;
}

void gravity_class_serialize (gravity_class_t *c, json_t *json) {
    const char *label = json_get_label(json, c->identifier);
    json_begin_object(json, label);
    
    json_add_cstring(json, GRAVITY_JSON_LABELTYPE, GRAVITY_JSON_CLASS);     // MANDATORY 1st FIELD
    json_add_cstring(json, GRAVITY_JSON_LABELIDENTIFIER, c->identifier);    // MANDATORY 2nd FIELD
    
    // avoid write superclass name if it is the default Object one
    if ((c->superclass) && (c->superclass->identifier) && (strcmp(c->superclass->identifier, GRAVITY_CLASS_OBJECT_NAME) != 0)) {
        json_add_cstring(json, GRAVITY_JSON_LABELSUPER, c->superclass->identifier);
    } else if (c->superlook) {
        json_add_cstring(json, GRAVITY_JSON_LABELSUPER, c->superlook);
    }

    // get c meta class
    gravity_class_t *meta = gravity_class_get_meta(c);

    // number of instance (and static) variables
    json_add_int(json, GRAVITY_JSON_LABELNIVAR, c->nivars);
    if ((c != meta) && (meta->nivars > 0)) json_add_int(json, GRAVITY_JSON_LABELSIVAR, meta->nivars);

    // struct flag
    if (c->is_struct) json_add_bool(json, GRAVITY_JSON_LABELSTRUCT, true);

    // serialize htable
    if (c->htable) {
        gravity_hash_iterate(c->htable, gravity_hash_serialize, (void *)json);
    }

    // serialize meta class
    if (c != meta) {
        // further proceed only if it has something to be serialized
        if ((meta->htable) && (gravity_hash_count(meta->htable) > 0)) {
            json_begin_array(json, GRAVITY_JSON_LABELMETA);
            gravity_hash_iterate(meta->htable, gravity_hash_serialize, (void *)json);
            json_end_array(json);
        }
    }

    json_end_object(json);
}

gravity_class_t *gravity_class_deserialize (gravity_vm *vm, json_value *json) {
    // sanity check
    if (json->type != json_object) return NULL;
    if (json->u.object.length < 3) return NULL;

    // scan identifier
    json_value *value = json->u.object.values[1].value;
    const char *key = json->u.object.values[1].name;

    // sanity check identifier
    if (string_casencmp(key, GRAVITY_JSON_LABELIDENTIFIER, strlen(key)) != 0) return NULL;
    assert(value->type == json_string);

    // create class and meta
    gravity_class_t *c = gravity_class_new_pair(vm, value->u.string.ptr, NULL, 0, 0);
    DEBUG_DESERIALIZE("DESERIALIZE CLASS: %p %s\n", c, value->u.string.ptr);

    // get its meta class
    gravity_class_t *meta = gravity_class_get_meta(c);

    uint32_t n = json->u.object.length;
    for (uint32_t i=2; i<n; ++i) { // from 2 to skip type and identifier

        // parse values
        value = json->u.object.values[i].value;
        key = json->u.object.values[i].name;

        if (value->type != json_object) {

            // super
            if (string_casencmp(key, GRAVITY_JSON_LABELSUPER, strlen(key)) == 0) {
                // the trick here is to re-use a runtime field to store a temporary static data like superclass name
                // (only if different than the default Object one)
                if (strcmp(value->u.string.ptr, GRAVITY_CLASS_OBJECT_NAME) != 0) {
                    c->xdata = (void *)string_dup(value->u.string.ptr);
                }
                continue;
            }

            // nivar
            if (string_casencmp(key, GRAVITY_JSON_LABELNIVAR, strlen(key)) == 0) {
                gravity_class_grow(c, (uint32_t)value->u.integer);
                continue;
            }

            // sivar
            if (string_casencmp(key, GRAVITY_JSON_LABELSIVAR, strlen(key)) == 0) {
                gravity_class_grow(meta, (uint32_t)value->u.integer);
                continue;
            }

            // struct
            if (string_casencmp(key, GRAVITY_JSON_LABELSTRUCT, strlen(key)) == 0) {
                c->is_struct = true;
                continue;
            }

            // meta
            if (string_casencmp(key, GRAVITY_JSON_LABELMETA, strlen(key)) == 0) {
                uint32_t m = value->u.array.length;
                for (uint32_t j=0; j<m; ++j) {
                    json_value *r = value->u.array.values[j];
                    if (r->type != json_object) continue;
                    gravity_object_t *obj = gravity_object_deserialize(vm, r);
                    if (!obj) goto abort_load;

                    const char *identifier = obj->identifier;
                    if (OBJECT_ISA_FUNCTION(obj)) obj = (gravity_object_t *)gravity_closure_new(vm, (gravity_function_t *)obj);
                    if (obj) gravity_class_bind(meta, identifier, VALUE_FROM_OBJECT(obj));
                    else goto abort_load;
                }
                continue;
            }

            // error here
            goto abort_load;
        }

        if (value->type == json_object) {
            gravity_object_t *obj = gravity_object_deserialize(vm, value);
            if (!obj) goto abort_load;

            const char *identifier = obj->identifier;
            if (OBJECT_ISA_FUNCTION(obj)) obj = (gravity_object_t *)gravity_closure_new(vm, (gravity_function_t *)obj);
            gravity_class_bind(c, identifier, VALUE_FROM_OBJECT(obj));
        }
    }

    return c;

abort_load:
    // do not free c here because it is already garbage collected
    return NULL;
}

static void gravity_class_free_internal (gravity_vm *vm, gravity_class_t *c, bool skip_base) {
    if (skip_base && (gravity_iscore_class(c) || gravity_isopt_class(c))) return;

    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)c, true));

    // check if bridged data needs to be freed too
    if (c->xdata && vm) {
        gravity_delegate_t *delegate = gravity_vm_delegate(vm);
        if (delegate->bridge_free) delegate->bridge_free(vm, (gravity_object_t *)c);
    }

    if (c->identifier) mem_free((void *)c->identifier);
    if (c->superlook) mem_free((void *)c->superlook);
    
    if (!skip_base) {
        // base classes have functions not registered inside VM so manually free all of them
        gravity_hash_iterate(c->htable, gravity_hash_finteralfree, NULL);
        gravity_hash_iterate(c->htable, gravity_hash_valuefree, NULL);
    }

    gravity_hash_free(c->htable);
    if (c->ivars) mem_free((void *)c->ivars);
    mem_free((void *)c);
}

void gravity_class_free_core (gravity_vm *vm, gravity_class_t *c) {
    gravity_class_free_internal(vm, c, false);
}

void gravity_class_free (gravity_vm *vm, gravity_class_t *c) {
    gravity_class_free_internal(vm, c, true);
}

inline gravity_object_t *gravity_class_lookup (gravity_class_t *c, gravity_value_t key) {
    while (c) {
        gravity_value_t *v = gravity_hash_lookup(c->htable, key);
        if (v) return (gravity_object_t *)v->p;
        c = c->superclass;
    }
    return NULL;
}

gravity_class_t *gravity_class_lookup_class_identifier (gravity_class_t *c, const char *identifier) {
    while (c) {
        if (string_cmp(c->identifier, identifier) == 0) return c;
        c = c->superclass;
    }
    return NULL;
}

inline gravity_closure_t *gravity_class_lookup_closure (gravity_class_t *c, gravity_value_t key) {
    gravity_object_t *obj = gravity_class_lookup(c, key);
    if (obj && OBJECT_ISA_CLOSURE(obj)) return (gravity_closure_t *)obj;
    return NULL;
}

inline gravity_closure_t *gravity_class_lookup_constructor (gravity_class_t *c, uint32_t nparams) {
    if (c->xdata) {
        // bridged class so check for special $initN function
        if (nparams == 0) {
            STATICVALUE_FROM_STRING(key, CLASS_INTERNAL_INIT_NAME, strlen(CLASS_INTERNAL_INIT_NAME));
            return (gravity_closure_t *)gravity_class_lookup(c, key);
        }

        // for bridged classed (which can have more than one init constructor like in objc) the convention is
        // to map each bridged init with a special $initN function (where N>0 is num params)
        char name[256]; snprintf(name, sizeof(name), "%s%d", CLASS_INTERNAL_INIT_NAME, nparams);
        STATICVALUE_FROM_STRING(key, name, strlen(name));
        return (gravity_closure_t *)gravity_class_lookup(c, key);
    }

    // for non bridge classes just check for constructor
    STATICVALUE_FROM_STRING(key, CLASS_CONSTRUCTOR_NAME, strlen(CLASS_CONSTRUCTOR_NAME));
    return (gravity_closure_t *)gravity_class_lookup(c, key);
}

uint32_t gravity_class_size (gravity_vm *vm, gravity_class_t *c) {
    SET_OBJECT_VISITED_FLAG(c, true);
    
    uint32_t class_size = sizeof(gravity_class_t) + (c->nivars * sizeof(gravity_value_t)) + string_size(c->identifier);

    uint32_t hash_size = 0;
    gravity_hash_iterate2(c->htable, gravity_hash_internalsize, (void *)&hash_size, (void *)vm);
    hash_size += gravity_hash_memsize(c->htable);

    gravity_delegate_t *delegate = gravity_vm_delegate(vm);
    if (c->xdata && delegate->bridge_size)
        class_size += delegate->bridge_size(vm, c->xdata);

    SET_OBJECT_VISITED_FLAG(c, false);
    return class_size;
}

void gravity_class_blacken (gravity_vm *vm, gravity_class_t *c) {
    gravity_vm_memupdate(vm, gravity_class_size(vm, c));

    // metaclass
    gravity_gray_object(vm, (gravity_object_t *)c->objclass);

    // superclass
    gravity_gray_object(vm, (gravity_object_t *)c->superclass);

    // internals
    gravity_hash_iterate(c->htable, gravity_hash_gray, (void *)vm);

    // ivars
    for (uint32_t i=0; i<c->nivars; ++i) {
        gravity_gray_value(vm, c->ivars[i]);
    }
}

// MARK: -

gravity_function_t *gravity_function_new (gravity_vm *vm, const char *identifier, uint16_t nparams, uint16_t nlocals, uint16_t ntemps, void *code) {
    gravity_function_t *f = (gravity_function_t *)mem_alloc(NULL, sizeof(gravity_function_t));
    assert(f);

    f->isa = gravity_class_function;
    f->identifier = (identifier) ? string_dup(identifier) : NULL;
    f->tag = EXEC_TYPE_NATIVE;
    f->nparams = nparams;
    f->nlocals = nlocals;
    f->ntemps = ntemps;
    f->nupvalues = 0;

    // only available in EXEC_TYPE_NATIVE case
    // code is != NULL when EXEC_TYPE_NATIVE
    if (code != NULL) {
        f->useargs = false;
        f->bytecode = (uint32_t *)code;
        marray_init(f->cpool);
        marray_init(f->pvalue);
        marray_init(f->pname);
    }

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*)f);
    return f;
}

gravity_function_t *gravity_function_new_internal (gravity_vm *vm, const char *identifier, gravity_c_internal exec, uint16_t nparams) {
    gravity_function_t *f = gravity_function_new(vm, identifier, nparams, 0, 0, NULL);
    f->tag = EXEC_TYPE_INTERNAL;
    f->internal = exec;
    return f;
}

gravity_function_t *gravity_function_new_special (gravity_vm *vm, const char *identifier, uint16_t index, void *getter, void *setter) {
    gravity_function_t *f = gravity_function_new(vm, identifier, 0, 0, 0, NULL);
    f->tag = EXEC_TYPE_SPECIAL;
    f->index = index;
    f->special[0] = getter;
    f->special[1] = setter;
    return f;
}

gravity_function_t *gravity_function_new_bridged (gravity_vm *vm, const char *identifier, void *xdata) {
    gravity_function_t *f = gravity_function_new(vm, identifier, 0, 0, 0, NULL);
    f->tag = EXEC_TYPE_BRIDGED;
    f->xdata = xdata;
    return f;
}

uint16_t gravity_function_cpool_add (gravity_vm *vm, gravity_function_t *f, gravity_value_t v) {
    assert(f->tag == EXEC_TYPE_NATIVE);

    size_t n = marray_size(f->cpool);
    for (size_t i=0; i<n; i++) {
        gravity_value_t v2 = marray_get(f->cpool, i);
        if (gravity_value_equals(v,v2)) {
            gravity_value_free(NULL, v);
            return (uint16_t)i;
        }
    }

    // vm is required here because I cannot know in advance if v is already in the pool or not
    // and value object v must be added to the VM only once
    if ((vm) && (gravity_value_isobject(v))) gravity_vm_transfer(vm, VALUE_AS_OBJECT(v));

    marray_push(gravity_value_t, f->cpool, v);
    return (uint16_t)marray_size(f->cpool)-1;
}

gravity_value_t gravity_function_cpool_get (gravity_function_t *f, uint16_t i) {
    assert(f->tag == EXEC_TYPE_NATIVE);
    return marray_get(f->cpool, i);
}

gravity_list_t *gravity_function_params_get (gravity_vm *vm, gravity_function_t *f) {
    #pragma unused(vm)
    gravity_list_t *list = NULL;
    
    if (f->tag == EXEC_TYPE_NATIVE) {
        // written by user in Gravity
    } else if (f->tag == EXEC_TYPE_BRIDGED && f->xdata) {
        // ask bridge
    } else if (f->tag == EXEC_TYPE_INTERNAL) {
        // native C function
    }
    
    return list;
}

void gravity_function_setxdata (gravity_function_t *f, void *xdata) {
    f->xdata = xdata;
}

static void gravity_function_array_serialize (gravity_function_t *f, json_t *json, gravity_value_r r) {
    assert(f->tag == EXEC_TYPE_NATIVE);
    size_t n = marray_size(r);

    for (size_t i=0; i<n; i++) {
        gravity_value_t v = marray_get(r, i);
        gravity_value_serialize(NULL, v, json);
    }
}

static void gravity_function_array_dump (gravity_function_t *f, gravity_value_r r) {
    assert(f->tag == EXEC_TYPE_NATIVE);
    size_t n = marray_size(r);

    for (size_t i=0; i<n; i++) {
        gravity_value_t v = marray_get(r, i);

        if (VALUE_ISA_NULL(v)) {
            printf("%05zu\tNULL\n", i);
            continue;
        }
        
        if (VALUE_ISA_UNDEFINED(v)) {
            printf("%05zu\tUNDEFINED\n", i);
            continue;
        }
        
        if (VALUE_ISA_BOOL(v)) {
            printf("%05zu\tBOOL: %d\n", i, (v.n == 0) ? 0 : 1);
            continue;
        }
        
        if (VALUE_ISA_INT(v)) {
            printf("%05zu\tINT: %" PRId64 "\n", i, (int64_t)v.n);
            continue;
        }
        
        if (VALUE_ISA_FLOAT(v)) {
            printf("%05zu\tFLOAT: %g\n", i, (double)v.f);
            continue;
        }
        
        if (VALUE_ISA_FUNCTION(v)) {
            gravity_function_t *vf = VALUE_AS_FUNCTION(v);
            printf("%05zu\tFUNC: %s\n", i, (vf->identifier) ? vf->identifier : "$anon");
            continue;
        }
        
        if (VALUE_ISA_CLASS(v)) {
            gravity_class_t *c = VALUE_AS_CLASS(v);
            printf("%05zu\tCLASS: %s\n", i, (c->identifier) ? c->identifier: "$anon");
            continue;
            
        }
        
        if (VALUE_ISA_STRING(v)) {
            printf("%05zu\tSTRING: %s\n", i, VALUE_AS_CSTRING(v));
            continue;
        }
        
        if (VALUE_ISA_LIST(v)) {
            gravity_list_t *value = VALUE_AS_LIST(v);
            size_t count = marray_size(value->array);
            printf("%05zu\tLIST: %zu items\n", i, count);
            continue;
            
        }
        
        if (VALUE_ISA_MAP(v)) {
            gravity_map_t *map = VALUE_AS_MAP(v);
            printf("%05zu\tMAP: %u items\n", i, gravity_hash_count(map->hash));
            continue;
        }
        
        // should never reach this point
        assert(0);
    }
}

static void gravity_function_bytecode_serialize (gravity_function_t *f, json_t *json) {
    if (!f->bytecode || !f->ninsts) {
        json_add_null(json, GRAVITY_JSON_LABELBYTECODE);
        return;
    }

    // bytecode
    uint32_t ninst = f->ninsts;
    uint32_t length = ninst * 2 * sizeof(uint32_t);
    uint8_t *hexchar = (uint8_t*) mem_alloc(NULL, sizeof(uint8_t) * length);

    for (uint32_t k=0, i=0; i < ninst; ++i) {
        uint32_t value = f->bytecode[i];

        for (int32_t j=2*sizeof(value)-1; j>=0; --j) {
            uint8_t c = "0123456789ABCDEF"[((value >> (j*4)) & 0xF)];
            hexchar[k++] = c;
        }
    }

    json_add_string(json, GRAVITY_JSON_LABELBYTECODE, (const char *)hexchar, length);
    mem_free(hexchar);
    
    // debug lineno
    if (!f->lineno) return;
    
    ninst = f->ninsts;
    length = ninst * 2 * sizeof(uint32_t);
    hexchar = (uint8_t*) mem_alloc(NULL, sizeof(uint8_t) * length);
    
    for (uint32_t k=0, i=0; i < ninst; ++i) {
        uint32_t value = f->lineno[i];
        
        for (int32_t j=2*sizeof(value)-1; j>=0; --j) {
            uint8_t c = "0123456789ABCDEF"[((value >> (j*4)) & 0xF)];
            hexchar[k++] = c;
        }
    }
    
    json_add_string(json, GRAVITY_JSON_LABELLINENO, (const char *)hexchar, length);
    mem_free(hexchar);
}

uint32_t *gravity_bytecode_deserialize (const char *buffer, size_t len, uint32_t *n) {
    uint32_t ninst = (uint32_t)len / 8;
    uint32_t *bytecode = (uint32_t *)mem_alloc(NULL, sizeof(uint32_t) * (ninst + 1));    // +1 to get a 0 terminated bytecode (0 is opcode RET0)

    for (uint32_t j=0; j<ninst; ++j) {
        register uint32_t v = 0;

        for (uint32_t i=(j*8); i<=(j*8)+7; ++i) {
            // I was using a conversion code from
            // https://code.google.com/p/yara-project/source/browse/trunk/libyara/xtoi.c?r=150
            // but it caused issues under ARM processor so I decided to switch to an easier to read/maintain code
            // http://codereview.stackexchange.com/questions/42976/hexadecimal-to-integer-conversion-function

            // no needs to have also the case:
            // if (c >= 'a' && c <= 'f') {
            //        c = c - 'a' + 10;
            // }
            // because bytecode is always uppercase
            register uint32_t c = buffer[i];

            if (c >= 'A' && c <= 'F') {
                c = c - 'A' + 10;
            } else if (c >= '0' && c <= '9') {
                c -= '0';
            } else goto abort_conversion;

            v = v << 4 | c;

        }

        bytecode[j] = v;
    }

    *n = ninst;
    return bytecode;

abort_conversion:
    *n = 0;
    if (bytecode) mem_free(bytecode);
    return NULL;
}

void gravity_function_dump (gravity_function_t *f, code_dump_function codef) {
    printf("Function: %s\n", (f->identifier) ? f->identifier : "$anon");
    printf("Params:%d Locals:%d Temp:%d Upvalues:%d Tag:%d xdata:%p\n", f->nparams, f->nlocals, f->ntemps, f->nupvalues, f->tag, f->xdata);

    if (f->tag == EXEC_TYPE_NATIVE) {
        if (marray_size(f->cpool)) printf("======= CONST POOL =======\n");
        gravity_function_array_dump(f, f->cpool);
        
        if (marray_size(f->pname)) printf("======= PARAM NAMES =======\n");
        gravity_function_array_dump(f, f->pname);
        
        if (marray_size(f->pvalue)) printf("======= PARAM VALUES =======\n");
        gravity_function_array_dump(f, f->pvalue);

        printf("======= BYTECODE =======\n");
        if ((f->bytecode) && (codef)) codef(f->bytecode);
    }

    printf("\n");
}

void gravity_function_special_serialize (gravity_function_t *f, const char *key, json_t *json) {
    const char *label = json_get_label(json, key);
    json_begin_object(json, label);

    json_add_cstring(json, GRAVITY_JSON_LABELTYPE, GRAVITY_JSON_FUNCTION);    // MANDATORY 1st FIELD
    json_add_cstring(json, GRAVITY_JSON_LABELIDENTIFIER, key);                // MANDATORY 2nd FIELD
    json_add_int(json, GRAVITY_JSON_LABELTAG, f->tag);

    // common fields
    json_add_int(json, GRAVITY_JSON_LABELNPARAM, f->nparams);
    json_add_bool(json, GRAVITY_JSON_LABELARGS, f->useargs);
    json_add_int(json, GRAVITY_JSON_LABELINDEX, f->index);

    if (f->special[0]) {
        gravity_function_t *f2 = (gravity_function_t*)f->special[0];
        f2->identifier = GRAVITY_JSON_GETTER;
        gravity_function_serialize(f2, json);
        f2->identifier = NULL;
    }
    if (f->special[1]) {
        gravity_function_t *f2 = (gravity_function_t*)f->special[1];
        f2->identifier = GRAVITY_JSON_SETTER;
        gravity_function_serialize(f2, json);
        f2->identifier = NULL;
    }

    json_end_object(json);
}

void gravity_function_serialize (gravity_function_t *f, json_t *json) {
    // special functions need a special serialization
    if (f->tag == EXEC_TYPE_SPECIAL) {
        gravity_function_special_serialize(f, f->identifier, json);
        return;
    }

    // compute identifier (cannot be NULL)
    const char *identifier = f->identifier;
    char temp[256];
    if (!identifier) {snprintf(temp, sizeof(temp), "$anon_%p", f); identifier = temp;}

    const char *label = json_get_label(json, identifier);
    json_begin_object(json, label);
    
    json_add_cstring(json, GRAVITY_JSON_LABELTYPE, GRAVITY_JSON_FUNCTION);  // MANDATORY 1st FIELD
    json_add_cstring(json, GRAVITY_JSON_LABELIDENTIFIER, identifier);       // MANDATORY 2nd FIELD (not for getter/setter)
    json_add_int(json, GRAVITY_JSON_LABELTAG, f->tag);

    // common fields
    json_add_int(json, GRAVITY_JSON_LABELNPARAM, f->nparams);
    json_add_bool(json, GRAVITY_JSON_LABELARGS, f->useargs);

    if (f->tag == EXEC_TYPE_NATIVE) {
        // native only fields
        json_add_int(json, GRAVITY_JSON_LABELNLOCAL, f->nlocals);
        json_add_int(json, GRAVITY_JSON_LABELNTEMP, f->ntemps);
        json_add_int(json, GRAVITY_JSON_LABELNUPV, f->nupvalues);
        json_add_double(json, GRAVITY_JSON_LABELPURITY, f->purity);

        // bytecode
        gravity_function_bytecode_serialize(f, json);

        // constant pool
        json_begin_array(json, GRAVITY_JSON_LABELPOOL);
        gravity_function_array_serialize(f, json, f->cpool);
        json_end_array(json);
        
        // default values (if any)
        if (marray_size(f->pvalue)) {
            json_begin_array(json, GRAVITY_JSON_LABELPVALUES);
            gravity_function_array_serialize(f, json, f->pvalue);
        json_end_array(json);
    }

        // arg names (if any)
        if (marray_size(f->pname)) {
            json_begin_array(json, GRAVITY_JSON_LABELPNAMES);
            gravity_function_array_serialize(f, json, f->pname);
            json_end_array(json);
        }
    }
    
    json_end_object(json);
}

gravity_function_t *gravity_function_deserialize (gravity_vm *vm, json_value *json) {
    gravity_function_t *f = gravity_function_new(vm, NULL, 0, 0, 0, NULL);

    DEBUG_DESERIALIZE("DESERIALIZE FUNCTION: %p\n", f);

    bool identifier_parsed = false;
    bool getter_parsed = false;
    bool setter_parsed = false;
    bool index_parsed = false;
    bool bytecode_parsed = false;
    bool cpool_parsed = false;
    bool nparams_parsed = false;
    bool nlocals_parsed = false;
    bool ntemp_parsed = false;
    bool nupvalues_parsed = false;
    bool nargs_parsed = false;
    bool tag_parsed = false;

    uint32_t n = json->u.object.length;
    for (uint32_t i=1; i<n; ++i) { // from 1 to skip type
        const char *label = json->u.object.values[i].name;
        json_value *value = json->u.object.values[i].value;
        size_t label_size = strlen(label);

        // identifier
        if (string_casencmp(label, GRAVITY_JSON_LABELIDENTIFIER, label_size) == 0) {
            if (value->type != json_string) goto abort_load;
            if (identifier_parsed) goto abort_load;
            if (strncmp(value->u.string.ptr, "$anon", 5) != 0) {
                f->identifier = string_dup(value->u.string.ptr);
                DEBUG_DESERIALIZE("IDENTIFIER: %s\n", value->u.string.ptr);
            }
            identifier_parsed = true;
            continue;
        }

        // tag
        if (string_casencmp(label, GRAVITY_JSON_LABELTAG, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (tag_parsed) goto abort_load;
            f->tag = (uint16_t)value->u.integer;
            tag_parsed = true;
            continue;
        }

        // index (only in special functions)
        if (string_casencmp(label, GRAVITY_JSON_LABELINDEX, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (f->tag != EXEC_TYPE_SPECIAL) goto abort_load;
            if (index_parsed) goto abort_load;
            f->index = (uint16_t)value->u.integer;
            index_parsed = true;
            continue;
        }

        // getter (only in special functions)
        if (string_casencmp(label, GRAVITY_JSON_GETTER, strlen(GRAVITY_JSON_GETTER)) == 0) {
            if (f->tag != EXEC_TYPE_SPECIAL) goto abort_load;
            if (getter_parsed) goto abort_load;
            gravity_function_t *getter = gravity_function_deserialize(vm, value);
            if (!getter) goto abort_load;
            f->special[0] = gravity_closure_new(vm, getter);
            getter_parsed = true;
            continue;
        }

        // setter (only in special functions)
        if (string_casencmp(label, GRAVITY_JSON_SETTER, strlen(GRAVITY_JSON_SETTER)) == 0) {
            if (f->tag != EXEC_TYPE_SPECIAL) goto abort_load;
            if (setter_parsed) goto abort_load;
            gravity_function_t *setter = gravity_function_deserialize(vm, value);
            if (!setter) goto abort_load;
            f->special[1] = gravity_closure_new(vm, setter);
            setter_parsed = true;
            continue;
        }

        // nparams
        if (string_casencmp(label, GRAVITY_JSON_LABELNPARAM, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (nparams_parsed) goto abort_load;
            f->nparams = (uint16_t)value->u.integer;
            nparams_parsed = true;
            continue;
        }

        // nlocals
        if (string_casencmp(label, GRAVITY_JSON_LABELNLOCAL, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (nlocals_parsed) goto abort_load;
            f->nlocals = (uint16_t)value->u.integer;
            nlocals_parsed = true;
            continue;
        }

        // ntemps
        if (string_casencmp(label, GRAVITY_JSON_LABELNTEMP, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (ntemp_parsed) goto abort_load;
            f->ntemps = (uint16_t)value->u.integer;
            ntemp_parsed = true;
            continue;
        }

        // nupvalues
        if (string_casencmp(label, GRAVITY_JSON_LABELNUPV, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (nupvalues_parsed) goto abort_load;
            f->nupvalues = (uint16_t)value->u.integer;
            nupvalues_parsed = true;
            continue;
        }

        // args
        if (string_casencmp(label, GRAVITY_JSON_LABELARGS, label_size) == 0) {
            if (value->type != json_boolean) goto abort_load;
            if (nargs_parsed) goto abort_load;
            f->useargs = (bool)value->u.boolean;
            nargs_parsed = true;
            continue;
        }

        // bytecode
        if (string_casencmp(label, GRAVITY_JSON_LABELBYTECODE, label_size) == 0) {
            if (bytecode_parsed) goto abort_load;
            if (value->type == json_null) {
                // if function is empty then just one RET0 implicit bytecode instruction
                f->ninsts = 0;
                f->bytecode = (uint32_t *)mem_alloc(NULL, sizeof(uint32_t) * (f->ninsts + 1));
            } else {
                if (value->type != json_string) goto abort_load;
                if (f->tag != EXEC_TYPE_NATIVE) goto abort_load;
                f->bytecode = gravity_bytecode_deserialize(value->u.string.ptr, value->u.string.length, &f->ninsts);
            }
            bytecode_parsed = true;
            continue;
        }

        // lineno debug info
        if (string_casencmp(label, GRAVITY_JSON_LABELLINENO, label_size) == 0) {
            if (value->type == json_string) f->lineno = gravity_bytecode_deserialize(value->u.string.ptr, value->u.string.length, &f->ninsts);
        }
        
        // arguments names
        if (string_casencmp(label, GRAVITY_JSON_LABELPNAMES, label_size) == 0) {
            if (value->type != json_array) goto abort_load;
            if (f->tag != EXEC_TYPE_NATIVE) goto abort_load;
            uint32_t m = value->u.array.length;
            for (uint32_t j=0; j<m; ++j) {
                json_value *r = value->u.array.values[j];
                if (r->type != json_string) goto abort_load;
                marray_push(gravity_value_t, f->pname, VALUE_FROM_STRING(NULL, r->u.string.ptr, r->u.string.length));
            }
        }
        
        // arguments default values
        if (string_casencmp(label, GRAVITY_JSON_LABELPVALUES, label_size) == 0) {
            if (value->type != json_array) goto abort_load;
            if (f->tag != EXEC_TYPE_NATIVE) goto abort_load;
            
            uint32_t m = value->u.array.length;
            for (uint32_t j=0; j<m; ++j) {
                json_value *r = value->u.array.values[j];
                switch (r->type) {
                    case json_integer:
                        marray_push(gravity_value_t, f->pvalue, VALUE_FROM_INT((gravity_int_t)r->u.integer));
                        break;
                        
                    case json_double:
                        marray_push(gravity_value_t, f->pvalue, VALUE_FROM_FLOAT((gravity_float_t)r->u.dbl));
                        break;
                        
                    case json_boolean:
                        marray_push(gravity_value_t, f->pvalue, VALUE_FROM_BOOL(r->u.boolean));
                        break;
                        
                    case json_string:
                        marray_push(gravity_value_t, f->pvalue, VALUE_FROM_STRING(NULL, r->u.string.ptr, r->u.string.length));
                        break;
                        
                    case json_object:
                        marray_push(gravity_value_t, f->pvalue, VALUE_FROM_UNDEFINED);
                        break;
                        
                    case json_null:
                        marray_push(gravity_value_t, f->pvalue, VALUE_FROM_NULL);
                        break;
                        
                    case json_none:
                    case json_array:
                        marray_push(gravity_value_t, f->pvalue, VALUE_FROM_NULL);
                        break;
                }
            }
        }
        
        // cpool
        if (string_casencmp(label, GRAVITY_JSON_LABELPOOL, label_size) == 0) {
            if (value->type != json_array) goto abort_load;
            if (f->tag != EXEC_TYPE_NATIVE) goto abort_load;
            if (cpool_parsed) goto abort_load;
            cpool_parsed = true;

            uint32_t m = value->u.array.length;
            for (uint32_t j=0; j<m; ++j) {
                json_value *r = value->u.array.values[j];
                switch (r->type) {
                    case json_integer:
                        gravity_function_cpool_add(NULL, f, VALUE_FROM_INT((gravity_int_t)r->u.integer));
                        break;

                    case json_double:
                        gravity_function_cpool_add(NULL, f, VALUE_FROM_FLOAT((gravity_float_t)r->u.dbl));
                        break;

                    case json_boolean:
                        gravity_function_cpool_add(NULL, f, VALUE_FROM_BOOL(r->u.boolean));
                        break;

                    case json_string:
                        gravity_function_cpool_add(vm, f, VALUE_FROM_STRING(NULL, r->u.string.ptr, r->u.string.length));
                        break;

                    case json_object: {
                        gravity_object_t *obj = gravity_object_deserialize(vm, r);
                        if (!obj) goto abort_load;
                        gravity_function_cpool_add(NULL, f, VALUE_FROM_OBJECT(obj));
                        break;
                    }

                    case json_array: {
                        uint32_t count = r->u.array.length;
                        gravity_list_t *list = gravity_list_new (NULL, count);
                        if (!list) continue;

                        for (uint32_t k=0; k<count; ++k) {
                            json_value *jsonv = r->u.array.values[k];
                            gravity_value_t v;

                            // only literals allowed here
                            switch (jsonv->type) {
                                case json_integer: v = VALUE_FROM_INT((gravity_int_t)jsonv->u.integer); break;
                                case json_double: v = VALUE_FROM_FLOAT((gravity_float_t)jsonv->u.dbl); break;
                                case json_boolean: v = VALUE_FROM_BOOL(jsonv->u.boolean); break;
                                case json_string: v = VALUE_FROM_STRING(vm, jsonv->u.string.ptr, jsonv->u.string.length); break;
                                default: goto abort_load;
                            }

                            marray_push(gravity_value_t, list->array, v);
                        }
                        gravity_function_cpool_add(vm, f, VALUE_FROM_OBJECT(list));
                        break;
                    }

                    case json_none:
                    case json_null:
                        gravity_function_cpool_add(NULL, f, VALUE_FROM_NULL);
                        break;
                }
            }
        }
    }

    return f;

abort_load:
    // do not free f here because it is already garbage collected
    return NULL;
}

void gravity_function_free (gravity_vm *vm, gravity_function_t *f) {
    if (!f) return;

    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)f, true));

    // check if bridged data needs to be freed too
    if (f->xdata && vm) {
        gravity_delegate_t *delegate = gravity_vm_delegate(vm);
        if (delegate->bridge_free) delegate->bridge_free(vm, (gravity_object_t *)f);
    }

    if (f->identifier) mem_free((void *)f->identifier);
    if (f->tag == EXEC_TYPE_NATIVE) {
        if (f->bytecode) mem_free((void *)f->bytecode);
        if (f->lineno) mem_free((void *)f->lineno);
        
        // FREE EACH DEFAULT value
        size_t n = marray_size(f->pvalue);
        for (size_t i=0; i<n; i++) {
            gravity_value_t v = marray_get(f->pvalue, i);
            gravity_value_free(NULL, v);
        }
        marray_destroy(f->pvalue);
        
        // FREE EACH PARAM name
        n = marray_size(f->pname);
        for (size_t i=0; i<n; i++) {
            gravity_value_t v = marray_get(f->pname, i);
            gravity_value_free(NULL, v);
        }
        marray_destroy(f->pname);
        
        // DO NOT FREE EACH INDIVIDUAL CPOOL ITEM HERE
        marray_destroy(f->cpool);
    }
    mem_free((void *)f);
}

uint32_t gravity_function_size (gravity_vm *vm, gravity_function_t *f) {
    SET_OBJECT_VISITED_FLAG(f, true);
    
    uint32_t func_size = sizeof(gravity_function_t) + string_size(f->identifier);

    if (f->tag == EXEC_TYPE_NATIVE) {
        if (f->bytecode) func_size += f->ninsts * sizeof(uint32_t);
        // cpool size
        size_t n = marray_size(f->cpool);
        for (size_t i=0; i<n; i++) {
            gravity_value_t v = marray_get(f->cpool, i);
            func_size += gravity_value_size(vm, v);
        }
    } else if (f->tag == EXEC_TYPE_SPECIAL) {
        if (f->special[0]) func_size += gravity_closure_size(vm, (gravity_closure_t *)f->special[0]);
        if ((f->special[1]) && (f->special[0] != f->special[1])) func_size += gravity_closure_size(vm, (gravity_closure_t *)f->special[1]);
    } else if (f->tag == EXEC_TYPE_BRIDGED) {
        gravity_delegate_t *delegate = gravity_vm_delegate(vm);
        if (f->xdata && delegate->bridge_size)
            func_size += delegate->bridge_size(vm, f->xdata);
    }

    SET_OBJECT_VISITED_FLAG(f, false);
    return func_size;
}

void gravity_function_blacken (gravity_vm *vm, gravity_function_t *f) {
    gravity_vm_memupdate(vm, gravity_function_size(vm, f));

    if (f->tag == EXEC_TYPE_SPECIAL) {
        if (f->special[0]) gravity_gray_object(vm, (gravity_object_t *)f->special[0]);
        if (f->special[1]) gravity_gray_object(vm, (gravity_object_t *)f->special[1]);
    }

    if (f->tag == EXEC_TYPE_NATIVE) {
        // constant pool
        size_t n = marray_size(f->cpool);
        for (size_t i=0; i<n; i++) {
            gravity_value_t v = marray_get(f->cpool, i);
            gravity_gray_value(vm, v);
        }
    }
}

// MARK: -

gravity_closure_t *gravity_closure_new (gravity_vm *vm, gravity_function_t *f) {
    gravity_closure_t *closure = (gravity_closure_t *)mem_alloc(NULL, sizeof(gravity_closure_t));
    assert(closure);

    closure->isa = gravity_class_closure;
    closure->vm = vm;
    closure->f = f;

    // allocate upvalue array (+1 so I can simplify the iterator without the needs to access closure->f->nupvalues)
    uint16_t nupvalues = (f) ? f->nupvalues : 0;
    closure->upvalue = (nupvalues) ? (gravity_upvalue_t **)mem_alloc(NULL, sizeof(gravity_upvalue_t*) * (f->nupvalues + 1)) : NULL;

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*)closure);
    return closure;
}

void gravity_closure_free (gravity_vm *vm, gravity_closure_t *closure) {
    #pragma unused(vm)
    if (closure->refcount > 0) return;
    
    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)closure, true));
    if (closure->upvalue) mem_free(closure->upvalue);
    mem_free(closure);
}

uint32_t gravity_closure_size (gravity_vm *vm, gravity_closure_t *closure) {
    #pragma unused(vm)
    SET_OBJECT_VISITED_FLAG(closure, true);

    uint32_t closure_size = sizeof(gravity_closure_t);
    gravity_upvalue_t **upvalue = closure->upvalue;
    while (upvalue && upvalue[0]) {
        closure_size += sizeof(gravity_upvalue_t*);
        ++upvalue;
    }
    
    SET_OBJECT_VISITED_FLAG(closure, false);
    return closure_size;
}

void gravity_closure_inc_refcount (gravity_vm *vm, gravity_closure_t *closure) {
    if (closure->refcount == 0) gravity_gc_temppush(vm, (gravity_object_t *)closure);
    ++closure->refcount;
}

void gravity_closure_dec_refcount (gravity_vm *vm, gravity_closure_t *closure) {
    if (closure->refcount == 1) gravity_gc_tempnull(vm, (gravity_object_t *)closure);
    if (closure->refcount >= 1) --closure->refcount;
}

void gravity_closure_blacken (gravity_vm *vm, gravity_closure_t *closure) {
    gravity_vm_memupdate(vm, gravity_closure_size(vm, closure));

    // mark function
    gravity_gray_object(vm, (gravity_object_t*)closure->f);

    // mark each upvalue
    gravity_upvalue_t **upvalue = closure->upvalue;
    while (upvalue && upvalue[0]) {
        gravity_gray_object(vm, (gravity_object_t*)upvalue[0]);
        ++upvalue;
    }
    
    // mark context (if any)
    if (closure->context) gravity_gray_object(vm, closure->context);
}

// MARK: -

gravity_upvalue_t *gravity_upvalue_new (gravity_vm *vm, gravity_value_t *value) {
    gravity_upvalue_t *upvalue = (gravity_upvalue_t *)mem_alloc(NULL, sizeof(gravity_upvalue_t));

    upvalue->isa = gravity_class_upvalue;
    upvalue->value = value;
    upvalue->closed = VALUE_FROM_NULL;
    upvalue->next = NULL;

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) upvalue);
    return upvalue;
}

void gravity_upvalue_free(gravity_vm *vm, gravity_upvalue_t *upvalue) {
    #pragma unused(vm)
    
    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)upvalue, true));
    mem_free(upvalue);
}

uint32_t gravity_upvalue_size (gravity_vm *vm, gravity_upvalue_t *upvalue) {
    #pragma unused(vm, upvalue)
    
    SET_OBJECT_VISITED_FLAG(upvalue, true);
    uint32_t upvalue_size = sizeof(gravity_upvalue_t);
    SET_OBJECT_VISITED_FLAG(upvalue, false);
    
    return upvalue_size;
}

void gravity_upvalue_blacken (gravity_vm *vm, gravity_upvalue_t *upvalue) {
    gravity_vm_memupdate(vm, gravity_upvalue_size(vm, upvalue));
    // gray both closed and still opened values
    gravity_gray_value(vm, *upvalue->value);
    gravity_gray_value(vm, upvalue->closed);
}

// MARK: -

gravity_fiber_t *gravity_fiber_new (gravity_vm *vm, gravity_closure_t *closure, uint32_t nstack, uint32_t nframes) {
    gravity_fiber_t *fiber = (gravity_fiber_t *)mem_alloc(NULL, sizeof(gravity_fiber_t));
    assert(fiber);

    fiber->isa = gravity_class_fiber;
    fiber->caller = NULL;
    fiber->result = VALUE_FROM_NULL;

    if (nstack < DEFAULT_MINSTACK_SIZE) nstack = DEFAULT_MINSTACK_SIZE;
    fiber->stack = (gravity_value_t *)mem_alloc(NULL, sizeof(gravity_value_t) * nstack);
    fiber->stacktop = fiber->stack;
    fiber->stackalloc = nstack;

    if (nframes < DEFAULT_MINCFRAME_SIZE) nframes = DEFAULT_MINCFRAME_SIZE;
    fiber->frames = (gravity_callframe_t *)mem_alloc(NULL, sizeof(gravity_callframe_t) * nframes);
    fiber->framesalloc = nframes;
    fiber->nframes = 1;

    fiber->upvalues = NULL;

    gravity_callframe_t *frame = &fiber->frames[0];
    if (closure) {
        frame->closure = closure;
        frame->ip = (closure->f->tag == EXEC_TYPE_NATIVE) ? closure->f->bytecode : NULL;
    }
    frame->dest = 0;
    frame->stackstart = fiber->stack;

    // replace self with fiber instance
    frame->stackstart[0] = VALUE_FROM_OBJECT(fiber);
    
    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) fiber);
    return fiber;
}

void gravity_fiber_free (gravity_vm *vm, gravity_fiber_t *fiber) {
    #pragma unused(vm)

    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)fiber, true));
    if (fiber->error) mem_free(fiber->error);
    mem_free(fiber->stack);
    mem_free(fiber->frames);
    mem_free(fiber);
}

void gravity_fiber_reassign (gravity_fiber_t *fiber, gravity_closure_t *closure, uint16_t nargs) {
    gravity_callframe_t *frame = &fiber->frames[0];
    frame->closure = closure;
    frame->ip = (closure->f->tag == EXEC_TYPE_NATIVE) ? closure->f->bytecode : NULL;

    frame->dest = 0;
    frame->stackstart = fiber->stack;

    fiber->nframes = 1;
    fiber->upvalues = NULL;

    // update stacktop in order to be GC friendly
    fiber->stacktop += FN_COUNTREG(closure->f, nargs);
}

void gravity_fiber_reset (gravity_fiber_t *fiber) {
    fiber->caller = NULL;
    fiber->result = VALUE_FROM_NULL;
    fiber->nframes = 0;
    fiber->upvalues = NULL;
    fiber->stacktop = fiber->stack;
}

void gravity_fiber_seterror (gravity_fiber_t *fiber, const char *error) {
    if (fiber->error) mem_free(fiber->error);
    fiber->error = (char *)string_dup(error);
}

uint32_t gravity_fiber_size (gravity_vm *vm, gravity_fiber_t *fiber) {
    SET_OBJECT_VISITED_FLAG(fiber, true);
    
    // internal size
    uint32_t fiber_size = sizeof(gravity_fiber_t);
    fiber_size += fiber->stackalloc * sizeof(gravity_value_t);
    fiber_size += fiber->framesalloc * sizeof(gravity_callframe_t);

    // stack size
    for (gravity_value_t* slot = fiber->stack; slot < fiber->stacktop; ++slot) {
        fiber_size += gravity_value_size(vm, *slot);
    }

    fiber_size += string_size(fiber->error);
    fiber_size += gravity_object_size(vm, (gravity_object_t *)fiber->caller);

    SET_OBJECT_VISITED_FLAG(fiber, false);
    return fiber_size;
}

void gravity_fiber_blacken (gravity_vm *vm, gravity_fiber_t *fiber) {
    gravity_vm_memupdate(vm, gravity_fiber_size(vm, fiber));
    
    // gray call frame functions
    for (uint32_t i=0; i < fiber->nframes; ++i) {
        gravity_gray_object(vm, (gravity_object_t *)fiber->frames[i].closure);
		gravity_gray_object(vm, (gravity_object_t *)fiber->frames[i].args);
    }

    // gray stack variables
    for (gravity_value_t *slot = fiber->stack; slot < fiber->stacktop; ++slot) {
        gravity_gray_value(vm, *slot);
    }

    // gray upvalues
    gravity_upvalue_t *upvalue = fiber->upvalues;
    while (upvalue) {
        gravity_gray_object(vm, (gravity_object_t *)upvalue);
        upvalue = upvalue->next;
    }

    gravity_gray_object(vm, (gravity_object_t *)fiber->caller);
}

// MARK: -

void gravity_object_serialize (gravity_object_t *obj, json_t *json) {
    if (obj->isa == gravity_class_function)
        gravity_function_serialize((gravity_function_t *)obj, json);
    else if (obj->isa == gravity_class_class)
        gravity_class_serialize((gravity_class_t *)obj, json);
    else assert(0);
}

gravity_object_t *gravity_object_deserialize (gravity_vm *vm, json_value *entry) {
    // this function is able to deserialize ONLY objects with a type label
    
    // sanity check
    if (entry->type != json_object) return NULL;
    if (entry->u.object.length == 0) return NULL;

    // the first entry value must specify gravity object type
    const char *label = entry->u.object.values[0].name;
    json_value *value = entry->u.object.values[0].value;
    
    if (string_casencmp(label, GRAVITY_JSON_LABELTYPE, 4) != 0) {
        // if no label type found then assume it is a map object
        gravity_map_t *m = gravity_map_deserialize(vm, entry);
        return (gravity_object_t *)m;
    }
    
    // sanity check
    if (value->type != json_string) return NULL;

    // FUNCTION case
    if (string_casencmp(value->u.string.ptr, GRAVITY_JSON_FUNCTION, value->u.string.length) == 0) {
        gravity_function_t *f = gravity_function_deserialize(vm, entry);
        return (gravity_object_t *)f;
    }

    // CLASS case
    if (string_casencmp(value->u.string.ptr, GRAVITY_JSON_CLASS, value->u.string.length) == 0) {
        gravity_class_t *c = gravity_class_deserialize(vm, entry);
        return (gravity_object_t *)c;
    }

    // MAP/ENUM case
    if ((string_casencmp(value->u.string.ptr, GRAVITY_JSON_MAP, value->u.string.length) == 0) ||
        (string_casencmp(value->u.string.ptr, GRAVITY_JSON_ENUM, value->u.string.length) == 0)) {
        gravity_map_t *m = gravity_map_deserialize(vm, entry);
        return (gravity_object_t *)m;
    }
    
    // RANGE case
    if (string_casencmp(value->u.string.ptr, GRAVITY_JSON_RANGE, value->u.string.length) == 0) {
        gravity_range_t *range = gravity_range_deserialize(vm, entry);
        return (gravity_object_t *)range;
    }

    // unhandled case
    DEBUG_DESERIALIZE("gravity_object_deserialize unknown type");
    return NULL;
}
#undef REPORT_JSON_ERROR

const char *gravity_object_debug (gravity_object_t *obj, bool is_free) {
    if ((!obj) || (!OBJECT_IS_VALID(obj))) return "";

    if (OBJECT_ISA_INT(obj)) return "INT";
    if (OBJECT_ISA_FLOAT(obj)) return "FLOAT";
    if (OBJECT_ISA_BOOL(obj)) return "BOOL";
    if (OBJECT_ISA_NULL(obj)) return "NULL";

    static char buffer[512];
    if (OBJECT_ISA_FUNCTION(obj)) {
        const char *name = ((gravity_function_t*)obj)->identifier;
        if (!name) name = "ANONYMOUS";
        snprintf(buffer, sizeof(buffer), "FUNCTION %p %s", obj, name);
        return buffer;
    }

    if (OBJECT_ISA_CLOSURE(obj)) {
        // cannot guarantee ptr validity during a free
        const char *name = (is_free) ? NULL : ((gravity_closure_t*)obj)->f->identifier;
        if (!name) name = "ANONYMOUS";
        snprintf(buffer, sizeof(buffer), "CLOSURE %p %s", obj, name);
        return buffer;
    }

    if (OBJECT_ISA_CLASS(obj)) {
        const char *name = ((gravity_class_t*)obj)->identifier;
        if (!name) name = "ANONYMOUS";
        snprintf(buffer, sizeof(buffer), "CLASS %p %s", obj, name);
        return buffer;
    }

    if (OBJECT_ISA_STRING(obj)) {
        snprintf(buffer, sizeof(buffer), "STRING %p %s", obj, ((gravity_string_t*)obj)->s);
        return buffer;
    }

    if (OBJECT_ISA_INSTANCE(obj)) {
        // cannot guarantee ptr validity during a free
        gravity_class_t *c = (is_free) ? NULL : ((gravity_instance_t*)obj)->objclass;
        const char *name = (c && c->identifier) ? c->identifier : "ANONYMOUS";
        snprintf(buffer, sizeof(buffer), "INSTANCE %p OF %s", obj, name);
        return buffer;
    }

    if (OBJECT_ISA_RANGE(obj)) {
        snprintf(buffer, sizeof(buffer), "RANGE %p %ld %ld", obj, (long)((gravity_range_t*)obj)->from, (long)((gravity_range_t*)obj)->to);
        return buffer;
    }

    if (OBJECT_ISA_LIST(obj)) {
        snprintf(buffer, sizeof(buffer), "LIST %p (%ld items)", obj, (long)marray_size(((gravity_list_t*)obj)->array));
        return buffer;
    }

    if (OBJECT_ISA_MAP(obj)) {
         snprintf(buffer, sizeof(buffer), "MAP %p (%ld items)", obj, (long)gravity_hash_count(((gravity_map_t*)obj)->hash));
         return buffer;
    }

    if (OBJECT_ISA_FIBER(obj)) {
        snprintf(buffer, sizeof(buffer), "FIBER %p", obj);
        return buffer;
    }

    if (OBJECT_ISA_UPVALUE(obj)) {
        snprintf(buffer, sizeof(buffer), "UPVALUE %p", obj);
        return buffer;
    }

    return "N/A";
}

void gravity_object_free (gravity_vm *vm, gravity_object_t *obj) {
    if ((!obj) || (!OBJECT_IS_VALID(obj))) return;
    
    if (obj->gc.free) obj->gc.free(vm, obj);
    else if (OBJECT_ISA_CLASS(obj)) gravity_class_free(vm, (gravity_class_t *)obj);
    else if (OBJECT_ISA_FUNCTION(obj)) gravity_function_free(vm, (gravity_function_t *)obj);
    else if (OBJECT_ISA_CLOSURE(obj)) gravity_closure_free(vm, (gravity_closure_t *)obj);
    else if (OBJECT_ISA_INSTANCE(obj)) gravity_instance_free(vm, (gravity_instance_t *)obj);
    else if (OBJECT_ISA_LIST(obj)) gravity_list_free(vm, (gravity_list_t *)obj);
    else if (OBJECT_ISA_MAP(obj)) gravity_map_free(vm, (gravity_map_t *)obj);
    else if (OBJECT_ISA_FIBER(obj)) gravity_fiber_free(vm, (gravity_fiber_t *)obj);
    else if (OBJECT_ISA_RANGE(obj)) gravity_range_free(vm, (gravity_range_t *)obj);
    else if (OBJECT_ISA_MODULE(obj)) gravity_module_free(vm, (gravity_module_t *)obj);
    else if (OBJECT_ISA_STRING(obj)) gravity_string_free(vm, (gravity_string_t *)obj);
    else if (OBJECT_ISA_UPVALUE(obj)) gravity_upvalue_free(vm, (gravity_upvalue_t *)obj);
    else assert(0); // should never reach this point
}

uint32_t gravity_object_size (gravity_vm *vm, gravity_object_t *obj) {
    if ((!obj) || (!OBJECT_IS_VALID(obj))) return 0;
    
    // check if object has already been visited (to avoid infinite loop)
    if (obj->gc.visited) return 0;
    
    if (obj->gc.size) return obj->gc.size(vm, obj);
    else if (OBJECT_ISA_CLASS(obj)) return gravity_class_size(vm, (gravity_class_t *)obj);
    else if (OBJECT_ISA_FUNCTION(obj)) return gravity_function_size(vm, (gravity_function_t *)obj);
    else if (OBJECT_ISA_CLOSURE(obj)) return gravity_closure_size(vm, (gravity_closure_t *)obj);
    else if (OBJECT_ISA_INSTANCE(obj)) return gravity_instance_size(vm, (gravity_instance_t *)obj);
    else if (OBJECT_ISA_LIST(obj)) return gravity_list_size(vm, (gravity_list_t *)obj);
    else if (OBJECT_ISA_MAP(obj)) return gravity_map_size(vm, (gravity_map_t *)obj);
    else if (OBJECT_ISA_FIBER(obj)) return gravity_fiber_size(vm, (gravity_fiber_t *)obj);
    else if (OBJECT_ISA_RANGE(obj)) return gravity_range_size(vm, (gravity_range_t *)obj);
    else if (OBJECT_ISA_MODULE(obj)) return gravity_module_size(vm, (gravity_module_t *)obj);
    else if (OBJECT_ISA_STRING(obj)) return gravity_string_size(vm, (gravity_string_t *)obj);
    else if (OBJECT_ISA_UPVALUE(obj)) return gravity_upvalue_size(vm, (gravity_upvalue_t *)obj);
    return 0;
}

void gravity_object_blacken (gravity_vm *vm, gravity_object_t *obj) {
    if ((!obj) || (!OBJECT_IS_VALID(obj))) return;

    if (obj->gc.blacken) obj->gc.blacken(vm, obj);
    else if (OBJECT_ISA_CLASS(obj)) gravity_class_blacken(vm, (gravity_class_t *)obj);
    else if (OBJECT_ISA_FUNCTION(obj)) gravity_function_blacken(vm, (gravity_function_t *)obj);
    else if (OBJECT_ISA_CLOSURE(obj)) gravity_closure_blacken(vm, (gravity_closure_t *)obj);
    else if (OBJECT_ISA_INSTANCE(obj)) gravity_instance_blacken(vm, (gravity_instance_t *)obj);
    else if (OBJECT_ISA_LIST(obj)) gravity_list_blacken(vm, (gravity_list_t *)obj);
    else if (OBJECT_ISA_MAP(obj)) gravity_map_blacken(vm, (gravity_map_t *)obj);
    else if (OBJECT_ISA_FIBER(obj)) gravity_fiber_blacken(vm, (gravity_fiber_t *)obj);
    else if (OBJECT_ISA_RANGE(obj)) gravity_range_blacken(vm, (gravity_range_t *)obj);
    else if (OBJECT_ISA_MODULE(obj)) gravity_module_blacken(vm, (gravity_module_t *)obj);
    else if (OBJECT_ISA_STRING(obj)) gravity_string_blacken(vm, (gravity_string_t *)obj);
    else if (OBJECT_ISA_UPVALUE(obj)) gravity_upvalue_blacken(vm, (gravity_upvalue_t *)obj);
    //else assert(0); // should never reach this point
}

// MARK: -

gravity_instance_t *gravity_instance_new (gravity_vm *vm, gravity_class_t *c) {
    gravity_instance_t *instance = (gravity_instance_t *)mem_alloc(NULL, sizeof(gravity_instance_t));

    instance->isa = gravity_class_instance;
    instance->objclass = c;
    
    if (c->nivars) instance->ivars = (gravity_value_t *)mem_alloc(NULL, c->nivars * sizeof(gravity_value_t));
    for (uint32_t i=0; i<c->nivars; ++i) instance->ivars[i] = VALUE_FROM_NULL;
    
    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) instance);
    return instance;
}

gravity_instance_t *gravity_instance_clone (gravity_vm *vm, gravity_instance_t *src_instance) {
    gravity_class_t *c = src_instance->objclass;
  
    gravity_instance_t *instance = (gravity_instance_t *)mem_alloc(NULL, sizeof(gravity_instance_t));
    instance->isa = gravity_class_instance;
    instance->objclass = c;
    
    // if c is an anonymous class then it must be deeply copied
    if (gravity_class_is_anon(c)) {
        // clone class c and all its closures
    }
    
    gravity_delegate_t *delegate = gravity_vm_delegate(vm);

    instance->xdata = (src_instance->xdata && delegate->bridge_clone) ? delegate->bridge_clone(vm, src_instance->xdata) : NULL;
    
    if (c->nivars) instance->ivars = (gravity_value_t *)mem_alloc(NULL, c->nivars * sizeof(gravity_value_t));
    for (uint32_t i=0; i<c->nivars; ++i) instance->ivars[i] = src_instance->ivars[i];

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) instance);
    return instance;
}

void gravity_instance_setivar (gravity_instance_t *instance, uint32_t idx, gravity_value_t value) {
    if (idx < instance->objclass->nivars) instance->ivars[idx] = value;
}

void gravity_instance_setxdata (gravity_instance_t *i, void *xdata) {
    i->xdata = xdata;
}

void gravity_instance_free (gravity_vm *vm, gravity_instance_t *i) {
    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)i, true));

    // check if bridged data needs to be freed too
    if (i->xdata && vm) {
        gravity_delegate_t *delegate = gravity_vm_delegate(vm);
        if (delegate->bridge_free) delegate->bridge_free(vm, (gravity_object_t *)i);
    }

    if (i->ivars) mem_free(i->ivars);
    mem_free((void *)i);
}

gravity_closure_t *gravity_instance_lookup_event (gravity_instance_t *i, const char *name) {
    // TODO: implemented as gravity_class_lookup but should be the exact opposite

    STATICVALUE_FROM_STRING(key, name, strlen(name));
    gravity_class_t *c = i->objclass;
    while (c) {
        gravity_value_t *v = gravity_hash_lookup(c->htable, key);
        // NOTE: there could be events (like InitContainer) which are empty (bytecode NULL) should I handle them here?
        if ((v) && (OBJECT_ISA_CLOSURE(v->p))) return (gravity_closure_t *)v->p;
        c = c->superclass;
    }
    return NULL;
}

gravity_value_t gravity_instance_lookup_property (gravity_vm *vm, gravity_instance_t *i, gravity_value_t key) {
    gravity_closure_t *closure = gravity_class_lookup_closure(i->objclass, key);
    if (!closure) return VALUE_NOT_VALID;
    
    // check if it is a real property
    gravity_function_t *func = closure->f;
    if (!func || func->tag != EXEC_TYPE_SPECIAL) return VALUE_NOT_VALID;
    
    // check if it a computed property with a getter
    if (FUNCTION_ISA_GETTER(func)) {
        gravity_closure_t *getter = (gravity_closure_t *)func->special[EXEC_TYPE_SPECIAL_GETTER];
        if (gravity_vm_runclosure(vm, getter, VALUE_FROM_NULL, NULL, 0)) return gravity_vm_result(vm);
    }
    
    // now I am sure that it is a real property (non-computed)
    return i->ivars[func->index];
}

uint32_t gravity_instance_size (gravity_vm *vm, gravity_instance_t *i) {
    SET_OBJECT_VISITED_FLAG(i, true);
    
    uint32_t instance_size = sizeof(gravity_instance_t) + (i->objclass->nivars * sizeof(gravity_value_t));

    gravity_delegate_t *delegate = gravity_vm_delegate(vm);
    if (i->xdata && delegate->bridge_size)
        instance_size += delegate->bridge_size(vm, i->xdata);

    SET_OBJECT_VISITED_FLAG(i, false);
    return instance_size;
}

void gravity_instance_blacken (gravity_vm *vm, gravity_instance_t *i) {
    gravity_vm_memupdate(vm, gravity_instance_size(vm, i));

    // instance class
    gravity_gray_object(vm, (gravity_object_t *)i->objclass);

    // ivars
    for (uint32_t j=0; j<i->objclass->nivars; ++j) {
        gravity_gray_value(vm, i->ivars[j]);
    }
    
    // xdata
    if (i->xdata) {
        gravity_delegate_t *delegate = gravity_vm_delegate(vm);
        if (delegate->bridge_blacken) delegate->bridge_blacken(vm, i->xdata);
    }
}

void gravity_instance_serialize (gravity_instance_t *instance, json_t *json) {
    gravity_class_t *c = instance->objclass;
    
    const char *label = json_get_label(json, NULL);
    json_begin_object(json, label);
    
    json_add_cstring(json, GRAVITY_JSON_LABELTYPE, GRAVITY_JSON_INSTANCE);
    json_add_cstring(json, GRAVITY_JSON_CLASS, c->identifier);
    
    if (c->nivars) {
        json_begin_array(json, GRAVITY_JSON_LABELIVAR);
        for (uint32_t i=0; i<c->nivars; ++i) {
            gravity_value_serialize(NULL, instance->ivars[i], json);
        }
        json_end_array(json);
    }
    
    json_end_object(json);
}

bool gravity_instance_isstruct (gravity_instance_t *i) {
    return i->objclass->is_struct;
}

// MARK: -
static bool hash_value_compare_cb (gravity_value_t v1, gravity_value_t v2, void *data) {
    #pragma unused (data)
    return gravity_value_equals(v1, v2);
}

bool gravity_value_vm_equals (gravity_vm *vm, gravity_value_t v1, gravity_value_t v2) {
    bool result = gravity_value_equals(v1, v2);
    if (result || !vm) return result;

    // sanity check
    if (!(VALUE_ISA_INSTANCE(v1) && VALUE_ISA_INSTANCE(v2))) return false;

    // if here means that they are two heap allocated objects
    gravity_instance_t *obj1 = (gravity_instance_t *)VALUE_AS_OBJECT(v1);
    gravity_instance_t *obj2 = (gravity_instance_t *)VALUE_AS_OBJECT(v2);

    gravity_delegate_t *delegate = gravity_vm_delegate(vm);
    if (obj1->xdata && obj2->xdata && delegate->bridge_equals) {
        return delegate->bridge_equals(vm, obj1->xdata, obj2->xdata);
    }

    return false;
}

bool gravity_value_equals (gravity_value_t v1, gravity_value_t v2) {

    // check same class
    if (v1.isa != v2.isa) return false;

    // check same value for value types
    if ((v1.isa == gravity_class_int) || (v1.isa == gravity_class_bool) || (v1.isa == gravity_class_null)) {
        return (v1.n == v2.n);
    } else if (v1.isa == gravity_class_float) {
        #if GRAVITY_ENABLE_DOUBLE
        return (fabs(v1.f - v2.f) < EPSILON);
        #else
        return (fabsf(v1.f - v2.f) < EPSILON);
        #endif
    } else if (v1.isa == gravity_class_string) {
        gravity_string_t *s1 = VALUE_AS_STRING(v1);
        gravity_string_t *s2 = VALUE_AS_STRING(v2);
        if (s1->hash != s2->hash) return false;
        if (s1->len != s2->len) return false;
        // same hash and same len so let's compare bytes
        return (memcmp(s1->s, s2->s, s1->len) == 0);
    } else if (v1.isa == gravity_class_range) {
        gravity_range_t *r1 = VALUE_AS_RANGE(v1);
        gravity_range_t *r2 = VALUE_AS_RANGE(v2);
        return ((r1->from == r2->from) && (r1->to == r2->to));
    } else if (v1.isa == gravity_class_list) {
        gravity_list_t *list1 = VALUE_AS_LIST(v1);
        gravity_list_t *list2 = VALUE_AS_LIST(v2);
        if (marray_size(list1->array) != marray_size(list2->array)) return false;
        size_t count = marray_size(list1->array);
        for (size_t i=0; i<count; ++i) {
            gravity_value_t value1 = marray_get(list1->array, i);
            gravity_value_t value2 = marray_get(list2->array, i);
            if (!gravity_value_equals(value1, value2)) return false;
        }
        return true;
    } else if (v1.isa == gravity_class_map) {
        gravity_map_t *map1 = VALUE_AS_MAP(v1);
        gravity_map_t *map2 = VALUE_AS_MAP(v2);
        return gravity_hash_compare(map1->hash, map2->hash, hash_value_compare_cb, NULL);
    }

    // if here means that they are two heap allocated objects
    gravity_object_t *obj1 = VALUE_AS_OBJECT(v1);
    gravity_object_t *obj2 = VALUE_AS_OBJECT(v2);
    if (obj1->isa != obj2->isa) return false;

    return (obj1 == obj2);
}

uint32_t gravity_value_hash (gravity_value_t value) {
    if (value.isa == gravity_class_string)
        return VALUE_AS_STRING(value)->hash;

    if ((value.isa == gravity_class_int) || (value.isa == gravity_class_bool) || (value.isa == gravity_class_null))
        return gravity_hash_compute_int(value.n);

    if (value.isa == gravity_class_float)
        return gravity_hash_compute_float(value.f);

    return gravity_hash_compute_buffer((const char *)value.p, sizeof(gravity_object_t*));
}

inline gravity_class_t *gravity_value_getclass (gravity_value_t v) {
    if ((v.isa == gravity_class_class) && (v.p->objclass == gravity_class_object)) return (gravity_class_t *)v.p;
    if ((v.isa == gravity_class_instance) || (v.isa == gravity_class_class)) return (v.p) ? v.p->objclass : NULL;
    return v.isa;
}

inline gravity_class_t *gravity_value_getsuper (gravity_value_t v) {
    gravity_class_t *c = gravity_value_getclass(v);
    return (c && c->superclass) ? c->superclass : NULL;
}

void gravity_value_free (gravity_vm *vm, gravity_value_t v) {
    if (!gravity_value_isobject(v)) return;
    gravity_object_free(vm, VALUE_AS_OBJECT(v));
}

static void gravity_map_serialize_iterator (gravity_hash_t *hashtable, gravity_value_t key, gravity_value_t v, void *data) {
    #pragma unused(hashtable)
    assert(key.isa == gravity_class_string);

    json_t *json = (json_t *)data;
    const char *key_value = VALUE_AS_STRING(key)->s;

    gravity_value_serialize(key_value, v, json);
}

void gravity_value_serialize (const char *key, gravity_value_t v, json_t *json) {
    // NULL
    if (VALUE_ISA_NULL(v)) {
        json_add_null(json, key);
        return;
    }
    
    // UNDEFINED (convention used to represent an UNDEFINED value)
    if (VALUE_ISA_UNDEFINED(v)) {
        if (json_option_isset(json, json_opt_no_undef)) {
            json_add_null(json, key);
        } else {
            json_begin_object(json, key);
            json_end_object(json);
        }
        return;
    }

    // BOOL
    if (VALUE_ISA_BOOL(v)) {
        json_add_bool(json, key, (v.n == 0) ? false : true);
        return;
    }

    // INT
    if (VALUE_ISA_INT(v)) {
        json_add_int(json, key, (int64_t)v.n);
        return;
    }

    // FLOAT
    if (VALUE_ISA_FLOAT(v)) {
        json_add_double(json, key, (double)v.f);
        return;
    }

    // FUNCTION
    if (VALUE_ISA_FUNCTION(v)) {
        json_set_label(json, key);
        gravity_function_serialize(VALUE_AS_FUNCTION(v), json);
        return;
    }
    
    // CLOSURE
    if (VALUE_ISA_CLOSURE(v)) {
        json_set_label(json, key);
        gravity_function_serialize(VALUE_AS_CLOSURE(v)->f, json);
        return;
    }

    // CLASS
    if (VALUE_ISA_CLASS(v)) {
        json_set_label(json, key);
        gravity_class_serialize(VALUE_AS_CLASS(v), json);
        return;
    }

    // STRING
    if (VALUE_ISA_STRING(v)) {
        gravity_string_t *value = VALUE_AS_STRING(v);
        json_add_string(json, key, value->s, value->len);
        return;
    }

    // LIST (ARRAY)
    if (VALUE_ISA_LIST(v)) {
        gravity_list_t *value = VALUE_AS_LIST(v);
        json_begin_array(json, key);
        size_t count = marray_size(value->array);
        for (size_t j=0; j<count; j++) {
            gravity_value_t item = marray_get(value->array, j);
            // here I am sure that value is a literal value
            gravity_value_serialize(NULL, item, json);
        }
        json_end_array(json);
        return;
    }

    // MAP (HASH)
    // a map is serialized only if it contains only literals, otherwise it is computed at runtime
    if (VALUE_ISA_MAP(v)) {
        gravity_map_t *value = VALUE_AS_MAP(v);
        json_begin_object(json, key);
        if (!json_option_isset(json, json_opt_no_maptype)) json_add_cstring(json, GRAVITY_JSON_LABELTYPE, GRAVITY_JSON_MAP);
        gravity_hash_iterate(value->hash, gravity_map_serialize_iterator, json);
        json_end_object(json);
        return;
    }
    
    // RANGE
    if (VALUE_ISA_RANGE(v)) {
        json_set_label(json, key);
        gravity_range_serialize(VALUE_AS_RANGE(v), json);
        return;
    }

    // INSTANCE
    if (VALUE_ISA_INSTANCE(v)) {
        json_set_label(json, key);
        gravity_instance_serialize(VALUE_AS_INSTANCE(v), json);
        return;
    }
    
    // FIBER
    if (VALUE_ISA_FIBER(v)) {
        return;
    }
    
    // should never reach this point
    assert(0);
}

bool gravity_value_isobject (gravity_value_t v) {
    // was:
    // if (VALUE_ISA_NOTVALID(v)) return false;
    // if (VALUE_ISA_INT(v)) return false;
    // if (VALUE_ISA_FLOAT(v)) return false;
    // if (VALUE_ISA_BOOL(v)) return false;
    // if (VALUE_ISA_NULL(v)) return false;
    // if (VALUE_ISA_UNDEFINED(v)) return false;
    // return true;

    if ((v.isa == NULL) || (v.isa == gravity_class_int) || (v.isa == gravity_class_float) ||
        (v.isa == gravity_class_bool) || (v.isa == gravity_class_null) || (v.p == NULL)) return false;
    
    // extra check to allow ONLY known objects
    if ((v.isa == gravity_class_string) || (v.isa == gravity_class_object) || (v.isa == gravity_class_function) ||
        (v.isa == gravity_class_closure) || (v.isa == gravity_class_fiber) || (v.isa == gravity_class_class) ||
        (v.isa == gravity_class_instance) || (v.isa == gravity_class_module) || (v.isa == gravity_class_list) ||
        (v.isa == gravity_class_map) || (v.isa == gravity_class_range) || (v.isa == gravity_class_upvalue)) return true;
    
    return false;
}

uint32_t gravity_value_size (gravity_vm *vm, gravity_value_t v) {
    return (gravity_value_isobject(v)) ? gravity_object_size(vm, (gravity_object_t*)v.p) : 0;
}

void *gravity_value_xdata (gravity_value_t value) {
    if (VALUE_ISA_INSTANCE(value)) {
        gravity_instance_t *i = VALUE_AS_INSTANCE(value);
        return i->xdata;
    } else if (VALUE_ISA_CLASS(value)) {
        gravity_class_t *c = VALUE_AS_CLASS(value);
        return c->xdata;
    }
    return NULL;
}

const char *gravity_value_name (gravity_value_t value) {
    if (VALUE_ISA_INSTANCE(value)) {
        gravity_instance_t *instance = VALUE_AS_INSTANCE(value);
        return instance->objclass->identifier;
    } else if (VALUE_ISA_CLASS(value)) {
        gravity_class_t *c = VALUE_AS_CLASS(value);
        return c->identifier;
    }
    return NULL;
}

void gravity_value_dump (gravity_vm *vm, gravity_value_t v, char *buffer, uint16_t len) {
    const char *type = NULL;
    const char *value = NULL;
    char        sbuffer[1024];

    if (buffer == NULL) buffer = sbuffer;
    if (len == 0) len = sizeof(sbuffer);

    if (v.isa == NULL) {
        type = "INVALID!";
        snprintf(buffer, len, "%s", type);
        value = buffer;
    } else if (v.isa == gravity_class_bool) {
        type = "BOOL";
        value = (v.n == 0) ? "false" : "true";
        snprintf(buffer, len, "(%s) %s", type, value);
        value = buffer;
    } else if (v.isa == gravity_class_null) {
        type = (v.n == 0) ? "NULL" : "UNDEFINED";
        snprintf(buffer, len, "%s", type);
        value = buffer;
    } else if (v.isa == gravity_class_int) {
        type = "INT";
        snprintf(buffer, len, "(%s) %" PRId64, type, v.n);
        value = buffer;
    } else if (v.isa == gravity_class_float) {
        type = "FLOAT";
        snprintf(buffer, len, "(%s) %g", type, v.f);
        value = buffer;
    } else if (v.isa == gravity_class_function) {
        type = "FUNCTION";
        value = VALUE_AS_FUNCTION(v)->identifier;
        snprintf(buffer, len, "(%s) %s (%p)", type, value, VALUE_AS_FUNCTION(v));
        value = buffer;
    } else if (v.isa == gravity_class_closure) {
        type = "CLOSURE";
        gravity_function_t *f = VALUE_AS_CLOSURE(v)->f;
        value = (f->identifier) ? (f->identifier) : "anon";
        snprintf(buffer, len, "(%s) %s (%p)", type, value, VALUE_AS_CLOSURE(v));
        value = buffer;
    } else if (v.isa == gravity_class_class) {
        type = "CLASS";
        value = VALUE_AS_CLASS(v)->identifier;
        snprintf(buffer, len, "(%s) %s (%p)", type, value, VALUE_AS_CLASS(v));
        value = buffer;
    } else if (v.isa == gravity_class_string) {
        type = "STRING";
        gravity_string_t *s = VALUE_AS_STRING(v);
        snprintf(buffer, len, "(%s) %.*s (%p)", type, s->len, s->s, s);
        value = buffer;
    } else if (v.isa == gravity_class_instance) {
        type = "INSTANCE";
        gravity_instance_t *i = VALUE_AS_INSTANCE(v);
        gravity_class_t *c = i->objclass;
        value = c->identifier;
        snprintf(buffer, len, "(%s) %s (%p)", type, value, i);
        value = buffer;
    } else if (v.isa == gravity_class_list) {
        type = "LIST";
        gravity_value_t sval = convert_value2string(vm, v);
        gravity_string_t *s = VALUE_AS_STRING(sval);
        snprintf(buffer, len, "(%s) %.*s (%p)", type, s->len, s->s, s);
        value = buffer;
    } else if (v.isa == gravity_class_map) {
        type = "MAP";
        gravity_value_t sval = convert_value2string(vm, v);
        gravity_string_t *s = VALUE_AS_STRING(sval);
        snprintf(buffer, len, "(%s) %.*s (%p)", type, s->len, s->s, s);
        value = buffer;
    } else if (v.isa == gravity_class_range) {
        type = "RANGE";
        gravity_range_t *r = VALUE_AS_RANGE(v);
        snprintf(buffer, len, "(%s) from %" PRId64 " to %" PRId64, type, r->from, r->to);
        value = buffer;
    } else if (v.isa == gravity_class_object) {
        type = "OBJECT";
        value = "N/A";
        snprintf(buffer, len, "(%s) %s", type, value);
        value = buffer;
    } else if (v.isa == gravity_class_fiber) {
        type = "FIBER";
        snprintf(buffer, len, "(%s) %p", type, v.p);
        value = buffer;
    } else {
        type = "N/A";
        value = "N/A";
        snprintf(buffer, len, "(%s) %s", type, value);
        value = buffer;
    }

    if (buffer == sbuffer) printf("%s\n", value);
}

// MARK: -
gravity_list_t *gravity_list_new (gravity_vm *vm, uint32_t n) {
    if (n > MAX_ALLOCATION) return NULL;

    gravity_list_t *list = (gravity_list_t *)mem_alloc(NULL, sizeof(gravity_list_t));

    list->isa = gravity_class_list;
    marray_init(list->array);
    marray_resize(gravity_value_t, list->array, n + MARRAY_DEFAULT_SIZE);

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) list);
    return list;
}

gravity_list_t *gravity_list_from_array (gravity_vm *vm, uint32_t n, gravity_value_t *p) {
    gravity_list_t *list = (gravity_list_t *)mem_alloc(NULL, sizeof(gravity_list_t));

    list->isa = gravity_class_list;
    marray_init(list->array);
    // elements must be copied because for the compiler their registers are TEMP
    // and could be reused by other successive operations
    for (size_t i=0; i<n; ++i) marray_push(gravity_value_t, list->array, p[i]);

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) list);
    return list;
}

void gravity_list_free (gravity_vm *vm, gravity_list_t *list) {
    #pragma unused(vm)
    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)list, true));
    marray_destroy(list->array);
    mem_free((void *)list);
}

void gravity_list_append_list (gravity_vm *vm, gravity_list_t *list1, gravity_list_t *list2) {
    #pragma unused(vm)
    // append list2 to list1
    size_t count = marray_size(list2->array);
    for (size_t i=0; i<count; ++i) {
        marray_push(gravity_value_t, list1->array, marray_get(list2->array, i));
    }
}

uint32_t gravity_list_size (gravity_vm *vm, gravity_list_t *list) {
    SET_OBJECT_VISITED_FLAG(list, true);
    
    uint32_t internal_size = 0;
    size_t count = marray_size(list->array);
    for (size_t i=0; i<count; ++i) {
        internal_size += gravity_value_size(vm, marray_get(list->array, i));
    }
    internal_size += sizeof(gravity_list_t);
    
    SET_OBJECT_VISITED_FLAG(list, false);
    return internal_size;
}

void gravity_list_blacken (gravity_vm *vm, gravity_list_t *list) {
    gravity_vm_memupdate(vm, gravity_list_size(vm, list));

    size_t count = marray_size(list->array);
    for (size_t i=0; i<count; ++i) {
        gravity_gray_value(vm, marray_get(list->array, i));
    }
}

// MARK: -
gravity_map_t *gravity_map_new (gravity_vm *vm, uint32_t n) {
    gravity_map_t *map = (gravity_map_t *)mem_alloc(NULL, sizeof(gravity_map_t));

    map->isa = gravity_class_map;
    map->hash = gravity_hash_create(n, gravity_value_hash, gravity_value_equals, NULL, NULL);

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) map);
    return map;
}

void gravity_map_free (gravity_vm *vm, gravity_map_t *map) {
    #pragma unused(vm)

    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)map, true));
    gravity_hash_free(map->hash);
    mem_free((void *)map);
}

void gravity_map_append_map (gravity_vm *vm, gravity_map_t *map1, gravity_map_t *map2) {
    #pragma unused(vm)
    // append map2 to map1
    gravity_hash_append(map1->hash, map2->hash);
}

void gravity_map_insert (gravity_vm *vm, gravity_map_t *map, gravity_value_t key, gravity_value_t value) {
    #pragma unused(vm)
    gravity_hash_insert(map->hash, key, value);
}

static gravity_map_t *gravity_map_deserialize (gravity_vm *vm, json_value *json) {
    uint32_t n = json->u.object.length;
    gravity_map_t *map = gravity_map_new(vm, n);

    DEBUG_DESERIALIZE("DESERIALIZE MAP: %p\n", map);

    for (uint32_t i=0; i<n; ++i) { // from 1 to skip type
        const char *label = json->u.object.values[i].name;
        json_value *jsonv = json->u.object.values[i].value;

        gravity_value_t key = VALUE_FROM_CSTRING(vm, label);
        gravity_value_t value;

        switch (jsonv->type) {
            case json_integer:
                value = VALUE_FROM_INT((gravity_int_t)jsonv->u.integer); break;
            case json_double:
                value = VALUE_FROM_FLOAT((gravity_float_t)jsonv->u.dbl); break;
            case json_boolean:
                value = VALUE_FROM_BOOL(jsonv->u.boolean); break;
            case json_string:
                value = VALUE_FROM_STRING(vm, jsonv->u.string.ptr, jsonv->u.string.length); break;
            case json_null:
                value = VALUE_FROM_NULL; break;
            case json_object: {
                gravity_object_t *obj = gravity_object_deserialize(vm, jsonv);
                value = (obj) ? VALUE_FROM_OBJECT(obj) : VALUE_FROM_NULL;
                break;
            }
            case json_array: {
                
            }
            default:
                goto abort_load;
        }

        gravity_map_insert(NULL, map, key, value);
    }
    return map;

abort_load:
    // do not free map here because it is already garbage collected
    return NULL;
}

uint32_t gravity_map_size (gravity_vm *vm, gravity_map_t *map) {
    SET_OBJECT_VISITED_FLAG(map, true);
    
    uint32_t hash_size = 0;
    gravity_hash_iterate2(map->hash, gravity_hash_internalsize, (void *)&hash_size, (void *)vm);
    hash_size += gravity_hash_memsize(map->hash);
    hash_size += sizeof(gravity_map_t);
    
    SET_OBJECT_VISITED_FLAG(map, false);
    return hash_size;
}

void gravity_map_blacken (gravity_vm *vm, gravity_map_t *map) {
    gravity_vm_memupdate(vm, gravity_map_size(vm, map));
    gravity_hash_iterate(map->hash, gravity_hash_gray, (void *)vm);
}

// MARK: -

gravity_range_t *gravity_range_new (gravity_vm *vm, gravity_int_t from_range, gravity_int_t to_range, bool inclusive) {
    gravity_range_t *range = mem_alloc(NULL, sizeof(gravity_range_t));

    range->isa = gravity_class_range;
    range->from = from_range;
    range->to = (inclusive) ? to_range : --to_range;

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) range);
    return range;
}

void gravity_range_free (gravity_vm *vm, gravity_range_t *range) {
    #pragma unused(vm)

    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)range, true));
    mem_free((void *)range);
}

uint32_t gravity_range_size (gravity_vm *vm, gravity_range_t *range) {
    #pragma unused(vm, range)
    
    SET_OBJECT_VISITED_FLAG(range, true);
    uint32_t range_size = sizeof(gravity_range_t);
    SET_OBJECT_VISITED_FLAG(range, false);
    
    return range_size;
}

void gravity_range_serialize (gravity_range_t *r, json_t *json) {
    const char *label = json_get_label(json, NULL);
    json_begin_object(json, label);
    json_add_cstring(json, GRAVITY_JSON_LABELTYPE, GRAVITY_JSON_RANGE);                    // MANDATORY 1st FIELD
    json_add_int(json, GRAVITY_JSON_LABELFROM, r->from);
    json_add_int(json, GRAVITY_JSON_LABELTO, r->to);
    json_end_object(json);
}

gravity_range_t *gravity_range_deserialize (gravity_vm *vm, json_value *json) {
    json_int_t from = 0;
    json_int_t to = 0;
    
    uint32_t n = json->u.object.length;
    for (uint32_t i=1; i<n; ++i) { // from 1 to skip type
        const char *label = json->u.object.values[i].name;
        json_value *value = json->u.object.values[i].value;
        size_t label_size = strlen(label);
        
        // from
        if (string_casencmp(label, GRAVITY_JSON_LABELFROM, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            from = value->u.integer;
            continue;
        }
        
        // to
        if (string_casencmp(label, GRAVITY_JSON_LABELTO, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            to = value->u.integer;
            continue;
        }
    }
    
    return gravity_range_new(vm, (gravity_int_t)from, (gravity_int_t)to, true);
    
abort_load:
    return NULL;
}

void gravity_range_blacken (gravity_vm *vm, gravity_range_t *range) {
    gravity_vm_memupdate(vm, gravity_range_size(vm, range));
}

// MARK: -

inline gravity_value_t gravity_string_to_value (gravity_vm *vm, const char *s, uint32_t len) {
    gravity_string_t *obj = mem_alloc(NULL, sizeof(gravity_string_t));
    if (len == AUTOLENGTH) len = (uint32_t)strlen(s);

    uint32_t alloc = MAXNUM(len+1, DEFAULT_MINSTRING_SIZE);
    char *ptr = mem_alloc(NULL, alloc);
    memcpy(ptr, s, len);

    obj->isa = gravity_class_string;
    obj->s = ptr;
    obj->len = len;
    obj->alloc = alloc;
    obj->hash = gravity_hash_compute_buffer((const char *)ptr, len);

    gravity_value_t value;
    value.isa = gravity_class_string;
    value.p = (gravity_object_t *)obj;

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) obj);
    return value;
}

inline gravity_string_t *gravity_string_new (gravity_vm *vm, char *s, uint32_t len, uint32_t alloc) {
    gravity_string_t *obj = mem_alloc(NULL, sizeof(gravity_string_t));
    if (len == AUTOLENGTH) len = (uint32_t)strlen(s);

    obj->isa = gravity_class_string;
    obj->s = (char *)s;
    obj->len = len;
    obj->alloc = (alloc) ? alloc : len;
    if (s && len) obj->hash = gravity_hash_compute_buffer((const char *)s, len);

    if (vm) gravity_vm_transfer(vm, (gravity_object_t*) obj);
    return obj;
}

inline void gravity_string_set (gravity_string_t *obj, char *s, uint32_t len) {
    obj->s = (char *)s;
    obj->len = len;
    obj->hash = gravity_hash_compute_buffer((const char *)s, len);
}

inline void gravity_string_free (gravity_vm *vm, gravity_string_t *value) {
    #pragma unused(vm)
    DEBUG_FREE("FREE %s", gravity_object_debug((gravity_object_t *)value, true));
    if (value->alloc) mem_free(value->s);
    mem_free(value);
}

uint32_t gravity_string_size (gravity_vm *vm, gravity_string_t *string) {
    #pragma unused(vm)
    SET_OBJECT_VISITED_FLAG(string, true);
    uint32_t string_size = (sizeof(gravity_string_t)) + string->alloc;
    SET_OBJECT_VISITED_FLAG(string, false);
    
    return string_size;
}

void gravity_string_blacken (gravity_vm *vm, gravity_string_t *string) {
    gravity_vm_memupdate(vm, gravity_string_size(vm, string));
}

inline gravity_value_t gravity_value_from_error(const char* msg) {
    return ((gravity_value_t){.isa = NULL, .p = ((gravity_object_t *)msg)});
}

inline gravity_value_t gravity_value_from_object(void *obj) {
    return ((gravity_value_t){.isa = (((gravity_object_t *)(obj))->isa), .p = (gravity_object_t *)(obj)});
}

inline gravity_value_t gravity_value_from_int(gravity_int_t n) {
    return ((gravity_value_t){.isa = gravity_class_int, .n = (n)});
}

inline gravity_value_t gravity_value_from_float(gravity_float_t f) {
    return ((gravity_value_t){.isa = gravity_class_float, .f = (f)});
}

inline gravity_value_t gravity_value_from_null(void) {
    return ((gravity_value_t){.isa = gravity_class_null, .n = 0});
}

inline gravity_value_t gravity_value_from_undefined(void) {
    return ((gravity_value_t){.isa = gravity_class_null, .n = 1});
}

inline gravity_value_t gravity_value_from_bool(bool b) {
    return ((gravity_value_t){.isa = gravity_class_bool, .n = (b)});
}
