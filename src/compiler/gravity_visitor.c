//
//  gravity_visitor.c
//  gravity
//
//  Created by Marco Bambini on 08/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_visitor.h"

// Visit a node invoking the associated callback.

#define VISIT(type)     if (!self->visit_##type) return; \
                        self->visit_##type(self, (gnode_##type##_t *) node); \
                        break;

static void default_action (gnode_t *node) {
    printf("Visitor unhandled case: %d\n", node->tag);
    assert(0);
}

void gvisit(gvisitor_t *self, gnode_t *node) {
    // this line added after implemented getter and setter,
    // because they are functions inside a COMPOUND_STATEMENT and can be NULL
    if (!node) return;

    // pre-visit
    if (self->visit_pre) self->visit_pre(self, node);

    switch (node->tag) {

        // statements (7)
        case NODE_LIST_STAT: VISIT(list_stmt);
        case NODE_COMPOUND_STAT: VISIT(compound_stmt);
        case NODE_LABEL_STAT: VISIT(label_stmt);
        case NODE_FLOW_STAT: VISIT(flow_stmt);
        case NODE_JUMP_STAT: VISIT(jump_stmt);
        case NODE_LOOP_STAT: VISIT(loop_stmt);
        case NODE_EMPTY_STAT: VISIT(empty_stmt);

        // declarations (5)
        case NODE_ENUM_DECL: VISIT(enum_decl);
        case NODE_FUNCTION_DECL: VISIT(function_decl);
        case NODE_VARIABLE_DECL: VISIT(variable_decl);
        case NODE_CLASS_DECL: VISIT(class_decl);
        case NODE_MODULE_DECL: VISIT(module_decl);
        // NODE_VARIABLE is handled by NODE_VARIABLE_DECL

        // expressions (8)
        case NODE_BINARY_EXPR: VISIT(binary_expr);
        case NODE_UNARY_EXPR: VISIT(unary_expr);
        case NODE_FILE_EXPR: VISIT(file_expr);
        case NODE_LIST_EXPR: VISIT(list_expr);
        case NODE_LITERAL_EXPR: VISIT(literal_expr);
        case NODE_IDENTIFIER_EXPR: VISIT(identifier_expr);
        case NODE_KEYWORD_EXPR: VISIT(keyword_expr);
        case NODE_POSTFIX_EXPR: VISIT(postfix_expr);

        // default assert
        default: default_action(node);
    }

    // post-visit
    if (self->visit_post) self->visit_post(self, node);
}
