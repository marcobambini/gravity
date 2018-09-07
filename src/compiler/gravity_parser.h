//
//  gravity_parser.h
//  gravity
//
//  Created by Marco Bambini on 01/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_PARSER__
#define __GRAVITY_PARSER__

#include "gravity_compiler.h"
#include "debug_macros.h"
#include "gravity_ast.h"

/*
    Parser is responsible to build the AST, convert strings and number from tokens and
    implement syntax error recovery strategy.

    Notes about error recovery:
    Each parse* function can return NULL in case of error but each function is RESPONSIBLE
    to make appropriate actions in order to handle/recover errors.

    Error recovery techniques can be:
    Shallow Error Recovery
    Deep Error Recovery
    https://javacc.java.net/doc/errorrecovery.html

 */

// opaque datatype
typedef struct gravity_parser_t    gravity_parser_t;

// public functions
gravity_parser_t    *gravity_parser_create (const char *source, size_t len, uint32_t fileid, bool is_static);
gnode_t             *gravity_parser_run (gravity_parser_t *parser, gravity_delegate_t *delegate);
void                gravity_parser_free (gravity_parser_t *parser);

#endif
