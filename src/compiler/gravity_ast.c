//
//  gravity_ast.c
//  gravity
//
//  Created by Marco Bambini on 02/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_ast.h"
#include "gravity_utils.h"
#include "gravity_visitor.h"
#include "gravity_symboltable.h"

#define SETBASE(node, tagv, _tok)		node->base.tag = tagv;			\
										node->base.token = _tok;
#define CHECK_REFCOUNT(_node)			if (_node->base.refcount > 0) {--_node->base.refcount; return;}

// MARK: -

void_r *void_array_create (void) {
	void_r *r = mem_alloc(sizeof(void_r));
	marray_init(*r);
	return r;
}

cstring_r *cstring_array_create (void) {
	cstring_r *r = mem_alloc(sizeof(cstring_r));
	gnode_array_init(r);
	return r;
}

gnode_r *gnode_array_create (void) {
	gnode_r *r = mem_alloc(sizeof(gnode_r));
	gnode_array_init(r);
	return r;
}

void gnode_array_sethead(gnode_r *list, gnode_t *node) {
	// get old size
	size_t list_size = gnode_array_size(list);
	
	// push node at the end to trigger memory allocation (if needed)
	gnode_array_push(list, node);
	
	// shift elements in array
	for (size_t i=list_size; i>0; --i) {
		list->p[i] = list->p[i-1];
	}
	
	// set new array head
	list->p[0] = node;
}

gnode_r *gnode_array_remove_byindex(gnode_r *old_list, size_t index) {
	// get old size
	size_t list_size = gnode_array_size(old_list);
	if (index >= list_size) return NULL;
	
	gnode_r *new_list = gnode_array_create();
	for (size_t i=0; i<list_size; ++i) {
		if (i == index) continue;
		gnode_t *node = gnode_array_get(old_list, i);
		gnode_array_push(new_list, node);
	}
	gnode_array_free(old_list);
	return new_list;
}

gupvalue_t *gnode_function_add_upvalue(gnode_function_decl_t *f, gnode_var_t *symbol, uint16_t n) {
	// create uplist if necessary
	if (!f->uplist) {
		f->uplist = mem_alloc(sizeof(gupvalue_r));
		gnode_array_init(f->uplist);
	}
	
	// lookup symbol in uplist (if any)
	gtype_array_each(f->uplist, {
		// symbol already found in uplist so return its index
		gnode_var_t *node = (gnode_var_t *)val->node;
		if (strcmp(node->identifier, symbol->identifier) == 0) return val;
	}, gupvalue_t *);
	
	// symbol not found in uplist so add it
	gupvalue_t *upvalue = mem_alloc(sizeof(gupvalue_t));
	upvalue->node = (gnode_t *)symbol;
	upvalue->index = (n == 1) ? symbol->index : (uint32_t)gnode_array_size(f->uplist);
	upvalue->selfindex = (uint32_t)gnode_array_size(f->uplist);
	upvalue->is_direct = (n == 1);
	marray_push(gupvalue_t*, *f->uplist, upvalue);
	
	// return symbol position in uplist
	return upvalue;
}

// MARK: - Statements initializers -

gnode_t *gnode_jump_stat_create (gtoken_s token, gnode_t *expr) {
	gnode_jump_stmt_t *node = (gnode_jump_stmt_t *)mem_alloc(sizeof(gnode_jump_stmt_t));
	
	SETBASE(node, NODE_JUMP_STAT, token);
	node->expr = expr;
	return (gnode_t *)node;
}

gnode_t *gnode_label_stat_create (gtoken_s token, gnode_t *expr, gnode_t *stmt) {
	gnode_label_stmt_t *node = (gnode_label_stmt_t *)mem_alloc(sizeof(gnode_label_stmt_t));
	
	SETBASE(node, NODE_LABEL_STAT, token);
	node->expr = expr;
	node->stmt = stmt;
	return (gnode_t *)node;
}

gnode_t *gnode_flow_stat_create (gtoken_s token, gnode_t *cond, gnode_t *stmt1, gnode_t *stmt2) {
	gnode_flow_stmt_t *node = (gnode_flow_stmt_t *)mem_alloc(sizeof(gnode_flow_stmt_t));
	
	SETBASE(node, NODE_FLOW_STAT, token);
	node->cond = cond;
	node->stmt = stmt1;
	node->elsestmt = stmt2;
	return (gnode_t *)node;
}

gnode_t *gnode_loop_stat_create (gtoken_s token, gnode_t *cond, gnode_t *stmt, gnode_t *expr) {
	gnode_loop_stmt_t *node = (gnode_loop_stmt_t *)mem_alloc(sizeof(gnode_loop_stmt_t));
	
	SETBASE(node, NODE_LOOP_STAT, token);
	node->cond = cond;
	node->stmt = stmt;
	node->expr = expr;
	node->nclose = UINT32_MAX;
	return (gnode_t *)node;
}

gnode_t *gnode_block_stat_create (gnode_n type, gtoken_s token, gnode_r *stmts) {
	gnode_compound_stmt_t *node = (gnode_compound_stmt_t *)mem_alloc(sizeof(gnode_compound_stmt_t));
	
	SETBASE(node, type, token);
	node->stmts = stmts;
	node->nclose = UINT32_MAX;
	return (gnode_t *)node;
}

gnode_t *gnode_empty_stat_create (gtoken_s token) {
	gnode_empty_stmt_t *node = (gnode_empty_stmt_t *)mem_alloc(sizeof(gnode_empty_stmt_t));
	
	SETBASE(node, NODE_EMPTY_STAT, token);
	return (gnode_t *)node;
}

// MARK: - Declarations initializers -

gnode_t *gnode_class_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_t *superclass, gnode_r *protocols, gnode_r *declarations, bool is_struct) {
	gnode_class_decl_t *node = (gnode_class_decl_t *)mem_alloc(sizeof(gnode_class_decl_t));
	node->is_struct = is_struct;
	
	// before class creation, iterate declarations and set proper access specifiers
	// default is PUBLIC but if IDENTIFIER begins with _ then set it to PRIVATE
	gnode_array_each(declarations, {
		if (val->tag == NODE_VARIABLE_DECL) {
			// default access specifier for variables is TOK_KEY_PUBLIC
			gnode_variable_decl_t *vdec_node = (gnode_variable_decl_t *)val;
			bool is_private = ((gnode_array_size(vdec_node->decls) > 0) && (((gnode_var_t *)gnode_array_get(vdec_node->decls, 0))->identifier[0] == '_'));
			if (vdec_node->access == 0) vdec_node->access = (is_private) ? TOK_KEY_PRIVATE : TOK_KEY_PUBLIC;
		} else if (val->tag == NODE_FUNCTION_DECL) {
			// default access specifier for functions is PUBLIC
			gnode_function_decl_t *fdec_node = (gnode_function_decl_t *)val;
			if (!fdec_node->identifier) continue;
			bool is_private = (fdec_node->identifier[0] == '_');
			if (fdec_node->access == 0) fdec_node->access = (is_private) ? TOK_KEY_PRIVATE : TOK_KEY_PUBLIC;
		} else if (val->tag == NODE_CLASS_DECL) {
			// default access specifier for inner class declarations is PUBLIC
			gnode_class_decl_t *cdec_node = (gnode_class_decl_t *)val;
			if (!cdec_node->identifier) continue;
			bool is_private = (cdec_node->identifier[0] == '_');
			if (cdec_node->access == 0) cdec_node->access = (is_private) ? TOK_KEY_PRIVATE : TOK_KEY_PUBLIC;
		}
	});
	
	
	SETBASE(node, NODE_CLASS_DECL, token);
	node->bridge = false;
	node->identifier = identifier;
	node->access = access_specifier;
	node->storage = storage_specifier;
	node->superclass = superclass;
	node->protocols = protocols;
	node->decls = declarations;
	node->nivar = 0;
	node->nsvar = 0;
	
	return (gnode_t *)node;
}

gnode_t *gnode_module_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *declarations) {
	gnode_module_decl_t *node = (gnode_module_decl_t *)mem_alloc(sizeof(gnode_module_decl_t));
	
	SETBASE(node, NODE_MODULE_DECL, token);
	node->identifier = identifier;
	node->access = access_specifier;
	node->storage = storage_specifier;
	node->decls = declarations;
	
	return (gnode_t *)node;
}

gnode_t *gnode_enum_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, symboltable_t *symtable) {
	gnode_enum_decl_t *node = (gnode_enum_decl_t *)mem_alloc(sizeof(gnode_enum_decl_t));
	
	SETBASE(node, NODE_ENUM_DECL, token);
	node->identifier = identifier;
	node->access = access_specifier;
	node->storage = storage_specifier;
	node->symtable= symtable;
	
	return (gnode_t *)node;
}

gnode_t *gnode_function_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *params, gnode_compound_stmt_t *block) {
	gnode_function_decl_t *node = (gnode_function_decl_t *)mem_alloc(sizeof(gnode_function_decl_t));
	
	SETBASE(node, NODE_FUNCTION_DECL, token);
	node->identifier = identifier;
	node->access = access_specifier;
	node->storage = storage_specifier;
	node->params = params;
	node->block = block;
	node->nlocals = 0;
	node->uplist = NULL;
	return (gnode_t *)node;
}

gnode_t *gnode_variable_decl_create (gtoken_s token, gtoken_t type, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *declarations) {
	gnode_variable_decl_t *node = (gnode_variable_decl_t *)mem_alloc(sizeof(gnode_variable_decl_t));
	
	SETBASE(node, NODE_VARIABLE_DECL, token);
	node->type = type;
	node->access = access_specifier;
	node->storage = storage_specifier;
	node->decls = declarations;
	return (gnode_t *)node;
}

gnode_t *gnode_variable_create (gtoken_s token, const char *identifier, const char *annotation_type, gtoken_t access_specifier, gnode_t *expr) {
	gnode_var_t *node = (gnode_var_t *)mem_alloc(sizeof(gnode_var_t));
	
	SETBASE(node, NODE_VARIABLE, token);
	node->identifier = identifier;
	node->annotation_type = annotation_type;
	node->expr = expr;
	node->access = access_specifier;
	return (gnode_t *)node;
}

// MARK: - Expressions initializers -

bool gnode_is_equal (gnode_t *node1, gnode_t *node2) {
	// very simple gnode verification for map key uniqueness
	gnode_base_t *_node1 = (gnode_base_t *)node1;
	gnode_base_t *_node2 = (gnode_base_t *)node2;
	if (_node1->base.tag != _node2->base.tag) return false;
	if (gnode_is_literal(node1)) {
		gnode_literal_expr_t *e1 = (gnode_literal_expr_t *)node1;
		gnode_literal_expr_t *e2 = (gnode_literal_expr_t *)node2;
		if (e1->type != e2->type) return false;
		// LITERAL_STRING, LITERAL_FLOAT, LITERAL_INT, LITERAL_BOOL
		if (e1->type == LITERAL_BOOL) return (e1->value.n64 == e1->value.n64);
		if (e1->type == LITERAL_INT) return (e1->value.n64 == e1->value.n64);
		if (e1->type == LITERAL_FLOAT) return (e1->value.d == e1->value.d);
		if (e1->type == LITERAL_STRING) return (strcmp(e1->value.str, e2->value.str)==0);
	}
	return false;
}

bool gnode_is_expression (gnode_t *node) {
	gnode_base_t *_node = (gnode_base_t *)node;
	return ((_node->base.tag >= NODE_BINARY_EXPR) && (_node->base.tag <= NODE_KEYWORD_EXPR));
}

bool gnode_is_literal (gnode_t *node) {
	gnode_base_t *_node = (gnode_base_t *)node;
	return (_node->base.tag == NODE_LITERAL_EXPR);
}

bool gnode_is_literal_int (gnode_t *node) {
	if (gnode_is_literal(node) == false) return false;
	gnode_literal_expr_t *_node = (gnode_literal_expr_t *)node;
	return (_node->type == LITERAL_INT);
}

bool gnode_is_literal_string (gnode_t *node) {
	if (gnode_is_literal(node) == false) return false;
	gnode_literal_expr_t *_node = (gnode_literal_expr_t *)node;
	return (_node->type == LITERAL_STRING);
}

bool gnode_is_literal_number (gnode_t *node) {
	if (gnode_is_literal(node) == false) return false;
	gnode_literal_expr_t *_node = (gnode_literal_expr_t *)node;
	return (_node->type != LITERAL_STRING);
}

gnode_t *gnode_binary_expr_create (gtoken_t op, gnode_t *left, gnode_t *right) {
	if (!left || !right) return NULL;
	
	gnode_binary_expr_t	*node = (gnode_binary_expr_t *)mem_alloc(sizeof(gnode_binary_expr_t));
	SETBASE(node, NODE_BINARY_EXPR, left->token);
	node->op = op;
	node->left = left;
	node->right = right;
	return (gnode_t *)node;
}

gnode_t *gnode_unary_expr_create (gtoken_t op, gnode_t *expr) {
	if (!expr) return NULL;
	
	gnode_unary_expr_t *node = (gnode_unary_expr_t *)mem_alloc(sizeof(gnode_unary_expr_t));
	SETBASE(node, NODE_UNARY_EXPR, expr->token);
	node->op = op;
	node->expr = expr;
	return (gnode_t *)node;
}

gnode_t *gnode_file_expr_create (gtoken_s token, cstring_r *list) {
	if (!list) return NULL;
	
	gnode_file_expr_t *node = (gnode_file_expr_t *)mem_alloc(sizeof(gnode_file_expr_t));
	SETBASE(node, NODE_FILE_EXPR, token);
	node->identifiers = list;
	return (gnode_t *)node;
}

gnode_t *gnode_identifier_expr_create (gtoken_s token, const char *identifier, const char *identifier2) {
	if (!identifier) return NULL;
	
	gnode_identifier_expr_t *node = (gnode_identifier_expr_t *)mem_alloc(sizeof(gnode_identifier_expr_t));
	SETBASE(node, NODE_IDENTIFIER_EXPR, token);
	node->value = identifier;
	node->value2 = identifier2;
	return (gnode_t *)node;
}

void gnode_literal_dump (gnode_literal_expr_t *node, char *buffer, int buffersize) {
	switch (node->type) {
		case LITERAL_STRING: snprintf(buffer, buffersize, "STRING: %.*s", node->len, node->value.str); break;
		case LITERAL_FLOAT: snprintf(buffer, buffersize, "FLOAT: %.2f", node->value.d); break;
		case LITERAL_INT: snprintf(buffer, buffersize, "INT: %lld", (int64_t)node->value.n64); break;
		case LITERAL_BOOL: snprintf(buffer, buffersize, "BOOL: %d", (int32_t)node->value.n64); break;
		default: assert(0); // should never reach this point
	}
}

static gnode_t *gnode_literal_value_expr_create (gtoken_s token, gliteral_t type, const char *s, double d, int64_t n64) {
	gnode_literal_expr_t *node = (gnode_literal_expr_t *)mem_alloc(sizeof(gnode_literal_expr_t));
	
	SETBASE(node, NODE_LITERAL_EXPR, token);
	node->type = type;
	node->len = 0;
	
	switch (type) {
		case LITERAL_STRING: node->value.str = (char *)s; break;
		case LITERAL_FLOAT: node->value.d = d; node->len = (d < FLT_MAX) ? 32 : 64; break;
		case LITERAL_INT: node->value.n64 = n64; node->len = (n64 < 2147483647) ? 32 : 64; break;
		case LITERAL_BOOL: node->value.n64 = n64; node->len = 32; break;
		default: assert(0); // should never reach this point
	}
	
	return (gnode_t *)node;
}

gnode_t *gnode_literal_string_expr_create (gtoken_s token, const char *s, uint32_t len) {
	gnode_literal_expr_t *node = (gnode_literal_expr_t *)gnode_literal_value_expr_create(token, LITERAL_STRING, NULL, 0, 0);
	
	node->len = len;
	node->value.str = (char *)mem_alloc(len+1);
	
	if (token.escaped) {
		node->value.str = string_unescape(s, &len, node->value.str);
		node->len = len;
	} else {
		memcpy((void *)node->value.str, (const void *)s, len);
	}
	
	return (gnode_t *)node;
}

gnode_t *gnode_literal_float_expr_create (gtoken_s token, double d) {
	return gnode_literal_value_expr_create(token, LITERAL_FLOAT, NULL, d, 0);
}

gnode_t *gnode_literal_int_expr_create (gtoken_s token, int64_t n) {
	return gnode_literal_value_expr_create(token, LITERAL_INT, NULL, 0, n);
}

gnode_t *gnode_literal_bool_expr_create (gtoken_s token, int32_t n) {
	return gnode_literal_value_expr_create(token, LITERAL_BOOL, NULL, 0, n);
}

gnode_t *gnode_keyword_expr_create (gtoken_s token) {
	gnode_keyword_expr_t *node = (gnode_keyword_expr_t *)mem_alloc(sizeof(gnode_keyword_expr_t));
	
	SETBASE(node, NODE_KEYWORD_EXPR, token);
	return (gnode_t *)node;
}

gnode_t *gnode_postfix_subexpr_create (gtoken_s token, gnode_n type, gnode_t *expr, gnode_r *list) {
	gnode_postfix_subexpr_t *node = (gnode_postfix_subexpr_t *)mem_alloc(sizeof(gnode_postfix_subexpr_t));
	
	if (type == NODE_CALL_EXPR)
		node->args = list;
	else
		node->expr = expr;
	
	SETBASE(node, type, token);
	return (gnode_t *)node;
}

gnode_t *gnode_postfix_expr_create (gtoken_s token, gnode_t *id, gnode_r *list) {
	gnode_postfix_expr_t *node = (gnode_postfix_expr_t *)mem_alloc(sizeof(gnode_postfix_expr_t));
	
	node->id = id;
	node->list = list;
	
	SETBASE(node, NODE_POSTFIX_EXPR, token);
	return (gnode_t *)node;
}

gnode_t *gnode_list_expr_create (gtoken_s token, gnode_r *list1, gnode_r *list2, bool ismap) {
	gnode_list_expr_t *node = (gnode_list_expr_t *)mem_alloc(sizeof(gnode_list_expr_t));
	
	SETBASE(node, NODE_LIST_EXPR, token);
	node->ismap = ismap;
	node->list1 = list1;
	node->list2 = list2;
	return (gnode_t *)node;
}

// MARK: -

gnode_t *gnode_duplicate (gnode_t *node, bool deep) {
	if (deep == true) {
		// deep is true so I need to examine node and perform a real duplication (only of the outer nodes)
		// deep is true ONLY when node can also be part of an assignment and its assignment flag can be
		// true is node is on the left and false when node is on the right
		// true flag is used only by adjust_assignment_expression in parser.c
		
		// node can be: identifier, file or postfix
		if (NODE_ISA(node, NODE_IDENTIFIER_EXPR)) {
			gnode_identifier_expr_t *expr = (gnode_identifier_expr_t *)node;
			return gnode_identifier_expr_create(expr->base.token, string_dup(expr->value), (expr->value2) ? string_dup(expr->value2) : NULL);
		} else if (NODE_ISA(node, NODE_FILE_EXPR)) {
			gnode_file_expr_t *expr = (gnode_file_expr_t *)node;
			cstring_r *list = cstring_array_create();
			size_t count = gnode_array_size(expr->identifiers);
			for (size_t i=0; i<count; ++i) {
				const char *identifier = gnode_array_get(expr->identifiers, i);
				cstring_array_push(list, string_dup(identifier));
			}
			return gnode_file_expr_create(expr->base.token, list);
		} else if (NODE_ISA(node, NODE_POSTFIX_EXPR)) {
			gnode_postfix_expr_t *expr = (gnode_postfix_expr_t *)node;
			gnode_t *id = gnode_duplicate(expr->id, false);
			gnode_r *list = gnode_array_create();
			gnode_array_each(expr->list, {gnode_array_push(list, gnode_duplicate(val, false));});
			return gnode_postfix_expr_create(expr->base.token, id, list);
		} else {
			printf("gnode_duplicate UNHANDLED case\n");
			assert(0); // should never reach this point
		}
		// just return the original node and since it is invalid for an assignment a semantic error will be generated
	}

	// it means that I can perform a light duplication where
	// duplicating a node means increase its refcount so it isn't freed more than once
	++node->refcount;
	return node;
}

// MARK: - AST deallocator -

// STATEMENTS
static void free_list_stmt (gvisitor_t *self, gnode_compound_stmt_t *node) {
	CHECK_REFCOUNT(node);
	gnode_array_each(node->stmts, {visit(val);});
	gnode_array_free(node->stmts);
	
	if (node->symtable) symboltable_free(node->symtable);
	mem_free((gnode_t*)node);
}

static void free_compound_stmt (gvisitor_t *self, gnode_compound_stmt_t *node) {
	CHECK_REFCOUNT(node);
	gnode_array_each(node->stmts, {visit(val);});
	gnode_array_free(node->stmts);
	
	if (node->symtable) symboltable_free(node->symtable);
	mem_free((gnode_t*)node);
}

static void free_label_stmt (gvisitor_t *self, gnode_label_stmt_t *node) {
	CHECK_REFCOUNT(node);
	if (node->expr) visit(node->expr);
	if (node->stmt) visit(node->stmt);
	mem_free((gnode_t*)node);
}

static void free_flow_stmt (gvisitor_t *self, gnode_flow_stmt_t *node) {
	CHECK_REFCOUNT(node);
	if (node->cond) visit(node->cond);
	if (node->stmt) visit(node->stmt);
	if (node->elsestmt) visit(node->elsestmt);
	mem_free((gnode_t*)node);
}

static void free_loop_stmt (gvisitor_t *self, gnode_loop_stmt_t *node) {
	CHECK_REFCOUNT(node);
	if (node->stmt) visit(node->stmt);
	if (node->cond) visit(node->cond);
	if (node->expr) visit(node->expr);
	mem_free((gnode_t*)node);
}

static void free_jump_stmt (gvisitor_t *self, gnode_jump_stmt_t *node) {
	CHECK_REFCOUNT(node);
	if (node->expr) visit(node->expr);
	mem_free((gnode_t*)node);
}

static void free_empty_stmt (gvisitor_t *self, gnode_empty_stmt_t *node) {
	#pragma unused(self)
	CHECK_REFCOUNT(node);
	mem_free((gnode_t*)node);
}

static void free_variable (gvisitor_t *self, gnode_var_t *p) {
	CHECK_REFCOUNT(p);
	if (p->identifier) mem_free((void *)p->identifier);
	if (p->annotation_type) mem_free((void *)p->annotation_type);
	if (p->expr) visit(p->expr);
	mem_free((void *)p);
}

static void free_function_decl (gvisitor_t *self, gnode_function_decl_t *node) {
	CHECK_REFCOUNT(node);
	if (node->symtable) symboltable_free(node->symtable);
	if (node->identifier) mem_free((void *)node->identifier);
	if (node->params) {
		gnode_array_each(node->params, {free_variable(self, (gnode_var_t *)val);});
		gnode_array_free(node->params);
	}
	
	if (node->block) visit((gnode_t *)node->block);
	if (node->uplist) {
		gtype_array_each(node->uplist, {mem_free(val);}, gupvalue_t*);
		gnode_array_free(node->uplist);
	}
	mem_free((gnode_t*)node);
}

static void free_variable_decl (gvisitor_t *self, gnode_variable_decl_t *node) {
	CHECK_REFCOUNT(node);
	if (node->decls) {
		gnode_array_each(node->decls, {free_variable(self, (gnode_var_t *)val);});
		gnode_array_free(node->decls);
	}
	mem_free((gnode_t*)node);
}

static void free_enum_decl (gvisitor_t *self, gnode_enum_decl_t *node) {
	#pragma unused(self)
	CHECK_REFCOUNT(node);
	if (node->identifier) mem_free((void *)node->identifier);
	if (node->symtable) symboltable_free(node->symtable);
	mem_free((gnode_t*)node);
}

static void free_class_decl (gvisitor_t *self, gnode_class_decl_t *node) {
	CHECK_REFCOUNT(node);
	if (node->identifier) mem_free((void *)node->identifier);
	if (node->decls) {
		gnode_array_each(node->decls, {visit(val);});
		gnode_array_free(node->decls);
	}
	
	if (node->symtable) symboltable_free(node->symtable);
	mem_free((gnode_t*)node);
}

static void free_module_decl (gvisitor_t *self, gnode_module_decl_t *node) {
	CHECK_REFCOUNT(node);
	if (node->identifier) mem_free((void *)node->identifier);
	if (node->decls) {
		gnode_array_each(node->decls, {visit(val);});
		gnode_array_free(node->decls);
	}
	
	if (node->symtable) symboltable_free(node->symtable);
	mem_free((gnode_t*)node);
}

static void free_binary_expr (gvisitor_t *self, gnode_binary_expr_t *node) {
	CHECK_REFCOUNT(node);
	if (node->left) visit(node->left);
	if (node->right) visit(node->right);
	mem_free((gnode_t*)node);
}

static void free_unary_expr (gvisitor_t *self, gnode_unary_expr_t *node) {
	CHECK_REFCOUNT(node);
	if (node->expr) visit(node->expr);
	mem_free((gnode_t*)node);
}

static void free_postfix_subexpr (gvisitor_t *self, gnode_postfix_subexpr_t *subnode) {
	CHECK_REFCOUNT(subnode);
	
	gnode_n tag = subnode->base.tag;
	if (tag == NODE_CALL_EXPR) {
		if (subnode->args) {
			gnode_array_each(subnode->args, visit(val););
			gnode_array_free(subnode->args);
		}
	} else {
		visit(subnode->expr);
	}
	
	mem_free((gnode_t*)subnode);
}

static void free_postfix_expr (gvisitor_t *self, gnode_postfix_expr_t *node) {
	CHECK_REFCOUNT(node);
	
	visit(node->id);
	
	// node->list can be NULL due to enum static conversion
	size_t count = gnode_array_size(node->list);
	for (size_t i=0; i<count; ++i) {
		gnode_postfix_subexpr_t *subnode = (gnode_postfix_subexpr_t *) gnode_array_get(node->list, i);
		free_postfix_subexpr(self, subnode);
	}
	if (node->list) gnode_array_free(node->list);
	mem_free((gnode_t*)node);
}

static void free_file_expr (gvisitor_t *self, gnode_file_expr_t *node) {
	#pragma unused(self)
	CHECK_REFCOUNT(node);
	cstring_array_each(node->identifiers, {
		mem_free((void *)val);
	});
	
	gnode_array_free(node->identifiers);
	mem_free((void *)node);
}

static void free_literal_expr (gvisitor_t *self, gnode_literal_expr_t *node) {
	#pragma unused(self)
	CHECK_REFCOUNT(node);
	if (node->type == LITERAL_STRING) mem_free((void *)node->value.str);
	mem_free((void *)node);
}

static void free_identifier_expr (gvisitor_t *self, gnode_identifier_expr_t *node) {
	#pragma unused(self, node)
	CHECK_REFCOUNT(node);
	if (node->value) mem_free((void *)node->value);
	if (node->value2) mem_free((void *)node->value2);
	mem_free((void *)node);
}

static void free_keyword_expr (gvisitor_t *self, gnode_keyword_expr_t *node) {
	#pragma unused(self)
	CHECK_REFCOUNT(node);
	mem_free((void *)node);
}

static void free_list_expr (gvisitor_t *self, gnode_list_expr_t *node) {
	CHECK_REFCOUNT(node);
	if (node->list1) {
		gnode_array_each(node->list1, {visit(val);});
		gnode_array_free(node->list1);
	}
	if (node->list2) {
		gnode_array_each(node->list2, {visit(val);});
		gnode_array_free(node->list2);
	}
	mem_free((gnode_t*)node);
}

static void gravity_astfree (void *node) {
	gvisitor_t visitor = {
		.nerr = 0,
		.data = NULL,
		.delegate = NULL,
		
		// STATEMENTS: 7
		.visit_list_stmt = free_list_stmt,
		.visit_compound_stmt = free_compound_stmt,
		.visit_label_stmt = free_label_stmt,
		.visit_flow_stmt = free_flow_stmt,
		.visit_loop_stmt = free_loop_stmt,
		.visit_jump_stmt = free_jump_stmt,
		.visit_empty_stmt = free_empty_stmt,
		
		// DECLARATIONS: 5
		.visit_function_decl = free_function_decl,
		.visit_variable_decl = free_variable_decl,
		.visit_enum_decl = free_enum_decl,
		.visit_class_decl = free_class_decl,
		.visit_module_decl = free_module_decl,
		
		// EXPRESSIONS: 7+1
		.visit_binary_expr = free_binary_expr,
		.visit_unary_expr = free_unary_expr,
		.visit_file_expr = free_file_expr,
		.visit_literal_expr = free_literal_expr,
		.visit_identifier_expr = free_identifier_expr,
		.visit_keyword_expr = free_keyword_expr,
		.visit_list_expr = free_list_expr,
		.visit_postfix_expr = free_postfix_expr
	};
	
	gvisit(&visitor, (gnode_t*)node);
}

// MARK: -

void gnode_free (gnode_t *ast) {
	gravity_astfree(ast);
}
