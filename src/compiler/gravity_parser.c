//
//  gravity_parser.c
//  gravity
//
//  Created by Marco Bambini on 01/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_symboltable.h"
#include "gravity_parser.h"
#include "gravity_macros.h"
#include "gravity_lexer.h"
#include "gravity_token.h"
#include "gravity_utils.h"
#include "gravity_array.h"
#include "gravity_core.h"
#include "gravity_ast.h"

typedef marray_t(gravity_lexer_t*)		lexer_r;

struct gravity_parser_t {
	lexer_r								*lexer;
	gnode_r								*statements;
	uint16_r							declarations;
	gravity_delegate_t					*delegate;
	
	double								time;
	uint32_t							nerrors;
	uint32_t							unique_id;
	uint32_t							last_error_lineno;
	
	// state ivars used by Pratt parser
	gtoken_t							current_token;
	gnode_t								*current_node;
};

// MARK: - PRATT parser specs -
// http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/
// http://javascript.crockford.com/tdop/tdop.html

// Precedence table as defined in Swift
// http://nshipster.com/swift-operators/
typedef enum {
	PREC_LOWEST,
	PREC_ASSIGN      = 90,	// = *= /= %= += -= <<= >>= &= ^= |=	(11 cases)
	PREC_TERNARY     = 100,	// ?:									  (1 case)
	PREC_LOGICAL_OR  = 110,	// ||									  (1 case)
	PREC_LOGICAL_AND = 120,	// &&									  (1 case)
	PREC_COMPARISON  = 130,	// < <= > >= == != === !== ~=			 (9 cases)
	PREC_ISA         = 132,	// isa									  (1 case)
	PREC_RANGE       = 135,	// ..< ...								 (2 cases)
	PREC_TERM        = 140,	// + - | ^								 (4 cases)
	PREC_FACTOR      = 150,	// * / % &								 (4 cases)
	PREC_SHIFT       = 160,	// << >>								 (2 cases)
	PREC_UNARY       = 170,	// + - ! ~								 (4 cases)
	PREC_CALL        = 200  // . ( [								 (3 cases)
} prec_level;

typedef gnode_t* (*parse_func) (gravity_parser_t *parser);

typedef struct {
	parse_func			prefix;
	parse_func			infix;
	prec_level			precedence;
	const char			*name;
	bool				right;
} grammar_rule;

// This table defines all of the parsing rules for the prefix and infix expressions in the grammar.
#define RULE(prec, fn1, fn2)			(grammar_rule){ fn1, fn2, prec, NULL, false}
#define PREFIX(prec, fn)				(grammar_rule){ fn, NULL, prec, NULL, false}
#define INFIX(prec, fn)					(grammar_rule){ NULL, fn, prec, NULL, false}
#define INFIX_OPERATOR(prec, name)		(grammar_rule){ NULL, parse_infix, prec, name, false}
#define INFIX_OPERATOR_RIGHT(prec,name) (grammar_rule){ NULL, parse_infix, prec, name, true}
#define PREFIX_OPERATOR(name)			(grammar_rule){ parse_unary, NULL, PREC_LOWEST, name, false}
#define OPERATOR(prec, name)			(grammar_rule){ parse_unary, parse_infix, prec, name, false}

// Global singleton grammar rule table
static grammar_rule rules[TOK_END];

// MARK: - Internal macros -
#define SEMICOLON_IS_OPTIONAL					1

#define REPORT_ERROR(_tok,...)					report_error(parser, GRAVITY_ERROR_SYNTAX, _tok, __VA_ARGS__)
#define REPORT_WARNING(_tok,...)				report_error(parser, GRAVITY_WARNING, _tok, __VA_ARGS__)

#define PUSH_DECLARATION(_decl)					marray_push(uint16_t, parser->declarations, (uint16_t)_decl)
#define PUSH_FUNCTION_DECLARATION()				PUSH_DECLARATION(NODE_FUNCTION_DECL)
#define PUSH_CLASS_DECLARATION()				PUSH_DECLARATION(NODE_CLASS_DECL)
#define POP_DECLARATION()						marray_pop(parser->declarations)
#define LAST_DECLARATION()						(marray_size(parser->declarations) ? marray_last(parser->declarations) : 0)
#define IS_FUNCTION_ENCLOSED()					(LAST_DECLARATION() == NODE_FUNCTION_DECL)
#define IS_CLASS_ENCLOSED()						(LAST_DECLARATION() == NODE_CLASS_DECL)
#define CHECK_NODE(_n)							if (!_n) return NULL

#define POP_LEXER								(gravity_lexer_t *)marray_pop(*parser->lexer)
#define CURRENT_LEXER							(gravity_lexer_t *)marray_last(*parser->lexer)
#define DECLARE_LEXER							gravity_lexer_t *lexer = CURRENT_LEXER; DEBUG_LEXER(lexer)

#define STATIC_TOKEN_CSTRING(_s,_n,_l,_b,_t)	char _s[_n] = {0}; uint32_t _l = 0;			\
												const char *_b = token_string(_t, &_l);		\
												if (_l) memcpy(_s, _b, MINNUM(_n, _l))

// MARK: - Prototypes -
static const char *parse_identifier (gravity_parser_t *parser);
static gnode_t *parse_statement (gravity_parser_t *parser);
static gnode_r *parse_optional_parameter_declaration (gravity_parser_t *parser);
static gnode_t *parse_compound_statement (gravity_parser_t *parser);
static gnode_t *parse_expression (gravity_parser_t *parser);
static gnode_t *parse_declaration_statement (gravity_parser_t *parser);
static gnode_t *parse_function (gravity_parser_t *parser, bool is_declaration, gtoken_t access_specifier, gtoken_t storage_specifier);
static gnode_t *adjust_assignment_expression (gtoken_t tok, gnode_t *lnode, gnode_t *rnode);

// MARK: - Utils functions -

static void report_error (gravity_parser_t *parser, error_type_t error_type, gtoken_s token, const char *format, ...) {
	// consider just one error for each line;
	if (parser->last_error_lineno == token.lineno) return;
	parser->last_error_lineno = token.lineno;
	
	// increment internal error counter
	++parser->nerrors;
	
	// get error callback (if any)
	void *data = (parser->delegate) ? parser->delegate->xdata : NULL;
	gravity_error_callback error_fn = (parser->delegate) ? ((gravity_delegate_t *)parser->delegate)->error_callback : NULL;
	
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
		.lineno = token.lineno,
		.colno = token.colno,
		.fileid = token.fileid,
		.offset = token.position
	};
	
	// finally call error callback
	if (error_fn) error_fn(error_type, buffer, error_desc, data);
	else printf("%s\n", buffer);
}

static gnode_t *parse_error (gravity_parser_t *parser) {
	DECLARE_LEXER;
	gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	REPORT_ERROR(token, "%s", token.value);
	return NULL;
}

// RETURN:
//	- true if next token is equal to token passed as parameter (token is also consumed)
//	- false if next token is not equal (and no error is reported)
//
static bool parse_optional (gravity_parser_t *parser, gtoken_t token) {
	DECLARE_LEXER;
	
	gtoken_t peek = gravity_lexer_peek(lexer);
	if (token_iserror(peek)) {
		parse_error(parser);
		peek = gravity_lexer_peek(lexer);
	}
	
	if (peek == token) {
		gravity_lexer_next(lexer); // consume expected token
		return true;
	}
	
	// do not report any error in this case
	return false;
}

static bool parse_required (gravity_parser_t *parser, gtoken_t token) {
	if (parse_optional(parser, token)) return true;
	
	// token not found (and not consumed) so an error strategy must be implemented here
	
	// simple (but not simpler) error recovery
	// parser should keep track of what I am parsing
	// so based on tok I could have a token list of terminal symbols
	// call next until first sync symbol (or EOF or start of another terminal symbol is found)
	
	// simple error recovery, just consume next and report error
	DECLARE_LEXER;
	gtoken_t next = gravity_lexer_next(lexer);
	gtoken_s unexpected_token = gravity_lexer_token(lexer);
	REPORT_ERROR(unexpected_token, "Expected %s but found %s.", token_name(token), token_name(next));
	return false;
}

static bool parse_semicolon (gravity_parser_t *parser) {
	#if SEMICOLON_IS_OPTIONAL
	DECLARE_LEXER;
	if (gravity_lexer_peek(lexer) == TOK_OP_SEMICOLON) {gravity_lexer_next(lexer); return true;}
	return false;
	#else
	return parse_required(parser, TOK_OP_SEMICOLON);
	#endif
}

gnode_t *parse_function (gravity_parser_t *parser, bool is_declaration, gtoken_t access_specifier, gtoken_t storage_specifier) {
	DECLARE_LEXER;
	
	// access_specifier? storage_specifier? already parsed
	// 'function' IDENTIFIER '(' parameter_declaration_clause? ')' compound_statement
	
	// consume FUNC keyword (or peek for OPEN_CURLYBRACE)
	bool is_implicit = (gravity_lexer_peek(lexer) == TOK_OP_OPEN_CURLYBRACE);
	gtoken_s token = gravity_lexer_token(lexer);
	if (!is_implicit) {
		gtoken_t type = gravity_lexer_next(lexer);
		token = gravity_lexer_token(lexer);
		assert(type == TOK_KEY_FUNC);
	}
	
	// parse IDENTIFIER
	const char *identifier = NULL;
	if (is_declaration) {
		gtoken_t peek = gravity_lexer_peek(lexer);
		if (token_isoperator(peek)) {
			gravity_lexer_next(lexer);
			identifier = string_dup(token_name(peek));
		} else {
			identifier = parse_identifier(parser);
		}
		DEBUG_PARSER("parse_function_declaration %s", identifier);
	}
	
	// check and consume TOK_OP_OPEN_PARENTHESIS
	if (!is_implicit) parse_required(parser, TOK_OP_OPEN_PARENTHESIS);
	
	// parse optional parameter declaration clause
	gnode_r *params = (!is_implicit) ? parse_optional_parameter_declaration(parser) : NULL;
	
	// check and consume TOK_OP_CLOSED_PARENTHESIS
	if (!is_implicit) parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
	
	// parse compound statement
	PUSH_FUNCTION_DECLARATION();
	gnode_compound_stmt_t *compound = (gnode_compound_stmt_t*)parse_compound_statement(parser);
	POP_DECLARATION();
	
	// parse optional semicolon
	parse_semicolon(parser);
	
	return gnode_function_decl_create(token, identifier, access_specifier, storage_specifier, params, compound);
}

static char *cstring_from_token (gravity_parser_t *parser, gtoken_s token) {
	#pragma unused(parser)
	uint32_t len = 0;
	const char *buffer = token_string(token, &len);
	
	char *str = (char *)mem_alloc(len+1);
	memcpy(str, buffer, len);
	return str;
}

static gnode_t *local_store_declaration (const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_t *declaration) {
	gnode_r *decls = gnode_array_create();
	gnode_t *decl = gnode_variable_create(declaration->token, identifier ? string_dup(identifier) : NULL, NULL, access_specifier, declaration);
	gnode_array_push(decls, decl);
	return gnode_variable_decl_create(declaration->token, TOK_KEY_VAR, access_specifier, storage_specifier, decls);
}

static gliteral_t decode_number_binary (gtoken_s token, int64_t *n) {
	// from 2 in order to skip 0b
	*n = number_from_bin(&token.value[2], token.bytes-2);
	return LITERAL_INT;
}

static gliteral_t decode_number_octal (gtoken_s token, int64_t *n) {
	STATIC_TOKEN_CSTRING(str, 512, len, buffer, token);
	if (len) *n = (int64_t) number_from_oct(&str[2], len-2);
	return LITERAL_INT;
}

static gliteral_t decode_number_hex (gtoken_s token, int64_t *n, double *d) {
	#pragma unused(d)
	STATIC_TOKEN_CSTRING(str, 512, len, buffer, token);
	if (len) *n = (int64_t) number_from_hex(str, token.bytes);
	return LITERAL_INT;
}

// MARK: - Expressions -

static gnode_t *parse_ternary_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_ternary_expression");
	DECLARE_LEXER;
	
	// conditional expression already parsed
	gnode_t *cond = parser->current_node;
	if (!cond) return NULL;
	
	// '?' expression ':' expression
	
	// '?' already consumed
	gtoken_s token = gravity_lexer_token(lexer);
	
	// parse expression 1
	gnode_t *expr1 = parse_expression(parser);
	CHECK_NODE(expr1);
	
	parse_required(parser, TOK_OP_COLON);
	
	// parse expression 2
	gnode_t *expr2 = parse_expression(parser);
	CHECK_NODE(expr2);
	
	return gnode_flow_stat_create(token, cond, expr1, expr2);
}

static gnode_t *parse_file_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_file_expression");
	DECLARE_LEXER;
	
	// at least one identifier is mandatory
	// 'file' ('.' IDENTIFIER)+
	
	gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	
	if (gravity_lexer_peek(lexer) != TOK_OP_DOT) {
		REPORT_ERROR(token, "A .identifier list is expected here.");
		return NULL;
	}
	
	cstring_r *list = cstring_array_create();
	while (gravity_lexer_peek(lexer) == TOK_OP_DOT) {
		gravity_lexer_next(lexer); // consume TOK_OP_DOT
		const char *identifier = parse_identifier(parser);
		if (!identifier) return NULL;
		cstring_array_push(list, identifier);
	}
	
	return gnode_file_expr_create(token, list);
}

static const char *parse_identifier (gravity_parser_t *parser) {
	DECLARE_LEXER;
	
	// parse IDENTIFIER is always mandatory
	gtoken_t type = gravity_lexer_peek(lexer);
	if (type != TOK_IDENTIFIER) {
		if (type == TOK_ERROR) parse_error(parser);
		else REPORT_ERROR(gravity_lexer_token(lexer), "Expected identifier but found %s", token_name(type));
		return NULL;
	}
	
	gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	const char *identifier = cstring_from_token(parser, token);
	return identifier;
}

static const char *parse_optional_type_annotation (gravity_parser_t *parser) {
	DECLARE_LEXER;
	const char	*type_annotation = NULL;
	gtoken_t	peek = gravity_lexer_peek(lexer);
	
	// type annotation
	// function foo (a: string, b: number)
	
	// check for optional type_annotation
	if (peek == TOK_OP_COLON) {
		gravity_lexer_next(lexer); // consume TOK_OP_COLON
		
		// parse identifier
		type_annotation = parse_identifier(parser);
		if (!type_annotation) return NULL;
	}
	
	return type_annotation;
}

static gnode_t *parse_parentheses_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_parentheses_expression");
	
	// check and consume TOK_OP_OPEN_PARENTHESIS
	parse_required(parser, TOK_OP_OPEN_PARENTHESIS);
	
	// parse expression
	gnode_t *expr = parse_expression(parser);
	CHECK_NODE(expr);
	
	// check and consume TOK_OP_CLOSED_PARENTHESIS
	parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
	
	return expr;
}

static gnode_t *parse_list_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_list_expression");
	DECLARE_LEXER;
	
	/*
	 list_expression
	 :	'[' ((expression) (',' expression)*)? ']'		// array or empty array
	 |	'[' ((map_entry (',' map_entry)*) | ':') ']'	// map or empty map
	 ;
	 
	 map_entry
	 :	STRING ':' expression
	 ;
	 */
	
	// consume first '['
	parse_required(parser, TOK_OP_OPEN_SQUAREBRACKET);
	
	// this saved token is necessary to save start of the list/map
	gtoken_s token = gravity_lexer_token(lexer);
	
	// check for special empty list
	if (gravity_lexer_peek(lexer) == TOK_OP_CLOSED_SQUAREBRACKET) {
		gravity_lexer_next(lexer); // consume TOK_OP_CLOSED_SQUAREBRACKET
		return gnode_list_expr_create(token, NULL, NULL, false);
	}
	
	// check for special empty map
	if (gravity_lexer_peek(lexer) == TOK_OP_COLON) {
		gravity_lexer_next(lexer); // consume TOK_OP_COLON
		parse_required(parser, TOK_OP_CLOSED_SQUAREBRACKET);
		return gnode_list_expr_create(token, NULL, NULL, true);
	}
	
	// parse first expression (if any) outside of the list/map loop
	// in order to check if it is a list or map expression
	gnode_t *expr1 = parse_expression(parser);
	
	// if next token is a colon then assume a map
	bool ismap = (gravity_lexer_peek(lexer) == TOK_OP_COLON);
	
	// a list expression can be an array [expr1, expr2] or a map [string1: expr1, string2: expr2]
	// cannot be mixed so be very restrictive here
	
	gnode_r *list1 = gnode_array_create();
	gnode_r *list2 = (ismap) ? gnode_array_create() : NULL;
	if (expr1) gnode_array_push(list1, expr1);
	
	if (ismap) {
		parse_required(parser, TOK_OP_COLON);
		gnode_t *expr2 = parse_expression(parser);
		if (expr2) gnode_array_push(list2, expr2);
	}
	
	while (gravity_lexer_peek(lexer) == TOK_OP_COMMA) {
		gravity_lexer_next(lexer); // consume TOK_OP_COMMA
		
		// parse first expression
		expr1 = parse_expression(parser);
		if (expr1) gnode_array_push(list1, expr1);
		
		if (ismap) {
			parse_required(parser, TOK_OP_COLON);
			gnode_t *expr2 = parse_expression(parser);
			if (expr2) gnode_array_push(list2, expr2);
		}
	}
	
	parse_required(parser, TOK_OP_CLOSED_SQUAREBRACKET);
	return gnode_list_expr_create(token, list1, list2, ismap);
}

static gnode_t *parse_function_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_function_expression");
	
	// 'func' '(' parameter_declaration_clause? ')' compound_statement
	// or
	// compound_statement (implicit func and implicit parameters)
	/*
		example:
		func foo () {
	 var bar = func(x) {return x*2;}
	 return bar(3);
		}
	 
		it is equivalent to:
		
		func foo () {
	 func bar(x) {return x*2;}
	 return bar(3);
		}
		
	 */
	
	// check if func is a function expression or
	// if it is a func keyword used to refers to
	// the current executing function
	
	return parse_function(parser, false, 0, 0);
}

static gnode_t *parse_identifier_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_identifier_expression");
	DECLARE_LEXER;
	
	const char	*identifier = parse_identifier(parser);
	if (!identifier) return NULL;
	DEBUG_PARSER("IDENTIFIER: %s", identifier);
	
	gtoken_s token = gravity_lexer_token(lexer);
	return gnode_identifier_expr_create(token, identifier, NULL);
}

static gnode_t *parse_identifier_or_keyword_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_identifier_expression");
	DECLARE_LEXER;
	
	// check if token is a keyword
	uint32_t idx_start, idx_end;
	token_keywords_indexes(&idx_start, &idx_end);
	
	gtoken_t peek = gravity_lexer_peek(lexer);
	if ((peek >= idx_start) && (peek <= idx_end)) {
		
		// consume token keyword
		gtoken_t keyword = gravity_lexer_next(lexer);
		gtoken_s token = gravity_lexer_token(lexer);
		
		// convert from keyword to identifier
		const char	*identifier = string_dup(token_name(keyword));
		return gnode_identifier_expr_create(token, identifier, NULL);
	}
	
	// default case
	return parse_identifier_expression(parser);
}

static gnode_t *parse_number_expression (gtoken_s token) {
	DEBUG_PARSER("parse_number_expression");
	
	// what I know here is that token is a well formed NUMBER
	// so I just need to properly decode it
	
	const char	*value = token.value;
	gliteral_t	type;
	int64_t		n = 0;
	double		d = 0;
	
	if (value[0] == '0') {
		int c = toupper(value[1]);
		if (c == 'B') {type = decode_number_binary(token, &n); goto report_node;}
		else if (c == 'O') {type = decode_number_octal(token, &n); goto report_node;}
		else if (c == 'X') {type = decode_number_hex(token, &n, &d); goto report_node;}
	}
	
	// number is decimal (check if is float)
	bool isfloat = false;
	for (uint32_t i=0; i<token.bytes; ++i) {
		if (value[i] == '.') {isfloat = true; break;}
	}
	
	STATIC_TOKEN_CSTRING(str, 512, len, buffer, token);
	if (isfloat) {
		d = strtod(str, NULL);
		type = LITERAL_FLOAT;
		DEBUG_PARSER("FLOAT: %.2f", d);
	}
	else {
		n = (int64_t) strtoll(str, NULL, 0);
		type = LITERAL_INT;
		DEBUG_PARSER("INT: %lld", n);
	}
	
report_node:
	if (type == LITERAL_FLOAT) return gnode_literal_float_expr_create(token, (double)d);
	else if (type == LITERAL_INT) return gnode_literal_int_expr_create(token, n);
	else assert(0);
	
	return NULL;
}

static gnode_t *parse_literal_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_literal_expression");
	DECLARE_LEXER;
	
	gtoken_t type = gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	
	if (type == TOK_STRING) {
		uint32_t len = 0;
		const char *value = token_string(token, &len);
		DEBUG_PARSER("STRING: %.*s", len, value);
		return gnode_literal_string_expr_create(token, value, len);
	}
	
	if (type == TOK_KEY_TRUE || type == TOK_KEY_FALSE) {
		return gnode_literal_bool_expr_create(token, (int32_t)(type == TOK_KEY_TRUE) ? 1 : 0);
	}
	
	if (type != TOK_NUMBER) {
		REPORT_ERROR(token, "Expected literal expression but found %s.", token_name(type));
		return NULL;
	}
	
	return parse_number_expression(token);
}

static gnode_t *parse_keyword_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_keyword_expression");
	DECLARE_LEXER;
	
	gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	
	return gnode_keyword_expr_create(token);
}

static gnode_r *parse_arguments_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_arguments_expression");
	DECLARE_LEXER;
	
	// it's OK for a call_expression_list to be empty
	if (gravity_lexer_peek(lexer) == TOK_OP_CLOSED_PARENTHESIS) return NULL;
	
	bool arg_expected = true;
	gnode_r *list = gnode_array_create();
	
	while (1) {
		gtoken_t peek = gravity_lexer_peek(lexer);
		
		if (peek == TOK_OP_COMMA) {
			// added the ability to convert ,, to ,undefined,
			gnode_array_push(list, gnode_keyword_expr_create(UNDEF_TOKEN));
			arg_expected = true;
			
			// consume next TOK_OP_COMMA and check for special ,) case
			gravity_lexer_next(lexer);
			if (gravity_lexer_peek(lexer) == TOK_OP_CLOSED_PARENTHESIS)
				gnode_array_push(list, gnode_keyword_expr_create(UNDEF_TOKEN));
			
		} else {
			// check exit condition
			if ((peek == TOK_EOF) || (peek == TOK_OP_CLOSED_PARENTHESIS))
				break;
			
			// I am going to parse and expression but is it allowed?
			if (!arg_expected) {
				REPORT_ERROR(gravity_lexer_token_next(lexer), "Missing , in function call.");
				return list;
			}
			
			// parse expression
			gnode_t *expr = parse_expression(parser);
			if (expr) gnode_array_push(list, expr);
			
			// consume next TOK_OP_COMMA and check for special ,) case
			peek = gravity_lexer_peek(lexer);
			if (peek == TOK_OP_COMMA) {
				gravity_lexer_next(lexer);
				if (gravity_lexer_peek(lexer) == TOK_OP_CLOSED_PARENTHESIS)
					gnode_array_push(list, gnode_keyword_expr_create(UNDEF_TOKEN));
			}
			
			// arg is expected only if a comma is consumed
			// this fixes syntax errors like System.print("Hello" " World")
			arg_expected = (peek == TOK_OP_COMMA);
		}
	}
	
	return list;
}

static gnode_t *parse_postfix_expression (gravity_parser_t *parser, gtoken_t tok) {
	DEBUG_PARSER("parse_postfix_expression");
	DECLARE_LEXER;
	
	// '[' assignment_expression ']' => Subscript operator
	// '(' expression_list? ')' => Function call operator
	// '.' IDENTIFIER => Member access operator
	
	// tok already consumed and used to identify postfix sub-expression
	gnode_t *lnode = parser->current_node;
	gtoken_s token = gravity_lexer_token(lexer);
	
	// a postfix expression is an expression followed by a list of other expressions (separated by specific tokens)
	gnode_r *list = gnode_array_create();
	while (1) {
		gnode_t *node = NULL;
		
		if (tok == TOK_OP_OPEN_SQUAREBRACKET) {
			gnode_t *expr = parse_expression(parser);
			gtoken_s subtoken = gravity_lexer_token(lexer);
			parse_required(parser, TOK_OP_CLOSED_SQUAREBRACKET);
			node = gnode_postfix_subexpr_create(subtoken, NODE_SUBSCRIPT_EXPR, expr, NULL);
		} else if (tok == TOK_OP_OPEN_PARENTHESIS) {
			gnode_r *args = parse_arguments_expression(parser);	// can be NULL and it's OK
			gtoken_s subtoken = gravity_lexer_token(lexer);
			parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
			node = gnode_postfix_subexpr_create(subtoken, NODE_CALL_EXPR, NULL, args);
		} else if (tok == TOK_OP_DOT) {
			// was parse_identifier_expression but we need to allow also keywords here in order
			// to be able to supports expressions like name.repeat (repeat is a keyword but in this
			// context it should be interpreted as an identifier)
			gnode_t *expr = parse_identifier_or_keyword_expression(parser);
			gtoken_s subtoken = gravity_lexer_token(lexer);
			node = gnode_postfix_subexpr_create(subtoken, NODE_ACCESS_EXPR, expr, NULL);
		} else {
			// should never reach this point
			assert(0);
		}
		
		// add subnode to list
		gnode_array_push(list, node);
		
		// check if postifx expression has more sub-nodes
		gtoken_t peek = gravity_lexer_peek(lexer);
		if ((peek != TOK_OP_OPEN_SQUAREBRACKET) && (peek != TOK_OP_OPEN_PARENTHESIS) && (peek != TOK_OP_DOT)) break;
		tok = gravity_lexer_next(lexer);
	}
	
	return gnode_postfix_expr_create(token, lnode, list);
}

static gnode_t *parse_postfix_subscript (gravity_parser_t *parser) {
	// NOTE:
	// Gravity does not support a syntax like m[1,2] for matrix access (not m[1,2,3])
	// but it supports a syntax like m[1][2] (or m[1][2][3])
	
	DEBUG_PARSER("parse_postfix_subscript");
	return parse_postfix_expression(parser, TOK_OP_OPEN_SQUAREBRACKET);
}

static gnode_t *parse_postfix_access (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_postfix_access");
	return parse_postfix_expression(parser, TOK_OP_DOT);
}

static gnode_t *parse_postfix_call (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_postfix_call");
	return parse_postfix_expression(parser, TOK_OP_OPEN_PARENTHESIS);
}

static gnode_t *parse_precedence(gravity_parser_t *parser, prec_level precedence) {
	DEBUG_PARSER("parse_precedence (level %d)", precedence);
	DECLARE_LEXER;
	
	// peek next
	gtoken_t type = gravity_lexer_peek(lexer);
	if (type == TOK_EOF) return NULL;
	
	parse_func prefix = rules[type].prefix;
	if (prefix == NULL) {
		// gravity_lexer_token reports the latest succesfully scanned token but since we need to report
		// an error for a peeked token then we force reporting "next" token
		REPORT_ERROR(gravity_lexer_token_next(lexer), "Expected expression but found %s.", token_name(type));
		return NULL;
	}
	gnode_t *node = prefix(parser);
	
	gtoken_t peek = gravity_lexer_peek(lexer);
	if (peek == TOK_EOF) return NULL;
	
	while (precedence < rules[peek].precedence) {
		gtoken_t tok = gravity_lexer_next(lexer);
		grammar_rule *rule = &rules[tok];
		
		parser->current_token = tok;
		parser->current_node = node;
		node = rule->infix(parser);
		
		peek = gravity_lexer_peek(lexer);
		if (peek == TOK_EOF) return NULL;
	}
	
	return node;
}

static gnode_t *parse_expression (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_expression");
	DECLARE_LEXER;
	
	// parse_expression is the default case called when no other case in parse_statament can be resolved
	// due to some syntax errors an infinte loop condition can be verified
	gtoken_s tok1 = gravity_lexer_token(lexer);
	
	gnode_t *expr = parse_precedence(parser, PREC_LOWEST);
	
	// expr is NULL means than an error condition has been encountered (and a potential infite loop)
	if (expr == NULL) {
		gtoken_s tok2 = gravity_lexer_token(lexer);
		// if current token is equal to the token saved before the recursion than skip token in order to avoid infinite loop
		if (token_identical(&tok1, &tok2)) gravity_lexer_next(lexer);
	}
	
	return expr;
}

static gnode_t *parse_unary (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_unary");
	DECLARE_LEXER;
	
	gtoken_t tok = gravity_lexer_next(lexer);
	gnode_t *node = parse_precedence(parser, PREC_UNARY);
	return gnode_unary_expr_create(tok, node);
}

static gnode_t *parse_infix (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_infix");
	
	gtoken_t tok = parser->current_token;
	gnode_t *lnode = parser->current_node;
	
	// we can make right associative operators by reducing the right binding power
	grammar_rule *rule = &rules[tok];
	prec_level precedence = (rule->right) ? rule->precedence-1 : rule->precedence;
	
	gnode_t *rnode = parse_precedence(parser, precedence);
	if ((tok != TOK_OP_ASSIGN) && token_isassignment(tok)) return adjust_assignment_expression(tok, lnode, rnode);
	return gnode_binary_expr_create(tok, lnode, rnode);
}

// MARK: -

static gnode_t *adjust_assignment_expression (gtoken_t tok, gnode_t *lnode, gnode_t *rnode) {
	DEBUG_PARSER("adjust_assignment_expression");
	
	// called when tok is an assignment != TOK_OP_ASSIGN
	// convert expressions:
	// a += 1	=> a = a + 1
	// a -= 1	=> a = a - 1
	// a *= 1	=> a = a * 1
	// a /= 1	=> a = a / 1
	// a %= 1	=> a = a % 1
	// a <<=1	=> a = a << 1
	// a >>=1	=> a = a >> 1
	// a &= 1	=> a = a & 1
	// a |= 1	=> a = a | 1
	// a ^= 1	=> a = a ^ 1
	
	gtoken_t t;
	switch (tok) {
		case TOK_OP_MUL_ASSIGN: t = TOK_OP_MUL; break;
		case TOK_OP_DIV_ASSIGN: t = TOK_OP_DIV; break;
		case TOK_OP_REM_ASSIGN: t = TOK_OP_REM; break;
		case TOK_OP_ADD_ASSIGN: t = TOK_OP_ADD; break;
		case TOK_OP_SUB_ASSIGN: t = TOK_OP_SUB; break;
		case TOK_OP_SHIFT_LEFT_ASSIGN: t = TOK_OP_SHIFT_LEFT; break;
		case TOK_OP_SHIFT_RIGHT_ASSIGN: t = TOK_OP_SHIFT_RIGHT; break;
		case TOK_OP_BIT_AND_ASSIGN: t = TOK_OP_BIT_AND; break;
		case TOK_OP_BIT_OR_ASSIGN: t = TOK_OP_BIT_OR; break;
		case TOK_OP_BIT_XOR_ASSIGN: t = TOK_OP_BIT_XOR; break;
		
		// should never reach this point
		default: assert(0); break;
	}
	
	// duplicate node is mandatory here, otherwise the deallocator will try to free memory occopied by the same node twice
	rnode = gnode_binary_expr_create(t, gnode_duplicate(lnode, true), rnode);
	tok = TOK_OP_ASSIGN;
	
	// its an assignment expression so switch the order
	return gnode_binary_expr_create(tok, lnode, rnode);
}

static void init_grammer_rules (void) {
	static bool created = false;
	if (created) return;
	created = true;
	
	// rules is a static variable initialized to 0
	// so we automatically have all members initialized to UNUSED
	
	rules[TOK_OP_OPEN_PARENTHESIS] = RULE(PREC_CALL, parse_parentheses_expression, parse_postfix_call);
	rules[TOK_OP_OPEN_SQUAREBRACKET] = RULE(PREC_CALL, parse_list_expression, parse_postfix_subscript);
	rules[TOK_OP_DOT] = RULE(PREC_CALL, parse_literal_expression, parse_postfix_access);
	
	rules[TOK_OP_OPEN_CURLYBRACE] = PREFIX(PREC_LOWEST, parse_function_expression);
	rules[TOK_KEY_FUNC] = PREFIX(PREC_LOWEST, parse_function_expression);
	
	rules[TOK_IDENTIFIER] = PREFIX(PREC_LOWEST, parse_identifier_expression);
	rules[TOK_STRING] = PREFIX(PREC_LOWEST, parse_literal_expression);
	rules[TOK_NUMBER] = PREFIX(PREC_LOWEST, parse_literal_expression);
	
	rules[TOK_KEY_UNDEFINED] = PREFIX(PREC_LOWEST, parse_keyword_expression);
	rules[TOK_KEY_CURRARGS] = PREFIX(PREC_LOWEST, parse_keyword_expression);
	rules[TOK_KEY_CURRFUNC] = PREFIX(PREC_LOWEST, parse_keyword_expression);
	rules[TOK_KEY_SUPER] = PREFIX(PREC_LOWEST, parse_keyword_expression);
	rules[TOK_KEY_FILE] = PREFIX(PREC_LOWEST, parse_file_expression);
	rules[TOK_KEY_NULL] = PREFIX(PREC_LOWEST, parse_keyword_expression);
	rules[TOK_KEY_TRUE] = PREFIX(PREC_LOWEST, parse_keyword_expression);
	rules[TOK_KEY_FALSE] = PREFIX(PREC_LOWEST, parse_keyword_expression);
	
	rules[TOK_OP_SHIFT_LEFT] = INFIX_OPERATOR(PREC_SHIFT, "<<");
	rules[TOK_OP_SHIFT_RIGHT] = INFIX_OPERATOR(PREC_SHIFT, ">>");
	
	rules[TOK_OP_MUL] = INFIX_OPERATOR(PREC_FACTOR, "*");
	rules[TOK_OP_DIV] = INFIX_OPERATOR(PREC_FACTOR, "/");
	rules[TOK_OP_REM] = INFIX_OPERATOR(PREC_FACTOR, "%");
	rules[TOK_OP_BIT_AND] = INFIX_OPERATOR(PREC_FACTOR, "&");
	rules[TOK_OP_ADD] = OPERATOR(PREC_TERM, "+");
	rules[TOK_OP_SUB] = OPERATOR(PREC_TERM, "-");
	rules[TOK_OP_BIT_OR] = INFIX_OPERATOR(PREC_TERM, "|");
	rules[TOK_OP_BIT_XOR] = INFIX_OPERATOR(PREC_TERM, "^");
	rules[TOK_OP_BIT_NOT] = PREFIX_OPERATOR("~");
	
	rules[TOK_OP_RANGE_EXCLUDED] = INFIX_OPERATOR(PREC_RANGE, "..<");
	rules[TOK_OP_RANGE_INCLUDED] = INFIX_OPERATOR(PREC_RANGE, "...");
	
	rules[TOK_KEY_ISA] = INFIX_OPERATOR(PREC_ISA, "is");
	rules[TOK_OP_LESS] = INFIX_OPERATOR(PREC_COMPARISON, "<");
	rules[TOK_OP_LESS_EQUAL] = INFIX_OPERATOR(PREC_COMPARISON, "<=");
	rules[TOK_OP_GREATER] = INFIX_OPERATOR(PREC_COMPARISON, ">");
	rules[TOK_OP_GREATER_EQUAL] = INFIX_OPERATOR(PREC_COMPARISON, ">=");
	rules[TOK_OP_ISEQUAL] = INFIX_OPERATOR(PREC_COMPARISON, "==");
	rules[TOK_OP_ISNOTEQUAL] = INFIX_OPERATOR(PREC_COMPARISON, "!=");
	rules[TOK_OP_ISIDENTICAL] = INFIX_OPERATOR(PREC_COMPARISON, "===");
	rules[TOK_OP_ISNOTIDENTICAL] = INFIX_OPERATOR(PREC_COMPARISON, "!==");
	rules[TOK_OP_PATTERN_MATCH] = INFIX_OPERATOR(PREC_COMPARISON, "~=");
	
	rules[TOK_OP_AND] = INFIX_OPERATOR_RIGHT(PREC_LOGICAL_AND, "&&");
	rules[TOK_OP_OR] = INFIX_OPERATOR_RIGHT(PREC_LOGICAL_OR, "||");
	rules[TOK_OP_TERNARY] = INFIX(PREC_TERNARY, parse_ternary_expression);
	
	rules[TOK_OP_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "=");
	rules[TOK_OP_MUL_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "*=");
	rules[TOK_OP_DIV_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "/=");
	rules[TOK_OP_REM_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "%=");
	rules[TOK_OP_ADD_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "+=");
	rules[TOK_OP_SUB_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "-=");
	rules[TOK_OP_SHIFT_LEFT_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "<<=");
	rules[TOK_OP_SHIFT_RIGHT_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, ">>=");
	rules[TOK_OP_BIT_AND_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "=&");
	rules[TOK_OP_BIT_OR_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "|=");
	rules[TOK_OP_BIT_XOR_ASSIGN] = INFIX_OPERATOR(PREC_ASSIGN, "^=");
	
	rules[TOK_OP_NOT] = PREFIX_OPERATOR("!");
}

// MARK: - Declarations -

static gnode_t *parse_getter_setter (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_getter_setter");
	DECLARE_LEXER;
	
	gnode_t *getter = NULL;
	gnode_t *setter = NULL;
	gtoken_s token_block = gravity_lexer_token(lexer);
	
	while (gravity_lexer_peek(lexer) != TOK_OP_CLOSED_CURLYBRACE) {
		const char *identifier = parse_identifier(parser);
		if (!identifier) goto parse_error;
		
		bool		is_getter = false;
		gtoken_s	token = gravity_lexer_token(lexer);
		gnode_r		*params = NULL;
		
		// getter case: does not have explicit parameters (only implicit self)
		if (strcmp(identifier, GETTER_FUNCTION_NAME) == 0) {
			is_getter = true;
			params = gnode_array_create();	// add implicit SELF param
			gnode_array_push(params, gnode_variable_create(NO_TOKEN, string_dup(SELF_PARAMETER_NAME), NULL, 0, NULL));
		}
		
		// setter case: could have explicit parameters (otherwise value is implicit)
		if (strcmp(identifier, SETTER_FUNCTION_NAME) == 0) {
			is_getter = false;
			// check if parameters are explicit
			if (gravity_lexer_peek(lexer) == TOK_OP_OPEN_PARENTHESIS) {
				parse_required(parser, TOK_OP_OPEN_PARENTHESIS);
				params = parse_optional_parameter_declaration(parser);	// add implicit SELF
				parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
			} else {
				params = gnode_array_create();	// add implicit SELF and VALUE params
				gnode_array_push(params, gnode_variable_create(NO_TOKEN, string_dup(SELF_PARAMETER_NAME), NULL, 0, NULL));
				gnode_array_push(params, gnode_variable_create(NO_TOKEN, string_dup(SETTER_PARAMETER_NAME), NULL, 0, NULL));
			}
		}
		mem_free(identifier);
		
		// parse compound statement
		PUSH_FUNCTION_DECLARATION();
		gnode_compound_stmt_t *compound = (gnode_compound_stmt_t*)parse_compound_statement(parser);
		POP_DECLARATION();
		gnode_t *f = gnode_function_decl_create(token, NULL, 0, 0, params, compound);
		
		// assign f to the right function
		if (is_getter) getter = f; else setter = f;
	}
	
	gnode_r *functions = gnode_array_create();
	gnode_array_push(functions, (getter) ? getter : NULL);	// getter is at index 0
	gnode_array_push(functions, (setter) ? setter : NULL);	// setter is at index 1
	
	// a compound node is used to capture getter and setter
	return gnode_block_stat_create(NODE_COMPOUND_STAT, token_block, functions);
	
parse_error:
	return NULL;
}

static gnode_t *parse_variable_declaration (gravity_parser_t *parser, bool isstatement, gtoken_t access_specifier, gtoken_t storage_specifier) {
	DEBUG_PARSER("parse_variable_declaration");
	DECLARE_LEXER;
	
	gnode_r		*decls = NULL;
	gnode_t		*decl = NULL;
	gnode_t		*expr = NULL;
	const char	*identifier = NULL;
	const char	*type_annotation = NULL;
	gtoken_t	type;
	gtoken_t	peek;
	gtoken_s	token, token2;
	
	// access_specifier? storage_specifier? variable_declaration ';'
	// variable_declaration: variable_declarator decl_item
	// variable_declarator: 'const' | 'var'
	// decl_item: (IDENTIFIER assignment?) (',' IDENTIFIER assignment?)*
	// assignment
	
	// sanity check on variable type
	type = gravity_lexer_next(lexer);
	if (!token_isvariable_declaration(type)) {
		REPORT_ERROR(gravity_lexer_token(lexer), "VAR or CONST expected here but found %s.", token_name(type));
		return NULL;
	}
	token = gravity_lexer_token(lexer);
	
	// initialize node array
	decls = gnode_array_create();
	
loop:
	identifier = parse_identifier(parser);
	if (!identifier) return NULL;
	token2 = gravity_lexer_token(lexer);
	
	// type annotation is optional so it can be NULL
	type_annotation = parse_optional_type_annotation(parser);
	DEBUG_PARSER("IDENTIFIER: %s %s", identifier, (type_annotation) ? type_annotation : "");
	
	// check for optional assignment or getter/setter declaration (ONLY = is ALLOWED here!)
	expr = NULL;
	peek = gravity_lexer_peek(lexer);
	if (token_isvariable_assignment(peek)) {
		gravity_lexer_next(lexer); // consume ASSIGNMENT
		expr = parse_expression(parser);
	} else if (peek == TOK_OP_OPEN_CURLYBRACE) {
		gravity_lexer_next(lexer); // consume TOK_OP_OPEN_CURLYBRACE
		expr = parse_getter_setter(parser);
		parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);
	}
	
	// sanity checks
	// 1. CONST must be followed by an assignment expression ?
	// 2. check if identifier is unique inside variable declarations
	
	decl = gnode_variable_create(token2, identifier, type_annotation, access_specifier, expr);
	if (decl) gnode_array_push(decls, decl);
	
	peek = gravity_lexer_peek(lexer);
	if (peek == TOK_OP_COMMA) {
		gravity_lexer_next(lexer); // consume TOK_OP_COMMA
		goto loop;
	}
	
	// check and consume TOK_OP_SEMICOLON (ALWAYS required for assignments)
	// Aaron: I would keep it consistent, even if it's not strictly required.
	// Otherwise we end up with all of JavaScript's terrible ideas. ;-)
	// So I would require the semicolon at the end of any assignment statement.
	if (isstatement) parse_semicolon(parser);
	
	return gnode_variable_decl_create(token, type, access_specifier, storage_specifier, decls);
}

static gnode_t *parse_enum_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
	DEBUG_PARSER("parse_enum_declaration");
	DECLARE_LEXER;
	
	// enum is a bit different than the traditional C like enum statements
	// in Gravity enum can contains String, Integer, Boolean and Float BUT cannot be mixed
	// Integer case can also skip values and autoincrement will be applied
	// String and Float must have a default value
	// in any case default value must be unique (as identifiers)
	// this code will take care of parsing and syntax check for the above restrictions
	
	// in order to simplify node struct all the sematic checks are performed here
	// even if it is not a best practice, I find a lot easier to perform all the checks
	// directly into the parser
	// parse_enum_declaration is the only reason why gravity_symboltable.h is included here
	
	// checks are:
	// 1: unique internal identifiers
	// 2: unique internal values
	// 3: if not INT then a default value is mandatory
	// 4: all values must be literals
	
	// 'enum' IDENTIFIER '{' enum_list '}' ';'
	// enum_list: enum_list_item (',' enum_list_item)*
	// enum_list_item: IDENTIFIER ('=' LITERAL)?
	
	// NODE_ENUM_DECL
	
	// optional scope already consumed
	gtoken_t type = gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	assert(type == TOK_KEY_ENUM);
	
	// parse IDENTIFIER
	const char *identifier = parse_identifier(parser);
	DEBUG_PARSER("parse_enum_declaration %s", identifier);
	
	// check and consume TOK_OP_OPEN_CURLYBRACE
	parse_required(parser, TOK_OP_OPEN_CURLYBRACE);
	
	symboltable_t	*symtable = symboltable_create(true);	// enum symbol table (symtable is OK because order is not important inside an enum)
	int64_t			enum_autoint = 0;						// autoincrement value (in case of INT enum)
	uint32_t		enum_counter = 0;						// enum internal counter (first value (if any) determines enum type)
	gliteral_t		enum_type = LITERAL_INT;				// enum type (default to int)
	
	while (1) {
		// check for empty enum
		if (gravity_lexer_peek(lexer) == TOK_OP_CLOSED_CURLYBRACE) break;
		
		// identifier is mandatory here
		const char *enum_id = NULL;
		gtoken_t peek = gravity_lexer_peek(lexer);
		gtoken_s enumid_token = NO_TOKEN;
		if (peek == TOK_IDENTIFIER) {
			enum_id = parse_identifier(parser);
			enumid_token = gravity_lexer_token(lexer);
		}
		if (!enum_id) {
			REPORT_ERROR(enumid_token, "Identifier expected here (found %s).", token_name(peek));
		}
		
		// peek next that can be only = or , or }
		peek = gravity_lexer_peek(lexer);
		gtoken_s enum_token = gravity_lexer_token(lexer);
		if (!token_isvariable_assignment(peek) && (peek != TOK_OP_COMMA) && (peek != TOK_OP_CLOSED_CURLYBRACE)) {
			REPORT_ERROR(enum_token, "Token %s not allowed here.", token_name(peek));
		}
		
		// check for assignment (ONLY = is ALLOWED here!)
		// assignment is optional ONLY for LITERAL_TYPE_INT case
		if ((!token_isvariable_assignment(peek)) && (enum_type != LITERAL_INT)) {
			REPORT_ERROR(enum_token, "A default value is expected here (found %s).", token_name(peek));
		}
		
		// check for optional default value (optional only in LITERAL_INT case)
		gnode_base_t *enum_value = NULL;
		if (token_isvariable_assignment(peek)) {
			gravity_lexer_next(lexer); // consume ASSIGNMENT
			enum_value = (gnode_base_t *)parse_expression(parser);
		}
		
		if (enum_value) {
			// make sure that value is a literal (or a unary expression like +num or -num)
			gnode_literal_expr_t *enum_literal = NULL;
			if (enum_value->base.tag == NODE_LITERAL_EXPR) {
				enum_literal = (gnode_literal_expr_t *)enum_value;
			} else if (enum_value->base.tag == NODE_UNARY_EXPR) {
				gnode_unary_expr_t *unary = (gnode_unary_expr_t *)enum_value;
				gnode_base_t *expr = (gnode_base_t *)unary->expr;
				
				// sanity check on unary expression
				if (expr->base.tag != NODE_LITERAL_EXPR) {
					REPORT_ERROR(enum_token, "%s", "Literal value expected here.");
					continue;
				}
				
				if ((unary->op != TOK_OP_SUB) && (unary->op != TOK_OP_ADD)) {
					REPORT_ERROR(enum_token, "%s", "Only + or - allowed in enum value definition.");
					continue;
				}
				
				enum_literal = (gnode_literal_expr_t *)expr;
				if ((enum_literal->type != LITERAL_FLOAT) && (enum_literal->type != LITERAL_INT)) {
					REPORT_ERROR(enum_token, "%s", "A number is expected after a + or - unary expression in an enum definition.");
					continue;
				}
				
				if (unary->op == TOK_OP_SUB) {
					if (enum_literal->type == LITERAL_FLOAT) enum_literal->value.d = -enum_literal->value.d;
					else if (enum_literal->type == LITERAL_INT) enum_literal->value.n64 = -enum_literal->value.n64;
					else assert(0); // should never reach this point
				}
				
			} else {
				REPORT_ERROR(enum_token, "%s", "Literal value expected here.");
				continue;
			}
			
			// first assignment (if any) determines enum type, otherwise default INT case is assumed
			if (enum_counter == 0) {
				if (enum_literal->type == LITERAL_STRING) enum_type = LITERAL_STRING;
				else if (enum_literal->type == LITERAL_FLOAT) enum_type = LITERAL_FLOAT;
				else if (enum_literal->type == LITERAL_BOOL) enum_type = LITERAL_BOOL;
			}
			
			// check if literal value conforms to enum type
			if (enum_literal->type != enum_type) {
				REPORT_ERROR(enum_token, "%s", "Literal value of type %s expected here.", token_literal_name(enum_literal->type));
			}
			
			// update enum_autoint value to next value
			if (enum_literal->type == LITERAL_INT) {
				enum_autoint = enum_literal->value.n64 + 1;
			}
			
		} else {
			enum_value = (gnode_base_t *)gnode_literal_int_expr_create(NO_TOKEN, enum_autoint);
			++enum_autoint;
		}
		
		// update internal enum counter
		++enum_counter;
		
		// enum identifier could be NULL due to an already reported error
		if (enum_id) {
			if (!symboltable_insert(symtable, enum_id, (void *)enum_value)) {
				REPORT_ERROR(enumid_token, "Identifier %s redeclared.", enum_id);
				gnode_free((gnode_t *)enum_value); // free value here because it has not beed saved into symbol table
			}
			mem_free(enum_id); // because key is duplicated inside internal hash table
		}
		
		peek = gravity_lexer_peek(lexer);
		if (peek != TOK_OP_COMMA) break;
		
		// consume TOK_OP_COMMA and continue loop
		gravity_lexer_next(lexer);
	}
	
	// check and consume TOK_OP_CLOSED_CURLYBRACE
	parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);
	
	// consume semicolon
	parse_semicolon(parser);
	
	// check for empty enum (not allowed)
	if (enum_counter == 0) {
		REPORT_ERROR(token, "Empty enum %s not allowed.", identifier);
	}
	
	gnode_t *node = gnode_enum_decl_create(token, identifier, access_specifier, storage_specifier, symtable);
	if (IS_FUNCTION_ENCLOSED()) return local_store_declaration(identifier, access_specifier, storage_specifier, node);
	return node;
}

static gnode_t *parse_module_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
	DEBUG_PARSER("parse_module_declaration");
	
	// 'module' IDENTIFIER '{' declaration_statement* '}' ';'
	
	// optional scope already consumed
	DECLARE_LEXER;
	gtoken_t type = gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	assert(type == TOK_KEY_MODULE);
	
	// parse IDENTIFIER
	const char *identifier = parse_identifier(parser);
	DEBUG_PARSER("parse_module_declaration %s", identifier);
	
	// parse optional curly brace
	bool curly_brace = parse_optional(parser, TOK_OP_OPEN_CURLYBRACE);
	
	gnode_r *declarations = gnode_array_create();
	while (token_isdeclaration_statement(gravity_lexer_peek(lexer))) {
		gnode_t *decl = parse_declaration_statement(parser);
		if (decl) gnode_array_push(declarations, decl);
	}
	
	// check and consume TOK_OP_CLOSED_CURLYBRACE
	if (curly_brace) parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);
	
	parse_semicolon(parser);
	
	return gnode_module_decl_create(token, identifier, access_specifier, storage_specifier, declarations);
}

static gnode_t *parse_event_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
	#pragma unused(parser, access_specifier, storage_specifier)
	
	// 'event' IDENTIFIER '(' parameter_declaration_clause? ')' ';'
	
	// NODE_EVENT_DECL
	assert(0);
	return NULL;
}

static gnode_t *parse_function_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
	gnode_t *node = parse_function(parser, true, access_specifier, storage_specifier);
	
	// convert a function declaration within another function to a local variable assignment
	// for example:
	//
	// func foo() {
	//	func bar() {...}
	// }
	//
	// is converter to:
	//
	// func foo() {
	//	var bar = func() {...}
	// }
	//
	// conversion is performed inside the parser
	// so next semantic checks can perform
	// identifier uniqueness checks
	
	if (IS_FUNCTION_ENCLOSED()) return local_store_declaration(((gnode_function_decl_t *)node)->identifier, access_specifier, storage_specifier, node);
	return node;
}

static gnode_t *parse_id (gravity_parser_t *parser) {
	DECLARE_LEXER;
	const char	*identifier1 = NULL;
	const char	*identifier2 = NULL;
	gtoken_t	peek;
	gtoken_s	token;
	
	// IDENTIFIER |	(IDENTIFIER)('.' IDENTIFIER)
	
	DEBUG_PARSER("parse_id");
	
	identifier1 = parse_identifier(parser);
	
	token = gravity_lexer_token(lexer);
	peek = gravity_lexer_peek(lexer);
	if (peek == TOK_OP_DOT) {
		gravity_lexer_next(lexer); // consume TOK_OP_DOT
		identifier2 = parse_identifier(parser);
	}
	
	DEBUG_PARSER("ID: %s %s", identifier1, (identifier2) ? identifier2 : "");
	return gnode_identifier_expr_create(token, identifier1, identifier2);
}

static gnode_r *parse_protocols (gravity_parser_t *parser) {
	DECLARE_LEXER;
	gtoken_t peek;
	gnode_t	 *node = NULL;
	gnode_r	 *list = NULL;
	
	// (id) (',' id)*
	
	peek = gravity_lexer_peek(lexer);
	if (peek == TOK_OP_GREATER) return NULL; // just an empty protocols implementation statement
	
	list = gnode_array_create();
	
loop:
	if (!token_isidentifier(peek)) goto abort;
	node = parse_id(parser);
	if (node) gnode_array_push(list, node);
	
	peek = gravity_lexer_peek(lexer);
	if (peek == TOK_OP_COMMA) {
		gravity_lexer_next(lexer); // consume TOK_OP_COMMA
		goto loop;
	}
	
	return list;
	
abort:
	if (list) gnode_array_free(list);
	return NULL;
}

static gnode_t *parse_class_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
	DEBUG_PARSER("parse_class_declaration");
	DECLARE_LEXER;
	
	// access_specifier? storage_specifier? 'class' IDENTIFIER class_superclass? class_protocols? '{' declaration_statement* '}' ';'
	// class_superclass: (':') id
	// class_protocols: '<' (id) (',' id)* '>'
	
	// optional scope already consumed (when here I am sure type is TOK_KEY_CLASS or TOK_KEY_STRUCT)
	gtoken_t type = gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	bool is_struct = (type == TOK_KEY_STRUCT);
	
	// parse IDENTIFIER
	const char *identifier = parse_identifier(parser);
	
	// check for optional superclass
	gnode_t *super = NULL;
	gnode_r *protocols = NULL;
	gtoken_t peek = gravity_lexer_peek(lexer);
	if (peek == TOK_OP_COLON) {
		gravity_lexer_next(lexer); // consume TOK_OP_COLON
		super = parse_id(parser);
	}
	
	// check for optional protocols (not supported in this version)
	peek = gravity_lexer_peek(lexer);
	if (peek == TOK_OP_LESS) {
		gravity_lexer_next(lexer); // consume '<'
		protocols = parse_protocols(parser);
		parse_required(parser, TOK_OP_GREATER);  // consume '>'
	}
	
	// check and consume TOK_OP_OPEN_CURLYBRACE
	parse_required(parser, TOK_OP_OPEN_CURLYBRACE);
	gnode_r *declarations = gnode_array_create();
	
	// if class is declared inside another class then a hidden implicit privare "outer" instance var at index 0
	// is automatically added
	if (IS_CLASS_ENCLOSED()) {
		gnode_r *decls = gnode_array_create();
		gnode_t *outer_var = gnode_variable_create(NO_TOKEN, string_dup(OUTER_IVAR_NAME), NULL, 0, NULL);
		gnode_array_push(decls, outer_var);
		
		gnode_t *outer_decl = gnode_variable_decl_create(NO_TOKEN, TOK_KEY_VAR, TOK_KEY_PRIVATE, 0, decls);
		gnode_array_push(declarations, outer_decl);
	}
	
	PUSH_CLASS_DECLARATION();
	while (token_isdeclaration_statement(gravity_lexer_peek(lexer))) {
		gnode_t *decl = parse_declaration_statement(parser);
		if (decl) gnode_array_push(declarations, decl);
	}
	POP_DECLARATION();
	
	// check and consume TOK_OP_CLOSED_CURLYBRACE
	parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);
	
	// to check
	parse_semicolon(parser);
	
	gnode_t *node = gnode_class_decl_create(token, identifier, access_specifier, storage_specifier, super, protocols, declarations, is_struct);
	if (IS_FUNCTION_ENCLOSED()) return local_store_declaration(identifier, access_specifier, storage_specifier, node);
	return node;
}

static gnode_r *parse_optional_parameter_declaration (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_parameter_declaration");
	DECLARE_LEXER;
	
	gtoken_s	token = NO_TOKEN;
	gnode_t		*node = NULL;
	const char	*identifier = NULL;
	const char	*type_annotation = NULL;
	
	// (IDENTIFIER type_annotation?) (',' type_annotation)*
	// type_annotation: ':' identifier
	
	gnode_r *params = gnode_array_create();
	assert(params);
	
	// check if implicit self parameter must be added
	// was if (IS_CLASS_ENCLOSED()*/) { ... add SELF PARAMETER ...}
	// but we decided to ALWAYS pass SELF because it simplified cases
	// like c1().p1.p1.p1(1234);
	
	// ALWAYS add an implicit SELF parameter
	// string_dup mandatory here because when the node will be freed
	// memory for the identifier will be deallocated
	node = gnode_variable_create(token, string_dup(SELF_PARAMETER_NAME), type_annotation, 0, NULL);
	if (node) gnode_array_push(params, node);
		
	// parameter declaration clause is ALWAYS optional
	gtoken_t peek = gravity_lexer_peek(lexer);
	if (peek == TOK_OP_CLOSED_PARENTHESIS) return params;
	
	// so there is at leat one explicit parameter
loop:
	// initialize variables
	type_annotation = NULL;
	
	// parse identifier
	identifier = parse_identifier(parser);
	token = gravity_lexer_token(lexer);
	
	// parse optional type annotation
	type_annotation = parse_optional_type_annotation(parser);
		
	// fill parameters array with the new node
	node = gnode_variable_create(token, identifier, type_annotation, 0, NULL);
	if (node) gnode_array_push(params, node);
	
	// check for optional comma in order to decide
	// if the loop should continue or not
	peek = gravity_lexer_peek(lexer);
	if (peek == TOK_OP_COMMA) {
		gravity_lexer_next(lexer); // consume TOK_OP_COMMA
		goto loop;
	}
	
	return params;
}

// MARK: - Macro -

typedef enum {
	UNITTEST_NONE,
	UNITTEST_NAME,
	UNITTEST_ERROR,
	UNITTEST_RESULT,
	UNITTEST_ERROR_ROW,
	UNITTEST_ERROR_COL,
	UNITTEST_NOTE
} unittest_t;

static unittest_t parse_unittest_identifier (const char *identifier) {
	if (string_cmp(identifier, "name") == 0) return UNITTEST_NAME;
	if (string_cmp(identifier, "note") == 0) return UNITTEST_NOTE;
	if (string_cmp(identifier, "error") == 0) return UNITTEST_ERROR;
	if (string_cmp(identifier, "error_row") == 0) return UNITTEST_ERROR_ROW;
	if (string_cmp(identifier, "error_col") == 0) return UNITTEST_ERROR_COL;
	if (string_cmp(identifier, "result") == 0) return UNITTEST_RESULT;
	
	return UNITTEST_NONE;
}

static gnode_t *parse_unittest_macro (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_unittest_macro");
	DECLARE_LEXER;
	
	// @unittest {
	//		name: "Unit test name";
	//		note: "Some notes here";
	//		error: NONE, SYNTAX, RUNTIME, WARNING;
	//		error_row: number;
	//		error_col: number;
	//		result: LITERAL;
	// '}' ';'?
	
	gnode_literal_expr_t	*name_node = NULL;
	gnode_literal_expr_t	*note_node = NULL;
	gnode_identifier_expr_t	*err_node = NULL;
	gnode_literal_expr_t	*row_node = NULL;
	gnode_literal_expr_t	*col_node = NULL;
	gnode_literal_expr_t	*value_node = NULL;
	
	parse_required(parser, TOK_OP_OPEN_CURLYBRACE);
	while (gravity_lexer_peek(lexer) != TOK_OP_CLOSED_CURLYBRACE) {
		const char	*id = parse_identifier(parser);
		if (id == NULL) goto handle_error;
		parse_required(parser, TOK_OP_COLON);
		
		unittest_t type = parse_unittest_identifier(id);
		mem_free(id);
		
		if (type == UNITTEST_NAME) {
			name_node = (gnode_literal_expr_t *)parse_literal_expression(parser);
			if (name_node == NULL) goto handle_error;
		}
		else if (type == UNITTEST_NOTE) {
			note_node = (gnode_literal_expr_t *)parse_literal_expression(parser);
			if (note_node == NULL) goto handle_error;
		}
		else if (type == UNITTEST_ERROR) {
			err_node = (gnode_identifier_expr_t *)parse_identifier_expression(parser);
			if (err_node == NULL) goto handle_error;
		}
		else if (type == UNITTEST_ERROR_ROW) {
			row_node = (gnode_literal_expr_t *)parse_literal_expression(parser);
			if (row_node == NULL) goto handle_error;
		}
		else if (type == UNITTEST_ERROR_COL) {
			col_node = (gnode_literal_expr_t *)parse_literal_expression(parser);
			if (col_node == NULL) goto handle_error;
		}
		else if (type == UNITTEST_RESULT) {
			gtoken_t op = TOK_EOF;
			gtoken_t peek = gravity_lexer_peek(lexer);
			
			// check if peek is a + or - sign
			if ((peek == TOK_OP_SUB) || (peek == TOK_OP_ADD))
				op = gravity_lexer_next(lexer);
			else if (peek == TOK_KEY_NULL) {
				// an expected return value can now be keyword NULL
				gravity_lexer_next(lexer); // consume NULL keyword
				value_node = NULL;
				goto handle_continue;
			}
			
			value_node = (gnode_literal_expr_t *)parse_literal_expression(parser);
			if (value_node == NULL) goto handle_error;
			
			// if a negative sign has been parsed then manually fix the literal expression (if it is a number)
			if (op == TOK_OP_SUB) {
				if (value_node->type == LITERAL_INT) value_node->value.n64 = -value_node->value.n64;
				else if (value_node->type == LITERAL_FLOAT) value_node->value.d = -value_node->value.d;
			}
		}
		else {
			REPORT_ERROR(gravity_lexer_token(lexer), "Unknown token found in @unittest declaration.");
			goto handle_error;
		}
		
	handle_continue:
		parse_semicolon(parser);
	}
	
	parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);
	parse_semicolon(parser);
	
	// decode unit array and report error/unittest
	// unit test name max length is 1024
	const char			*description = NULL;
	const char			*note = NULL;
	char				buffer[1024];
	char				buffer2[1024];
	error_type_t		expected_error = GRAVITY_ERROR_NONE;
	gravity_value_t		expected_value = VALUE_FROM_NULL;
	int32_t				expected_nrow = -1;
	int32_t				expected_ncol = -1;
	
	// unittest name should be a literal string
	if ((name_node) && (name_node->type == LITERAL_STRING)) {
		// no more C strings in AST so we need a static buffer
		snprintf(buffer, sizeof(buffer), "%.*s", name_node->len, name_node->value.str);
		description = buffer;
	}
	
	// note (optional) should be a literal string
	if ((note_node) && (note_node->type == LITERAL_STRING)) {
		// no more C strings in AST so we need a static buffer
		snprintf(buffer2, sizeof(buffer2), "%.*s", note_node->len, note_node->value.str);
		note = buffer2;
	}
	
	// decode expected error: NONE, SYNTAX, SEMANTIC, RUNTIME, WARNING
	if (err_node) {
		if (string_cmp(err_node->value, "NONE") == 0)
			expected_error = GRAVITY_ERROR_NONE;
		else if (string_cmp(err_node->value, "SYNTAX") == 0)
			expected_error = GRAVITY_ERROR_SYNTAX;
		else if (string_cmp(err_node->value, "SEMANTIC") == 0)
			expected_error = GRAVITY_ERROR_SEMANTIC;
		else if (string_cmp(err_node->value, "RUNTIME") == 0)
			expected_error = GRAVITY_ERROR_RUNTIME;
		else if (string_cmp(err_node->value, "WARNING") == 0)
			expected_error = GRAVITY_WARNING;
	}
	
	// decode error line/col
	if ((row_node) && (row_node->type == LITERAL_INT)) {
		expected_nrow = (int32_t)row_node->value.n64;
	}
	if ((col_node) && (col_node->type == LITERAL_INT)) {
		expected_ncol = (int32_t)col_node->value.n64;
	}
	
	// decode unittest expected result
	if (value_node) {
		if (value_node->type == LITERAL_STRING)
			expected_value = VALUE_FROM_CSTRING(NULL, value_node->value.str);
		else if (value_node->type == LITERAL_INT)
			expected_value = VALUE_FROM_INT((gravity_int_t)value_node->value.n64);
		else if (value_node->type == LITERAL_FLOAT)
			expected_value = VALUE_FROM_FLOAT((gravity_float_t)value_node->value.d);
		else if (value_node->type == LITERAL_BOOL)
			expected_value = (value_node->value.n64) ? VALUE_FROM_TRUE : VALUE_FROM_FALSE;
	}
	
	// report unittest to delegate
	if ((parser->delegate) && (parser->delegate->unittest_callback)) {
		gravity_unittest_callback unittest_cb = parser->delegate->unittest_callback;
		unittest_cb(expected_error, description, note, expected_value, expected_nrow, expected_ncol, parser->delegate->xdata);
	} else {
		// it was unit test responsability to free expected_value but if no unit test delegate is set I should take care of it
		gravity_value_free(NULL, expected_value);
	}
	
	// free temp nodes
	if (name_node) gnode_free((gnode_t*)name_node);
	if (note_node) gnode_free((gnode_t*)note_node);
	if (err_node) gnode_free((gnode_t*)err_node);
	if (row_node) gnode_free((gnode_t*)row_node);
	if (col_node) gnode_free((gnode_t*)col_node);
	if (value_node) gnode_free((gnode_t*)value_node);
	
	// always return NULL
handle_error:
	return NULL;
}

static gnode_t *parse_include_macro (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_include_macro");
	DECLARE_LEXER;
	
	// process filename (can be an identifier or a literal string)
	// only literals are supported in this version
	gtoken_t		type;
	gtoken_s		token;
	const char		*module_name;
	gravity_lexer_t	*newlexer;
	
loop:
	newlexer = NULL;
	type = gravity_lexer_next(lexer);
	token = gravity_lexer_token(lexer);
	
	// check if it is a string token
	if (type != TOK_STRING) {
		REPORT_ERROR(token, "Expected file name but found %s.", token_name(type));
		return NULL;
	}
	
	// check pre-requisites
	if ((!parser->delegate) || (!parser->delegate->loadfile_callback)) {
		REPORT_ERROR(gravity_lexer_token(lexer), "%s", "Unable to load file because no loadfile callback registered in delegate.");
		return NULL;
	}
	
	// parse string
	module_name = cstring_from_token(parser, token);
	size_t size = 0;
	uint32_t fileid = 0;
	
	// module_name is a filename and it is used by lexer to store filename into tokens
	// tokens are then stored inside AST nodes in order to locate errors into source code
	// AST can live a lot longer than both lexer and parser so we need a way to persistent
	// store these chuncks of memory
	const char *source = parser->delegate->loadfile_callback(module_name, &size, &fileid, parser->delegate->xdata);
	if (source) newlexer = gravity_lexer_create(source, size, fileid, false);
	
	if (newlexer) {
		// push new lexer into lexer stack
		marray_push(gravity_lexer_t*, *parser->lexer, newlexer);
	} else {
		REPORT_ERROR(token, "Unable to load file %s.", module_name);
	}
	
	// check for optional comma
	if (gravity_lexer_peek(lexer) == TOK_OP_COMMA) {
		gravity_lexer_next(lexer); // consume TOK_OP_COMMA
		goto loop;
	}
	
	// parse semicolon
	parse_semicolon(parser);
	
	return NULL;
}

// MARK: - Statements -

static gnode_t *parse_label_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_label_statement");
	
	// 'case' expression ':' statement
	// 'default' ':' statement
	
	DECLARE_LEXER;
	gtoken_t type = gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	assert((type == TOK_KEY_CASE) || (type == TOK_KEY_DEFAULT));
	
	// case specific expression
	gnode_t *expr = NULL;
	if (type == TOK_KEY_CASE) {
		expr = parse_expression(parser);
	}
	
	// common part
	parse_required(parser, TOK_OP_COLON);
	gnode_t *stmt = parse_statement(parser);
	
	return gnode_label_stat_create(token, expr, stmt);
}

static gnode_t *parse_flow_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_flow_statement");
	
	// 'if' '(' expression ')' statement ('else' statement)?
	// 'switch' '(' expression ')' statement
	
	DECLARE_LEXER;
	gtoken_t type = gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	assert((type == TOK_KEY_IF) || (type == TOK_KEY_SWITCH));
	
	// check optional TOK_OP_OPEN_PARENTHESIS
	bool is_parenthesize = parse_optional(parser, TOK_OP_OPEN_PARENTHESIS);
	
	// parse common expression
	gnode_t *cond = parse_expression(parser);
	
	// check and consume TOK_OP_CLOSED_PARENTHESIS
	if (is_parenthesize) parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
	
	// parse common statement
	gnode_t *stmt1 = parse_statement(parser);
	gnode_t *stmt2 = NULL;
	if ((type == TOK_KEY_IF) && (gravity_lexer_peek(lexer) == TOK_KEY_ELSE)) {
		gravity_lexer_next(lexer); // consume TOK_KEY_ELSE
		stmt2 = parse_statement(parser);
	}
	
	return gnode_flow_stat_create(token, cond, stmt1, stmt2);
}

static gnode_t *parse_loop_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_loop_statement");
	
	// 'while' '(' expression ')' statement
	// 'repeat' statement 'while' '(' expression ')' ';'
	// 'for' '(' condition 'in' expression ')' statement
	
	DECLARE_LEXER;
	gnode_t		*cond = NULL;
	gnode_t		*stmt = NULL;
	gnode_t		*expr = NULL;
	
	gtoken_t type = gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	assert((type == TOK_KEY_WHILE) || (type == TOK_KEY_REPEAT)  || (type == TOK_KEY_FOR));
	
	// 'while' '(' expression ')' statement
	if (type == TOK_KEY_WHILE) {
		// check optional TOK_OP_OPEN_PARENTHESIS
		bool is_parenthesize = parse_optional(parser, TOK_OP_OPEN_PARENTHESIS);
		
		// parse while condition
		cond = parse_expression(parser);
		
		// check and consume TOK_OP_CLOSED_PARENTHESIS
		if (is_parenthesize) parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
		
		// parse while statement
		stmt = parse_statement(parser);
		
		goto return_node;
	}
	
	// 'repeat' statement 'while' '(' expression ')' ';'
	if (type == TOK_KEY_REPEAT) {
		// parse repeat statement
		stmt = parse_statement(parser);
		
		// check and consume TOK_KEY_WHILE
		parse_required(parser, TOK_KEY_WHILE);
		
		// check optional TOK_OP_OPEN_PARENTHESIS
		bool is_parenthesize = parse_optional(parser, TOK_OP_OPEN_PARENTHESIS);
		
		// parse while expression
		expr = parse_expression(parser);
		
		// check and consume TOK_OP_CLOSED_PARENTHESIS
		if (is_parenthesize) parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
		
		// semicolon
		parse_semicolon(parser);
		
		goto return_node;
	}
	
	// 'for' '(' condition 'in' expression ')' statement
	if (type == TOK_KEY_FOR) {
		// check optional TOK_OP_OPEN_PARENTHESIS
		bool is_parenthesize = parse_optional(parser, TOK_OP_OPEN_PARENTHESIS);
		
		// parse condition (means parse variable declaration or expression)
		if (token_isvariable_declaration(gravity_lexer_peek(lexer))) {
			cond = parse_variable_declaration(parser, false, 0, 0);
		} else {
			cond = parse_expression(parser);
		}
			
		// check and consume TOK_KEY_IN
		parse_required(parser, TOK_KEY_IN);
		
		// parse expression
		expr = parse_expression(parser);
		
		// check and consume TOK_OP_CLOSED_PARENTHESIS
		if (is_parenthesize) parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
		
		// parse for statement
		stmt = parse_statement(parser);
	}
		
return_node:
	return gnode_loop_stat_create(token, cond, stmt, expr);
}

static gnode_t *parse_jump_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_jump_statement");
	
	// 'break' ';'
	// 'continue' ';'
	// 'return' expression? ';'
	
	DECLARE_LEXER;
	gtoken_t type = gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	assert((type == TOK_KEY_BREAK) || (type == TOK_KEY_CONTINUE) || (type == TOK_KEY_RETURN));
	
	gnode_t	*expr = NULL;
	if ((type == TOK_KEY_RETURN) && (gravity_lexer_peek(lexer) != TOK_OP_SEMICOLON)) {
		expr = parse_expression(parser);
	}
	
	parse_semicolon(parser);
	return gnode_jump_stat_create(token, expr);
}

static gnode_t *parse_compound_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_compound_statement");
	
	// '{' (statement+)? '}'
	
	// check and consume TOK_OP_OPEN_CURLYBRACE
	parse_required(parser, TOK_OP_OPEN_CURLYBRACE);
	
	DECLARE_LEXER;
	gtoken_s token = gravity_lexer_token(lexer);
	gnode_r *stmts = gnode_array_create();
	while (token_isstatement(gravity_lexer_peek(lexer))) {
		gnode_t *node = parse_statement(parser);
		if (node) gnode_array_push(stmts, node);
	}
	
	// check and consume TOK_OP_CLOSED_CURLYBRACE
	parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);
	
	return gnode_block_stat_create(NODE_COMPOUND_STAT, token, stmts);
}

static gnode_t *parse_empty_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_empty_statement");
	
	// ;
	
	DECLARE_LEXER;
	gravity_lexer_next(lexer);
	gtoken_s token = gravity_lexer_token(lexer);
	return gnode_empty_stat_create(token);
}

static gnode_t *parse_declaration_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_declaration_statement");
	
	DECLARE_LEXER;
	gtoken_t peek = gravity_lexer_peek(lexer);
	gtoken_t access_specifier = 0;	// 0 means no access specifier set
	gtoken_t storage_specifier = 0;	// 0 means no storage specifier set
	
	// check if an access specifier is set
	if (token_isaccess_specifier(peek)) {
		access_specifier = gravity_lexer_next(lexer);
		peek = gravity_lexer_peek(lexer);
	}
	
	// check if a storage specifier is set
	if (token_isstorage_specifier(peek)) {
		storage_specifier = gravity_lexer_next(lexer);
		peek = gravity_lexer_peek(lexer);
	}
	
	// it is a syntax error to specify an access or storage specifier followed by an empty declaration
	if ((peek == TOK_OP_SEMICOLON) && ((access_specifier) || (storage_specifier))) {
		REPORT_ERROR(gravity_lexer_token(lexer), "%s", "Access or storage specifier cannot be used here.");
	}
	
	switch (peek) {
		case TOK_KEY_FUNC: return parse_function_declaration(parser, access_specifier, storage_specifier);
		case TOK_KEY_ENUM: return parse_enum_declaration(parser, access_specifier, storage_specifier);
		case TOK_KEY_MODULE: return parse_module_declaration(parser, access_specifier, storage_specifier);
		case TOK_KEY_EVENT: return parse_event_declaration(parser, access_specifier, storage_specifier);
		case TOK_KEY_CLASS:
		case TOK_KEY_STRUCT: return parse_class_declaration(parser, access_specifier, storage_specifier);
		case TOK_OP_SEMICOLON: return parse_empty_statement(parser);
		case TOK_KEY_VAR:
		case TOK_KEY_CONST: return parse_variable_declaration(parser, true, access_specifier, storage_specifier);
		default: REPORT_ERROR(gravity_lexer_token(lexer), "Unrecognized token %s.", token_name(peek)); return NULL;
	}
	
	// should never reach this point
	assert(0);
	return NULL;
}

static gnode_t *parse_import_statement (gravity_parser_t *parser) {
	#pragma unused(parser)
	DEBUG_PARSER("parse_import_statement");
	
	// import is a syntactic sugar for System.import
	return NULL;
}

static gnode_t *parse_macro_statement (gravity_parser_t *parser) {
	typedef enum {
		MACRO_UNKNOWN = 0,
		MACRO_UNITEST = 1,
		MACRO_INCLUDE = 2
	} builtin_macro;
	
	DEBUG_PARSER("parse_macro_statement");
	DECLARE_LEXER;
	
	// consume special # symbol
	gtoken_t type = gravity_lexer_next(lexer);
	assert(type == TOK_MACRO);
	
	// check for #! and interpret #! shebang bash as one line comment
	// only if found on first line
	if (gravity_lexer_peek(lexer) == TOK_OP_NOT && gravity_lexer_lineno(lexer) == 1) {
		// consume special ! symbol
		type = gravity_lexer_next(lexer);
		assert(type == TOK_OP_NOT);
		
		// skip until EOL
		gravity_lexer_skip_line(lexer);
		return NULL;
	}
	
	// macro has its own parser because I don't want to mess standard syntax
	const char *macroid = parse_identifier(parser);
	if (macroid == NULL) goto handle_error;
	
	// check macro
	builtin_macro macro_type = MACRO_UNKNOWN;
	if (string_cmp(macroid, "unittest") == 0) macro_type = MACRO_UNITEST;
	else if (string_cmp(macroid, "include") == 0) macro_type = MACRO_INCLUDE;
	mem_free(macroid);
	
	switch (macro_type) {
		case MACRO_UNITEST:
			return parse_unittest_macro(parser);
			break;
			
		case MACRO_INCLUDE:
			return parse_include_macro(parser);
			break;
			
		case MACRO_UNKNOWN:
			break;
	}
	
handle_error:
	REPORT_WARNING(gravity_lexer_token(lexer), "%s", "Unknown macro token. Declaration will be ignored.");
	return NULL;
}

static gnode_t *parse_special_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_special_statement");
	DECLARE_LEXER;
	
	// consume special @ symbol
	gtoken_t type = gravity_lexer_next(lexer);
	assert(type == TOK_SPECIAL);
	
	// special is really special so it has its own parser
	// because I don't want to mess standard syntax
	const char *specialid = parse_identifier(parser);
	if (specialid == NULL) goto handle_error;
	
handle_error:
	REPORT_WARNING(gravity_lexer_token(lexer), "%s", "Unknown special token. Declaration will be ignored.");
	return NULL;
}

static gnode_t *parse_expression_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_expression_statement");
	
	gnode_t *expr = parse_expression(parser);
	parse_semicolon(parser);
	return expr;
}

static gnode_t *parse_statement (gravity_parser_t *parser) {
	DEBUG_PARSER("parse_statement");
	
	// label_statement
	// flow_statement
	// loop_statement
	// jump_statement
	// compound_statement
	// declaration_statement
	// empty_statement
	// import_statement
	// expression_statement (default)
	
	DECLARE_LEXER;
	gtoken_t token = gravity_lexer_peek(lexer);
	if (token_iserror(token)) return parse_error(parser);
	
	if (token_islabel_statement(token)) return parse_label_statement(parser);
	else if (token_isflow_statement(token)) return parse_flow_statement(parser);
	else if (token_isloop_statement(token)) return parse_loop_statement(parser);
	else if (token_isjump_statement(token)) return parse_jump_statement(parser);
	else if (token_iscompound_statement(token)) return parse_compound_statement(parser);
	else if (token_isdeclaration_statement(token)) return parse_declaration_statement(parser);
	else if (token_isempty_statement(token)) return parse_empty_statement(parser);
	else if (token_isimport_statement(token)) return parse_import_statement(parser);
	else if (token_isspecial_statement(token)) return parse_special_statement(parser);
	else if (token_ismacro(token)) return parse_macro_statement(parser);
	
	return parse_expression_statement(parser); // DEFAULT
}

// MARK: - Internal functions -

static void parser_register_core_classes (gravity_parser_t *parser) {
	const char **list = NULL;
	uint32_t n = gravity_core_identifiers(&list);
	if (!n) return;
	
	// for each core identifier create a dummy extern variable node
	gnode_r	*decls = gnode_array_create();
	for (uint32_t i=0; i<n; ++i) {
		const char *identifier = list[i];
		gnode_t *node = gnode_variable_create(NO_TOKEN, string_dup(identifier), NULL, 0, NULL);
		gnode_array_push(decls, node);
	}
	
	// register a variable declaration node in global statements
	gnode_t *node = gnode_variable_decl_create(NO_TOKEN, TOK_KEY_VAR, 0, TOK_KEY_EXTERN, decls);;
	gnode_array_push(parser->statements, (gnode_t *)node);
}

static uint32_t parser_run (gravity_parser_t *parser) {
	DEBUG_PARSER("=== BEGIN PARSING ===");
	
	// register core classes as extern globals
	parser_register_core_classes(parser);
	
	nanotime_t t1 = nanotime();
	do {
		while (gravity_lexer_peek(CURRENT_LEXER)) {
			gnode_t *node = parse_statement(parser);
			if (node) gnode_array_push(parser->statements, node);
		}
		
		// since it is a stack of lexers then check if it is a real EOF
		gravity_lexer_t *lexer = CURRENT_LEXER;
		gravity_lexer_free(lexer);
		marray_pop(*parser->lexer);
		
	} while (marray_size(*parser->lexer));
	nanotime_t t2 = nanotime();
	parser->time = millitime(t1, t2);
	
	DEBUG_PARSER("===  END PARSING  ===\n");
	
	return parser->nerrors;
}

static void parser_cleanup (gravity_parser_t *parser) {
	// in case of error (so AST is not returned)
	// then cleanup internal nodes
	gnode_t *node= gnode_block_stat_create(NODE_LIST_STAT, NO_TOKEN, parser->statements);
	gnode_free(node);
}

static void parser_appendcode (const char *source, gravity_parser_t *parser) {
	if (source == NULL) return;
	
	size_t len = strlen(source);
	if (len <= 0) return;
	
	// build a new lexer based on source code to prepend
	gravity_lexer_t *lexer1 = gravity_lexer_create(source, len, 0, true);
	if (!lexer1) return;
	
	// pop current lexer
	gravity_lexer_t *lexer2 = POP_LEXER;
	
	// swap lexer2 with lexer1
	marray_push(gravity_lexer_t*, *parser->lexer, lexer1);
	marray_push(gravity_lexer_t*, *parser->lexer, lexer2);
}

// MARK: - Public functions -

gravity_parser_t *gravity_parser_create (const char *source, size_t len, uint32_t fileid, bool is_static) {
	init_grammer_rules();
	
	gravity_parser_t *parser = mem_alloc(sizeof(gravity_parser_t));
	if (!parser) return NULL;
	
	gravity_lexer_t *lexer = gravity_lexer_create(source, len, fileid, is_static);
	if (!lexer) goto abort_init;
	
	parser->lexer = mem_alloc(sizeof(lexer_r));
	marray_init(*parser->lexer);
	marray_push(gravity_lexer_t*, *parser->lexer, lexer);
	
	parser->statements = gnode_array_create();
	if (!parser->statements) goto abort_init;
	
	marray_init(parser->declarations);
	
	parser->last_error_lineno = UINT32_MAX;
	return parser;
	
abort_init:
	gravity_parser_free(parser);
	return NULL;
}

gnode_t *gravity_parser_run (gravity_parser_t *parser, gravity_delegate_t *delegate) {
	parser->delegate = delegate;
	gravity_lexer_setdelegate(CURRENT_LEXER, delegate);
	
	// check if some user code needs to be prepended
	if ((delegate) && (delegate->precode_callback))
		parser_appendcode(delegate->precode_callback(delegate->xdata), parser);
	
	// if there are syntax errors then just returns
	if (parser_run(parser) > 0) {
		parser_cleanup (parser);
		return NULL;
	}
	
	// if there are some open declarations then there should be an error somewhere
	if (marray_size(parser->declarations) > 0) return NULL;
	
	// return ast
	return gnode_block_stat_create(NODE_LIST_STAT, NO_TOKEN, parser->statements);
}

void gravity_parser_free (gravity_parser_t *parser) {
	// free memory for stack of lexers
	if (parser->lexer) {
		size_t _len = marray_size(*parser->lexer);
		for (size_t i=0; i<_len; ++i) {
			gravity_lexer_t *lexer = (gravity_lexer_t *)marray_get(*parser->lexer, i);
			gravity_lexer_free(lexer);
		}
		marray_destroy(*parser->lexer);
		mem_free(parser->lexer);
	}
	
	marray_destroy(parser->declarations);
	// parser->statements is returned from gravity_parser_run
	// and must be deallocated using gnode_free
	
	mem_free(parser);
}
