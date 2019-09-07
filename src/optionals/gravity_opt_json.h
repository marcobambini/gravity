//
//  gravity_opt_json.h
//  gravity
//
//  Created by Marco Bambini on 20/08/2019.
//  Copyright © 2019 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_JSON__
#define __GRAVITY_JSON__

#define GRAVITY_CLASS_JSON_NAME         "JSON"

#include "gravity_value.h"

void gravity_json_register (gravity_vm *vm);
void gravity_json_free (void);
bool gravity_isjson_class (gravity_class_t *c);
const char *gravity_json_name (void);

#endif
