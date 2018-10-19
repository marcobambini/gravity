//
//  gravity_visitor.h
//  gravity
//
//  Created by Marco Bambini on 08/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_VISITOR__
#define __GRAVITY_VISITOR__

#include "gravity_ast.h"

#define visit(node) gvisit(self, node)

typedef struct gvisitor {
    uint32_t    nerr;           // to store err counter state
    void        *data;          // to store a ptr state
    bool        bflag;          // to store a bool flag
    void        *delegate;      // delegate callback

    // COMMON
    void (* visit_pre)(struct gvisitor *self, gnode_t *node);
    void (* visit_post)(struct gvisitor *self, gnode_t *node);

    // count must be equal to enum gnode_n defined in gravity_ast.h less 3

    // STATEMENTS: 7
    void (* visit_list_stmt)(struct gvisitor *self, gnode_compound_stmt_t *node);
    void (* visit_compound_stmt)(struct gvisitor *self, gnode_compound_stmt_t *node);
    void (* visit_label_stmt)(struct gvisitor *self, gnode_label_stmt_t *node);
    void (* visit_flow_stmt)(struct gvisitor *self, gnode_flow_stmt_t *node);
    void (* visit_jump_stmt)(struct gvisitor *self, gnode_jump_stmt_t *node);
    void (* visit_loop_stmt)(struct gvisitor *self, gnode_loop_stmt_t *node);
    void (* visit_empty_stmt)(struct gvisitor *self, gnode_empty_stmt_t *node);

    // DECLARATIONS: 5+1 (NODE_VARIABLE handled by NODE_VARIABLE_DECL case)
    void (* visit_function_decl)(struct gvisitor *self, gnode_function_decl_t *node);
    void (* visit_variable_decl)(struct gvisitor *self, gnode_variable_decl_t *node);
    void (* visit_enum_decl)(struct gvisitor *self, gnode_enum_decl_t *node);
    void (* visit_class_decl)(struct gvisitor *self, gnode_class_decl_t *node);
    void (* visit_module_decl)(struct gvisitor *self, gnode_module_decl_t *node);

    // EXPRESSIONS: 7+3 (CALL EXPRESSIONS handled by one callback)
    void (* visit_binary_expr)(struct gvisitor *self, gnode_binary_expr_t *node);
    void (* visit_unary_expr)(struct gvisitor *self, gnode_unary_expr_t *node);
    void (* visit_file_expr)(struct gvisitor *self, gnode_file_expr_t *node);
    void (* visit_literal_expr)(struct gvisitor *self, gnode_literal_expr_t *node);
    void (* visit_identifier_expr)(struct gvisitor *self, gnode_identifier_expr_t *node);
    void (* visit_keyword_expr)(struct gvisitor *self, gnode_keyword_expr_t *node);
    void (* visit_list_expr)(struct gvisitor *self, gnode_list_expr_t *node);
    void (* visit_postfix_expr)(struct gvisitor *self, gnode_postfix_expr_t *node);
} gvisitor_t;

void gvisit(gvisitor_t *self, gnode_t *node);

#endif
