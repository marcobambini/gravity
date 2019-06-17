//
//  gravity_http.h
//  gravity
//
//  Created by Marco Bambini on 14/08/2017.
//  Copyright © 2017 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_HTTP__
#define __GRAVITY_HTTP__

#include "gravity_value.h"

void gravity_http_register (gravity_vm *vm);
void gravity_http_free (void);
bool gravity_ishttp_class (gravity_class_t *c);
const char *gravity_http_name (void);

#endif
