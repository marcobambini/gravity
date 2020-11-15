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

void gravity_file_register (gravity_vm *vm);
void gravity_file_free (void);
bool gravity_isfile_class (gravity_class_t *c);
const char *gravity_file_name (void);

#endif
