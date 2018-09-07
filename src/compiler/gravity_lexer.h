//
//  gravity_lexer.h
//  gravity
//
//  Created by Marco Bambini on 30/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_LEXER__
#define __GRAVITY_LEXER__

#include "gravity_delegate.h"
#include "gravity_token.h"
#include "debug_macros.h"

/*
    Lexer is built in such a way that no memory allocations are necessary during usage
    (except for the gravity_lexer_t opaque datatype allocated within gravity_lexer_create).

    Example:
    gravity_lexer *lexer = gravity_lexer_create(...);
    while (gravity_lexer_next(lexer)) {
        // do something here
    }
    gravity_lexer_free(lexer);

    gravity_lexer_next (and gravity_lexer_peek) returns an int token (gtoken_t)
    which represents what has been currently scanned. When EOF is reached TOK_EOF is
    returned (with value 0) and the while loop exits.

    In order to have token details, gravity_lexer_token must be called.
    In case of a scan error TOK_ERROR is returned and error details can be extracted
    from the token itself. In order to be able to not allocate any memory during
    tokenization STRINGs and NUMBERs are just sanity checked but not converted.
    It is parser responsability to perform the right conversion.

 */

// opaque datatype
typedef struct gravity_lexer_t gravity_lexer_t;

// public functions
gravity_lexer_t     *gravity_lexer_create (const char *source, size_t len, uint32_t fileid, bool is_static);
void                gravity_lexer_setdelegate (gravity_lexer_t *lexer, gravity_delegate_t *delegate);
void                gravity_lexer_free (gravity_lexer_t *lexer);

gtoken_t            gravity_lexer_peek (gravity_lexer_t *lexer);
gtoken_t            gravity_lexer_next (gravity_lexer_t *lexer);
gtoken_s            gravity_lexer_token (gravity_lexer_t *lexer);
gtoken_s            gravity_lexer_token_next (gravity_lexer_t *lexer);
gtoken_t            gravity_lexer_token_type (gravity_lexer_t *lexer);
void                gravity_lexer_token_dump (gtoken_s token);
void                gravity_lexer_skip_line (gravity_lexer_t *lexer);
uint32_t            gravity_lexer_lineno (gravity_lexer_t *lexer);

#if GRAVITY_LEXER_DEGUB
void                gravity_lexer_debug (gravity_lexer_t *lexer);
#endif

#endif
