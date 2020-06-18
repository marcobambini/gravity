//
//  debug_macros.h
//  gravity
//
//  Created by Marco Bambini on 30/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_CMACROS__
#define __GRAVITY_CMACROS__

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include "gravity_memory.h"
#include "gravity_config.h"

#define GRAVITY_LEXEM_DEBUG             0
#define GRAVITY_LEXER_DEGUB             0
#define GRAVITY_PARSER_DEBUG            0
#define GRAVITY_SEMA1_DEBUG             0
#define GRAVITY_SEMA2_DEBUG             0
#define GRAVITY_AST_DEBUG               0
#define GRAVITY_LOOKUP_DEBUG            0
#define GRAVITY_SYMTABLE_DEBUG          0
#define GRAVITY_CODEGEN_DEBUG           0
#define GRAVITY_OPCODE_DEBUG            0
#define GRAVITY_BYTECODE_DEBUG          0
#define GRAVITY_REGISTER_DEBUG          0
#define GRAVITY_FREE_DEBUG              0
#define GRAVITY_DESERIALIZE_DEBUG       0

#define PRINT_LINE(...)                 printf(__VA_ARGS__);printf("\n");fflush(stdout)

#if GRAVITY_LEXER_DEGUB
#define DEBUG_LEXER(l)                  gravity_lexer_debug(l)
#else
#define DEBUG_LEXER(...)
#endif

#if GRAVITY_LEXEM_DEBUG
#define DEBUG_LEXEM(...)                do { if (!lexer->peeking) { \
                                            printf("(%03d, %03d, %02d) ", lexer->token.lineno, lexer->token.colno, lexer->token.position); \
                                            PRINT_LINE(__VA_ARGS__);} \
                                        } while(0)
#else
#define DEBUG_LEXEM(...)
#endif

#if GRAVITY_PARSER_DEBUG
#define DEBUG_PARSER(...)               PRINT_LINE(__VA_ARGS__)
#else
#define gravity_parser_debug(p)
#define DEBUG_PARSER(...)
#endif

#if GRAVITY_SEMA1_DEBUG
#define DEBUG_SEMA1(...)                PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_SEMA1(...)
#endif

#if GRAVITY_SEMA2_DEBUG
#define DEBUG_SEMA2(...)                PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_SEMA2(...)
#endif

#if GRAVITY_LOOKUP_DEBUG
#define DEBUG_LOOKUP(...)               PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_LOOKUP(...)
#endif

#if GRAVITY_SYMTABLE_DEBUG
#define DEBUG_SYMTABLE(...)             printf("%*s",ident*4," ");PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_SYMTABLE(...)
#endif

#if GRAVITY_CODEGEN_DEBUG
#define DEBUG_CODEGEN(...)              PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_CODEGEN(...)
#endif

#if GRAVITY_OPCODE_DEBUG
#define DEBUG_OPCODE(...)               PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_OPCODE(...)
#endif

#if GRAVITY_BYTECODE_DEBUG
#define DEBUG_BYTECODE(...)             PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_BYTECODE(...)
#endif

#if GRAVITY_REGISTER_DEBUG
#define DEBUG_REGISTER(...)             PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_REGISTER(...)
#endif

#if GRAVITY_FREE_DEBUG
#define DEBUG_FREE(...)                 PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_FREE(...)
#endif

#if GRAVITY_DESERIALIZE_DEBUG
#define DEBUG_DESERIALIZE(...)          PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_DESERIALIZE(...)
#endif

#define DEBUG_ALWAYS(...)               PRINT_LINE(__VA_ARGS__)

#endif
