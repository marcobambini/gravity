//
//  console.h
//  gravity
//
//  Created by Marco Bambini on 23/02/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_CONSOLE__
#define __GRAVITY_CONSOLE__

#include <stdio.h>
#include "gravity_memory.h"
#include "gravity_delegate.h"

const char *current_filepath (const char *base, const char *file);
void report_error (gravity_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata);
void report_log (const char *message, void *xdata);

#endif
