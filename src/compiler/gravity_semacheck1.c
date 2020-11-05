//
//  gravity_semacheck1.c
//  gravity
//
//  Created by Marco Bambini on 08/10/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_symboltable.h"
#include "gravity_semacheck1.h"
#include "gravity_compiler.h"
#include "gravity_visitor.h"

#define REPORT_ERROR(node,...)          {report_error(self, (gnode_t *)node, __VA_ARGS__); return;}

#define DECLARE_SYMTABLE()              symboltable_t *symtable = (symboltable_t *)self->data
#define SAVE_SYMTABLE()                 symboltable_t *saved = symtable
#define CREATE_SYMTABLE(tag)            INC_IDENT; SAVE_SYMTABLE(); self->data = (void *)symboltable_create(tag);
#define RESTORE_SYMTABLE()              DEC_IDENT; node->symtable = ((symboltable_t *)self->data); self->data = (void *)saved

#if GRAVITY_SYMTABLE_DEBUG
static int ident =0;
#define INC_IDENT        ++ident
#define DEC_IDENT        --ident
#else
#define INC_IDENT
#define DEC_IDENT
#endif

// MARK: -

static void report_error (gvisitor_t *self, gnode_t *node, const char *format, ...) {
    // TODO: add lasterror here like in semacheck2
    
    // increment internal error counter
    ++self->nerr;

    // sanity check
    if (!node) return;
    
    // get error callback (if any)
    void *data = (self->delegate) ? ((gravity_delegate_t *)self->delegate)->xdata : NULL;
    gravity_error_callback error_fn = (self->delegate) ? ((gravity_delegate_t *)self->delegate)->error_callback : NULL;

    // build error message
    char buffer[1024];
    va_list arg;
    if (format) {
        va_start (arg, format);
        vsnprintf(buffer, sizeof(buffer), format, arg);
        va_end (arg);
    }

    // setup error struct
    error_desc_t error_desc = {
        .lineno = node->token.lineno,
        .colno = node->token.colno,
        .fileid = node->token.fileid,
        .offset = node->token.position
    };

    // finally call error callback
    if (error_fn) error_fn(NULL, GRAVITY_ERROR_SEMANTIC, buffer, error_desc, data);
    else printf("%s\n", buffer);
}

// MARK: - Declarations -

static void visit_list_stmt (gvisitor_t *self, gnode_compound_stmt_t *node) {
    DECLARE_SYMTABLE();
    DEBUG_SYMTABLE("GLOBALS");

    node->symtable = symtable;    // GLOBALS
    gnode_array_each(node->stmts, {
        visit(val);
    });
}

static void visit_function_decl (gvisitor_t *self, gnode_function_decl_t *node) {
    DECLARE_SYMTABLE();
    DEBUG_SYMTABLE("function: %s", node->identifier);

    // function identifier
    const char *identifier = node->identifier;
    
    // reserve a special name for static objects defined inside a class
    // to avoid name collision between class ivar and meta-class ivar
    char buffer[512];
    if ((symboltable_tag(symtable) == SYMTABLE_TAG_CLASS) && node->storage == TOK_KEY_STATIC) {
        snprintf(buffer, sizeof(buffer), "$%s", identifier);
        identifier = (const char *)buffer;
    }
    
    // function identifier
    if (!symboltable_insert(symtable, identifier, (void *)node)) {
        REPORT_ERROR(node, "Identifier %s redeclared.", node->identifier);
    }

    // we are just interested in non-local declarations so don't further scan function node
    // node->symtable is NULL here and it will be created in semacheck2
}

static void visit_variable_decl (gvisitor_t *self, gnode_variable_decl_t *node) {
    DECLARE_SYMTABLE();
    char buffer[512];

    bool is_static = (node->storage == TOK_KEY_STATIC);
    gnode_array_each(node->decls, {
        gnode_var_t *p = (gnode_var_t *)val;
        
        // variable identifier
        const char  *identifier = p->identifier;
        
        // reserve a special name for static objects defined inside a class
        // to avoid name collision between class ivar and meta-class ivar
        if ((symboltable_tag(symtable) == SYMTABLE_TAG_CLASS) && is_static) {
            snprintf(buffer, sizeof(buffer), "$%s", identifier);
            identifier = (const char *)buffer;
        }
        
        if (!symboltable_insert(symtable, identifier, (void *)p)) {
            REPORT_ERROR(p, "Identifier %s redeclared.", p->identifier);
        }
        
        // in CLASS case set a relative ivar index (if ivar is not computed)
        if (symboltable_tag(symtable) == SYMTABLE_TAG_CLASS && p->iscomputed == false) {
            p->index = symboltable_setivar(symtable, is_static);
            DEBUG_SYMTABLE("ivar: %s index: %d", p->identifier, p->index);
        } else {
            DEBUG_SYMTABLE("variable: %s", p->identifier);
        }
    });
}

static void visit_enum_decl (gvisitor_t *self, gnode_enum_decl_t *node) {
    DECLARE_SYMTABLE();

    DEBUG_SYMTABLE("enum: %s", node->identifier);

    // check enum identifier uniqueness in current symbol table
    if (!symboltable_insert(symtable, node->identifier, (void *)node)) {
        REPORT_ERROR(node, "Identifier %s redeclared.", node->identifier);
    }
}

static void visit_class_decl (gvisitor_t *self, gnode_class_decl_t *node) {
    DECLARE_SYMTABLE();

    DEBUG_SYMTABLE("class: %s", node->identifier);

    // class identifier
    const char *identifier = node->identifier;
    
    // reserve a special name for static objects defined inside a class
    // to avoid name collision between class ivar and meta-class ivar
    char buffer[512];
    if ((symboltable_tag(symtable) == SYMTABLE_TAG_CLASS) && node->storage == TOK_KEY_STATIC) {
        snprintf(buffer, sizeof(buffer), "$%s", identifier);
        identifier = (const char *)buffer;
    }
    
    // class identifier
    if (!symboltable_insert(symtable, identifier, (void *)node)) {
        REPORT_ERROR(node, "Identifier %s redeclared.", node->identifier);
    }

    CREATE_SYMTABLE(SYMTABLE_TAG_CLASS);
    gnode_array_each(node->decls, {
        visit(val);
    });
    RESTORE_SYMTABLE();
}

static void visit_module_decl (gvisitor_t *self, gnode_module_decl_t *node) {
    DECLARE_SYMTABLE();

    DEBUG_SYMTABLE("module: %s", node->identifier);

    // module identifier
    if (!symboltable_insert(symtable, node->identifier, (void *)node)) {
        REPORT_ERROR(node, "Identifier %s redeclared.", node->identifier);
    }

    CREATE_SYMTABLE(SYMTABLE_TAG_MODULE);
    gnode_array_each(node->decls, {
        visit(val);
    });
    RESTORE_SYMTABLE();
}

// MARK: -

bool gravity_semacheck1 (gnode_t *node, gravity_delegate_t *delegate) {
    symboltable_t *context = symboltable_create(SYMTABLE_TAG_GLOBAL);

    gvisitor_t visitor = {
        .nerr = 0,                            // used to store number of found errors
        .data = (void *)context,            // used to store a pointer to the global symbol table
        .delegate = (void *)delegate,        // compiler delegate to report errors

        // COMMON
        .visit_pre = NULL,
        .visit_post = NULL,

        // STATEMENTS: 7
        .visit_list_stmt = visit_list_stmt,
        .visit_compound_stmt = NULL,
        .visit_label_stmt = NULL,
        .visit_flow_stmt = NULL,
        .visit_loop_stmt = NULL,
        .visit_jump_stmt = NULL,
        .visit_empty_stmt = NULL,

        // DECLARATIONS: 5
        .visit_function_decl = visit_function_decl,
        .visit_variable_decl = visit_variable_decl,
        .visit_enum_decl = visit_enum_decl,
        .visit_class_decl = visit_class_decl,
        .visit_module_decl = visit_module_decl,

        // EXPRESSIONS: 8
        .visit_binary_expr = NULL,
        .visit_unary_expr = NULL,
        .visit_file_expr = NULL,
        .visit_literal_expr = NULL,
        .visit_identifier_expr = NULL,
        .visit_keyword_expr = NULL,
        .visit_list_expr = NULL,
        .visit_postfix_expr = NULL,
    };

    DEBUG_SYMTABLE("=== SYMBOL TABLE ===");
    gvisit(&visitor, node);
    DEBUG_SYMTABLE("====================\n");

    return (visitor.nerr == 0);
}
