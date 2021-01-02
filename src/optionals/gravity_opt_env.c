/**
 * @file
 * This file provides the environment class (`ENV`).
 * It utilizes a couple of custom overloads to enhance usage and mimic the 
 * usage within other scripting and programming languages.
 */

#include <stdlib.h>
#include <string.h>

#include "gravity_vm.h"
#include "gravity_core.h"
#include "gravity_hash.h"
#include "gravity_utils.h"
#include "gravity_macros.h"
#include "gravity_vmmacros.h"
#include "gravity_opcodes.h"
#include "gravity_debug.h"

#include "gravity_opt_env.h"

#if defined(_WIN32)
#define setenv(_key, _value_, _unused)      _putenv_s(_key, _value_)
#define unsetenv(_key)                      _putenv_s(_key, "")
#endif

static gravity_class_t              *gravity_class_env = NULL;
static uint32_t                     refcount = 0;

static int                          argc = -1;
static gravity_list_t               *argv = NULL;

/**
 * Wraps `getenv()` to be used with Gravity.
 *
 */
static bool gravity_env_get(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    if(!VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("Environment variable key must be a string.");
    }

    char *key = VALUE_AS_CSTRING(args[1]);
    char *value = getenv(key);
    gravity_value_t rt = VALUE_FROM_UNDEFINED;

    // GRAVITY_DEBUG_PRINT("[ENV::GET args : %i] %s => %s\n", nargs, key, value);

    if (value) {
        rt = VALUE_FROM_STRING(vm, value, (uint32_t)strlen(value));
    }

    RETURN_VALUE(rt, rindex);
}

/**
 * @brief  Wraps putenv() into a Gravity callable function
 * @param  vm The Gravity Virtual Maschine this function is associated with.
 * @param  args List of arguments passed to this function
 * @param  nargs Number of arguments passed to this function
 * @param  rindex Slot-index for the return value to be stored in.
 * @retval  Weather this function was successful or not.
 */
static bool gravity_env_set(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    if(!VALUE_ISA_STRING(args[1]) || (!VALUE_ISA_STRING(args[2]) && !VALUE_ISA_NULL(args[2]))) {
        RETURN_ERROR("Environment variable key and value must both be strings.");
    }

    gravity_string_t *key = VALUE_AS_STRING(args[1]);
    gravity_string_t *value = (VALUE_ISA_STRING(args[2])) ? VALUE_AS_STRING(args[2]) : NULL;

    // GRAVITY_DEBUG_PRINT("[ENV::SET args : %i] %s => %s\n", nargs, key, value);

    int rt = (value) ? setenv(key->s, value->s, 1) : unsetenv(key->s);
    RETURN_VALUE(VALUE_FROM_INT(rt), rindex);
}

static bool gravity_env_keys(gravity_vm *vm, gravity_value_t *args, uint16_t nparams, uint32_t rindex) {
    #pragma unused(args, nparams)
    
    extern char **environ;
    gravity_list_t *keys = gravity_list_new(vm, 16);
    
    for (char **env = environ; *env; ++env) {
        char *entry = *env;
        
        // env is in the form key=value
        uint32_t len = 0;
        for (uint32_t i=0; entry[len]; ++i, ++len) {
            if (entry[i] == '=') break;
        }
        gravity_value_t key = VALUE_FROM_STRING(vm, entry, len);
        marray_push(gravity_value_t, keys->array, key);
    }

    RETURN_VALUE(VALUE_FROM_OBJECT(keys), rindex);
}

static bool gravity_env_argc(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
   #pragma unused(vm, args, nargs)
   RETURN_VALUE((argc != -1) ? VALUE_FROM_INT(argc) : VALUE_FROM_NULL, rindex);
}

static bool gravity_env_argv(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
   #pragma unused(vm, args, nargs)
   RETURN_VALUE((argv) ? VALUE_FROM_OBJECT(argv) : VALUE_FROM_NULL, rindex);
}

// MARK: - Internals -

static void create_optional_class (void) {
    gravity_class_env = gravity_class_new_pair(NULL, GRAVITY_CLASS_ENV_NAME, NULL, 0, 0);
    gravity_class_t *meta = gravity_class_get_meta(gravity_class_env);
    
    // .get(key) and .set(key, value)
    gravity_class_bind(meta, "get", NEW_CLOSURE_VALUE(gravity_env_get));
    gravity_class_bind(meta, "set", NEW_CLOSURE_VALUE(gravity_env_set));
    gravity_class_bind(meta, "keys", NEW_CLOSURE_VALUE(gravity_env_keys));
    
    // Allow map-access
    gravity_class_bind(meta, GRAVITY_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(gravity_env_get));
    gravity_class_bind(meta, GRAVITY_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(gravity_env_set));

    gravity_closure_t *closure = NULL;
    closure = computed_property_create(NULL, NEW_FUNCTION(gravity_env_argc), NULL);
    gravity_class_bind(meta, "argc", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(gravity_env_argv), NULL);
    gravity_class_bind(meta, "argv", VALUE_FROM_OBJECT(closure));

    SETMETA_INITED(gravity_class_env);
}

// MARK: - Commons -

void gravity_env_register(gravity_vm *vm) {
    if (!gravity_class_env) create_optional_class();
    ++refcount;
    
    if (!vm || gravity_vm_ismini(vm)) return;
    gravity_vm_setvalue(vm, GRAVITY_CLASS_ENV_NAME, VALUE_FROM_OBJECT(gravity_class_env));
}

void gravity_env_register_args(gravity_vm *vm, uint32_t _argc, const char **_argv) {
    argc = _argc;
    argv = gravity_list_new(vm, argc);
    for (int i = 0; i < _argc; ++i) {
        gravity_value_t arg = VALUE_FROM_CSTRING(vm, _argv[i]);
        marray_push(gravity_value_t, argv->array, arg);
    }
}

void gravity_env_free (void) {
    if (!gravity_class_env) return;
    if (--refcount) return;

    gravity_class_t *meta = gravity_class_get_meta(gravity_class_env);
    computed_property_free(meta, "argc", true);
    computed_property_free(meta, "argv", true);
    gravity_class_free_core(NULL, meta);
    gravity_class_free_core(NULL, gravity_class_env);

    gravity_class_env = NULL;
}

bool gravity_isenv_class (gravity_class_t *c) {
    return (c == gravity_class_env);
}

const char *gravity_env_name (void) {
    return GRAVITY_CLASS_ENV_NAME;
}
