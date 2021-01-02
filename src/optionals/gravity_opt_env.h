#ifndef GRAVITY_ENV_H
#define GRAVITY_ENV_H

#define GRAVITY_CLASS_ENV_NAME "ENV"

#include "gravity_value.h"

void gravity_env_register (gravity_vm *vm);
void gravity_env_register_args(gravity_vm *vm, uint32_t _argc, const char **_argv);
void gravity_env_free (void);
bool gravity_isenv_class (gravity_class_t *c);
const char *gravity_env_name (void);

#endif
