//
//  semacheck1.h
//  gravity
//
//  Created by Marco Bambini on 08/10/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_SEMACHECK1__
#define __GRAVITY_SEMACHECK1__

// Semantic check step 1 does not have the notion of context and scope.
// It just gathers non-local names into a symbol table and check for uniqueness.
//
// Only declarations (non-locals) are visited and a symbol table is created.
//
//
// In order to debug symbol table just enable the GRAVITY_SYMTABLE_DEBUG macro
// in debug_macros.h

// Semantic Check Step 1 enables to resolve cases like:
/*
        function foo() {
            return bar();
        }

        function bar() {
            ...
        }

        or

        class foo:bar {
            ...
        }

        class bar {
            ...
        }

        and

        class foo {
            var a;

            function bar() {
                return a + b;
            }

            var b;
        }
 */

// It's a mandatory step in order to account for forward references
// allowed in any non-local declaration

#include "gravity_ast.h"
#include "gravity_delegate.h"

bool gravity_semacheck1(gnode_t *node, gravity_delegate_t *delegate);

#endif
