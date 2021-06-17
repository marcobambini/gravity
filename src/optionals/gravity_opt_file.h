//
//  gravity_opt_file.h
//  gravity
//
//  Created by Marco Bambini on 15/11/2020.
//  Copyright © 2020 Creolabs. All rights reserved.
//

#ifndef __GRAVITY_FILE__
#define __GRAVITY_FILE__

#define GRAVITY_CLASS_FILE_NAME             "File"

#include "gravity_value.h"

GRAVITY_API void gravity_file_register (gravity_vm *vm);
GRAVITY_API void gravity_file_free (void);
GRAVITY_API bool gravity_isfile_class (gravity_class_t *c);
GRAVITY_API const char *gravity_file_name (void);

#endif
