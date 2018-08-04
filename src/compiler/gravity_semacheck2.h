//
//  gravity_semacheck2.h
//  gravity
//
//  Created by Marco Bambini on 12/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_SEMACHECK2__
#define __GRAVITY_SEMACHECK2__

#include "gravity_ast.h"
#include "gravity_delegate.h"

// Responsible to gather and check local identifiers
// Complete check for all identifiers and report not found errors

bool gravity_semacheck2(gnode_t *node, gravity_delegate_t *delegate);

/*

    The following table summarizes what can be defined inside a declaration:

    -------+---------------------------------------------------------+
           |   func   |   var   |   enum   |   class   |   module    |
    -------+---------------------------------------------------------+
    func   |   YES    |   YES   |   NO     |   YES     |   YES       |
    -------+---------------------------------------------------------+
    var    |   YES    |   NO    |   NO     |   YES     |   YES       |
    -------+---------------------------------------------------------+
    enum   |   YES    |   NO    |   NO     |   YES     |   YES       |
    -------+---------------------------------------------------------+
    class  |   YES    |   NO    |   NO     |   YES     |   YES       |
    -------+---------------------------------------------------------+
    module |   NO     |   NO    |   NO     |   NO      |   NO        |
    -------+---------------------------------------------------------+

    Everything declared inside a func is a local, so for example:

    func foo {
        func a...;
        enum b...;
        class c..;
    }

    is converted by codegen to:

    func foo {
        var a = func...;
        var b = enum...;
        var c = class..;
    }

    Even if the ONLY valid syntax is anonymous func assignment, user will not be able
    to assign an anonymous enum or class to a variable. Restriction is applied by parser
    and reported as a syntax error.
    Define a module inside a function is not allowed (no real technical reason but I found
    it a very bad programming practice), restriction is applied by samantic checker.

 */


// TECH NOTE:
// At the end of semacheck2:
//
// Each declaration and compound statement will have its own symbol table (symtable field)
// symtable in:
// NODE_LIST_STAT and NODE_COMPOUND_STAT
// FUNCTION_DECL and FUNCTION_EXPR
// ENUM_DECL
// CLASS_DECL
// MODULE_DECL
//
// Each identifier will have a reference to its declaration (symbol field)
// symbol field in:
// NODE_FILE
// NODE_IDENTIFIER
// NODE_ID
//
// Each declaration will have a reference to its enclosing declaration (env field)
// env field in:
// FUNCTION_DECL and FUNCTION_EXPR
// VARIABLE
// ENUM_DECL
// CLASS_DECL
// MODULE_DECL

#endif
