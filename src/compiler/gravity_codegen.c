//
//  gravity_codegen.c
//  gravity
//
//  Created by Marco Bambini on 09/10/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_codegen.h"
#include "gravity_symboltable.h"
#include "gravity_optimizer.h"
#include "gravity_visitor.h"
#include "gravity_ircode.h"
#include "gravity_utils.h"
#include "gravity_array.h"
#include "gravity_hash.h"

typedef marray_t(gnode_class_decl_t *)		gnode_class_r;
struct codegen_t {
	gravity_object_r	context;
	gnode_class_r		superfix;
	gravity_vm			*vm;
};
typedef struct codegen_t codegen_t;

#define CONTEXT_PUSH(x)					marray_push(gravity_object_t*, ((codegen_t *)self->data)->context, (gravity_object_t*)x)
#define CONTEXT_POP()					marray_pop(((codegen_t *)self->data)->context)
#define CONTEXT_GET()					marray_last(((codegen_t *)self->data)->context)
#define CONTEXT_IS_MODULE(x)			((OBJECT_ISA_FUNCTION(x)) && (string_cmp(((gravity_function_t *)x)->identifier, INITMODULE_NAME) == 0))

#define DECLARE_CONTEXT()				gravity_object_t *context_object = CONTEXT_GET();
#define DECLARE_FUNCTION_CONTEXT()		DECLARE_CONTEXT();																	\
										assert(OBJECT_ISA_FUNCTION(context_object));										\
										gravity_function_t *context_function = (gravity_function_t *)context_object;
#define DECLARE_CLASS_CONTEXT()			DECLARE_CONTEXT();																	\
										assert(OBJECT_ISA_CLASS(context_object));											\
										gravity_class_t *context_class = (gravity_class_t *)context_object;
#define DECLARE_CODE()					DECLARE_FUNCTION_CONTEXT();															\
										ircode_t *code = (ircode_t *)context_function->bytecode;

#define IS_IMPLICIT_SELF(_expr)			(NODE_ISA(_expr, NODE_IDENTIFIER_EXPR) &&											\
										(((gnode_identifier_expr_t *)_expr)->location.type == LOCATION_CLASS_IVAR_SAME) &&	\
										(((gnode_identifier_expr_t *)_expr)->location.nup == 0) &&							\
										(((gnode_identifier_expr_t *)_expr)->location.index == UINT16_MAX))

#define IS_SUPER(_expr)					(NODE_ISA(_expr, NODE_KEYWORD_EXPR) && (((gnode_keyword_expr_t *)_expr)->base.token.type == TOK_KEY_SUPER))

#define GET_VM()						((codegen_t *)self->data)->vm

#define IS_LAST_LOOP(n1,n2)				(n1+1==n2)

#if 0
#define CODEGEN_COUNT_REGISTERS(_n)						uint32_t _n = ircode_register_count(code)
#define CODEGEN_ASSERT_REGISTERS(_n1,_n2,_v)			assert(_n2 -_n1 == (_v))
#else
#define CODEGEN_COUNT_REGISTERS(_n)
#define CODEGEN_ASSERT_REGISTERS(_n1,_n2,_v)
#endif

// MARK: -

static void report_error (gvisitor_t *self, gnode_t *node, const char *format, ...) {
	// increment internal error counter
	++self->nerr;
	
	// get error callback (if any)
	void *data = (self->delegate) ? ((gravity_delegate_t *)self->delegate)->xdata : NULL;
	gravity_error_callback error_fn = (self->delegate) ? ((gravity_delegate_t *)self->delegate)->error_callback : NULL;
	
	// build error message
	char		buffer[1024];
	va_list		arg;
	if (format) {
		va_start (arg, format);
		vsnprintf(buffer, sizeof(buffer), format, arg);
		va_end (arg);
	}
	
	// setup error struct
	error_desc_t error_desc = {
		.code = 0,
		.lineno = node->token.lineno,
		.colno = node->token.colno,
		.fileid = node->token.fileid,
		.offset = node->token.position
	};
	
	// finally call error callback
	if (error_fn) error_fn(GRAVITY_ERROR_SEMANTIC, buffer, error_desc, data);
	else printf("%s\n", buffer);
}

// MARK: -
static opcode_t token2opcode (gtoken_t op) {
	switch (op) {
		// BIT
		case TOK_OP_SHIFT_LEFT: return LSHIFT;
		case TOK_OP_SHIFT_RIGHT: return RSHIFT;
		case TOK_OP_BIT_NOT: return BNOT;
		case TOK_OP_BIT_AND: return BAND;
		case TOK_OP_BIT_OR: return BOR;
		case TOK_OP_BIT_XOR: return BXOR;
			
		// MATH
		case TOK_OP_ADD: return ADD;
		case TOK_OP_SUB: return SUB;
		case TOK_OP_DIV: return DIV;
		case TOK_OP_MUL: return MUL;
		case TOK_OP_REM: return REM;
		// NEG not handled here
			
		// COMPARISON
		case TOK_KEY_ISA: return ISA;
		case TOK_OP_LESS: return LT;
		case TOK_OP_GREATER: return GT;
		case TOK_OP_LESS_EQUAL: return LEQ;
		case TOK_OP_GREATER_EQUAL: return GEQ;
		case TOK_OP_ISEQUAL: return EQ;
		case TOK_OP_ISNOTEQUAL: return NEQ;
		case TOK_OP_ISIDENTICAL: return EQQ;
		case TOK_OP_ISNOTIDENTICAL: return NEQQ;
		case TOK_OP_PATTERN_MATCH: return MATCH;
		
		// LOGICAL
		case TOK_OP_AND: return AND;
		case TOK_OP_NOT: return NOT;
		case TOK_OP_OR: return OR;
		
		default: assert(0); break;  // should never reach this point
	}

	assert(0);
	return NOT; // huehue, geddit?
}

#if 0
static gravity_value_t literal2value (gnode_literal_expr_t *node) {
	if (node->type == LITERAL_STRING) return VALUE_FROM_STRING(NULL, node->value.str, node->len);
	if (node->type == LITERAL_FLOAT) return VALUE_FROM_FLOAT(node->value.d);
	if (node->type == LITERAL_INT) return VALUE_FROM_INT(node->value.n64);
	return VALUE_FROM_INT(node->value.n64); // default BOOLEAN case
}

static gravity_list_t *literals2list (gvisitor_t *self, gnode_r *r, uint32_t start, uint32_t stop) {
	gravity_list_t *list = gravity_list_new(GET_VM(), stop-start);
	
	for (uint32_t i=start; i<stop; ++i) {
		gnode_literal_expr_t *node = (gnode_literal_expr_t *)marray_get(*r, i);
		gravity_value_t value = literal2value(node);
		marray_push(gravity_value_t, list->array, value);
	}
	
	return list;
}

static gravity_map_t *literals2map (gvisitor_t *self, gnode_r *r1, gnode_r *r2, uint32_t start, uint32_t stop) {
	gravity_map_t *map = gravity_map_new(GET_VM(), stop-start);
	
	for (uint32_t i=start; i<stop; ++i) {
		gnode_literal_expr_t *_key = (gnode_literal_expr_t *)marray_get(*r1, i);
		gnode_literal_expr_t *_value = (gnode_literal_expr_t *)marray_get(*r2, i);
		
		// when here I am sure that both key and value are literals
		// so they can be LITERAL_STRING, LITERAL_FLOAT, LITERAL_INT, LITERAL_BOOL
		gravity_value_t key = literal2value(_key);
		gravity_value_t value = literal2value(_value);
		gravity_map_insert(NULL, map, key, value);
	}
	
	return map;
}

static gravity_map_t *enum2map (gvisitor_t *self, gnode_enum_decl_t *node) {
	uint32_t count = symboltable_count(node->symtable, 0);
	gravity_map_t *map = gravity_map_new(GET_VM(), count);
	
	// FixMe
	
	return map;
}

static bool check_literals_list (gvisitor_t *self, gnode_list_expr_t *node, bool ismap, size_t idxstart, size_t idxend, uint32_t dest) {
	DEBUG_CODEGEN("check_literal_list_expr");
	DECLARE_CODE();
	
	// first check if all nodes inside this chuck are all literals
	// and in this case apply a more efficient SETLIST variant
	for (size_t j=idxstart; j < idxend; ++j) {
		gnode_t *e = gnode_array_get(node->list1, j);
		if (!gnode_is_literal(e)) return false;
		if (ismap) {
			// additional check on key that must be a string literal in case of a map
			if (!gnode_is_literal_string(e)) return false;
			e = gnode_array_get(node->list2, j);
			if (!gnode_is_literal(e)) return false;
		}
	}
	
	// emit optimized (cpoll based) version
	gravity_value_t v;
	
	if (ismap) {
		gravity_map_t *map = literals2map(self, node->list1, node->list2, (uint32_t)idxstart, (uint32_t)idxend);
		v = VALUE_FROM_OBJECT(map);
	} else {
		gravity_list_t *list = literals2list(self, node->list1, (uint32_t)idxstart, (uint32_t)idxend);
		v = VALUE_FROM_OBJECT(list);
	}
	uint16_t index = gravity_function_cpool_add(GET_VM(), context_function, v);
	ircode_add(code, SETLIST, dest, 0, index);
	
	return true;
}
#endif

static uint32_t node2index (gnode_t * node) {
	// node can be a VARIABLE declaration or a local IDENTIFIER
	
	if (NODE_ISA(node, NODE_VARIABLE_DECL)) {
		gnode_variable_decl_t *expr = (gnode_variable_decl_t *)node;
		assert(gnode_array_size(expr->decls) == 1);
		
		gnode_var_t *var = (gnode_var_t *)gnode_array_get(expr->decls, 0);
		return var->index;
	}
	
	if (NODE_ISA(node, NODE_IDENTIFIER_EXPR)) {
		gnode_identifier_expr_t *expr = (gnode_identifier_expr_t *)node;
		assert(expr->location.type == LOCATION_LOCAL);
		return expr->location.index;
	}
	
	// should never reach this point because semacheck2 should take care of the check
	assert(0);
	return UINT32_MAX;
}

static void fix_superclasses (gvisitor_t *self) {
	// this function cannot fail because superclasses was already checked in samecheck2 so I am sure that they exist somewhere
	codegen_t		*data = (codegen_t *)self->data;
	gnode_class_r	*superfix = &data->superfix;
	
	size_t count = gnode_array_size(superfix);
	for (size_t i=0; i<count; ++i) {
		gnode_class_decl_t *node = (gnode_class_decl_t *)gnode_array_get(superfix, i);
		gnode_class_decl_t *super = (gnode_class_decl_t *)node->superclass;
		
		gravity_class_t	*c = (gravity_class_t *)node->data;
		gravity_class_setsuper(c, (gravity_class_t *)super->data);
	}
}

// this function can be called ONLY from visit_postfix_expr where a context has been pushed
static uint32_t compute_self_register (gvisitor_t *self, gnode_t *node, uint32_t target_register) {
	DEBUG_CODEGEN("compute_self_register");
	DECLARE_CODE();
	
	// check for special implicit self slot
	if (IS_IMPLICIT_SELF(node)) return 0;
	
	// check for super keyword
	if (IS_SUPER(node)) return 0;
	
	// if node refers to an outer class then load outer class from hidden _outer ivar and return its register
	if ((NODE_ISA(node, NODE_IDENTIFIER_EXPR) && ((gnode_identifier_expr_t *)node)->location.type == LOCATION_CLASS_IVAR_OUTER)) {
		gnode_identifier_expr_t *expr = (gnode_identifier_expr_t *)node;
		uint32_t dest = ircode_register_push_temp(code);
		uint32_t target = 0;
		
		for (uint16_t i=0; i<expr->location.nup; ++i) {
			ircode_add(code, LOAD, dest, target, 0 + MAX_REGISTERS);
			target = dest;
		}
		
		return ircode_register_pop_context_protect(code, true);
	}
	
	// no special register found, so just return the target
	return target_register;
}

// MARK: - Statements -

static void visit_list_stmt (gvisitor_t *self, gnode_compound_stmt_t *node) {
	DEBUG_CODEGEN("visit_list_stmt");
	gnode_array_each(node->stmts, {visit(val);});
}

static void visit_compound_stmt (gvisitor_t *self, gnode_compound_stmt_t *node) {
	DEBUG_CODEGEN("visit_compound_stmt");
	
	gnode_array_each(node->stmts, {
		visit(val);
		
		// check if context is a function
		DECLARE_CONTEXT();
		bool is_func_ctx = OBJECT_ISA_FUNCTION(context_object);
		if (!is_func_ctx) continue;
		
		// in case of function context cleanup temporary registers
		gravity_function_t *f = (gravity_function_t*)context_object;
		ircode_t *code = (ircode_t *)f->bytecode;
		ircode_register_clear_temps(code);
	});
	
	if (node->nclose != UINT32_MAX) {
		DECLARE_CODE();
		ircode_add(code, CLOSE, node->nclose, 0, 0);
	}
}

static void visit_label_stmt (gvisitor_t *self, gnode_label_stmt_t *node) {
	DEBUG_CODEGEN("visit_label_stmt");
	
	gtoken_t type = NODE_TOKEN_TYPE(node);
	assert((type == TOK_KEY_DEFAULT) || (type == TOK_KEY_CASE));
	
	if (type == TOK_KEY_DEFAULT) {visit(node->stmt);}
	else if (type == TOK_KEY_CASE) {visit(node->expr); visit(node->stmt);}
}

static void visit_flow_if_stmt (gvisitor_t *self, gnode_flow_stmt_t *node) {
	DEBUG_CODEGEN("visit_flow_if_stmt");
	DECLARE_CODE();
	
	/*
		 <condition>
		 if-false: goto $end
		 <then-part>
		 goto $true
		 $end:
		 <else-part>
		 $true:
		 
		 TRUE  := getLabel
		 FALSE := getLabel
		 compile condition
		 emit ifeq FALSE
		 compile stm1
		 emit goto TRUE
		 emit FALSE
		 compile stm2
		 emit TRUE
	 */
	
	uint32_t labelTrue  = ircode_newlabel(code);
	uint32_t labelFalse = ircode_newlabel(code);
	
	visit(node->cond);
	ircode_add(code, JUMPF, ircode_register_pop(code), labelFalse, 0);
	
	visit(node->stmt);
	if (node->elsestmt) ircode_add(code, JUMP, labelTrue, 0, 0);
	
	ircode_marklabel(code, labelFalse);
	if (node->elsestmt) {
		visit(node->elsestmt);
		ircode_marklabel(code, labelTrue);
	}
}

static void visit_flow_switch_stmt (gvisitor_t *self, gnode_flow_stmt_t *node) {
	DEBUG_CODEGEN("visit_flow_switch_stmt");
	
	visit(node->cond);
	visit(node->stmt);
}

static void visit_flow_stmt (gvisitor_t *self, gnode_flow_stmt_t *node) {
	DEBUG_CODEGEN("visit_flow_stmt");
	
	gtoken_t type = NODE_TOKEN_TYPE(node);
	assert((type == TOK_KEY_IF) || (type == TOK_KEY_SWITCH));
	
	if (type == TOK_KEY_IF) {
		visit_flow_if_stmt(self, node);
	} else if (type == TOK_KEY_SWITCH) {
		visit_flow_switch_stmt(self, node);
	}
}

static void visit_loop_while_stmt (gvisitor_t *self, gnode_loop_stmt_t *node) {
	DEBUG_CODEGEN("visit_loop_while_stmt");
	DECLARE_CODE();
	
	/*
		 $start:	<condition>
		 if-false: goto $end
		 <body>
		 goto $start
		 $end:
		 
		 START := getLabel
		 END  := getLabel
		 emit START
		 compile exp
		 emit ifeq END
		 compile stm
		 emit goto START
		 emit END
	 */
	
	uint32_t labelTrue  = ircode_newlabel(code);
	uint32_t labelFalse = ircode_newlabel(code);
	ircode_setlabel_true(code, labelTrue);
	ircode_setlabel_false(code, labelFalse);
	
	ircode_marklabel(code, labelTrue);
	visit(node->cond);
	ircode_add(code, JUMPF, ircode_register_pop(code), labelFalse, 0);
	
	visit(node->stmt);
	ircode_add(code, JUMP, labelTrue, 0, 0);
	
	ircode_marklabel(code, labelFalse);
	
	ircode_unsetlabel_true(code);
	ircode_unsetlabel_false(code);
}

static void visit_loop_repeat_stmt (gvisitor_t *self, gnode_loop_stmt_t *node) {
	DEBUG_CODEGEN("visit_loop_repeat_stmt");
	DECLARE_CODE();
	
	/*
		 $start:	<body>
		 <expression>
		 if-false: goto $start
		 $end:
	 */
	
	uint32_t labelTrue  = ircode_newlabel(code);
	uint32_t labelFalse = ircode_newlabel(code);	// end label is necessary to handle optional break statement
	ircode_setlabel_true(code, labelTrue);
	ircode_setlabel_false(code, labelFalse);
	
	ircode_marklabel(code, labelTrue);
	visit(node->stmt);
	visit(node->expr);
	ircode_add(code, JUMPF, ircode_register_pop(code), labelFalse, 0);
	ircode_add(code, JUMP, labelTrue, 0, 0);
	
	ircode_marklabel(code, labelFalse);
	
	ircode_unsetlabel_true(code);
	ircode_unsetlabel_false(code);
}

static void visit_loop_for_stmt (gvisitor_t *self, gnode_loop_stmt_t *node) {
	// https://www.natashatherobot.com/swift-alternatives-to-c-style-for-loops/
	
	DEBUG_CODEGEN("visit_loop_for_stmt");
	DECLARE_CODE();
	
	// FOR loop is transformed to a WHILE loop
	//
	// from:
	// for (cond in expr) {
	//    stmp;
	// }
	//
	// to:
	// {
	//    var $expr = expr;
	//    var $value = $expr.iterate(null);
	//    while ($value) {
	//		cond = $expr.next($value);
	//		stmp;
	//		$value = $expr.iterate($value);
	//    }
	// }
	
	uint32_t $expr = ircode_register_push_temp(code);			// ++TEMP => 1
	uint32_t $value = ircode_register_push_temp(code);			// ++TEMP => 2
	// $expr and $value are temporary registers that must not be cleared by ircode_register_clear_temps
	// in visit_compound_statement, so mark them to skip clear
	ircode_register_set_skip_clear(code, $expr);
	ircode_register_set_skip_clear(code, $value);
	
	uint16_t iterate_idx = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_CSTRING(NULL, ITERATOR_INIT_FUNCTION));
	uint16_t next_idx = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_CSTRING(NULL, ITERATOR_NEXT_FUNCTION));
	uint32_t cond_idx = node2index(node->cond);
	
	// generate code for $expr = expr (so expr is only evaluated once)
	visit(node->expr);
	uint32_t once_expr = ircode_register_pop(code);
	ircode_add(code, MOVE, $expr, once_expr, 0);
	
	// generate code for $value = $expr.iterate(null);
	uint32_t iterate_fn = ircode_register_push_temp(code);		// ++TEMP => 3
	ircode_add(code, LOADK, iterate_fn, iterate_idx, 0);
	ircode_add(code, LOAD, iterate_fn, $expr, iterate_fn);
	ircode_register_set_skip_clear(code, iterate_fn);
	
	uint32_t next_fn = ircode_register_push_temp(code);			// ++TEMP => 4
	ircode_add(code, LOADK, next_fn, next_idx, 0);
	ircode_add(code, LOAD, next_fn, $expr, next_fn);
	ircode_register_set_skip_clear(code, next_fn);
	
	uint32_t temp1 = ircode_register_push_temp(code);			// ++TEMP => 5
	ircode_add(code, MOVE, temp1, iterate_fn, 0);
	uint32_t temp2 = ircode_register_push_temp(code);			// ++TEMP => 6
	ircode_add(code, MOVE, temp2, $expr, 0);
	temp2 = ircode_register_push_temp(code);					// ++TEMP => 7
	ircode_add(code, LOADK, temp2, CPOOL_VALUE_NULL, 0);
	ircode_add(code, CALL, $value, temp1, 2);
	ircode_register_pop(code);									// --TEMP => 6
	ircode_register_pop(code);									// --TEMP => 5
	ircode_register_pop(code);									// --TEMP => 4
	
	// while code
	uint32_t labelTrue  = ircode_newlabel(code);
	uint32_t labelFalse = ircode_newlabel(code);
	ircode_setlabel_true(code, labelTrue);
	ircode_setlabel_false(code, labelFalse);
	
	ircode_marklabel(code, labelTrue);
	ircode_add(code, JUMPF, $value, labelFalse, 1);				// flag JUMPF instruction to check ONLY BOOL values
	
	// cond = $expr.next($value);
	// cond is a local variable
	temp1 = ircode_register_push_temp(code);					// ++TEMP => 5
	ircode_add(code, MOVE, temp1, next_fn, 0);
	temp2 = ircode_register_push_temp(code);					// ++TEMP => 6
	ircode_add(code, MOVE, temp2, $expr, 0);
	temp2 = ircode_register_push_temp(code);					// ++TEMP => 7
	ircode_add(code, MOVE, temp2, $value, 0);
	ircode_add(code, CALL, cond_idx, temp1, 2);
	
	// process statement
	visit(node->stmt);
	
	// pop next_fn temp register AFTER user code because function ptr must be protected inside loop
	ircode_register_pop(code);									// --TEMP => 6
	ircode_register_pop(code);									// --TEMP => 5
	ircode_register_pop(code);									// --TEMP => 4
	
	// update $value for the next check
	// $value = $expr.iterate($value);
	temp1 = ircode_register_push_temp(code);					// ++TEMP => 5
	ircode_add(code, MOVE, temp1, iterate_fn, 0);
	temp2 = ircode_register_push_temp(code);					// ++TEMP => 6
	ircode_add(code, MOVE, temp2, $expr, 0);
	temp2 = ircode_register_push_temp(code);					// ++TEMP => 7
	ircode_add(code, MOVE, temp2, $value, 0);
	ircode_add(code, CALL, $value, temp1, 2);
	ircode_register_pop(code);									// --TEMP => 6
	ircode_register_pop(code);									// --TEMP => 5
	ircode_register_pop(code);									// --TEMP => 4
	
	ircode_add(code, JUMP, labelTrue, 0, 0);
	
	ircode_marklabel(code, labelFalse);
	
	ircode_unsetlabel_true(code);
	ircode_unsetlabel_false(code);
	
	ircode_register_pop(code);									// --TEMP => 3
	ircode_register_pop(code);									// --TEMP => 2
	ircode_register_pop(code);									// --TEMP => 1
	ircode_register_pop(code);									// --TEMP => 0
	
	ircode_register_unset_skip_clear(code, $expr);
	ircode_register_unset_skip_clear(code, $value);
	ircode_register_unset_skip_clear(code, iterate_fn);
	ircode_register_unset_skip_clear(code, next_fn);
	
	if (node->nclose != UINT32_MAX) {
		ircode_add(code, CLOSE, node->nclose, 0, 0);
	}
}

static void visit_loop_stmt (gvisitor_t *self, gnode_loop_stmt_t *node) {
	DEBUG_CODEGEN("visit_loop_stmt");
	
	gtoken_t type = NODE_TOKEN_TYPE(node);
	assert((type == TOK_KEY_WHILE) || (type == TOK_KEY_REPEAT) || (type == TOK_KEY_FOR));
	
	if (type == TOK_KEY_WHILE) {
		visit_loop_while_stmt(self, node);
	} else if (type == TOK_KEY_REPEAT) {
		visit_loop_repeat_stmt(self, node);
	} else if (type == TOK_KEY_FOR) {
		visit_loop_for_stmt(self, node);
	}
}

static void visit_jump_stmt (gvisitor_t *self, gnode_jump_stmt_t *node) {
	DEBUG_CODEGEN("visit_jump_stmt");
	DECLARE_CODE();
	
	gtoken_t type = NODE_TOKEN_TYPE(node);
	assert((type == TOK_KEY_BREAK) || (type == TOK_KEY_CONTINUE) || (type == TOK_KEY_RETURN));
	
	if (type == TOK_KEY_BREAK) {
		uint32_t label = ircode_getlabel_false(code);
		ircode_add(code, JUMP, label, 0, 0); // goto $end;
	} else if (type == TOK_KEY_CONTINUE) {
		uint32_t label = ircode_getlabel_true(code);
		ircode_add(code, JUMP, label, 0, 0); // goto $start;
	} else if (type == TOK_KEY_RETURN) {
		if (node->expr) {
			visit(node->expr);
			ircode_add(code, RET, ircode_register_pop(code), 0, 0);
		} else {
			ircode_add(code, RET0, 0, 0, 0);
		}
	}
}

static void visit_empty_stmt (gvisitor_t *self, gnode_empty_stmt_t *node) {
	#pragma unused(self, node)
	DEBUG_CODEGEN("visit_empty_stmt");
	
	DECLARE_CODE();
	ircode_add(code, NOP, 0, 0, 0);
}

// MARK: - Declarations -

static void store_declaration (gvisitor_t *self, gravity_object_t *obj, bool is_static, gnode_function_decl_t *node) {
	DEBUG_CODEGEN("store_object_declaration");
	assert(obj);
	
	DECLARE_CONTEXT();
	bool is_module = CONTEXT_IS_MODULE(context_object);
	bool is_class = OBJECT_ISA_CLASS(context_object);
	bool is_local = ((is_module == false) && (is_class == false));
	if (is_static) assert(is_class); // static makes sense only for class objects
	
	if (is_local || is_module) {
		gravity_function_t *context_function = (gravity_function_t *)context_object;
		ircode_t *code = (ircode_t *)context_function->bytecode;
		
		// add object to cpool and get its index
		uint16_t index = gravity_function_cpool_add(NULL, context_function, VALUE_FROM_OBJECT(obj));
		
		// if it is a function then generate a CLOSURE opcode instead of LOADK
		if (OBJECT_ISA_FUNCTION(obj)) {
			gravity_function_t *f = (gravity_function_t *)obj;
			uint32_t regnum = ircode_register_push_temp(code);
			ircode_add(code, CLOSURE, regnum, index, 0);
			uint32_t upindex = 0;
			for (uint16_t i=0; i<f->nupvalues; ++i) {
				gupvalue_t *upvalue = (gupvalue_t *)gnode_array_get(node->uplist, i);
				uint32_t opindex = (upvalue->is_direct) ? upvalue->index : upindex++;
				ircode_add(code, MOVE, opindex, (upvalue->is_direct) ? 1 : 0, 0);
			}
		} else {
			ircode_add_constant(code, index);
		}
			
		if (is_module && obj->identifier) {
			index = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_CSTRING(NULL, obj->identifier));
			ircode_add(code, STOREG, ircode_register_pop(code), index, 0);
		}
			
		return;
	}
	
	if (is_class) {
		gravity_class_t *context_class = (gravity_class_t *)context_object;
		context_class = (is_static) ? context_class->objclass : context_class;
		gravity_class_bind(context_class, obj->identifier, VALUE_FROM_OBJECT(obj));
		return;
	}
	
	// should never reach this point
	assert(0);
}

// used ONLY by CODEGEN
static gravity_function_t *class_lookup_nosuper (gravity_class_t *c, const char *name) {
	STATICVALUE_FROM_STRING(key, name, strlen(name));
	gravity_value_t *v = gravity_hash_lookup(c->htable, key);
	return (v) ? (gravity_function_t*)v->p : NULL;
}

static void process_constructor (gvisitor_t *self, gravity_class_t *c) {
	DEBUG_CODEGEN("process_constructor");
	
	// $init is an internal function used to initialize instance variables to a default value
	// in case of subclasses USER is RESPONSIBLE to call super.init();
	// in case of subclasses COMPILER is RESPONSIBLE to create the appropriate $init call chain
	
	// check internal $init function
	gravity_function_t *internal_init_function = class_lookup_nosuper(c, CLASS_INTERNAL_INIT_NAME);
	
	// check for user constructor function
	gravity_function_t *constructor_function = class_lookup_nosuper(c, CLASS_CONSTRUCTOR_NAME);
	
	// build appropriate $init function
	gravity_class_t *super = c->superclass;
	uint32_t ninit = 2;
	while (super) {
		gravity_function_t *super_init = class_lookup_nosuper(super, CLASS_INTERNAL_INIT_NAME);
		if (super_init) {
			// copy super init code to internal_init code
			if (!internal_init_function) internal_init_function = gravity_function_new(NULL, CLASS_INTERNAL_INIT_NAME, 1, 0, 0, ircode_create(1));
			
			// build unique internal init name ($init2, $init3 and so on)
			char name[256];
			snprintf(name, sizeof(name), "%s%d", CLASS_INTERNAL_INIT_NAME, ninit++);
			
			// add new internal init to class and call it from main $init function
			// super_init should not be duplicated here because class hash table values are not freed (only keys are freed)
			gravity_class_bind(c, name, VALUE_FROM_OBJECT(super_init));
			uint16_t index = gravity_function_cpool_add(NULL, internal_init_function, VALUE_FROM_CSTRING(GET_VM(), name));
			ircode_patch_init((ircode_t *)internal_init_function->bytecode, index);
		}
		super = super->superclass;
	}
	
	// 4 cases to handle:
	
	// 1. internal_init and constuctor are not present, so nothing to do here
	if ((!internal_init_function) && (!constructor_function)) goto check_meta;
	
//	// 2. internal init is present and constructor is not used
//	if ((internal_init_function) && (!constructor_function)) {
//		// add a RET0 command
//		ircode_t *code = (ircode_t *)internal_init_function->bytecode;
//		ircode_add(code, RET0, 0, 0, 0);
//		
//		// bind internal init as constructor
//		gravity_class_bind(c, CLASS_CONSTRUCTOR_NAME, VALUE_FROM_OBJECT(internal_init_function));
//		gravity_object_setenclosing((gravity_object_t *)internal_init_function, (gravity_object_t *)c);
//		goto process_funcs;
//	}
	
	// 3. internal init is present so constructor is mandatory
	if (internal_init_function) {
		// convert ircode to bytecode for $init special function and add a RET0 command
		ircode_t *code = (ircode_t *)internal_init_function->bytecode;
		ircode_add(code, RET0, 0, 0, 0);
		
		if (constructor_function == NULL) {
			constructor_function = gravity_function_new(NULL, CLASS_CONSTRUCTOR_NAME, 1, 0, 2, ircode_create(1));
			ircode_t *code2 = (ircode_t *)constructor_function->bytecode;
			ircode_add_skip(code2);	// LOADK
			ircode_add_skip(code2);	// LOAD
			ircode_add_skip(code2);	// MOVE
			ircode_add_skip(code2);	// CALL
			gravity_class_bind(c, CLASS_CONSTRUCTOR_NAME, VALUE_FROM_OBJECT(constructor_function));
		}
	}
	
	// 4. constructor is present so internal init is optional
	if (constructor_function) {
		// add an implicit RET 0 (RET self) to the end of the constructor
		ircode_t *code = (ircode_t *)constructor_function->bytecode;
		ircode_add(code, RET, 0, 0, 0);
		
		if (internal_init_function) {
			// if an internal init function is present ($init) then add a call to it as a first instruction
			uint16_t index = gravity_function_cpool_add(GET_VM(), constructor_function, VALUE_FROM_CSTRING(NULL, CLASS_INTERNAL_INIT_NAME));
			
			// load constant
			uint32_t dest = ircode_register_push_temp(code);
			ircode_set_index(0, code, LOADK, dest, index, 0);
			
			// load from lookup
			ircode_set_index(1, code, LOAD, dest, 0, dest);
			
			// prepare parameters
			uint32_t dest2 = ircode_register_push_temp(code);
			ircode_set_index(2, code, MOVE, dest2, 0, 0);
			ircode_register_pop(code);
			
			// execute call
			ircode_set_index(3, code, CALL, dest, dest, 1);
		}
	}
	
//process_funcs:
	if (internal_init_function) gravity_optimizer(internal_init_function);
	if (constructor_function) gravity_optimizer(constructor_function);
	
check_meta:
	// recursively process constructor but stop when object or class class is found, otherwise an infinite loop is triggered
	if ((c->objclass) && (c->objclass->isa) && (c->objclass->isa != c->objclass->objclass)) process_constructor(self, c->objclass);
}

static void process_getter_setter (gvisitor_t *self, gnode_var_t *p, gravity_class_t *c) {
	gnode_compound_stmt_t	*expr = (gnode_compound_stmt_t *)p->expr;
	gnode_function_decl_t	*getter = (gnode_function_decl_t *)gnode_array_get(expr->stmts, 0);
	gnode_function_decl_t	*setter = (gnode_function_decl_t *)gnode_array_get(expr->stmts, 1);
	
	gnode_function_decl_t	*f1[2] = {getter, setter};
	gravity_function_t		*f2[2] = {NULL, NULL};
	
	for (uint16_t i=0; i<2; ++i) {
		gnode_function_decl_t *node = f1[i];
		if (!node) continue;
		
		// create gravity function
		uint16_t nparams = (node->params) ? (uint16_t)marray_size(*node->params) : 0;
		f2[i] = gravity_function_new(NULL, NULL, nparams, node->nlocals, 0, ircode_create(node->nlocals+nparams));
		
		// process inner block
		CONTEXT_PUSH(f2[i]);
		gnode_compound_stmt_t *block = node->block;
		if (block) {gnode_array_each(block->stmts, {visit(val);});}
		CONTEXT_POP();
		
		gravity_optimizer(f2[i]);
	}
	
	// getter and setter NULL means default
	// since getter and setter are methods and not simple functions, do not transfer to VM
	gravity_function_t *f = gravity_function_new_special(NULL, NULL, GRAVITY_COMPUTED_INDEX, f2[0], f2[1]);
	gravity_class_bind(c, p->identifier, VALUE_FROM_OBJECT(f));
}

static void visit_function_decl (gvisitor_t *self, gnode_function_decl_t *node) {
	DEBUG_CODEGEN("visit_function_decl %s", node->identifier);
	
	// extern means it will be provided at runtime by the delegate
	if (node->storage == TOK_KEY_EXTERN) return;
	
	DECLARE_CONTEXT();
	bool is_class_ctx = OBJECT_ISA_CLASS(context_object);
	
	// create new function object
	uint16_t nparams = (node->params) ? (uint16_t)marray_size(*node->params) : 0;
	gravity_function_t *f = gravity_function_new((is_class_ctx) ? NULL : GET_VM(), node->identifier,
												 nparams, node->nlocals, 0, (void *)ircode_create(node->nlocals+nparams));
	
	// check if f is a special constructor function (init)
	// name must be CLASS_CONSTRUCTOR_NAME and context_object must be a class
	bool is_constructor = (string_cmp(node->identifier, CLASS_CONSTRUCTOR_NAME) == 0) && (is_class_ctx);
	
	CONTEXT_PUSH(f);
	
	if (is_constructor) {
		// reserve first four instructions that could be later filled with a CALL to $init
		// see process_constructor for more information
		ircode_t *code = (ircode_t *)f->bytecode;
		ircode_add_skip(code);
		ircode_add_skip(code);
		ircode_add_skip(code);
		ircode_add_skip(code);
	}
	
	// process inner block
	ircode_t *code = (ircode_t *)f->bytecode;
	if (node->block) {
		gnode_array_each(node->block->stmts, {
			// process node
			visit(val);
			// reset temp registers after each node
			ircode_register_clear_temps(code);
		});
	}
	
	// check for upvalues
	if (node->uplist) f->nupvalues = (uint16_t)gnode_array_size(node->uplist);
	
	// remove current function
	CONTEXT_POP();
	
	// check for ircode errors
	if (ircode_iserror((ircode_t *)f->bytecode))
		report_error(self, (gnode_t *)node, "Maximum number of available registers used in function %s.", f->identifier);
	
	// store function in current context
	store_declaration(self, (gravity_object_t *)f, (node->storage == TOK_KEY_STATIC), node);
	
	// convert ircode to bytecode (postpone optimization of the constructor)
	if (!is_constructor) gravity_optimizer(f);
}

static void visit_variable_decl (gvisitor_t *self, gnode_variable_decl_t *node) {
	DEBUG_CODEGEN("visit_variable_decl");
	DECLARE_CONTEXT();
	
	// no initialization for extern variables since the real value will be provided at runtime
	if (node->storage == TOK_KEY_EXTERN) return;
	
	bool is_module = CONTEXT_IS_MODULE(context_object);
	bool is_class = OBJECT_ISA_CLASS(context_object);
	bool is_local = ((is_module == false) && (is_class == false));
	
	// loop through declarations
	size_t count = gnode_array_size(node->decls);
	for (size_t i=0; i<count; ++i) {
		gnode_var_t *p = (gnode_var_t *)gnode_array_get(node->decls, i);
		DEBUG_CODEGEN("visit_variable_decl %s", p->identifier);
		
		// variable declarations can be specified in:
		// FUNCTION (local variable)
		// MODULE (module variable)
		// CLASS (property)
		
		if (is_local) {
			// it is a local variable declaration (default initialized to NULL)
			// code is generate ONLY if an init expression is specified
			//
			//	example:
			//
			//	func foo () {
			//		var a = 10;
			//	}
			//
			//	LOADI	1 10	; move 10 into register 1
			//	MOVE	0 1		; move register 1 into register 0
			//
			
			// generate expression code
			if (p->expr) visit(p->expr); // context is a function
				
			gravity_function_t *context_function = (gravity_function_t *)context_object;
			ircode_t *code = (ircode_t *)context_function->bytecode;
			if (p->expr) {
				// assign to variable result of the expression
				ircode_add(code, MOVE, p->index, ircode_register_pop(code), 0);
			} else {
				// no default assignment expression found so initialize to NULL
				ircode_add(code, LOADK, p->index, CPOOL_VALUE_NULL, 0);
			}
			continue;
		}
		
		if (is_module) {
			// it is a module variable (default initialized to null)
			// code must ALWAYS be generated
			//
			// example 1:
			//	var a;
			//
			//	LOADK	0 NULL	; move null into register 0
			//	STOREG	0 0		; move register 0 into hash(constant_pool(0))
			//
			// example 2:
			//	var a = 10;
			//
			//	LOADI	0 10	; move 10 into register 0
			//	STOREG	0 0		; move register 0 into hash(constant_pool(0))
			//
			
			gravity_function_t *context_function = (gravity_function_t *)context_object;
			ircode_t *code = (ircode_t *)context_function->bytecode;
			uint16_t index = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_CSTRING(NULL, p->identifier));
			
			if (p->expr) {
				visit(p->expr); // context is a function
			} else {
				ircode_add_constant(code, CPOOL_VALUE_NULL);
			}
			ircode_add(code, STOREG, ircode_register_pop(code), index, 0);
			continue;
		}
		
		if (is_class) {
			bool is_static = (node->storage == TOK_KEY_STATIC);
			gravity_class_t *context_class = (is_static) ? ((gravity_class_t *)context_object)->objclass : (gravity_class_t *)context_object;
			
			// check computed property case first
			if ((p->expr) && (p->expr->tag == NODE_COMPOUND_STAT)) {
				process_getter_setter(self, p, context_class);
				continue;
			}
			
			// create ivar first
			uint16_t ivar_index = (p->index >= 0) ? p->index : gravity_class_add_ivar(context_class, NULL);
			
			// add default getter and setter ONLY if property is public
			if (node->access == TOK_KEY_PUBLIC) {
				// since getter and setter are methods and not simple functions, do not transfer to VM
				gravity_function_t *f = gravity_function_new_special(NULL, NULL, ivar_index, NULL, NULL); // getter and setter NULL means default
				gravity_class_bind(context_class, p->identifier, VALUE_FROM_OBJECT(f));
			}
			DEBUG_CODEGEN("Class: %s (static: %d) property: %s index: %d", context_class->identifier, is_static, p->identifier, ivar_index);
			
			// it is a property (default initialized to null)
			// code must be generated ONLY if an init expression is specified
			
			//
			//  example:
			// 	class foo {
			// 		var a = 10;
			// 	}
			//
			//  get $init function
			//  depending if variable has been created static
			//  and push it as current declaration then:
			//
			// 	LOADI	0 10	; move 10 into register 0
			// 	STOREF	0 0		; move register 0 into property 0
			
			if (p->expr) {
				// was gravity_class_lookup but that means than $init or init will be recursively searched also in super classes
				gravity_function_t *init_function = class_lookup_nosuper(context_class, CLASS_INTERNAL_INIT_NAME);
				if (init_function == NULL) {
					// no $init method found so create a new one
					init_function = gravity_function_new (NULL, CLASS_INTERNAL_INIT_NAME, 1, 0, 0, ircode_create(1));
					gravity_class_bind(context_class, CLASS_INTERNAL_INIT_NAME, VALUE_FROM_OBJECT(init_function));
				}
				
				CONTEXT_PUSH(init_function);
				ircode_t *code = (ircode_t *)init_function->bytecode;
				visit(p->expr);
				uint32_t dest = ircode_register_pop(code);
				ircode_add(code, STORE, dest, 0, p->index + MAX_REGISTERS);
				CONTEXT_POP();
			}
			
			continue;
		}
		
		// should never reach this point
		assert(0);
	}
}

static void visit_enum_decl (gvisitor_t *self, gnode_enum_decl_t *node) {
	#pragma unused(self,node)
	DEBUG_CODEGEN("visit_enum_decl %s", node->identifier);
	
	// enum is a map at runtime
	// enum foo {a=1,b=2,...}
	// is translated to
	// var foo = [a:1,b:2,...]
}

static void visit_class_decl (gvisitor_t *self, gnode_class_decl_t *node) {
	DEBUG_CODEGEN("visit_class_decl %s", node->identifier);
	
	// extern means it will be provided at runtime by the delegate
	if (node->storage == TOK_KEY_EXTERN) return;
	
	// create a new pair of classes (class itself and its meta)
	gravity_class_t *c = gravity_class_new_pair(GET_VM(), node->identifier, NULL, node->nivar, node->nsvar);
	
	// mark the class as a struct
	c->is_struct = node->is_struct;
	
	// check if class has a declared superclass
	if (node->superclass) {
		// node->superclass should be a gnode_class_decl_t at this point
		assert(NODE_ISA_CLASS(node->superclass));
		
		gnode_class_decl_t *super = (gnode_class_decl_t *)node->superclass;
		if (super->data) {
			// means that superclass has already been processed and its runtime representation is available
			gravity_class_setsuper(c, (gravity_class_t *)super->data);
		} else {
			// superclass has not yet processed so we need recheck the node at the end of the visit
			// add node to superfix for later processing
			codegen_t *data = (codegen_t *)self->data;
			marray_push(gnode_class_decl_t *, data->superfix, node);
		}
	}
	
	CONTEXT_PUSH(c);
	
	// process inner declarations
	gnode_array_each(node->decls, {visit(val);});
	
	// adjust declaration stack
	CONTEXT_POP();
	
	// fix constructor chain
	process_constructor(self, c);
	
	// store class declaration in current context
	store_declaration(self, (gravity_object_t *)c, (node->storage == TOK_KEY_STATIC), NULL);
	
	// save runtime representation
	// since this class could be a superclass of another class
	// then save opaque gravity_class_t pointer to node->data
	node->data = (void *)c;
}

static void visit_module_decl (gvisitor_t *self, gnode_module_decl_t *node) {
	#pragma unused(self, node)
	DEBUG_CODEGEN("visit_module_decl %s", node->identifier);

	// a module should be like a class with static entries
	// instantiated with import
	
//	gravity_module_t *module = gravity_module_new(GET_VM(), node->identifier);
//	CONTEXT_PUSH(module);
//	
//	// process inner declarations
//	gnode_array_each(node->decls, {visit(val);});
//	
//	// adjust declaration stack
//	CONTEXT_POP();
}

// MARK: - Expressions -

//static void process_special_enum (gvisitor_t *self, gnode_enum_decl_t *node, gnode_identifier_expr_t *identifier) {
//	symboltable_t *symtable = node->symtable;
//	gnode_t *v = symboltable_lookup(symtable, identifier->value);
//	assert(v);
//	assert(NODE_ISA(v, NODE_LITERAL_EXPR));
//	visit(v);
//}

static void visit_binary_expr (gvisitor_t *self, gnode_binary_expr_t *node) {
	DEBUG_CODEGEN("visit_binary_expr %s", token_name(node->op));
	DECLARE_CODE();
	
	// assignment is right associative
	if (node->op == TOK_OP_ASSIGN) {
		CODEGEN_COUNT_REGISTERS(n1);
		visit(node->right);
		visit(node->left);	// left expression can be: IDENTIFIER, FILE, POSTIFIX (not a call)
		CODEGEN_COUNT_REGISTERS(n2);
		CODEGEN_ASSERT_REGISTERS(n1, n2, 0);
		return;
	}
	
	CODEGEN_COUNT_REGISTERS(n1);
	
	// visiting binary operation from left to right
	visit(node->left);
	visit(node->right);
	
	uint32_t r3 = ircode_register_pop(code);
	uint32_t r2 = ircode_register_pop(code);
	uint32_t r1 = ircode_register_push_temp(code);
	
	// a special instruction needs to be generated for a binary expression of type RANGE
	if ((node->op == TOK_OP_RANGE_INCLUDED) || (node->op == TOK_OP_RANGE_EXCLUDED)) {
		ircode_add_tag(code, RANGENEW, r1, r2, r3, (node->op == TOK_OP_RANGE_INCLUDED) ? RANGE_INCLUDE_TAG : RANGE_EXCLUDE_TAG);
		return;
	}
	
	// generate code for binary OP
	opcode_t op = token2opcode(node->op);
	ircode_add(code, op, r1, r2, r3);
		
	CODEGEN_COUNT_REGISTERS(n2);
	CODEGEN_ASSERT_REGISTERS(n1, n2, 1);
}

static void visit_unary_expr (gvisitor_t *self, gnode_unary_expr_t *node) {
	DEBUG_CODEGEN("visit_unary_expr %s", token_name(node->op));
	DECLARE_CODE();
	
	CODEGEN_COUNT_REGISTERS(n1);
	
	// unary expression can be:
	//	+	Unary PLUS
	//	-	Unary MINUS
	//	!	Logical NOT
	//	~	Bitwise NOT
	
	visit(node->expr);
	if (node->op == TOK_OP_ADD) {
		// +1 is just 1 and more generally +expr is just expr so ignore + and proceed
		return;
	}
	
	uint32_t r2 = ircode_register_pop(code);
	uint32_t r1 = ircode_register_push_temp(code);
	
	opcode_t op = (node->op == TOK_OP_SUB) ? NEG : token2opcode(node->op);
	ircode_add(code, op, r1, r2, 0);
	
	CODEGEN_COUNT_REGISTERS(n2);
	CODEGEN_ASSERT_REGISTERS(n1, n2, 1);
}

static void visit_postfix_expr (gvisitor_t *self, gnode_postfix_expr_t *node) {
	DEBUG_CODEGEN("visit_call_expr");
	
	// node->list usually cannot be NULL, it is NULL only as a result of a static enum transformation (see semacheck2.c)
	if (node->list == NULL) {
		visit(node->id);
		return;
	}
	
	DECLARE_CODE();
	CODEGEN_COUNT_REGISTERS(n1);
	ircode_push_context(code);
	
	// disable MOVE optimization
	ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 0);
	
	// generate code for the common id node
	visit(node->id);
	
	bool is_super = IS_SUPER(node->id);
	
	// register that contains callable object
	uint32_t target_register = ircode_register_pop_context_protect(code, true);
	
	// register where to store final result
	uint32_t dest_register = target_register;
	
	// mandatory self register (initialized to 0 in case of implicit self or explicit super)
	uint32_r self_list; marray_init(self_list);
	uint32_t first_self_register = compute_self_register(self, node->id, target_register);
	marray_push(uint32_t, self_list, first_self_register);
	
	// process each subnode and set is_assignment flag
	bool is_assignment = node->base.is_assignment;
	size_t count = gnode_array_size(node->list);
	
	for (size_t i=0; i<count; ++i) {
		// a subnode can be a CALL_EXPR		=> id.()
		// a NODE_ACCESS_EXPR				=> id.id2
		// a NODE_SUBSCRIPT_EXPR			=> id[]
		// or ANY combination of the them!	=> id.id2.id3()[24]().id5()
		gnode_postfix_subexpr_t *subnode = (gnode_postfix_subexpr_t *)gnode_array_get(node->list, i);
		
		// identify postfix type: NODE_CALL_EXPR, NODE_ACCESS_EXPR, NODE_SUBSCRIPT_EXPR
		gnode_n tag = subnode->base.tag;
		
		// check assignment flag
		bool is_real_assigment = (is_assignment && IS_LAST_LOOP(i, count));
		
		if (tag == NODE_CALL_EXPR) {
			// a CALL instruction needs to properly prepare stack before execution
			// format is CALL A B C
			
			// where A is the destination register
			// B is the callable object
			// and C is the number of arguments passed to the CALL
			// arguments must be on the stack (so gravity VM can use a register window in order to speed up instruction)
			// and are expected to be starting from B+1
			
			// check dest register
			bool dest_is_temp = ircode_register_istemp(code, dest_register);
			if (!dest_is_temp) {
				dest_register = ircode_register_push_temp(code);
				dest_is_temp = true;
			}
			
			// add target register (must be temp)
			uint32_t temp_target_register = ircode_register_push_temp(code);
			ircode_add(code, MOVE, temp_target_register, target_register, 0);
			ircode_register_pop_context_protect(code, true);
			
			// always add SELF parameter (must be temp+1)
			uint32_t self_register = marray_pop(self_list);
			uint32_t temp_self_register = ircode_register_push_temp(code);
			ircode_add(code, MOVE, temp_self_register, self_register, 0);
			ircode_register_pop_context_protect(code, true);
			
			// process each parameter (each must be temp+2 ... temp+n)
			marray_decl_init(uint32_r, args);
			size_t n = gnode_array_size(subnode->args);
			for (size_t j=0; j<n; ++j) {
				// process each argument
				gnode_t *arg = (gnode_t *)gnode_array_get(subnode->args, j);
				ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 1);
				visit(arg);
				ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 0);
				uint32_t nreg = ircode_register_pop_context_protect(code, true);
				// make sure args are in consecutive register locations (from temp_target_register + 1 to temp_target_register + n)
				if (nreg != temp_target_register + j + 2) {
					uint32_t temp = ircode_register_push_temp(code);
					if (temp == 0) return; // temp value == 0 means codegen error (error will be automatically reported later in visit_function_decl
					ircode_add(code, MOVE, temp, nreg, 0);
					ircode_register_clear(code, nreg);
					nreg = ircode_register_pop_context_protect(code, true);
				}
				assert(nreg == temp_target_register + j + 2);
				marray_push(uint32_t, args, nreg);
			}
			
			// generate instruction CALL with count parameters (taking in account self)
			ircode_add(code, CALL, dest_register, temp_target_register, (uint32_t)n+1);
			
			// cleanup temp registers
			ircode_register_clear(code, temp_target_register);
			ircode_register_clear(code, temp_self_register);
			n = gnode_array_size(subnode->args);
			for (size_t j=0; j<n; ++j) {
				uint32_t reg = marray_get(args, j);
				ircode_register_clear(code, reg);
			}
			
			// update self list
			marray_push(uint32_t, self_list, dest_register);
			
			// a call returns a value
			if (IS_LAST_LOOP(i, count)) {
				if (ircode_register_count(code)) {
					// code added in order to protect the extra register pushed in case
					// of code like: f(20)(30)
					uint32_t last_register = ircode_register_last(code);
					if (dest_is_temp && last_register == dest_register) dest_is_temp = false;
				}
				if (dest_is_temp) ircode_register_push(code, dest_register);
				ircode_register_protect_outside_context(code, dest_register);
			}
			
			// free temp args array
			marray_destroy(args);
			
		} else if (tag == NODE_ACCESS_EXPR) {
			// process identifier node (semantic check assures that each node is an identifier)
			gnode_identifier_expr_t *expr = (gnode_identifier_expr_t *)subnode->expr;
			uint32_t index = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_CSTRING(NULL, expr->value));
			uint32_t index_register = ircode_register_push_temp(code);
			ircode_add(code, LOADK, index_register, index, 0);
			ircode_register_pop(code);
			
			// generate LOAD/STORE instruction
			dest_register = (is_real_assigment) ? ircode_register_pop(code) : ircode_register_push_temp(code);
			if (is_super) ircode_add(code, LOADS, dest_register, target_register, index_register);
			else ircode_add(code, (is_real_assigment) ? STORE : LOAD, dest_register, target_register, index_register);
			if ((!is_real_assigment) && (i+1<count)) ircode_register_pop_context_protect(code, true);
			
			// update self list (if latest instruction)
			// this was added in order to properly emit instructions for nested_class.gravity unit test
			if (!IS_LAST_LOOP(i, count)) {
				gnode_postfix_subexpr_t *nextnode = (gnode_postfix_subexpr_t *)gnode_array_get(node->list, i+1);
				if (nextnode->base.tag != NODE_CALL_EXPR) marray_push(uint32_t, self_list, dest_register);
			}
			
		} else if (tag == NODE_SUBSCRIPT_EXPR) {
			// process index
			ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 1);
			visit(subnode->expr);
			ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 0);
			uint32_t index = ircode_register_pop(code);
			
			// generate LOADAT/STOREAT instruction
			dest_register = (is_real_assigment) ? ircode_register_pop(code) : ircode_register_push_temp(code);
			ircode_add(code, (is_assignment) ? STOREAT : LOADAT, dest_register, target_register, index);
			if ((!is_real_assigment) && (i+1<count)) ircode_register_pop_context_protect(code, true);
		}
		
		// reset is_super flag
		is_super = false;
		
		// update target
		target_register = dest_register;
	}
	
	marray_destroy(self_list);
	ircode_pop_context(code);
	
	CODEGEN_COUNT_REGISTERS(n2);
	CODEGEN_ASSERT_REGISTERS(n1, n2, (is_assignment) ? -1 : 1);
	
	// re-enable MOVE optimization
	ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 1);
}

static void visit_file_expr (gvisitor_t *self, gnode_file_expr_t *node) {
	DEBUG_CODEGEN("visit_file_expr");
	DECLARE_CODE();
	
	CODEGEN_COUNT_REGISTERS(n1);
	
	// check if the node is a left expression of an assignment
	bool is_assignment = node->base.is_assignment;
	
	size_t count = gnode_array_size(node->identifiers);
	for (size_t i=0; i<count; ++i) {
		const char *identifier = gnode_array_get(node->identifiers, i);
		uint16_t kindex = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_CSTRING(NULL, identifier));
		
		if ((is_assignment) && (IS_LAST_LOOP(i, count)))
			ircode_add(code, STOREG, ircode_register_pop(code), kindex, 0);
		else
			ircode_add(code, LOADG, ircode_register_push_temp(code), kindex, 0);
	}
	
	CODEGEN_COUNT_REGISTERS(n2);
	CODEGEN_ASSERT_REGISTERS(n1, n2, (is_assignment) ? -1 : 1);
}

static void visit_literal_expr (gvisitor_t *self, gnode_literal_expr_t *node) {
	/*
	 
	 NOTE:
	 
	 doubles and int64 should be added to the constant pool but I avoid
	 adding them here so the optimizer has a way to perform better constant folding:
	 http://en.wikipedia.org/wiki/Constant_folding
	 http://www.compileroptimizations.com/category/constant_folding.htm
	 
	 */
	
	DEBUG_CODEGEN("visit_literal_expr");
	DECLARE_CODE();
	
	CODEGEN_COUNT_REGISTERS(n1);
	
	switch (node->type) {
		case LITERAL_STRING: {
			// LOADK temp, s
			uint16_t index = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_STRING(NULL, node->value.str, node->len));
			ircode_add_constant(code, index);
			DEBUG_CODEGEN("visit_literal_expr (string) %s", node->value.str);
		} break;
			
		case LITERAL_FLOAT:
			// LOADI temp, d
			ircode_add_double(code, node->value.d);
			DEBUG_CODEGEN("visit_literal_expr (float) %.2f", node->value.d);
			break;
		
		case LITERAL_INT:
			// LOADI temp, n
			ircode_add_int(code, node->value.n64);
			DEBUG_CODEGEN("visit_literal_expr (int) %lld", node->value.n64);
			break;
			
		case LITERAL_BOOL: {
			uint32_t value = (node->value.n64 == 0) ? CPOOL_VALUE_FALSE : CPOOL_VALUE_TRUE;
			ircode_add_constant(code, value);
			DEBUG_CODEGEN("visit_literal_expr (bool) %lld", node->value.n64);
		} break;
			
		default: assert(0);
	}
	
	CODEGEN_COUNT_REGISTERS(n2);
	CODEGEN_ASSERT_REGISTERS(n1, n2, 1);
}

static void visit_identifier_expr (gvisitor_t *self, gnode_identifier_expr_t *node) {
	DEBUG_CODEGEN("visit_identifier_expr %s", node->value);
	DECLARE_CODE();
	
	CODEGEN_COUNT_REGISTERS(n1);
	
	// check if the node is a left expression of an assignment
	bool is_assignment = node->base.is_assignment;
	
	const char			*identifier = node->value;		// identifier as c string
	gnode_location_type	type = node->location.type;		// location type
	uint16_t			index = node->location.index;	// symbol index
	uint16_t			nup = node->location.nup;		// upvalue index or outer index
	
	switch (type) {
			
		// local variable
		case LOCATION_LOCAL: {
			if (is_assignment)
				ircode_add(code, MOVE, index, ircode_register_pop(code), 0);
			else
				ircode_register_push(code, index);
		} break;
		
		// module (global) variable
		case LOCATION_GLOBAL: {
			uint16_t kindex = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_CSTRING(NULL, identifier));
			if (is_assignment)
				ircode_add(code, STOREG, ircode_register_pop(code), kindex, 0);
			else
				ircode_add(code, LOADG, ircode_register_push_temp(code), kindex, 0);
		} break;
		
		// upvalue access
		case LOCATION_UPVALUE: {
			gupvalue_t *upvalue = node->upvalue;
			if (is_assignment)
				ircode_add(code, STOREU, ircode_register_pop(code), upvalue->selfindex, 0);
			else
				ircode_add(code, LOADU, ircode_register_push_temp(code), upvalue->selfindex, 0);
		} break;
			
		// class ivar case (outer case just need a loop lookup before)
		case LOCATION_CLASS_IVAR_OUTER:
		case LOCATION_CLASS_IVAR_SAME: {
			// needs to differentiate ivar (indexed by an integer) from other cases (indexed by a string)
			bool		is_ivar = (index != UINT16_MAX);
			uint32_t	dest = 0;
			uint32_t	target = 0;
			
			if (type == LOCATION_CLASS_IVAR_OUTER) {
				dest = ircode_register_push_temp(code);
				for (uint16_t i=0; i<nup; ++i) {
					ircode_add(code, LOAD, dest, target, 0 + MAX_REGISTERS);
					target = dest;
				}
				if (is_assignment) ircode_register_pop(code);
			}
			
			uint32_t index_register;
			if (is_ivar) {
				// ivar case, use an index to load/retrieve it
				index_register = index + MAX_REGISTERS;
			} else {
				// not an ivar so it could be another class declaration like a func, a class or an enum
				// use lookup in order to retrieve it (assignment is handled here so you can change a
				// first class citizen at runtime too)
				uint16_t kindex = gravity_function_cpool_add(GET_VM(), context_function, VALUE_FROM_CSTRING(NULL, identifier));
				index_register = ircode_register_push_temp(code);
				ircode_add(code, LOADK, index_register, kindex, 0);
				ircode_register_pop(code);
			}
			
			if (is_assignment) {
				// should be prohibited by semantic to store something into a non ivar slot?
				dest = ircode_register_pop(code); // consume temp register
				ircode_add(code, STORE, dest, target, index_register);
			} else {
				dest = (type == LOCATION_CLASS_IVAR_OUTER) ? target : ircode_register_push_temp(code);
				ircode_add(code, LOAD, dest , target, index_register);
			}
		} break;
	}
		
	CODEGEN_COUNT_REGISTERS(n2);
	CODEGEN_ASSERT_REGISTERS(n1, n2, (is_assignment) ? -1 : 1);
}

static void visit_keyword_expr (gvisitor_t *self, gnode_keyword_expr_t *node) {
	DEBUG_CODEGEN("visit_keyword_expr %s", token_name(node->base.token.type));
	DECLARE_CODE();
	
	CODEGEN_COUNT_REGISTERS(n1);
	
	gtoken_t type = NODE_TOKEN_TYPE(node);
	switch (type) {
		case TOK_KEY_CURRFUNC:
			ircode_add_constant(code, CPOOL_VALUE_FUNC);
			break;
			
		case TOK_KEY_NULL:
			ircode_add_constant(code, CPOOL_VALUE_NULL);
			break;
			
		case TOK_KEY_SUPER:
			ircode_add_constant(code, CPOOL_VALUE_SUPER);
			break;
			
		case TOK_KEY_CURRARGS:
			// compiler can know in advance if arguments special array is used
			context_function->useargs = true;
			ircode_add_constant(code, CPOOL_VALUE_ARGUMENTS);
			break;
			
		case TOK_KEY_UNDEFINED:
			ircode_add_constant(code, CPOOL_VALUE_UNDEFINED);
			break;
			
		case TOK_KEY_TRUE:
			ircode_add_constant(code, CPOOL_VALUE_TRUE);
			break;
			
		case TOK_KEY_FALSE:
			ircode_add_constant(code, CPOOL_VALUE_FALSE);
			break;
			
		default:
			// should never reach this point
			assert(0);
			break;
	}
	
	CODEGEN_COUNT_REGISTERS(n2);
	CODEGEN_ASSERT_REGISTERS(n1, n2, 1);
}

static void visit_list_expr (gvisitor_t *self, gnode_list_expr_t *node) {
	DEBUG_CODEGEN("visit_list_expr");
	DECLARE_CODE();
	
	CODEGEN_COUNT_REGISTERS(n1);
	
	bool	 ismap = node->ismap;
	uint32_t n = (uint32_t)gnode_array_size(node->list1);
	
	// a map requires twice registers than a list
	uint32_t max_fields = (ismap) ? MAX_FIELDSxFLUSH : MAX_FIELDSxFLUSH*2;
	
	// destination register of a new instruction is ALWAYS a temp register
	// then the optimizer could decide to optimize and merge the step
	uint32_t dest = ircode_register_push_temp(code);
	ircode_add(code, (ismap) ? MAPNEW : LISTNEW, dest, n, 0);
	if (n == 0) return;
	
	// this is just like Lua "fields per flush"
	// basically nodes are processed in a finite chunk
	// and then added to the list/map
	uint32_t nloops = n / max_fields;
	if (n % max_fields != 0) ++nloops;
	uint32_t nprocessed = 0;
	
	ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 0);
	while (nprocessed < n) {
		size_t k = (n - nprocessed > max_fields) ? max_fields : (n - nprocessed);
		size_t idxstart = nprocessed;
		size_t idxend = nprocessed + k;
		nprocessed += k;
		
		// check if this chunk can be optimized
		// if (check_literals_list(self, node, ismap, idxstart, idxend, dest)) continue;
		
		// save register context
		ircode_push_context(code);
			
		// process each node
		for (size_t i=1, j=idxstart; j<idxend; ++j) {
			gnode_t *e = gnode_array_get(node->list1, j);
			visit(e);
			uint32_t nreg = ircode_register_pop_context_protect(code, true);
			
			if (nreg != dest + i) {
				uint32_t temp_register = ircode_register_push_temp(code);
				ircode_add(code, MOVE, temp_register, nreg, 0);
				ircode_register_clear(code, nreg);
				ircode_register_pop_context_protect(code, true);
				assert(temp_register == dest + i);
			}
			
			if (ismap) {
				e = gnode_array_get(node->list2, j);
				ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 1);
				visit(e);
				ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 0);
				nreg = ircode_register_pop_context_protect(code, true);
				
				if (nreg != dest + i + 1) {
					uint32_t temp_register = ircode_register_push_temp(code);
					ircode_add(code, MOVE, temp_register, nreg, 0);
					ircode_register_clear(code, nreg);
					ircode_register_pop_context_protect(code, true);
					assert(temp_register == dest + i + 1);
				}
			}
			
			i += (ismap) ? 2 : 1;
		}
		
		// emit proper SETLIST instruction
		// since in a map registers are always used in pairs (key, value) it is
		// extremely easier to just set reg1 to be always 0 and use r in a loop
		ircode_add(code, SETLIST, dest, (uint32_t)(idxend-idxstart), 0);
		
		// restore register context
		ircode_pop_context(code);
	}
	
	ircode_pragma(code, PRAGMA_MOVE_OPTIMIZATION, 1);
	CODEGEN_COUNT_REGISTERS(n2);
	CODEGEN_ASSERT_REGISTERS(n1, n2, 1);
}

// MARK: -

gravity_function_t *gravity_codegen(gnode_t *node, gravity_delegate_t *delegate, gravity_vm *vm) {
	codegen_t data;
	data.vm = vm;
	marray_init(data.context);
	marray_init(data.superfix);
	
	ircode_t *code = ircode_create(0);
	gravity_function_t *f = gravity_function_new(vm, INITMODULE_NAME, 0, 0, 0, code);
	marray_push(gravity_object_t*, data.context, (gravity_object_t *)f);
	
	gvisitor_t visitor = {
		.nerr = 0,						// used for internal codegen errors
		.data = &data,					// used to store a pointer to codegen struct
		.delegate = (void *)delegate,	// compiler delegate to report errors
		
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
	
	DEBUG_CODEGEN("=== BEGIN CODEGEN ===");
	gvisit(&visitor, node);
	DEBUG_CODEGEN("\n");
	
	// check for special superfix
	if (marray_size(data.superfix)) {
		fix_superclasses(&visitor);
	}
	
	// pop globals instance init special function
	marray_pop(data.context);	
	assert(marray_size(data.context) == 0);
	marray_destroy(data.context);
	
	// in case of codegen errors explicity free code and return NULL
	if (visitor.nerr != 0) {ircode_free(code); f->bytecode = NULL;}
	return (visitor.nerr == 0) ? f : NULL;
}
