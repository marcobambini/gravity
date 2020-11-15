//
//  gravity_semacheck2.c
//  gravity
//
//  Created by Marco Bambini on 12/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_symboltable.h"
#include "gravity_semacheck2.h"
#include "gravity_compiler.h"
#include "gravity_visitor.h"
#include "gravity_core.h"

struct semacheck_t {
    gnode_r         *declarations;      // declarations stack
    uint16_r        statements;         // statements stack
    uint32_t        lasterror;          // last error line number to prevent reporting more than one error per line
};
typedef struct semacheck_t semacheck_t;

#define REPORT_ERROR(node,...)          report_error(self, GRAVITY_ERROR_SEMANTIC, (gnode_t *)node, __VA_ARGS__);
#define REPORT_WARNING(node,...)        report_error(self, GRAVITY_WARNING, (gnode_t *)node, __VA_ARGS__)

#define STATEMENTS                      ((semacheck_t *)self->data)->statements
#define PUSH_STATEMENT(stat)            marray_push(uint16_t, STATEMENTS, (uint16_t)stat)
#define POP_STATEMENT()                 marray_pop(STATEMENTS)
#define TOP_STATEMENT()                 (marray_size(STATEMENTS) ? marray_last(STATEMENTS) : 0)
#define TOP_STATEMENT_ISA(stat)         (TOP_STATEMENT() == stat)
#define TOP_STATEMENT_ISA_SWITCH()      (TOP_STATEMENT_ISA(TOK_KEY_SWITCH))
#define TOP_STATEMENT_ISA_LOOP()        ((TOP_STATEMENT_ISA(TOK_KEY_WHILE))        ||        \
                                        (TOP_STATEMENT_ISA(TOK_KEY_REPEAT))        ||        \
                                        (TOP_STATEMENT_ISA(TOK_KEY_FOR)))

#define PUSH_DECLARATION(node)          gnode_array_push(((semacheck_t *)self->data)->declarations, (gnode_t*)node)
#define POP_DECLARATION()               gnode_array_pop(((semacheck_t *)self->data)->declarations)
#define TOP_DECLARATION()               gnode_array_get(((semacheck_t *)self->data)->declarations, gnode_array_size(((semacheck_t *)self->data)->declarations)-1)
#define ISA(n1,_tag)                    ((n1) ? (((gnode_t *)n1)->tag == _tag) : 0)
#define ISA_CLASS(n1)                   (((gnode_t *)n1)->tag == NODE_CLASS_DECL)
#define ISA_VAR_DECLARATION(n1)         (((gnode_t *)n1)->tag == NODE_VARIABLE_DECL)
#define ISA_VARIABLE(n1)                (((gnode_t *)n1)->tag == NODE_VARIABLE)
#define ISA_LITERAL(n1)                 (((gnode_t *)n1)->tag == NODE_LITERAL_EXPR)
#define ISA_IDENTIFIER(n1)              (((gnode_t *)n1)->tag == NODE_IDENTIFIER_EXPR)
#define ISA_ID(n1)                      (((gnode_t *)n1)->tag == NODE_ID_EXPR)

#define SET_LOCAL_INDEX(var, symtable)  var->index = (uint16_t)symboltable_local_index(symtable)
#define SET_NODE_LOCATION(_node, _type, _idx, _nup) _node->location.type = _type; _node->location.index = _idx; _node->location.nup = _nup;

// MARK: -

static void report_error (gvisitor_t *self, error_type_t error_type, gnode_t *node, const char *format, ...) {
    semacheck_t *current = (semacheck_t *)self->data;
    
    // check last error line in order to prevent to emit multiple errors for the same row
    if (node && node->token.lineno == current->lasterror) return;
    
    // increment internal error counter (and save last reported line) only if it was a real error
    if (error_type != GRAVITY_WARNING) {
        ++self->nerr;
        current->lasterror = (node) ? node->token.lineno : 0;
    }

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
        .lineno = (node) ? node->token.lineno : 0,
        .colno = (node) ? node->token.colno : 0,
        .fileid = (node) ? node->token.fileid : 0,
        .offset = (node) ? node->token.position : 0
    };

    // finally call error callback
    if (error_fn) error_fn(NULL, error_type, buffer, error_desc, data);
    else printf("%s\n", buffer);
}

static symboltable_t *symtable_from_node (gnode_t *node) {
    // globals
    if (ISA(node, NODE_LIST_STAT)) return ((gnode_compound_stmt_t *)node)->symtable;

    // class symtable
    if (ISA(node, NODE_CLASS_DECL)) return ((gnode_class_decl_t *)node)->symtable;

    // enum symtable
    if (ISA(node, NODE_ENUM_DECL)) return ((gnode_enum_decl_t *)node)->symtable;

    // module symtable
    if (ISA(node, NODE_MODULE_DECL)) return ((gnode_module_decl_t *)node)->symtable;

    // function symtable
    if (ISA(node, NODE_FUNCTION_DECL)) return ((gnode_function_decl_t *)node)->symtable;

    // should never reach this point
    return NULL;
}

// lookup identifier into node
static gnode_t *lookup_node (gnode_t *node, const char *identifier) {
    symboltable_t *symtable = symtable_from_node(node);
    if (!symtable) return NULL;
    return symboltable_lookup(symtable, identifier);
}

// lookup an identifier into a stack of symbol tables
// location inside node is updated with the result
// and node found is returned
static gnode_t *lookup_identifier (gvisitor_t *self, const char *identifier, gnode_identifier_expr_t *node) {
    gnode_r *decls = ((semacheck_t *)self->data)->declarations;
    size_t len = gnode_array_size(decls);
    if (len == 0) return NULL;

    uint16_t nf = 0; // number of functions traversed
    uint16_t nc = 0; // number of classes traversed
    char buffer[512];

    // get first node (the latest in the decls stack)
    gnode_t *base_node = gnode_array_get(decls, len-1);
    bool base_is_class = ISA(base_node, NODE_CLASS_DECL);
    bool base_is_static_function = (ISA(base_node, NODE_FUNCTION_DECL) && ((gnode_function_decl_t*)base_node)->storage == TOK_KEY_STATIC);

    for (int i=(int)len-1; i>=0; --i) {
        gnode_t *target = gnode_array_get(decls, i);

        // identify target type
        bool target_is_global = ISA(target, NODE_LIST_STAT);
        bool target_is_function = ISA(target, NODE_FUNCTION_DECL);
        bool target_is_class = ISA(target, NODE_CLASS_DECL);
        bool target_is_module = ISA(target, NODE_MODULE_DECL);

        // count number of traversed func/class
        if (target_is_function) ++nf;
        else if (target_is_class) ++nc;

        // if identifier has been declared in a static func
        // and lookup target is a class, then use its special
        // reserved name to perform the lookup
        const char *id = identifier;
        if (base_is_static_function && target_is_class) {
            snprintf(buffer, sizeof(buffer), "$%s", identifier);
            id = (const char *)buffer;
        }
        
        // lookup identifier is current target (obtained traversing the declaration stack)
        gnode_t *symbol = lookup_node(target, id);

        // sanity check: if base_node is a class and symbol was found inside a func then report an error
        if (symbol && target_is_function && base_is_class) {
            // added to explicitly prevent cases like:
            /*
                func foo() {
                    var a;
                    class b {
                        func bar() {return a;}
                    }
                }
             */
            REPORT_ERROR(node, "Unable to access local func var %s from within a class.", identifier);
            return NULL;
        }

        // if target is class and symbol is not found then lookup also its superclass hierarchy
        if (!symbol && target_is_class) {
            // lookup identifier in super (if not found target class)
            gnode_class_decl_t *c = (gnode_class_decl_t *)target;
            gnode_class_decl_t *super = (gnode_class_decl_t *)c->superclass;
            if (super && !NODE_ISA(super, NODE_CLASS_DECL)) {
                REPORT_ERROR(node, "Cannot set superclass of %s to non class object.", c->identifier);
                return NULL;
            }

            while (super) {
                symbol = lookup_node((gnode_t *)super, identifier);
                if (symbol) {
                    if (NODE_ISA(symbol, NODE_VARIABLE)) {
                        gnode_var_t *p = (gnode_var_t *)symbol;
                        if (p->access == TOK_KEY_PRIVATE) {
                            REPORT_ERROR(node, "Forbidden access to private ivar %s from a subclass.", p->identifier);
                            return NULL;
                        }
                    }
                    break;
                }
                super = (gnode_class_decl_t *)super->superclass;
            }
        }

        // continue lookup in declaration stack is symbol is not found
        if (!symbol) continue;

        // symbol found so process it bases on target type
        if (target_is_global) {
            DEBUG_LOOKUP("Identifier %s found in GLOBALS", identifier);

            // identifier found in global no other information is needed
            if (node) {
                SET_NODE_LOCATION(node, LOCATION_GLOBAL, 0, 0);
                node->symbol = symbol;
            }

            return symbol;
        }

        // if symbol is a variable then copy its index
        uint16_t index = UINT16_MAX;
        if (NODE_ISA(symbol, NODE_VARIABLE)) {
            gnode_var_t *p = (gnode_var_t *)symbol;
            if (!p->iscomputed) index = p->index;
        }

        if (target_is_function) {
            // Symbol found in a function
            if (nf > 1) {
                assert(ISA(base_node, NODE_FUNCTION_DECL));

                // symbol is upvalue and its index represents an index inside uplist
                gnode_var_t *var = (gnode_var_t *)symbol;
                gnode_function_decl_t *f = ((gnode_function_decl_t *)base_node);
                uint16_t n = nf - 1;
                gupvalue_t *upvalue = gnode_function_add_upvalue(f, var, n);

                // add upvalue to all enclosing functions
                // base_node has index = len - 1 so from (len - 2) up to n-1 levels
                uint16_t idx = (uint16_t)(len - 2);
                while (n > 1) {
                    gnode_t *enc_node = gnode_array_get(decls, idx);
                    if (!(ISA(enc_node, NODE_FUNCTION_DECL))) {
                        REPORT_ERROR(node, "An error occurred while setting upvalue for enclosing functions.");
                        return NULL;
                    }
                    gnode_function_add_upvalue((gnode_function_decl_t *)enc_node, var, --n);
                    --idx;
                }

                var->upvalue = true;
                node->upvalue = upvalue;
                SET_NODE_LOCATION(node, LOCATION_UPVALUE, index, nf);
            } else {
                // symbol is local
                SET_NODE_LOCATION(node, LOCATION_LOCAL, index, nf);
            }
            DEBUG_LOOKUP("Identifier %s found in FUNCTION %s (nf: %d index: %d)", identifier, ((gnode_function_decl_t *)target)->identifier, nf-1, index);
        }
        else if (target_is_class) {
            // Symbol found in a class
            SET_NODE_LOCATION(node, (nc == 1) ? LOCATION_CLASS_IVAR_SAME : LOCATION_CLASS_IVAR_OUTER, index, nc-1);
            DEBUG_LOOKUP("Identifier %s found in CLASS %s (up to %d outer levels)", identifier, ((gnode_class_decl_t *)target)->identifier, nc-1);
        }
        else if (target_is_module) {
            // Symbol found in a module
            // Module support not yet ready
        }
        else {
            // Should never reach this point
            assert(0);
        }

        node->symbol = symbol;
        return symbol;
    }

    DEBUG_LOOKUP("Identifier %s NOT FOUND\n", identifier);
    return NULL;
}

static gnode_t *lookup_symtable_id (gvisitor_t *self, gnode_identifier_expr_t *id, bool isclass) {
    gnode_t *target = NULL;

    gnode_t *target1 = lookup_identifier(self, id->value, id);
    if (!target1) {REPORT_ERROR((gnode_t *)id, "%s %s not found.", (isclass) ? "Class" : "Protocol", id->value); return NULL;}
    target = target1;

    if (id->value2) {
        gnode_t *target2 = lookup_node(target1, id->value2);
        if (!target2) {REPORT_ERROR((gnode_t *)id, "%s %s not found in %s.", (isclass) ? "Class" : "Protocol", id->value2, id->value); return NULL;}
        target = target2;
    }

    return target;
}

// MARK: -

static bool is_expression (gnode_t *node) {
    gnode_n tag = NODE_TAG(node);
    return ((tag >= NODE_BINARY_EXPR) && (tag <= NODE_ACCESS_EXPR)) || (tag == NODE_FLOW_STAT);
}

static bool is_expression_assignment (gnode_t *node) {
    if (!node) return false;
    gnode_n tag = NODE_TAG(node);
    if (tag == NODE_BINARY_EXPR) {
        gnode_binary_expr_t *expr = (gnode_binary_expr_t *)node;
        return (expr->op == TOK_OP_ASSIGN);
    }
    return false;
}

static bool is_expression_range (gnode_t *node) {
    gnode_n tag = NODE_TAG(node);
    if (tag == NODE_BINARY_EXPR) {
        gnode_binary_expr_t *expr = (gnode_binary_expr_t *)node;
        return ((expr->op == TOK_OP_RANGE_INCLUDED) || (expr->op == TOK_OP_RANGE_EXCLUDED));
    }
    return false;
}


static bool is_expression_valid (gnode_t *node) {
    if (!node) return false;

    /*
        From: http://c2.com/cgi/wiki?FirstClass

     |      Class of value
        Manipulation                   | First    Second    Third
        ===============================+================================
        Pass value as a parameter      | yes      yes       no
        Return value from a procedure  | yes      no        no
        Assign value into a variable   | yes      no        no

     */

    /*

     NODE_LIST_STAT, NODE_COMPOUND_STAT, NODE_LABEL_STAT, NODE_FLOW_STAT, NODE_JUMP_STAT,
     NODE_LOOP_STAT, NODE_EMPTY_STAT,

     // declarations: 6
     NODE_ENUM_DECL, NODE_FUNCTION_DECL, NODE_VARIABLE_DECL, NODE_CLASS_DECL, NODE_MODULE_DECL,
     NODE_VARIABLE,

     // expressions: 8
     NODE_BINARY_EXPR, NODE_UNARY_EXPR, NODE_FILE_EXPR,
     NODE_LIST_EXPR, NODE_LITERAL_EXPR, NODE_IDENTIFIER_EXPR, NODE_KEYWORD_EXPR,
     NODE_FUNCTION_EXPR,

     // postfix expression type: 2 + NODE_IDENTIFIER_EXPR
     NODE_CALL, NODE_SUBSCRIPT

     */

    // fixme
    gnode_n tag = NODE_TAG(node);
    switch (tag) {
        case NODE_UNARY_EXPR: {
            return is_expression_valid(((gnode_unary_expr_t *)node)->expr);
        }

        case NODE_BINARY_EXPR: {
            gnode_binary_expr_t *expr = (gnode_binary_expr_t *)node;
            if (expr->op == TOK_OP_ASSIGN) return false;
            if (!is_expression_valid(expr->left)) return false;
            return is_expression_valid(expr->right);
        }

        case NODE_FLOW_STAT: {
            gnode_flow_stmt_t *flow_stmt = (gnode_flow_stmt_t *)node;
            if (TOK_OP_TERNARY != NODE_TOKEN_TYPE(flow_stmt)) return false;
            return is_expression_valid(flow_stmt->cond) && is_expression_valid(flow_stmt->stmt) && is_expression_valid(flow_stmt->elsestmt);
        }

        case NODE_IDENTIFIER_EXPR: {
            return true;
        }

        case NODE_MODULE_DECL:
        case NODE_ENUM_DECL: {
            return false;
        }

        default: break;
    }

    return true;
}

static bool is_init_function (gnode_t *node) {
    if (ISA(node, NODE_FUNCTION_DECL)) {
        gnode_function_decl_t *f = (gnode_function_decl_t *)node;
        if (!f->identifier) return false;
        return (strcmp(f->identifier, CLASS_CONSTRUCTOR_NAME) == 0);
    }
    return false;
}

static bool is_init_infinite_loop(gvisitor_t *self, gnode_identifier_expr_t *identifier, gnode_r *list) {

    // for example:
    // class c1 {
    //     func init() {
    //         var a = c1();    // INFINITE LOOP
    //        var a = self();    // INFINITE LOOP
    //     }
    // }

    // conditions for an infinite loop in init:

    // 1. there should be at least 2 declarations in the stack
    gnode_r *decls = ((semacheck_t *)self->data)->declarations;
    size_t len = gnode_array_size(decls);
    if (len < 2) return false;

    // 2. current function is init
    if (!is_init_function(gnode_array_get(decls, len-1))) return false;

    // 3. outer declaration is a class
    gnode_t    *target_node = gnode_array_get(decls, len-2);
    if (!ISA(target_node, NODE_CLASS_DECL)) return false;

    // 4. identifier is self OR identifier->symbol points to target_node
    bool continue_check = false;
    if (identifier->symbol) continue_check = target_node == identifier->symbol;
    else continue_check = ((identifier->value) && (strcmp(identifier->value, SELF_PARAMETER_NAME) == 0));
    if (!continue_check) return false;

    // 5. check if next node is a call
    size_t count = gnode_array_size(list);
    if (count < 1) return false;
    gnode_postfix_subexpr_t *subnode = (gnode_postfix_subexpr_t *)gnode_array_get(list, 0);
    return (subnode->base.tag == NODE_CALL_EXPR);
}

static void check_access_storage_specifiers (gvisitor_t *self, gnode_t *node, gnode_n env, gtoken_t access, gtoken_t storage) {
    // check for module node
    if (NODE_TAG(node) == NODE_MODULE_DECL) {
        if (access != 0) REPORT_ERROR(node, "Access specifier cannot be used for module.");
        if (storage != 0) REPORT_ERROR(node, "Storage specifier cannot be used for module.");
    }

    // check fo access specifiers here
    // access specifier does make sense only inside module or class declaration
    // in any other enclosing environment must be considered a semantic error
    if ((access != 0) && (env != NODE_CLASS_DECL) && (env != NODE_MODULE_DECL)) {
        REPORT_ERROR(node, "Access specifier does not make sense here.");
    }

    // storage specifier (STATIC) makes sense only inside a class declaration
    if ((storage == TOK_KEY_STATIC) && (env != NODE_CLASS_DECL)) {
        REPORT_ERROR(node, "Static storage specifier does not make sense outside a class declaration.");
    }
}

static bool check_assignment_expression (gvisitor_t *self, gnode_binary_expr_t *node) {
    // in case of assignment check left node: assure assignment is made to identifier or other valid expressions
    // for example left expression cannot be a literal (to prevent 3 = 2)

    gnode_n tag = NODE_TAG(node->left);
    bool result = ((tag == NODE_IDENTIFIER_EXPR) || (tag == NODE_FILE_EXPR) || (tag == NODE_POSTFIX_EXPR));

    // more checks in the postfix case
    if (tag == NODE_POSTFIX_EXPR) {
        gnode_postfix_expr_t *expr = (gnode_postfix_expr_t *)node->left;

        // in case of postfix expression
        // enum has already been processed so it appears as a literal with expr->list NULL
        // inside a postfix expression node
        // check enum case (enum cannot be assigned)
        if (ISA(expr->id, NODE_LITERAL_EXPR)) {
            result = false;
        } else {
            if (!expr->list) return false;
            // basically the LATEST node of a postfix expression cannot be a CALL in an assignment
            // so we are avoiding expressions like: a(123) = ...; or a.b.c(1,2) = ...;
            size_t count = gnode_array_size(expr->list);
            gnode_postfix_subexpr_t *subnode = (gnode_postfix_subexpr_t *)gnode_array_get(expr->list, count-1);
            if (!subnode) return false;
            result = (NODE_TAG(subnode) != NODE_CALL_EXPR);
        }
    }

    // set is_assignment flag (default to false)
    node->left->is_assignment = result;

    if (!result) REPORT_ERROR(node->left, "Wrong assignment expression.");
    return result;
}

static bool check_range_expression (gvisitor_t *self, gnode_binary_expr_t *node) {
    // simple check, if nodes are literals then they must be INT
    gnode_t *r[2] = {node->left, node->right};

    for (int i=0; i<2; ++i) {
        gnode_t *range = r[i];
        if (ISA_LITERAL(range)) {
            gnode_literal_expr_t *expr = (gnode_literal_expr_t *)range;
            if (expr->type != LITERAL_INT) {REPORT_ERROR(node, "Range must be integer."); return false;}
        }
    }
    return true;
}

static bool check_class_ivar (gvisitor_t *self, gnode_class_decl_t *classnode, gnode_variable_decl_t *node) {
    size_t count = gnode_array_size(node->decls);

    gnode_class_decl_t *supernode = (gnode_class_decl_t *)classnode->superclass;
    if (!NODE_ISA(supernode, NODE_CLASS_DECL)) return false;

    for (size_t i=0; i<count; ++i) {
        gnode_var_t *p = (gnode_var_t *)gnode_array_get(node->decls, i);
        if (!p) continue;
        DEBUG_SEMA2("check_ivar %s", p->identifier);

        // do not check internal outer var
        if (string_cmp(p->identifier, OUTER_IVAR_NAME) == 0) continue;

        while (supernode) {
            symboltable_t *symtable = supernode->symtable;
            if (symboltable_lookup(symtable, p->identifier) != NULL) {
                REPORT_WARNING((gnode_t *)node, "Property '%s' defined in class '%s' already defined in its superclass %s.",
                               p->identifier, classnode->identifier, supernode->identifier);
            }

            if (supernode->superclass && !NODE_ISA(supernode->superclass, NODE_CLASS_DECL)) {
                REPORT_ERROR(supernode, "Unable to find superclass %s for class %s.", supernode->identifier, ((gnode_identifier_expr_t *)supernode->superclass)->value);
                supernode->superclass = NULL;
                return false;
            }

            supernode = (gnode_class_decl_t *)supernode->superclass;
        }
    }

    return true;
}

static void free_postfix_subexpr (gnode_postfix_subexpr_t *subnode) {
    // check refcount
    if (subnode->base.refcount > 0) {--subnode->base.refcount; return;}

    // manually free postfix subnode
    gnode_n tag = subnode->base.tag;
    if (tag == NODE_CALL_EXPR) {
        if (subnode->args) {
            gnode_array_each(subnode->args, gnode_free(val););
            gnode_array_free(subnode->args);
        }
    } else {
        gnode_free(subnode->expr);
    }

    mem_free((gnode_t*)subnode);
}

// MARK: - Statements -

static void visit_list_stmt (gvisitor_t *self, gnode_compound_stmt_t *node) {
    DEBUG_SEMA2("visit_list_stmt");

    PUSH_DECLARATION(node);
    gnode_array_each(node->stmts, {visit(val);});
    POP_DECLARATION();
}

static void visit_compound_stmt (gvisitor_t *self, gnode_compound_stmt_t *node) {
    DEBUG_SEMA2("visit_compound_stmt");

    gnode_t            *top = TOP_DECLARATION();
    symboltable_t    *symtable = symtable_from_node(top);

    if (!symtable) return;
    symboltable_enter_scope(symtable);
    gnode_array_each(node->stmts, {visit(val);});

    symboltable_exit_scope(symtable, &node->nclose);
}

static void visit_label_stmt (gvisitor_t *self, gnode_label_stmt_t *node) {
    DEBUG_SEMA2("visit_label_stmt");

    gtoken_t type = NODE_TOKEN_TYPE(node);
    if (!TOP_STATEMENT_ISA_SWITCH()) {
        if (type == TOK_KEY_DEFAULT) REPORT_ERROR(node, "'default' statement not in switch statement.");
        if (type == TOK_KEY_CASE) REPORT_ERROR(node, "'case' statement not in switch statement.");
    }

    if (type == TOK_KEY_DEFAULT) {visit(node->stmt);}
    else if (type == TOK_KEY_CASE) {visit(node->expr); visit(node->stmt);}
}

static void visit_flow_stmt (gvisitor_t *self, gnode_flow_stmt_t *node) {
    DEBUG_SEMA2("visit_flow_stmt");

    // assignment has no side effect so report error in case of assignment
    if (is_expression_assignment(node->cond)) REPORT_ERROR(node->cond, "Assignment not allowed here");

    gtoken_t type = NODE_TOKEN_TYPE(node);
    if (type == TOK_KEY_IF) {
        visit(node->cond);
        visit(node->stmt);
        if (node->elsestmt) visit(node->elsestmt);
    } else if (type == TOK_KEY_SWITCH) {
        PUSH_STATEMENT(type);
        visit(node->cond);
        visit(node->stmt);
        POP_STATEMENT();
    } else if (type == TOK_OP_TERNARY) {
        visit(node->cond);
        visit(node->stmt);
        visit(node->elsestmt);
    }
}

static void visit_loop_stmt (gvisitor_t *self, gnode_loop_stmt_t *node) {
    DEBUG_SEMA2("visit_loop_stmt");

    gtoken_t type = NODE_TOKEN_TYPE(node);
    PUSH_STATEMENT(type);

    // check pre-conditions
    const char *LOOP_NAME = NULL;
    gnode_t *cond = NULL;
    if (type == TOK_KEY_WHILE) {LOOP_NAME = "WHILE"; cond = node->cond;}
    else if (type == TOK_KEY_REPEAT) {LOOP_NAME = "REPEAT"; cond = node->expr;}
    else if (type == TOK_KEY_FOR) {LOOP_NAME = "FOR"; cond = node->cond;}

    // sanity check
    if (type == TOK_KEY_WHILE) {
        if (!node->cond) {REPORT_ERROR(node, "Missing %s condition.", LOOP_NAME); return;}
        if (!node->stmt) {REPORT_ERROR(node, "Missing %s statement.", LOOP_NAME); return;}
    } else if (type == TOK_KEY_REPEAT) {
        if (!node->stmt) {REPORT_ERROR(node, "Missing %s statement.", LOOP_NAME); return;}
        if (!node->expr) {REPORT_ERROR(node, "Missing %s expression.", LOOP_NAME); return;}
    } else if (type == TOK_KEY_FOR) {
        if (!node->cond) {REPORT_ERROR(node, "Missing %s condition.", LOOP_NAME); return;}
        if (!node->expr) {REPORT_ERROR(node, "Missing %s expression.", LOOP_NAME); return;}
        if (!node->stmt) {REPORT_ERROR(node, "Missing %s statement.", LOOP_NAME); return;}
    }

    if (is_expression_assignment(cond)) {
        REPORT_ERROR(cond, "Assignments in Gravity does not return a value so cannot be used inside a %s condition.", LOOP_NAME);
        return;
    }

    // FOR condition MUST be a VARIABLE declaration or an IDENTIFIER
    if (type == TOK_KEY_FOR) {
        bool type_check = (NODE_ISA(node->cond, NODE_VARIABLE_DECL) || NODE_ISA(node->cond, NODE_IDENTIFIER_EXPR));
        if (!type_check) REPORT_ERROR(cond, "FOR declaration must be a variable declaration or a local identifier.");

        if (NODE_ISA(node->cond, NODE_VARIABLE_DECL)) {
            gnode_variable_decl_t *var = (gnode_variable_decl_t *)node->cond;

            // assure var declares just ONE variable
            if (gnode_array_size(var->decls) > 1) REPORT_ERROR(cond, "Cannot declare more than one variable inside a FOR loop.");

            // assure that there is no assignment expression
            gnode_var_t *p = (gnode_var_t *)gnode_array_get(var->decls, 0);
            if (p->expr) REPORT_ERROR(cond, "Assignment expression prohibited in a FOR loop.");
        }
    }

    if (type == TOK_KEY_WHILE) {
        visit(node->cond);
        visit(node->stmt);
    } else if (type == TOK_KEY_REPEAT) {
        visit(node->stmt);
        visit(node->expr);
    } else if (type == TOK_KEY_FOR) {
        symboltable_t *symtable = symtable_from_node(TOP_DECLARATION());
        symboltable_enter_scope(symtable);
        visit(node->cond);
        if (NODE_ISA(node->cond, NODE_IDENTIFIER_EXPR)) {
            //if cond is not a var declaration then it must be a local identifier
            gnode_identifier_expr_t *expr = (gnode_identifier_expr_t *)node->cond;
            if (expr->location.type != LOCATION_LOCAL) REPORT_ERROR(cond, "FOR declaration must be a variable declaration or a local identifier.");
        }
        visit(node->expr);
        visit(node->stmt);

        symboltable_exit_scope(symtable, &node->nclose);
    }
    POP_STATEMENT();
}

static void visit_jump_stmt (gvisitor_t *self, gnode_jump_stmt_t *node) {
    DEBUG_SEMA2("visit_jump_stmt");

    gtoken_t type = NODE_TOKEN_TYPE(node);
    if (type == TOK_KEY_BREAK) {
        if (!(TOP_STATEMENT_ISA_LOOP() || TOP_STATEMENT_ISA_SWITCH()))
            REPORT_ERROR(node, "'break' statement not in loop or switch statement.");
    }
    else if (type == TOK_KEY_CONTINUE) {
        if (!TOP_STATEMENT_ISA_LOOP()) REPORT_ERROR(node, "'continue' statement not in loop statement.");
    }
    else if (type == TOK_KEY_RETURN) {
        gnode_t *n1 = TOP_DECLARATION(); // n1 == NULL means globals
        if (!ISA(n1, NODE_FUNCTION_DECL)) REPORT_ERROR(node, "'return' statement not in a function definition.");

        if (node->expr) {
            visit(node->expr);
            if (!is_expression_valid(node->expr)) {
                REPORT_ERROR(node->expr, "Invalid expression.");
                return;
            }
        }
    } else {
        assert(0);
    }
}

static void visit_empty_stmt (gvisitor_t *self, gnode_empty_stmt_t *node) {
    DEBUG_SEMA2("visit_empty_stmt");

    // get top declaration
    gnode_t *top = TOP_DECLARATION();
    if (!NODE_ISA_FUNCTION(top)) REPORT_ERROR(node, "Extraneous semicolon error.");

    return;
}

// MARK: - Declarations -

static void visit_function_decl (gvisitor_t *self, gnode_function_decl_t *node) {
    DEBUG_SEMA2("visit_function_decl %s", node->identifier);

    // set top declaration
    gnode_t *top = TOP_DECLARATION();

    // check if optional access and storage specifiers make sense in current context
    check_access_storage_specifiers(self, (gnode_t *)node, NODE_TAG(top), node->access, node->storage);

    // get enclosing declaration
    node->env = top;

    // enter function scope
    PUSH_DECLARATION(node);
    symboltable_t *symtable = symboltable_create(SYMTABLE_TAG_FUNC);
    symboltable_enter_scope(symtable);

    // process parameters
    node->symtable = symtable;
    if (node->params) {
        gnode_array_each(node->params, {
            gnode_var_t *p = (gnode_var_t *)val;
            p->env = (gnode_t*)node;
            if (!symboltable_insert(symtable, p->identifier, (void *)p)) {
                REPORT_ERROR(p, "Parameter %s redeclared.", p->identifier);
                continue;
            }
            SET_LOCAL_INDEX(p, symtable);
            DEBUG_SEMA2("Local:%s index:%d", p->identifier, p->index);
        });
    }

    // process inner block
    gnode_compound_stmt_t *block = node->block;
    if (block) {gnode_array_each(block->stmts, {visit(val);});}

    // exit function scope
    uint16_t nparams = (node->params) ? (uint16_t)marray_size(*node->params) : 0;
    uint32_t nlocals = symboltable_exit_scope(symtable, NULL);
    if (nlocals > MAX_LOCALS) {
        REPORT_ERROR(node, "Maximum number of local variables reached in function %s (max:%d found:%d).",
                                           node->identifier, MAX_LOCALS, nlocals);
    } else {
        node->nlocals = (uint16_t)nlocals - nparams;
        node->nparams = nparams;
    }

    // check upvalue limit
    uint32_t nupvalues = (node->uplist) ? (uint32_t)marray_size(*node->uplist) : 0;
    if (nupvalues > MAX_UPVALUES) REPORT_ERROR(node, "Maximum number of upvalues reached in function %s (max:%d found:%d).",
                                               node->identifier, MAX_LOCALS, nupvalues);

    POP_DECLARATION();

    DEBUG_SEMA2("MAX LOCALS for function %s: %d", node->identifier, node->nlocals);
}

static void visit_variable_decl (gvisitor_t *self, gnode_variable_decl_t *node) {
    gnode_t         *top = TOP_DECLARATION();
    symboltable_t   *symtable = symtable_from_node(top);
    size_t          count = gnode_array_size(node->decls);
    gnode_n         env = NODE_TAG(top);
    bool            env_is_function = (env == NODE_FUNCTION_DECL);

    // check if optional access and storage specifiers make sense in current context
    check_access_storage_specifiers(self, (gnode_t *)node, env, node->access, node->storage);

    // loop to check each individual declaration
    for (size_t i=0; i<count; ++i) {
        gnode_var_t *p = (gnode_var_t *)gnode_array_get(node->decls, i);
        DEBUG_SEMA2("visit_variable_decl %s", p->identifier);

        // set enclosing environment
        p->env = top;

        // visit expression first in order to prevent var a = a
        // variable with a initial value (or with a getter/setter)
        if (p->expr) visit(p->expr);
        if (NODE_ISA(p->expr, NODE_ENUM_DECL)) continue;
        
//        // check for manifest type
//        if (p->annotation_type) {
//            // struct gnode_var_t was modified with
//            // // untagged union, if no type is declared then this union is NULL otherwise
//            // union {
//            //     const char          *annotation_type;   // optional annotation type
//            //     gnode_class_decl_t  *class_type;        // class type (set in semacheck2 if annotation_type is not NULL)
//            // };
//            gnode_t *class_type = lookup_identifier(self, p->annotation_type, NULL);
//            if (!class_type) {REPORT_WARNING(p, "Unable to find type %s.", p->annotation_type);}
//        //     if (!NODE_ISA(class_type, NODE_CLASS_DECL)) {REPORT_ERROR(p, "Unable to set non class type %s.", p->annotation_type); continue;}
//        //     p->class_type = (gnode_class_decl_t *)class_type;
//        }
        
        if (env_is_function) {
            // local variable defined inside a function
            if (!symboltable_insert(symtable, p->identifier, (void *)p)) {
                REPORT_ERROR(p, "Identifier %s redeclared.", p->identifier);
                continue;
            }
            SET_LOCAL_INDEX(p, symtable);
            DEBUG_SEMA2("Local:%s index:%d", p->identifier, p->index);
        } else if (env == NODE_CLASS_DECL) {
            if (p->iscomputed) continue;
            
            // variable defined inside a class => property
            gnode_class_decl_t *c = (gnode_class_decl_t *)top;

            // compute new ivar index
            (node->storage == TOK_KEY_STATIC) ? ++c->nsvar : ++c->nivar;

            // super class is a static information so I can solve the fragile class problem at compilation time
            gnode_class_decl_t *super = (gnode_class_decl_t *)c->superclass;
            if (super && !NODE_ISA(super, NODE_CLASS_DECL)) return;
            
            uint32_t n2 = 0;
            while (super) {
                n2 += (node->storage == TOK_KEY_STATIC) ? super->nsvar : super->nivar;
                super = (gnode_class_decl_t *)super->superclass;
            }

            p->index += n2;
            DEBUG_SEMA2("Class: %s property:%s index:%d (static %d)", c->identifier, p->identifier, p->index, (node->storage == TOK_KEY_STATIC));
        }
    }
}

static void visit_enum_decl (gvisitor_t *self, gnode_enum_decl_t *node) {
    DEBUG_SEMA2("visit_enum_decl %s", node->identifier);

    // check if optional access and storage specifiers make sense in current context
    gnode_t *top = TOP_DECLARATION();
    check_access_storage_specifiers(self, (gnode_t *)node, NODE_TAG(top), node->access, node->storage);
    
    if (NODE_ISA_FUNCTION(top)) {
        // it is a local defined enum
        symboltable_t *symtable = symtable_from_node(top);
        if (!symboltable_insert(symtable, node->identifier, (void *)node)) {
            REPORT_ERROR(node, "Identifier %s redeclared.", node->identifier);
        }
    }
}

static void visit_class_decl (gvisitor_t *self, gnode_class_decl_t *node) {
    DEBUG_SEMA2("visit_class_decl %s", node->identifier);

    gnode_t *top = TOP_DECLARATION();

    // check if optional access and storage specifiers make sense in current context
    check_access_storage_specifiers(self, (gnode_t *)node, NODE_TAG(top), node->access, node->storage);

    // set class enclosing (can be globals, a class or a function)
    node->env = top;

    // sanity check on class name
    if (string_cmp(node->identifier, CLASS_CONSTRUCTOR_NAME) == 0) {
        REPORT_ERROR(node, "%s is a special name and cannot be used as class identifier.", CLASS_CONSTRUCTOR_NAME);
        return;
    }
    
    // check superclass
    if (node->superclass) {
        // get super class identifier and reset the field (so in case of error it cannot be accessed)
        gnode_identifier_expr_t *id = (gnode_identifier_expr_t *)node->superclass;
        node->superclass = NULL;

        // sanity check
        if (gravity_core_class_from_name(id->value)) {
            REPORT_ERROR(id, "Unable to subclass built-in core class %s.", id->value);
            return;
        }

        // lookup super node
        gnode_t *target = lookup_symtable_id(self, id, true);
        node->superclass = target;

        if (!target) {
            REPORT_ERROR(id, "Unable to find superclass %s for class %s.", id->value, node->identifier);
        } else {
            gnode_class_decl_t *target_class = (gnode_class_decl_t *)gnode2class(target, &node->super_extern);
            if (!target_class) {
                REPORT_ERROR(id, "Unable to set non class %s as superclass of %s.", id->value, node->identifier);
            } else if ((gnode_class_decl_t *)node == (gnode_class_decl_t *)target_class->superclass) {
                REPORT_ERROR(id, "Unable to set circular class hierarchies (%s <-> %s).", id->value, node->identifier);
                gnode_free((gnode_t*)id);
                return;
            }
        }

        gnode_free((gnode_t*)id);
    }

    // check protocols (disable in this version because protocols are not yet supported)
    // if (node->protocols) {
    //    gnode_array_each(node->protocols, {
    //        gnode_id_expr_t *id = (gnode_id_expr_t *)val;
    //        gnode_t *target = lookup_symtable_id(self, id, false);
    //        if (!target) continue;
    //        id->symbol = target;
    //    });
    // }

    PUSH_DECLARATION(node);
    gnode_array_each(node->decls, {
        if ((node->superclass) && (ISA_VAR_DECLARATION(val))) {
            // check for redeclared ivar and if found report a warning
            check_class_ivar(self, (gnode_class_decl_t *)node, (gnode_variable_decl_t *)val);
        }
        visit(val);
    });
    POP_DECLARATION();
}

static void visit_module_decl (gvisitor_t *self, gnode_module_decl_t *node) {
    DEBUG_SEMA2("visit_module_decl %s", node->identifier);

    gnode_t *top = TOP_DECLARATION();

    // set and check module enclosing (only in file)
    node->env = top;
    if (NODE_TAG(top) != NODE_LIST_STAT) REPORT_ERROR(node, "Module %s cannot be declared here.", node->identifier);

    // check if optional access and storage specifiers make sense in current context
    check_access_storage_specifiers(self, (gnode_t *)node, NODE_TAG(top), node->access, node->storage);

    PUSH_DECLARATION(node);
    gnode_array_each(node->decls, {visit(val);});
    POP_DECLARATION();
}

// MARK: - Expressions -

static void visit_binary_expr (gvisitor_t *self, gnode_binary_expr_t *node) {
    DEBUG_SEMA2("visit_binary_expr %s", token_name(node->op));

    // sanity check
    if (!is_expression(node->left)) REPORT_ERROR(node->left, "LValue must be an expression.");
    if (!is_expression(node->right)) REPORT_ERROR(node->right, "RValue must be an expression.");

    // fill missing symbols
    visit(node->left);
    visit(node->right);

    if (!is_expression_valid(node->left)) REPORT_ERROR(node->left, "Invalid left expression.");
    if (!is_expression_valid(node->right)) REPORT_ERROR(node->right, "Invalid right expression.");

    // sanity check binary expressions
    if (is_expression_assignment((gnode_t*)node)) check_assignment_expression(self, node);
    else if (is_expression_range((gnode_t*)node)) check_range_expression(self, node);
}

static void visit_unary_expr (gvisitor_t *self, gnode_unary_expr_t *node) {
    DEBUG_SEMA2("visit_unary_expr %s", token_name(node->op));
    visit(node->expr);
    if (!is_expression_valid(node->expr)) REPORT_ERROR(node->expr, "Invalid expression.");
}

static void visit_postfix_expr (gvisitor_t *self, gnode_postfix_expr_t *node) {
    DEBUG_SEMA2("visit_postfix_expr");
    
    // sanity check
    if (!node->id) {REPORT_ERROR(node, "Invalid postfix expression."); return;}

    // a postfix expression is an expression that requires an in-context lookup that depends on id
    // in a statically typed language the loop should check every member of the postfix expression
    // usign the context of the previous lookup, for example:
    // a.b.c.d.e
    // means
    // lookup a and get its associated symbol table
    // lookup b in a
    // lookup c in the context of the previous lookup
    // lookup d in the context of the previous lookup
    // and so on in a loop
    // Gravity is a dynamically typed language so we cannot statically check membership in a statically way
    // because the lookup context can vary at runtime, for example
    // class C1 {...}
    // class C2 {...}
    // func foo(n) {if (n % 2 == 0) return C1(); else return C2();}
    // var c = foo(rand()).bar;
    // should bar be lookup in C1 or in C2?
    // we really can't know at compile time but only at runtime

    // lookup common part (and generate an error if id cannot be found)
    // id can be a primary expression
    visit(node->id);

    // try to obtain symbol table from id (if any)
    gnode_t *target = NULL;
    if (ISA(node->id, NODE_IDENTIFIER_EXPR)) {
        target = ((gnode_identifier_expr_t *)node->id)->symbol;
        if (ISA(target, NODE_VARIABLE)) target = NULL; // a variable does not contain a symbol table
    }

    // special enum case on list[0] (it is a static case)
    if (ISA(target, NODE_ENUM_DECL)) {
        // check first expression in the list (in case of enum MUST BE an identifier)
        gnode_postfix_subexpr_t *subnode = (gnode_postfix_subexpr_t *)gnode_array_get(node->list, 0);

        // enum sanity checks
        gnode_n tag = subnode->base.tag;
        if (tag != NODE_ACCESS_EXPR) {REPORT_ERROR(node->id, "Invalid enum expression."); return;}
        if (node->base.is_assignment) {REPORT_ERROR(node, "Assignment not allowed for an enum type."); return;}
        if (!ISA(subnode->expr, NODE_IDENTIFIER_EXPR)) {REPORT_ERROR(subnode, "Invalid enum expression."); return;}

        // lookup enum value
        gnode_identifier_expr_t *expr = (gnode_identifier_expr_t *)subnode->expr;
        const char *value = expr->value;
        gnode_t *v = lookup_node(target, value);
        if (!v) {REPORT_ERROR(subnode, "Unable to find %s in enum %s.", value, ((gnode_enum_decl_t *)target)->identifier); return;}

        // node.subnode must be replaced by a literal enum expression (returned by v)
        size_t n = gnode_array_size(node->list);
        if (n == 1) {
            // replace the entire gnode_postfix_expr_t node with v literal value
            // gnode_replace(node, v); NODE REPLACEMENT FUNCTION TO BE IMPLEMENTED
            gnode_free(node->id);

            // we need to explicitly free postfix subexpression here
            gnode_postfix_subexpr_t *subexpr = (gnode_postfix_subexpr_t *)gnode_array_get(node->list, 0);
            free_postfix_subexpr(subexpr);

            // list cannot be NULL in a postfix expression, we'll use this flag to identify a transformed enum expression
            gnode_array_free(node->list);
            node->list = NULL;
            node->id = gnode_duplicate(v, false);
        } else {
            // postfix expression contains more access nodes so just transform current postfix expression
            // 1. replace id node
            gnode_free(node->id);
            node->id = gnode_duplicate(v, false);

            // 2. free first node from node->list
            gnode_postfix_subexpr_t *subexpr = (gnode_postfix_subexpr_t *)gnode_array_get(node->list, 0);
            free_postfix_subexpr(subexpr);

            // 3. remove first node from node->list
            node->list = gnode_array_remove_byindex(node->list, 0);
        }

        return;
    }

    // check to avoid infinite loop in init
    if (ISA(node->id, NODE_IDENTIFIER_EXPR)) {
        if (is_init_infinite_loop(self, (gnode_identifier_expr_t *)node->id, node->list)) {
            REPORT_ERROR(node, "Infinite loop detected in init func.");
        }
    }

    bool is_super = (NODE_ISA(node->id, NODE_KEYWORD_EXPR) && (((gnode_keyword_expr_t *)node->id)->base.token.type == TOK_KEY_SUPER));
    bool is_assignment = node->base.is_assignment;

    // process each subnode
    size_t count = gnode_array_size(node->list);
    for (size_t i=0; i<count; ++i) {
        gnode_postfix_subexpr_t *subnode = (gnode_postfix_subexpr_t *)gnode_array_get(node->list, i);

        // identify postfix type: NODE_CALL_EXPR, NODE_ACCESS_EXPR, NODE_SUBSCRIPT_EXPR
        gnode_n tag = subnode->base.tag;

        // check assignment flag
        bool is_real_assigment = (is_assignment && (i+1 == count));

        // assignment sanity check
        if (is_real_assigment) {
            if (tag == NODE_CALL_EXPR) {REPORT_ERROR((gnode_t *)subnode, "Unable to assign a value to a function call."); return;}
            if (is_super) {REPORT_ERROR((gnode_t *)subnode, "Unable to explicitly modify super."); return;}
        }

        // for a function/method call visit each argument
        if (tag == NODE_CALL_EXPR) {
            size_t n = gnode_array_size(subnode->args);
            for (size_t j=0; j<n; ++j) {
                gnode_t *val = (gnode_t *)gnode_array_get(subnode->args, j);
                if (is_expression_assignment(val)) {REPORT_ERROR(val, "Assignment does not have side effects and so cannot be used as function argument."); return;}
                visit(val);
            }
            continue;
        }

        // for a subscript just visit its index expression
        if (tag == NODE_SUBSCRIPT_EXPR) {
            if (subnode->expr) visit(subnode->expr);
            continue;
        }

        // for a member access check each lookup type (but do not perform a lookup)
        if (tag == NODE_ACCESS_EXPR) {
            if (!ISA(subnode->expr, NODE_IDENTIFIER_EXPR)) REPORT_ERROR(subnode->expr, "Invalid access expression.");
            continue;
        }

        // should never reach this point
        DEBUG_SEMA2("UNRECOGNIZED POSTFIX OPTIONAL EXPRESSION");
        assert(0);
    }
}

static void visit_file_expr (gvisitor_t *self, gnode_file_expr_t *node) {
    DEBUG_SEMA2("visit_file_expr");

    gnode_r *decls = ((semacheck_t *)self->data)->declarations;
    gnode_t *globals = gnode_array_get(decls, 0);
    gnode_t *target = globals;
    size_t    n = gnode_array_size(node->identifiers);
    assert(n);

    // no need to scan the entire list because lookup must be performed at runtime so check just the first element
    n = 1;
    for (size_t i=0; i<n; ++i) {
        const char *identifier = gnode_array_get(node->identifiers, i);
        DEBUG_SEMA2("LOOKUP %s", identifier);

        gnode_t *symbol = lookup_node(target, identifier);
        if (!symbol) {REPORT_ERROR(node, "Module identifier %s not found.", identifier); break;}
        SET_NODE_LOCATION(node, LOCATION_GLOBAL, 0, 0);
        target = symbol;
    }
}

static void visit_literal_expr (gvisitor_t *self, gnode_literal_expr_t *node) {
    #pragma unused(self, node)

    #if GRAVITY_SEMANTIC_DEBUG
    char value[256];
    gnode_literal_dump(node, value, sizeof(value));
    DEBUG_SEMA2("visit_literal_expr %s", value);
    DEBUG_SEMA2("end visit_literal_expr");
    #endif

    if (node->type == LITERAL_STRING_INTERPOLATED) {
        gnode_array_each(node->value.r, {
            visit(val);
        });
    }
}

static void visit_identifier_expr (gvisitor_t *self, gnode_identifier_expr_t *node) {
    DEBUG_SEMA2("visit_identifier_expr %s", node->value);

    gnode_t *symbol = lookup_identifier(self, node->value, node);
    if (!symbol) REPORT_ERROR(node, "Identifier %s not found.", node->value);
}

static void visit_keyword_expr (gvisitor_t *self, gnode_keyword_expr_t *node) {
    #pragma unused(self, node)
    DEBUG_SEMA2("visit_keyword_expr %s", token_name(node->base.token.type));
}

static void visit_list_expr (gvisitor_t *self, gnode_list_expr_t *node) {
    size_t    n = gnode_array_size(node->list1);
    bool    ismap = (node->list2 != NULL);

    DEBUG_SEMA2("visit_list_expr (n: %zu ismap: %d)", n, ismap);

    for (size_t j=0; j<n; ++j) {
        gnode_t *e = gnode_array_get(node->list1, j);
        visit(e);

        if (ismap) {
            // key must be unique
            for (size_t k=0; k<n; ++k) {
                if (k == j) continue; // do not check itself
                gnode_t *key = gnode_array_get(node->list1, k);
                if (gnode_is_equal(e, key)) {
                    if (gnode_is_literal_string(key)) {
                        gnode_literal_expr_t *v = (gnode_literal_expr_t *)key;
                        REPORT_ERROR(key, "Duplicated key %s in map.", v->value.str);
                    } else REPORT_ERROR(key, "Duplicated key in map.");
                }
            }

            e = gnode_array_get(node->list2, j);
            visit(e);
        }
    }
}

// MARK: -

bool gravity_semacheck2 (gnode_t *node, gravity_delegate_t *delegate) {
    semacheck_t data = {.declarations = gnode_array_create(), .lasterror = 0};
    marray_init(data.statements);

    gvisitor_t visitor = {
        .nerr = 0,                            // used to store number of found errors
        .data = (void *)&data,                // used to store a pointer to the semantic check struct
        .delegate = (void *)delegate,        // compiler delegate to report errors

        // COMMON
        .visit_pre = NULL,
        .visit_post = NULL,

        // STATEMENTS: 7
        .visit_list_stmt = visit_list_stmt,
        .visit_compound_stmt = visit_compound_stmt,
        .visit_label_stmt = visit_label_stmt,
        .visit_flow_stmt = visit_flow_stmt,
        .visit_loop_stmt = visit_loop_stmt,
        .visit_jump_stmt = visit_jump_stmt,
        .visit_empty_stmt = visit_empty_stmt,

        // DECLARATIONS: 5
        .visit_function_decl = visit_function_decl,
        .visit_variable_decl = visit_variable_decl,
        .visit_enum_decl = visit_enum_decl,
        .visit_class_decl = visit_class_decl,
        .visit_module_decl = visit_module_decl,

        // EXPRESSIONS: 8
        .visit_binary_expr = visit_binary_expr,
        .visit_unary_expr = visit_unary_expr,
        .visit_file_expr = visit_file_expr,
        .visit_literal_expr = visit_literal_expr,
        .visit_identifier_expr = visit_identifier_expr,
        .visit_keyword_expr = visit_keyword_expr,
        .visit_list_expr = visit_list_expr,
        .visit_postfix_expr = visit_postfix_expr,
    };

    DEBUG_SEMA2("=== SEMANTIC CHECK STEP 2 ===");
    gvisit(&visitor, node);
    DEBUG_SEMA2("\n");

    marray_destroy(data.statements);
    gnode_array_free(data.declarations);
    return (visitor.nerr == 0);
}
