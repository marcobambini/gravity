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

#include "gravity_env.h"

#define ENV_CLASS_NAME "ENV"

/**
 * Wraps `getenv()` to be used with Gravity.
 * 
 * @param 
 */
bool gravity_env_get(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if(!VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("Environment variable key must be a string.");
    }

    char* key = VALUE_AS_CSTRING(args[1]);
    char* value = getenv(key);
    gravity_value_t rt;

    GRAVITY_DEBUG_PRINT("[ENV::GET args : %i] %s => %s\n", nargs, key, value);

    if (value == NULL) {
        rt = VALUE_FROM_UNDEFINED;
    } else {
        rt = VALUE_FROM_STRING(vm, value, strlen(value));
    }

    RETURN_VALUE(rt, rindex);
}

/**
 * @brief  Wraps putenv() into a Gravity callable function
 * @param  *vm: The Gravity Virtual Maschine this function is associated with.
 * @param  *args: List of arguments passed to this function
 * @param  nargs: Number of arguments passed to this function
 * @param  rindex: Slot-index for the return value to be stored in.
 * @retval  Weather this function was successful or not.
 */
bool gravity_env_set(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    if(!VALUE_ISA_STRING(args[1]) || !VALUE_ISA_STRING(args[2])) {
        RETURN_ERROR("Environment variable key and value must both be strings.");
    }

    gravity_string_t *key_var, *value_var;
    key_var = VALUE_AS_STRING(args[1]);
    value_var = VALUE_AS_STRING(args[2]);

    uint32_t len = key_var->alloc + value_var->alloc + 1;
    char *buf = (char *)mem_alloc(vm, len);
    snprintf(buf, len, "%s=%s", key_var->s, value_var->s);

    GRAVITY_DEBUG_PRINT(
        "[ENV::SET args : %i] (%.*s) \"%.*s\" => \"%.*s\"\n",
        nargs, (int)len, buf,
        key_var->len, key_var->s,
        value_var->len, value_var->s
    );

    int rt = putenv(buf);
    mem_free(buf);

    RETURN_VALUE(VALUE_FROM_INT(rt), rindex);
}

bool gravity_env_keys(gravity_vm *vm, gravity_value_t *args, uint16_t nparams, uint32_t rindex) {
    extern char **environ;
    char *evar = *environ;
    gravity_list_t *keys = gravity_list_new(vm, 1);

    for (int i = 1; evar != NULL; i++) {
        char key[strlen(evar)];
        uint16_t len;
        for (len = 0; evar[len] != '='; len++) {
            key[len] = evar[len];
        }
        marray_push(
            gravity_value_t, keys->array,
            VALUE_FROM_STRING(vm, key, len)
        );
        evar = *(environ + i);
    }

    RETURN_VALUE(VALUE_FROM_OBJECT(keys), rindex);
}

void gravity_env_register(gravity_vm *vm) {
    gravity_class_t *c = gravity_class_new_pair(vm, ENV_CLASS_NAME, NULL, 0, 0);
    gravity_class_t *m = gravity_class_get_meta(c);

    // .get(key) and .set(key, value)
    gravity_class_bind(m, "get", NEW_CLOSURE_VALUE(gravity_env_get));
    gravity_class_bind(m, "set", NEW_CLOSURE_VALUE(gravity_env_set));
    gravity_class_bind(m, "keys", NEW_CLOSURE_VALUE(gravity_env_keys));

    // Allow map-access
    gravity_class_bind(m, GRAVITY_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(gravity_env_get));
    gravity_class_bind(m, GRAVITY_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(gravity_env_set));

    // @TODO: add iteration support

    // Install
    gravity_vm_setvalue(vm, ENV_CLASS_NAME, VALUE_FROM_OBJECT(c));
}