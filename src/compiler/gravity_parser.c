//
//  gravity_parser.c
//  gravity
//
//  Created by Marco Bambini on 01/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_symboltable.h"
#include "gravity_optionals.h"
#include "gravity_parser.h"
#include "gravity_macros.h"
#include "gravity_lexer.h"
#include "gravity_token.h"
#include "gravity_utils.h"
#include "gravity_array.h"
#include "gravity_hash.h"
#include "gravity_core.h"
#include "gravity_ast.h"

typedef marray_t(gravity_lexer_t*)      lexer_r;

struct gravity_parser_t {
    lexer_r                             *lexer;             // stack of lexers (stack used in #include statements)
    gnode_r                             *declarations;      // used to keep track of nodes hierarchy
    gnode_r                             *statements;        // used to build AST
    gravity_delegate_t                  *delegate;          // compiler delegate
    uint16_r                            vdecl;              // to keep track of func expression in variable declaration nondes

    double                              time;
    uint32_t                            nerrors;
    uint32_t                            unique_id;
    uint32_t                            last_error_lineno;
    uint32_t                            depth;              // to keep track of maximum statements depth
    uint32_t                            expr_depth;         // to keep track of maximum expression depth

    // state ivars used by Pratt parser
    gtoken_t                            current_token;
    gnode_t                             *current_node;
};

// MARK: - PRATT parser specs -
// http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/
// http://javascript.crockford.com/tdop/tdop.html

// Precedence table as defined in Swift
// http://nshipster.com/swift-operators/
typedef enum {
    PREC_LOWEST,
    PREC_ASSIGN      = 90,     // = *= /= %= += -= <<= >>= &= ^= |=     (11 cases)
    PREC_TERNARY     = 100,    // ?:                                      (1 case)
    PREC_LOGICAL_OR  = 110,    // ||                                      (1 case)
    PREC_LOGICAL_AND = 120,    // &&                                      (1 case)
    PREC_COMPARISON  = 130,    // < <= > >= == != === !== ~=             (9 cases)
    PREC_ISA         = 132,    // isa                                     (1 case)
    PREC_RANGE       = 135,    // ..< ...                                (2 cases)
    PREC_TERM        = 140,    // + - | ^                                (4 cases)
    PREC_FACTOR      = 150,    // * / % &                                (4 cases)
    PREC_SHIFT       = 160,    // << >>                                  (2 cases)
    PREC_UNARY       = 170,    // + - ! ~                                (4 cases)
    PREC_CALL        = 200     // . ( [                                  (3 cases)
} prec_level;

typedef gnode_t* (*parse_func) (gravity_parser_t *parser);

typedef struct {
    parse_func          prefix;
    parse_func          infix;
    prec_level          precedence;
    const char          *name;
    bool                right;
} grammar_rule;

// This table defines all of the parsing rules for the prefix and infix expressions in the grammar.
#define RULE(prec, fn1, fn2)                (grammar_rule){ fn1, fn2, prec, NULL, false}
#define PREFIX(prec, fn)                    (grammar_rule){ fn, NULL, prec, NULL, false}
#define INFIX(prec, fn)                     (grammar_rule){ NULL, fn, prec, NULL, false}
#define INFIX_OPERATOR(prec, name)          (grammar_rule){ NULL, parse_infix, prec, name, false}
#define INFIX_OPERATOR_RIGHT(prec,name)     (grammar_rule){ NULL, parse_infix, prec, name, true}
#define PREFIX_OPERATOR(name)               (grammar_rule){ parse_unary, NULL, PREC_LOWEST, name, false}
#define OPERATOR(prec, name)                (grammar_rule){ parse_unary, parse_infix, prec, name, false}

// Global singleton grammar rule table
static grammar_rule rules[TOK_END];

// MARK: - Internal macros -
#define MAX_RECURSION_DEPTH                     1000
#define MAX_EXPRESSION_DEPTH                    512
#define MAX_NUMBER_LENGTH                       512
#define SEMICOLON_IS_OPTIONAL                   1

#define REPORT_ERROR(_tok,...)                  report_error(parser, GRAVITY_ERROR_SYNTAX, _tok, __VA_ARGS__)
#define REPORT_WARNING(_tok,...)                report_error(parser, GRAVITY_WARNING, _tok, __VA_ARGS__)

#define PUSH_DECLARATION(_decl)                 marray_push(gnode_t*, *parser->declarations, (gnode_t*)_decl)
#define POP_DECLARATION()                       marray_pop(*parser->declarations)
#define LAST_DECLARATION()                      (marray_size(*parser->declarations) ? marray_last(*parser->declarations) : NULL)
#define IS_FUNCTION_ENCLOSED()                  (get_enclosing(parser, NODE_FUNCTION_DECL))
#define IS_CLASS_ENCLOSED()                     (get_enclosing(parser, NODE_CLASS_DECL))
#define CHECK_NODE(_n)                          if (!_n) return NULL

#define POP_LEXER                               (gravity_lexer_t *)marray_pop(*parser->lexer)
#define CURRENT_LEXER                           (gravity_lexer_t *)marray_last(*parser->lexer)
#define DECLARE_LEXER                           gravity_lexer_t *lexer = CURRENT_LEXER; DEBUG_LEXER(lexer)
#define PARSER_CALL_CALLBACK(_t)                if (parser->delegate && parser->delegate->parser_callback) parser->delegate->parser_callback(&_t, parser->delegate->xdata)

#define STATIC_TOKEN_CSTRING(_s,_n,_l,_b,_t)    char _s[_n] = {0}; uint32_t _l = 0;            \
                                                const char *_b = token_string(_t, &_l);        \
                                                if (_l) memcpy(_s, _b, MINNUM(_n, _l))

// MARK: - Prototypes -
static const char *parse_identifier (gravity_parser_t *parser);
static gnode_t *parse_statement (gravity_parser_t *parser);
static gnode_r *parse_optional_parameter_declaration (gravity_parser_t *parser, bool is_implicit, bool *has_default_values);
static gnode_t *parse_compound_statement (gravity_parser_t *parser);
static gnode_t *parse_expression (gravity_parser_t *parser);
static gnode_t *parse_declaration_statement (gravity_parser_t *parser);
static gnode_t *parse_function (gravity_parser_t *parser, bool is_declaration, gtoken_t access_specifier, gtoken_t storage_specifier);
static gnode_t *adjust_assignment_expression (gravity_parser_t *parser, gtoken_t tok, gnode_t *lnode, gnode_t *rnode);
static gnode_t *parse_literal_expression (gravity_parser_t *parser);
static gnode_t *parse_macro_statement (gravity_parser_t *parser);

// MARK: - Utils functions -

static gnode_t *get_enclosing (gravity_parser_t *parser, gnode_n tag) {
    int32_t n = (int32_t)gnode_array_size(parser->declarations);
    if (!n) return NULL;

    --n;
    while (n >= 0) {
        gnode_t *decl = gnode_array_get(parser->declarations, n);
        if (!decl) return NULL;
        if (decl->tag == tag) return decl;
        --n;
    }

    return NULL;
}

static void patch_token_node (gnode_t *node, gtoken_s token) {
    node->token = token;
    
    if (node->tag == NODE_POSTFIX_EXPR) {
        gnode_postfix_expr_t *expr = (gnode_postfix_expr_t *)node;
        if (expr->id) expr->id->token = token;
        
        size_t count = gnode_array_size(expr->list);
        for (size_t i=0; i<count; ++i) {
            gnode_t *subnode = (gnode_t *)gnode_array_get(expr->list, i);
            if (subnode) subnode->token = token;
        }
    }
}

static void report_error (gravity_parser_t *parser, error_type_t error_type, gtoken_s token, const char *format, ...) {
    // just one error for each line
    if (parser->last_error_lineno == token.lineno) return;

    // increment internal error counter (and save last reported line) only if it was a real error
    if (error_type != GRAVITY_WARNING) {
        parser->last_error_lineno = token.lineno;
        ++parser->nerrors;
    }

    // get error callback (if any)
    void *data = (parser->delegate) ? parser->delegate->xdata : NULL;
    gravity_error_callback error_fn = (parser->delegate) ? ((gravity_delegate_t *)parser->delegate)->error_callback : NULL;

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
        .lineno = token.lineno,
        .colno = token.colno,
        .fileid = token.fileid,
        .offset = token.position
    };

    // finally call error callback
    if (error_fn) error_fn(NULL, error_type, buffer, error_desc, data);
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
//    - true if next token is equal to token passed as parameter (token is also consumed)
//    - false if next token is not equal (and no error is reported)
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

static bool parse_skip_until (gravity_parser_t *parser, gtoken_t token) {
    DECLARE_LEXER;

    while (1) {
        gtoken_t tok = gravity_lexer_next(lexer);
        if (tok == token) return true;
        if (tok == TOK_EOF) return false;
    }

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
        if (type != TOK_KEY_FUNC) {
            REPORT_ERROR(token, "Invalid function expression.");
            return NULL;
        }
    }

    // parse IDENTIFIER
    const char *identifier = NULL;
    if (is_declaration) {
        gtoken_t peek = gravity_lexer_peek(lexer);
        identifier = (token_isoperator(peek)) ? string_dup(token_name(gravity_lexer_next(lexer))) : parse_identifier(parser);
        DEBUG_PARSER("parse_function_declaration %s", identifier);
    }

    // create func declaration node
    gnode_function_decl_t *func = (gnode_function_decl_t *) gnode_function_decl_create(token, identifier, access_specifier, storage_specifier, NULL, NULL, LAST_DECLARATION());

    // check and consume TOK_OP_OPEN_PARENTHESIS
    if (!is_implicit) parse_required(parser, TOK_OP_OPEN_PARENTHESIS);

    // parse optional parameter declaration clause
    bool has_default_values = false;
    gnode_r *params = parse_optional_parameter_declaration(parser, is_implicit, &has_default_values);

    // check and consume TOK_OP_CLOSED_PARENTHESIS
    if (!is_implicit) parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);

    // parse compound statement
    PUSH_DECLARATION(func);
    gnode_compound_stmt_t *compound = (gnode_compound_stmt_t*)parse_compound_statement(parser);
    POP_DECLARATION();

    // if func is declarared inside a variable declaration node then the semicolon check must be
    // performed once at variable declaration node level ad not inside the func node
    bool is_inside_var_declaration = ((marray_size(parser->vdecl) > 0) && (marray_last(parser->vdecl) == 1));
    func->is_closure = is_inside_var_declaration;
    
    // parse optional semicolon
    if (!is_inside_var_declaration) parse_semicolon(parser);

    // finish func setup
    func->has_defaults = has_default_values;
    func->params = params;
    func->block = compound;
    return (gnode_t *)func;
}

static char *cstring_from_token (gravity_parser_t *parser, gtoken_s token) {
    #pragma unused(parser)
    uint32_t len = 0;
    const char *buffer = token_string(token, &len);

    char *str = (char *)mem_alloc(NULL, len+1);
    memcpy(str, buffer, len);
    return str;
}

static gnode_t *local_store_declaration (gravity_parser_t *parser, const char *identifier, const char *annotation_type, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_t *declaration) {
    gnode_r *decls = gnode_array_create();
    
    gnode_variable_decl_t *vdecl = (gnode_variable_decl_t *)gnode_variable_decl_create(declaration->token, TOK_KEY_VAR, access_specifier, storage_specifier, decls, LAST_DECLARATION());
    gnode_t *decl = gnode_variable_create(declaration->token, identifier ? string_dup(identifier) : NULL, annotation_type, declaration, LAST_DECLARATION(), vdecl);
    gnode_array_push(decls, decl);
    
    return (gnode_t *)vdecl;
}

static gnode_t *decl_check_access_specifier (gnode_t *node) {
    // set proper access specifiers
    // default is PUBLIC but if IDENTIFIER begins with _ then set it to PRIVATE
    if (node->tag == NODE_VARIABLE_DECL) {
        // default access specifier for variables is TOK_KEY_PUBLIC
        gnode_variable_decl_t *vdec_node = (gnode_variable_decl_t *)node;
        if (vdec_node->access == 0) {
            bool is_private = false;
            if (gnode_array_size(vdec_node->decls) > 0) {
                gnode_var_t *var = (gnode_var_t *)gnode_array_get(vdec_node->decls, 0);
                is_private = (var->identifier && var->identifier[0] == '_');
            }
            vdec_node->access = (is_private) ? TOK_KEY_PRIVATE : TOK_KEY_PUBLIC;
        }
    } else if (node->tag == NODE_FUNCTION_DECL) {
        // default access specifier for functions is PUBLIC
        gnode_function_decl_t *fdec_node = (gnode_function_decl_t *)node;
        if (!fdec_node->identifier)  return node;
        bool is_private = (fdec_node->identifier[0] == '_');
        if (fdec_node->access == 0) fdec_node->access = (is_private) ? TOK_KEY_PRIVATE : TOK_KEY_PUBLIC;
    } else if (node->tag == NODE_CLASS_DECL) {
        // default access specifier for inner class declarations is PUBLIC
        gnode_class_decl_t *cdec_node = (gnode_class_decl_t *)node;
        if (!cdec_node->identifier)  return node;
        bool is_private = (cdec_node->identifier[0] == '_');
        if (cdec_node->access == 0) cdec_node->access = (is_private) ? TOK_KEY_PRIVATE : TOK_KEY_PUBLIC;
    }

    return node;
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

	// read current token to extract node total length
	gtoken_s end_token = gravity_lexer_token(lexer);

    return gnode_flow_stat_create(token, cond, expr1, expr2, LAST_DECLARATION(), end_token.position + end_token.length - token.position);
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

    return gnode_file_expr_create(token, list, LAST_DECLARATION());
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
    const char    *type_annotation = NULL;
    gtoken_t    peek = gravity_lexer_peek(lexer);

    // type annotation
    // function foo (a: string, b: number)

    // check for optional type_annotation
    if (peek == TOK_OP_COLON) {
        gravity_lexer_next(lexer); // consume TOK_OP_COLON

        // parse identifier
        type_annotation = parse_identifier(parser);
    }

    return type_annotation;
}

static gnode_t *parse_optional_default_value (gravity_parser_t *parser) {
    DECLARE_LEXER;
    gnode_t     *default_value = NULL;
    gtoken_t    peek = gravity_lexer_peek(lexer);
    
    // optional literal default value
    // function foo (a: string = "Hello", b: number = 3)
    // type annotation not enforced here
    
    // check for optional default value
    if (peek == TOK_OP_ASSIGN) {
        gravity_lexer_next(lexer); // consume TOK_OP_ASSIGN
        
        // parse literal value
        default_value = parse_literal_expression(parser);
    }
    
    return default_value;
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
     :    '[' ((expression) (',' expression)*)? ']'        // array or empty array
     |    '[' ((map_entry (',' map_entry)*) | ':') ']'    // map or empty map
     ;

     map_entry
     :    STRING ':' expression
     ;
     */

    // consume first '['
    parse_required(parser, TOK_OP_OPEN_SQUAREBRACKET);

    // this saved token is necessary to save start of the list/map
    gtoken_s token = gravity_lexer_token(lexer);

    // check for special empty list
    if (gravity_lexer_peek(lexer) == TOK_OP_CLOSED_SQUAREBRACKET) {
        gravity_lexer_next(lexer); // consume TOK_OP_CLOSED_SQUAREBRACKET
        return gnode_list_expr_create(token, NULL, NULL, false, LAST_DECLARATION());
    }

    // check for special empty map
    if (gravity_lexer_peek(lexer) == TOK_OP_COLON) {
        gravity_lexer_next(lexer); // consume TOK_OP_COLON
        parse_required(parser, TOK_OP_CLOSED_SQUAREBRACKET);
        return gnode_list_expr_create(token, NULL, NULL, true, LAST_DECLARATION());
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
    return gnode_list_expr_create(token, list1, list2, ismap, LAST_DECLARATION());
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

    gnode_t *node = parse_function(parser, false, 0, 0);
    return node;
}

static gnode_t *parse_identifier_expression (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_identifier_expression");
    DECLARE_LEXER;

    const char *identifier = parse_identifier(parser);
    if (!identifier) return NULL;
    DEBUG_PARSER("IDENTIFIER: %s", identifier);

    gtoken_s token = gravity_lexer_token(lexer);
    return gnode_identifier_expr_create(token, identifier, NULL, LAST_DECLARATION());
}

static gnode_t *parse_identifier_or_keyword_expression (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_identifier_expression");
    DECLARE_LEXER;

    // check if token is a keyword
    uint32_t idx_start, idx_end;
    token_keywords_indexes(&idx_start, &idx_end);

    gtoken_t peek = gravity_lexer_peek(lexer);
    if (((uint32_t)peek >= idx_start) && ((uint32_t)peek <= idx_end)) {

        // consume token keyword
        gtoken_t keyword = gravity_lexer_next(lexer);
        gtoken_s token = gravity_lexer_token(lexer);

        // convert from keyword to identifier
        const char    *identifier = string_dup(token_name(keyword));
        return gnode_identifier_expr_create(token, identifier, NULL, LAST_DECLARATION());
    }

    // default case
    return parse_identifier_expression(parser);
}

static gnode_t *parse_number_expression (gravity_parser_t *parser, gtoken_s token) {
    DEBUG_PARSER("parse_number_expression");

    // check for special built-in cases first
    if (token.builtin != BUILTIN_NONE) {
        if (token.builtin == BUILTIN_LINE) return gnode_literal_int_expr_create(token, token.lineno, LAST_DECLARATION());
        else if (token.builtin == BUILTIN_COLUMN) return gnode_literal_int_expr_create(token, token.colno, LAST_DECLARATION());
    }

    // what I know here is that token is a well formed NUMBER
    // so I just need to properly decode it

    const char    *value = token.value;
    gliteral_t    type;
    int64_t        n = 0;
    double        d = 0;

    if (value[0] == '0') {
        int c = toupper(value[1]);
        if (c == 'B') {type = decode_number_binary(token, &n); goto report_node;}
        else if (c == 'O') {type = decode_number_octal(token, &n); goto report_node;}
        else if (c == 'X') {type = decode_number_hex(token, &n, &d); goto report_node;}
    }

    // number is decimal (check if it is float)
    bool isfloat = false;
    for (uint32_t i=0; i<token.bytes; ++i) {
        if (value[i] == '.') {isfloat = true; break;}
        if (value[i] == 'e') {isfloat = true; break;}
    }

    STATIC_TOKEN_CSTRING(str, MAX_NUMBER_LENGTH, len, buffer, token);
    if (len >= MAX_NUMBER_LENGTH) {
        REPORT_ERROR(token, "Malformed numeric expression.");
        return NULL;
    }

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
    if (type == LITERAL_FLOAT) return gnode_literal_float_expr_create(token, (double)d, LAST_DECLARATION());
    else if (type == LITERAL_INT) return gnode_literal_int_expr_create(token, n, LAST_DECLARATION());
    else assert(0);

    return NULL;
}

static gnode_t *parse_analyze_literal_string (gravity_parser_t *parser, gtoken_s token, const char *s, uint32_t len) {

    // check for special built-in cases first
    if (token.builtin != BUILTIN_NONE) {
        if (token.builtin == BUILTIN_FILE) {
            if (parser->delegate && parser->delegate->filename_callback) {
                const char *filename = parser->delegate->filename_callback(token.fileid, parser->delegate->xdata);
                if (!filename) filename = "";
                return gnode_literal_string_expr_create(token, (char *)filename, (uint32_t)strlen(filename), false, LAST_DECLARATION());
            }
        }
        else if (token.builtin == BUILTIN_FUNC) {
            gnode_function_decl_t *node = (gnode_function_decl_t *)get_enclosing(parser, NODE_FUNCTION_DECL);
            const char *identifier = (node && node->identifier) ? (node->identifier) : "";
            return gnode_literal_string_expr_create(token, (char *)identifier, (uint32_t)strlen(identifier), false, LAST_DECLARATION());
        }
        else if (token.builtin == BUILTIN_CLASS) {
            gnode_class_decl_t *node = (gnode_class_decl_t *)get_enclosing(parser, NODE_CLASS_DECL);
            const char *identifier = (node && node->identifier) ? (node->identifier) : "";
            return gnode_literal_string_expr_create(token, (char *)identifier, (uint32_t)strlen(identifier), false, LAST_DECLARATION());
        }
    }

    // used in string interpolation
    gnode_r *r = NULL;

    // analyze s (of length len) for escaped characters or for interpolations
    char *buffer = mem_alloc(NULL, len+1);
    uint32_t length = 0;

    for (uint32_t i=0; i<len;) {
        int c = s[i];
        if (c == '\\') {
            // handle escape sequence here
            if (i+1 >= len) {REPORT_ERROR(token, "Unexpected EOF inside a string literal"); goto return_string;}
            switch (s[i+1]) {
                case '\'': c = '\''; ++i; break;
                case '"':  c = '"'; ++i; break;
                case '\\': c = '\\'; ++i; break;
                case 'a': c = '\a'; ++i; break;
                case 'b': c = '\b'; ++i; break;
                case 'f': c = '\f'; ++i; break;
                case 'n': c = '\n'; ++i; break;
                case 'r': c = '\r'; ++i; break;
                case 't': c = '\t'; ++i; break;
                case 'v': c = '\v'; ++i; break;
                case 'x': {
                    // double hex digits sequence
                    // \XFF
                    if (i+1+2 >= len) {REPORT_ERROR(token, "Unexpected EOF inside a string literal"); goto return_string;}
                    // setup a static buffer assuming the next two characters are hex
                    char b[3] = {s[i+2], s[i+3], 0};
                    // convert from base 16 to base 10 (FF is at maximum 255)
                    c = (int)strtoul(b, NULL, 16);
                    buffer[length] = c;
                    // i+2 is until \x plus 2 hex characters
                    i+=2+2; ++length;
                    continue;
                }
                case 'u':  {
                    // 4 digits unicode sequence
                    // \uXXXX
                    if (i+1+4 >= len) {REPORT_ERROR(token, "Unexpected EOF inside a string literal"); goto return_string;}
                    // setup a static buffer assuming the next four characters are hex
                    char b[5] = {s[i+2], s[i+3], s[i+4], s[i+5], 0};
                    // convert from base 16 to base 10 (FFFF is at maximum 65535)
                    uint32_t n = (uint32_t)strtoul(b, NULL, 16);
                    length += utf8_encode(&buffer[length], n);
                    i+=2+4;
                    continue;
                }
                case 'U':  {
                    // 8 digits unicode sequence
                    // \uXXXXXXXX
                    if (i+1+8 >= len) {REPORT_ERROR(token, "Unexpected EOF inside a string literal"); goto return_string;}
                    // setup a static buffer assuming the next height characters are hex
                    char b[9] = {s[i+2], s[i+3], s[i+4], s[i+5], s[i+6], s[i+7], s[i+8], s[i+9], 0};
                    // convert from base 16 to base 10 (FFFF is at maximum 4294967295)
                    uint32_t n = (uint32_t)strtoul(b, NULL, 16);
                    length += utf8_encode(&buffer[length], n);
                    i+=2+8;
                    continue;
                }
                case '(': {
                    // string interpolation case
                    i+=2; // skip \ and (
                    uint32_t j=i;
                    uint32_t nesting_level = 0;
                    bool subfound = false;
                    while (i<len) {
                        if (s[i] == ')') {
                            if (nesting_level == 0) subfound = true;
                            else --nesting_level;
                        }
                        else if (s[i] == '(') {
                            ++nesting_level;
                        }
                        ++i;
                        if (subfound) break;
                    }
                    if (!subfound || nesting_level != 0) {
                        REPORT_ERROR(token, "Malformed interpolation string not closed by )");
                        goto return_string;
                    }

                    uint32_t sublen = i - j;

                    // create a new temp lexer
                    gravity_lexer_t *sublexer = gravity_lexer_create(&s[j], sublen, 0, true);
                    marray_push(gravity_lexer_t*, *parser->lexer, sublexer);

                    // parse interpolated expression
                    gnode_t *subnode = parse_expression(parser);

                    // add expression to r
                    if (subnode) {
                        // subnode contains information from a temp lexer so let's fix it
                        patch_token_node(subnode, token);
                        
                        if (!r) r = gnode_array_create();
                        if (length) gnode_array_push(r, gnode_literal_string_expr_create(token, buffer, length, true, LAST_DECLARATION()));
                        gnode_array_push(r, subnode);
                    }

                    // free temp lexer
                    marray_pop(*parser->lexer);
                    gravity_lexer_free(sublexer);
                    if (!subnode) goto return_string;

                    buffer = mem_alloc(NULL, len+1);
                    length = 0;

                    continue;
                }
                default:
                    // ignore unknown sequence
                    break;
            }

        }
        buffer[length] = c;
        ++i; ++length;
    }

return_string:
    // append the last string if any and if interpolation mode is on
    if (r && length) gnode_array_push(r, gnode_literal_string_expr_create(token, buffer, length, true, LAST_DECLARATION()));

    // return a node (even in case of error) so its memory will be automatically freed
    return (r) ? gnode_string_interpolation_create(token, r, LAST_DECLARATION()) : gnode_literal_string_expr_create(token, buffer, length, true, LAST_DECLARATION());
}

gnode_t *parse_literal_expression (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_literal_expression");
    DECLARE_LEXER;

    gtoken_t type = gravity_lexer_next(lexer);
    gtoken_s token = gravity_lexer_token(lexer);

    if (type == TOK_STRING) {
        uint32_t len = 0;
        const char *value = token_string(token, &len);
        DEBUG_PARSER("STRING: %.*s", len, value);
        // run string analyzer because string is returned as is from the lexer
        // but string can contains escaping sequences and interpolations that
        // need to be processed
        return parse_analyze_literal_string(parser, token, value, len);
    }

    if (type == TOK_KEY_TRUE || type == TOK_KEY_FALSE) {
        return gnode_literal_bool_expr_create(token, (int32_t)(type == TOK_KEY_TRUE) ? 1 : 0, LAST_DECLARATION());
    }

    if (type != TOK_NUMBER) {
        REPORT_ERROR(token, "Expected literal expression but found %s.", token_name(type));
        return NULL;
    }

    return parse_number_expression(parser, token);
}

static gnode_t *parse_keyword_expression (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_keyword_expression");
    DECLARE_LEXER;

    gravity_lexer_next(lexer);
    gtoken_s token = gravity_lexer_token(lexer);

    return gnode_keyword_expr_create(token, LAST_DECLARATION());
}

static gnode_r *parse_arguments_expression (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_arguments_expression");
    DECLARE_LEXER;

    // it's OK for a call_expression_list to be empty
    if (gravity_lexer_peek(lexer) == TOK_OP_CLOSED_PARENTHESIS) return NULL;

    // https://en.wikipedia.org/wiki/Named_parameter
    // with the introduction of named parameters there are a lot
    // of sub-cases to handle here, for example I cannot know in
    // advance if a call has named parameters or not from the
    // beginning because we also support mixed calls (both position
    // and named parameters)
    // so basically I collect two arrays here
    // one for names (or positions) and one for values
    // if the call is not a named call then the useless
    // array is discarded
    
    bool arg_expected = true;
    gnode_r *list = gnode_array_create();

    uint32_t index = 0;
    while (1) {
        gtoken_t peek = gravity_lexer_peek(lexer);

        if (peek == TOK_OP_COMMA) {
            // added the ability to convert ,, to ,undefined,
            gnode_array_push(list, gnode_keyword_expr_create(UNDEF_TOKEN, LAST_DECLARATION()));
            arg_expected = true;

            // consume next TOK_OP_COMMA and check for special ,) case
            gravity_lexer_next(lexer);
            if (gravity_lexer_peek(lexer) == TOK_OP_CLOSED_PARENTHESIS)
                gnode_array_push(list, gnode_keyword_expr_create(UNDEF_TOKEN, LAST_DECLARATION()));

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
                    gnode_array_push(list, gnode_keyword_expr_create(UNDEF_TOKEN, LAST_DECLARATION()));
            }

            // arg is expected only if a comma is consumed
            // this fixes syntax errors like System.print("Hello" " World")
            arg_expected = (peek == TOK_OP_COMMA);
        }
        
        ++index;
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
            node = gnode_postfix_subexpr_create(subtoken, NODE_SUBSCRIPT_EXPR, expr, NULL, LAST_DECLARATION());
        } else if (tok == TOK_OP_OPEN_PARENTHESIS) {
            gnode_r *args = parse_arguments_expression(parser);    // can be NULL and it's OK
            gtoken_s subtoken = gravity_lexer_token(lexer);
            parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
            node = gnode_postfix_subexpr_create(subtoken, NODE_CALL_EXPR, NULL, args, LAST_DECLARATION());
        } else if (tok == TOK_OP_DOT) {
            // was parse_identifier_expression but we need to allow also keywords here in order
            // to be able to supports expressions like name.repeat (repeat is a keyword but in this
            // context it should be interpreted as an identifier)
            gnode_t *expr = parse_identifier_or_keyword_expression(parser);
            gtoken_s subtoken = gravity_lexer_token(lexer);
            node = gnode_postfix_subexpr_create(subtoken, NODE_ACCESS_EXPR, expr, NULL, LAST_DECLARATION());
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

    return gnode_postfix_expr_create(token, lnode, list, LAST_DECLARATION());
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

    // peek next and check for EOF
    gtoken_t type = gravity_lexer_peek(lexer);
    if (type == TOK_EOF) return NULL;

    // execute prefix callback (if any)
    parse_func prefix = rules[type].prefix;

    // to protect stack from excessive recursion
    if (prefix && (++parser->expr_depth > MAX_EXPRESSION_DEPTH)) {
        // consume next token to avoid infinite loops
        gravity_lexer_next(lexer);
        REPORT_ERROR(gravity_lexer_token(lexer), "Maximum expression depth reached.");
        return NULL;
    }
    gnode_t *node = (prefix) ? prefix(parser) : NULL;
    if (prefix) --parser->expr_depth;

    if (!prefix || !node) {
        // we need to consume next token because error was triggered in peek
        gravity_lexer_next(lexer);
        REPORT_ERROR(gravity_lexer_token(lexer), "Expected expression but found %s.", token_name(type));
        return NULL;
    }

    // peek next and check for EOF
    gtoken_t peek = gravity_lexer_peek(lexer);
    if (peek == TOK_EOF) return node;

    while (precedence < rules[peek].precedence) {
        gtoken_t tok = gravity_lexer_next(lexer);
        grammar_rule *rule = &rules[tok];

        // execute infix callback
        parser->current_token = tok;
        parser->current_node = node;
        node = rule->infix(parser);

        // peek next and check for EOF
        peek = gravity_lexer_peek(lexer);
        if (peek == TOK_EOF) break;
    }

    return node;
}

static gnode_t *parse_expression (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_expression");
    return parse_precedence(parser, PREC_LOWEST);
}

static gnode_t *parse_unary (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_unary");
    DECLARE_LEXER;

    gtoken_t tok = gravity_lexer_next(lexer);
    gnode_t *node = parse_precedence(parser, PREC_UNARY);
    return gnode_unary_expr_create(tok, node, LAST_DECLARATION());
}

static gnode_t *parse_infix (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_infix");

    gtoken_t tok = parser->current_token;
    gnode_t *lnode = parser->current_node;

    // we can make right associative operators by reducing the right binding power
    grammar_rule *rule = &rules[tok];
    prec_level precedence = (rule->right) ? rule->precedence-1 : rule->precedence;

    gnode_t *rnode = parse_precedence(parser, precedence);
    if ((tok != TOK_OP_ASSIGN) && token_isassignment(tok)) return adjust_assignment_expression(parser, tok, lnode, rnode);
    return gnode_binary_expr_create(tok, lnode, rnode, LAST_DECLARATION());
}

// MARK: -

static gnode_t *adjust_assignment_expression (gravity_parser_t *parser, gtoken_t tok, gnode_t *lnode, gnode_t *rnode) {
    DEBUG_PARSER("adjust_assignment_expression");

    // called when tok is an assignment != TOK_OP_ASSIGN
    // convert expressions:
    // a += 1    => a = a + 1
    // a -= 1    => a = a - 1
    // a *= 1    => a = a * 1
    // a /= 1    => a = a / 1
    // a %= 1    => a = a % 1
    // a <<=1    => a = a << 1
    // a >>=1    => a = a >> 1
    // a &= 1    => a = a & 1
    // a |= 1    => a = a | 1
    // a ^= 1    => a = a ^ 1

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
    gnode_t *duplicate = gnode_duplicate(lnode, true);
    if (!duplicate) {DECLARE_LEXER; REPORT_ERROR(gravity_lexer_token(lexer), "An unexpected error occurred in %s", token_name(tok)); return NULL;}
    rnode = gnode_binary_expr_create(t, duplicate, rnode, LAST_DECLARATION());
    tok = TOK_OP_ASSIGN;

    // its an assignment expression so switch the order
    return gnode_binary_expr_create(tok, lnode, rnode, LAST_DECLARATION());
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

        bool is_getter = false;
        gtoken_s token = gravity_lexer_token(lexer);
        gnode_r *params = NULL;

        // getter case: does not have explicit parameters (only implicit self)
        if (strcmp(identifier, GETTER_FUNCTION_NAME) == 0) {
            is_getter = true;
            params = gnode_array_create();    // add implicit SELF param
            gnode_array_push(params, gnode_variable_create(NO_TOKEN, string_dup(SELF_PARAMETER_NAME), NULL, NULL, LAST_DECLARATION(), NULL));
        }

        // setter case: could have explicit parameters (otherwise value is implicit)
        if (strcmp(identifier, SETTER_FUNCTION_NAME) == 0) {
            is_getter = false;
            // check if parameters are explicit
            if (gravity_lexer_peek(lexer) == TOK_OP_OPEN_PARENTHESIS) {
                parse_required(parser, TOK_OP_OPEN_PARENTHESIS);
                params = parse_optional_parameter_declaration(parser, false, NULL);    // add implicit SELF
                parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);
            } else {
                params = gnode_array_create();    // add implicit SELF and VALUE params
                gnode_array_push(params, gnode_variable_create(NO_TOKEN, string_dup(SELF_PARAMETER_NAME), NULL, NULL, LAST_DECLARATION(), NULL));
                gnode_array_push(params, gnode_variable_create(NO_TOKEN, string_dup(SETTER_PARAMETER_NAME), NULL, NULL, LAST_DECLARATION(), NULL));
            }
        }
        mem_free(identifier);

        // create getter/setter func declaration
        gnode_t *f = gnode_function_decl_create(token, NULL, 0, 0, params, NULL, LAST_DECLARATION());
        // set storage to var so I can identify f as a special getter/setter function
        ((gnode_function_decl_t *)f)->storage = TOK_KEY_VAR;

        // parse compound statement
        PUSH_DECLARATION(f);
        gnode_compound_stmt_t *compound = (gnode_compound_stmt_t*)parse_compound_statement(parser);
        POP_DECLARATION();

        // finish func setup
        ((gnode_function_decl_t *)f)->block = compound;

        // assign f to the right function
        if (is_getter) getter = f; else setter = f;
    }

    gnode_r *functions = gnode_array_create();
    gnode_array_push(functions, (getter) ? getter : NULL);    // getter is at index 0
    gnode_array_push(functions, (setter) ? setter : NULL);    // setter is at index 1

	// read current token to extract node total length
	gtoken_s end_token = gravity_lexer_token(lexer);
	
    // a compound node is used to capture getter and setter
    return gnode_block_stat_create(NODE_COMPOUND_STAT, token_block, functions, LAST_DECLARATION(), end_token.position + end_token.length - token_block.position);

parse_error:
    return NULL;
}

static gnode_t *parse_variable_declaration (gravity_parser_t *parser, bool isstatement, gtoken_t access_specifier, gtoken_t storage_specifier) {
    DEBUG_PARSER("parse_variable_declaration");
    DECLARE_LEXER;

    gnode_r *decls = NULL;
    gnode_t *decl = NULL;
    gnode_t *expr = NULL;
    const char *identifier = NULL;
    const char *type_annotation = NULL;
    gtoken_t type;
    gtoken_t peek;
    gtoken_s token, token2;

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

    // create node variable declaration
    gnode_variable_decl_t *node = (gnode_variable_decl_t *) gnode_variable_decl_create(token, type, access_specifier, storage_specifier, NULL, LAST_DECLARATION());

    // initialize node array
    decls = gnode_array_create();

loop:
    identifier = parse_identifier(parser);
    if (!identifier) return NULL;
    token2 = gravity_lexer_token(lexer);

    // type annotation is optional so it can be NULL
    type_annotation = parse_optional_type_annotation(parser);
    DEBUG_PARSER("IDENTIFIER: %s %s", identifier, (type_annotation) ? type_annotation : "");
    if (type_annotation && parser->delegate && parser->delegate->type_callback) {
        parser->delegate->type_callback(&token2, type_annotation, parser->delegate->xdata);
    }

    // check for optional assignment or getter/setter declaration (ONLY = is ALLOWED here!)
    expr = NULL;
    bool is_computed = false;
    peek = gravity_lexer_peek(lexer);
    if (token_isvariable_assignment(peek)) {
        gravity_lexer_next(lexer); // consume ASSIGNMENT
        marray_push(uint16_t, parser->vdecl, 1);
        expr = parse_expression(parser);
        marray_pop(parser->vdecl);
    } else if (peek == TOK_OP_OPEN_CURLYBRACE) {
        gravity_lexer_next(lexer); // consume TOK_OP_OPEN_CURLYBRACE
        expr = parse_getter_setter(parser);
        parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);
        is_computed = true;
    }

    // sanity checks
    // 1. CONST must be followed by an assignment expression ?
    // 2. check if identifier is unique inside variable declarations

    decl = gnode_variable_create(token2, identifier, type_annotation, expr, LAST_DECLARATION(), node);
    if (decl) {
        ((gnode_var_t *)decl)->iscomputed = is_computed;
        gnode_array_push(decls, decl);
    }

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

    // finish to setup declaration
    node->decls = decls;

    return (gnode_t *)node;
}

static gnode_t *parse_enum_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
    DEBUG_PARSER("parse_enum_declaration");
    DECLARE_LEXER;

    // enum is a bit different than the traditional C like enum statements
    // in Gravity enum can contains String, Integer, Boolean and Float BUT cannot be mixed
    // Integer case can also skip values and autoincrement will be applied
    // String and Float must have a default value
    // this code will take care of parsing and syntax check for the above restrictions

    // in order to simplify node struct all the sematic checks are performed here
    // even if it is not a best practice, I find a lot easier to perform all the checks
    // directly into the parser
    // parse_enum_declaration is the only reason why gravity_symboltable.h is included here

    // checks are:
    // 1: unique internal identifiers
    // 2: if not INT then a default value is mandatory
    // 3: all values must be literals

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
    if (parse_required(parser, TOK_OP_OPEN_CURLYBRACE) == false) return NULL;

    symboltable_t *symtable = symboltable_create(SYMTABLE_TAG_ENUM);  // enum symbol table (symtable is OK because order is not important inside an enum)
    int64_t     enum_autoint = 0;           // autoincrement value (in case of INT enum)
    uint32_t    enum_counter = 0;           // enum internal counter (first value (if any) determines enum type)
    gliteral_t  enum_type = LITERAL_INT;    // enum type (default to int)

    // create enum node
    gnode_enum_decl_t *node = (gnode_enum_decl_t *)gnode_enum_decl_create(token, identifier, access_specifier, storage_specifier, symtable, LAST_DECLARATION());

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
                    REPORT_ERROR(enum_token, "Literal value expected here.");
                    continue;
                }

                if ((unary->op != TOK_OP_SUB) && (unary->op != TOK_OP_ADD)) {
                    REPORT_ERROR(enum_token, "Only + or - allowed in enum value definition.");
                    continue;
                }

                enum_literal = (gnode_literal_expr_t *)expr;
                if ((enum_literal->type != LITERAL_FLOAT) && (enum_literal->type != LITERAL_INT)) {
                    REPORT_ERROR(enum_token, "A number is expected after a + or - unary expression in an enum definition.");
                    continue;
                }

                if (unary->op == TOK_OP_SUB) {
                    gnode_t *temp = NULL;
                    if (enum_literal->type == LITERAL_FLOAT) {
                        //enum_literal->value.d = -enum_literal->value.d;
                        temp = gnode_literal_float_expr_create(enum_value->base.token, -enum_literal->value.d, LAST_DECLARATION());
                    }
                    else if (enum_literal->type == LITERAL_INT) {
                        //enum_literal->value.n64 = -enum_literal->value.n64;
                        temp = gnode_literal_int_expr_create(enum_value->base.token, -enum_literal->value.n64, LAST_DECLARATION());
                    }
                    
                    if (temp) {
                        gnode_free((gnode_t *)enum_value);
                        enum_value = (gnode_base_t *)temp;
                        enum_literal = (gnode_literal_expr_t *)temp;
                    }
                }

            } else {
                REPORT_ERROR(enum_token, "Literal value expected here.");
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
                REPORT_ERROR(enum_token, "Literal value of type %s expected here.", token_literal_name(enum_type));
            }

            // update enum_autoint value to next value
            if (enum_literal->type == LITERAL_INT) {
                enum_autoint = enum_literal->value.n64 + 1;
            }

        } else {
            enum_value = (gnode_base_t *)gnode_literal_int_expr_create(NO_TOKEN, enum_autoint, LAST_DECLARATION());
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

    if (IS_FUNCTION_ENCLOSED()) return local_store_declaration(parser, identifier, NULL, access_specifier, storage_specifier, (gnode_t *)node);
    return (gnode_t *)node;
}

static gnode_t *parse_module_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
    DEBUG_PARSER("parse_module_declaration");

    // module parsed but not yet supported
    // 'module' IDENTIFIER '{' declaration_statement* '}' ';'

    // optional scope already consumed
    DECLARE_LEXER;
    gtoken_t type = gravity_lexer_next(lexer);
    gtoken_s token = gravity_lexer_token(lexer);
    assert(type == TOK_KEY_MODULE);

    // parse IDENTIFIER
    const char *identifier = parse_identifier(parser);
    DEBUG_PARSER("parse_module_declaration %s", identifier);
    #pragma unused(identifier)

    // parse optional curly brace
    bool curly_brace = parse_optional(parser, TOK_OP_OPEN_CURLYBRACE);

    // create array of declarations nodes
    gnode_r *declarations = gnode_array_create();

    // create module node
    gnode_t *node = NULL;//gnode_module_decl_create(token, identifier, access_specifier, storage_specifier, declarations, meta, LAST_DECLARATION());
    #pragma unused(access_specifier,storage_specifier)

    while (token_isdeclaration_statement(gravity_lexer_peek(lexer))) {
        gnode_t *decl = parse_declaration_statement(parser);
        if (decl) gnode_array_push(declarations, decl);
    }

    // check and consume TOK_OP_CLOSED_CURLYBRACE
    if (curly_brace) parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);

    parse_semicolon(parser);

    REPORT_ERROR(token, "Module declarations not yet supported.");
    return node;
}

static gnode_t *parse_event_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
    #pragma unused(parser, access_specifier, storage_specifier)

    // 'event' IDENTIFIER '(' parameter_declaration_clause? ')' ';'

    // NODE_EVENT_DECL
    DECLARE_LEXER;
    gtoken_t type = gravity_lexer_next(lexer);
    gtoken_s token = gravity_lexer_token(lexer);
    assert(type == TOK_KEY_EVENT);

    REPORT_ERROR(token, "Event declarations not yet supported.");
    return NULL;
}

static gnode_t *parse_function_declaration (gravity_parser_t *parser, gtoken_t access_specifier, gtoken_t storage_specifier) {
    // convert a function declaration within another function to a local variable assignment
    // for example:
    //
    // func foo() {
    //    func bar() {...}
    // }
    //
    // is converter to:
    //
    // func foo() {
    //    var bar = func() {...}
    // }
    //
    // conversion is performed inside the parser
    // so next semantic checks can perform
    // identifier uniqueness checks
    gnode_t *node = parse_function(parser, true, access_specifier, storage_specifier);

    if (IS_FUNCTION_ENCLOSED()) {
        gnode_function_decl_t *func = (gnode_function_decl_t *)node;
        func->is_closure = true;
        return local_store_declaration(parser, func->identifier, NULL, access_specifier, storage_specifier, node);
    }
    return node;
}

static gnode_t *parse_id (gravity_parser_t *parser) {
    DECLARE_LEXER;
    const char    *identifier1 = NULL;
    const char    *identifier2 = NULL;
    gtoken_t    peek;
    gtoken_s    token;

    // IDENTIFIER |    (IDENTIFIER)('.' IDENTIFIER)

    DEBUG_PARSER("parse_id");

    identifier1 = parse_identifier(parser);

    token = gravity_lexer_token(lexer);
    peek = gravity_lexer_peek(lexer);
    if (peek == TOK_OP_DOT) {
        gravity_lexer_next(lexer); // consume TOK_OP_DOT
        identifier2 = parse_identifier(parser);
    }

    DEBUG_PARSER("ID: %s %s", identifier1, (identifier2) ? identifier2 : "");
    return gnode_identifier_expr_create(token, identifier1, identifier2, LAST_DECLARATION());
}

static gnode_r *parse_protocols (gravity_parser_t *parser) {
    DECLARE_LEXER;
    gtoken_t peek;
    gnode_t     *node = NULL;
    gnode_r     *list = NULL;

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
    if (storage_specifier != TOK_KEY_EXTERN) parse_required(parser, TOK_OP_OPEN_CURLYBRACE);
    gnode_r *declarations = gnode_array_create();

    // if class is declared inside another class then a hidden implicit private "outer" instance var at index 0
    // is automatically added
    if (IS_CLASS_ENCLOSED()) {
        gnode_r *decls = gnode_array_create();
        gnode_t *outer_var = gnode_variable_create(NO_TOKEN, string_dup(OUTER_IVAR_NAME), NULL, NULL, LAST_DECLARATION(), NULL);
        gnode_array_push(decls, outer_var);

        gnode_t *outer_decl = gnode_variable_decl_create(NO_TOKEN, TOK_KEY_VAR, TOK_KEY_PRIVATE, 0, decls, LAST_DECLARATION());
        gnode_array_push(declarations, outer_decl);
    }

    // create class declaration node
    gnode_class_decl_t *node = (gnode_class_decl_t*) gnode_class_decl_create(token, identifier, access_specifier, storage_specifier, super, protocols, NULL, is_struct, LAST_DECLARATION());

    if (storage_specifier != TOK_KEY_EXTERN) {
        PUSH_DECLARATION(node);
        peek = gravity_lexer_peek(lexer);
        while (token_isdeclaration_statement(peek) || token_ismacro(peek)) {
            gnode_t *decl = parse_declaration_statement(parser);
            if (decl) gnode_array_push(declarations, decl_check_access_specifier(decl));
            peek = gravity_lexer_peek(lexer);
        }
        POP_DECLARATION();
    }

    // check and consume TOK_OP_CLOSED_CURLYBRACE
    if (storage_specifier != TOK_KEY_EXTERN) parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);

    // to check
    parse_semicolon(parser);

    // finish setup node class
    node->decls = declarations;

    const char *class_manifest_type = gravity_class_class->identifier;
    if (IS_FUNCTION_ENCLOSED()) return local_store_declaration(parser, identifier, string_dup(class_manifest_type), access_specifier, storage_specifier, (gnode_t *)node);
    return (gnode_t *)node;
}

static gnode_r *parse_optional_parameter_declaration (gravity_parser_t *parser, bool is_implicit, bool *has_default_values) {
    DEBUG_PARSER("parse_parameter_declaration");
    DECLARE_LEXER;

    gtoken_s    token = NO_TOKEN;
    gnode_t     *node = NULL;
    gnode_t     *default_value = NULL;
    const char  *identifier = NULL;
    const char  *type_annotation = NULL;

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
    node = gnode_variable_create(token, string_dup(SELF_PARAMETER_NAME), type_annotation, NULL, LAST_DECLARATION(), NULL);
    DEBUG_PARSER("PARAMETER: %s %s", SELF_PARAMETER_NAME, (type_annotation) ? type_annotation : "");
    if (node) gnode_array_push(params, node);
    if (is_implicit) return params;

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
    if (type_annotation && parser->delegate && parser->delegate->type_callback) {
        parser->delegate->type_callback(&token, type_annotation, parser->delegate->xdata);
    }

    // parse optional default LITERAL value
    default_value = parse_optional_default_value(parser);
    if (default_value && has_default_values) *has_default_values = true;
    
    // fill parameters array with the new node
    node = gnode_variable_create(token, identifier, type_annotation, default_value, LAST_DECLARATION(), NULL);
    if (node) gnode_array_push(params, node);
    DEBUG_PARSER("PARAMETER: %s %s", identifier, (type_annotation) ? type_annotation : "");

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

    // #unittest {
    //        name: "Unit test name";
    //        note: "Some notes here";
    //        error: NONE, SYNTAX, RUNTIME, WARNING;
    //        error_row: number;
    //        error_col: number;
    //        result: LITERAL;
    // '}' ';'?
    
    // sanity check: unittest macro must be root of the document
    // 1. parse_statement
    // 2. parse_macro_statement
    if (marray_size(*parser->statements) != 2) {
        REPORT_ERROR(gravity_lexer_token(lexer), "#unittest macro cannot be embedded in a statement (it must be the root of the document).");
        return NULL;
    }

    gnode_literal_expr_t *name_node = NULL;
    gnode_literal_expr_t *note_node = NULL;
    gnode_identifier_expr_t *err_node = NULL;
    gnode_literal_expr_t *row_node = NULL;
    gnode_literal_expr_t *col_node = NULL;
    gnode_literal_expr_t *value_node = NULL;

    parse_required(parser, TOK_OP_OPEN_CURLYBRACE);
    while (gravity_lexer_peek(lexer) != TOK_OP_CLOSED_CURLYBRACE) {
        const char *id = parse_identifier(parser);
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
            REPORT_ERROR(gravity_lexer_token(lexer), "Unknown token found in #unittest declaration.");
            goto handle_error;
        }

    handle_continue:
        parse_semicolon(parser);
    }

    parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);
    parse_semicolon(parser);

    // decode unit array and report error/unittest
    // unit test name max length is 1024
    const char *description = NULL;
    const char *note = NULL;
    char buffer[1024];
    char buffer2[1024];
    error_type_t expected_error = GRAVITY_ERROR_NONE;
    gravity_value_t expected_value = VALUE_FROM_NULL;
    int32_t expected_nrow = -1;
    int32_t expected_ncol = -1;

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
        unittest_cb(NULL, expected_error, description, note, expected_value, expected_nrow, expected_ncol, parser->delegate->xdata);
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
    return NULL;

handle_error:
    parse_skip_until(parser, TOK_OP_CLOSED_CURLYBRACE);
    return NULL;
}

static gnode_t *parse_include_macro (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_include_macro");
    DECLARE_LEXER;

    // process filename (can be an identifier or a literal string)
    // only literals are supported in this version
    gtoken_t type;
    gtoken_s token;
    const char *module_name;
    gravity_lexer_t *newlexer;

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
        REPORT_ERROR(gravity_lexer_token(lexer), "Unable to load file because no loadfile callback registered in delegate.");
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
    bool is_static = false;
	const char *source = parser->delegate->loadfile_callback(module_name, &size, &fileid, parser->delegate->xdata, &is_static);
	if (source) newlexer = gravity_lexer_create(source, size, fileid, is_static);

    if (newlexer) {
        // push new lexer into lexer stack
        marray_push(gravity_lexer_t*, *parser->lexer, newlexer);
    } else {
        REPORT_ERROR(token, "Unable to load file %s.", module_name);
    }

    // cleanup memory
    if (module_name) mem_free(module_name);

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

    return gnode_label_stat_create(token, expr, stmt, LAST_DECLARATION());
}

static gnode_t *parse_flow_statement (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_flow_statement");

    // 'if' '(' expression ')' statement ('else' statement)?
    // 'switch' '(' expression ')' statement

    DECLARE_LEXER;
    gtoken_t type = gravity_lexer_next(lexer);
    gtoken_s token = gravity_lexer_token(lexer);
    assert((type == TOK_KEY_IF) || (type == TOK_KEY_SWITCH));

    // check required TOK_OP_OPEN_PARENTHESIS
    parse_required(parser, TOK_OP_OPEN_PARENTHESIS);

    // parse common expression
    gnode_t *cond = parse_expression(parser);

    // check and consume TOK_OP_CLOSED_PARENTHESIS
    parse_required(parser, TOK_OP_CLOSED_PARENTHESIS);

    // parse common statement
    gnode_t *stmt1 = parse_statement(parser);
    gnode_t *stmt2 = NULL;
    if ((type == TOK_KEY_IF) && (gravity_lexer_peek(lexer) == TOK_KEY_ELSE)) {
        gravity_lexer_next(lexer); // consume TOK_KEY_ELSE
        stmt2 = parse_statement(parser);
    }

	// read current token to extract node total length
	gtoken_s end_token = gravity_lexer_token(lexer);
	
	return gnode_flow_stat_create(token, cond, stmt1, stmt2, LAST_DECLARATION(), end_token.position + end_token.length - token.position);
}

static gnode_t *parse_loop_statement (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_loop_statement");

    // 'while' '(' expression ')' statement
    // 'repeat' statement 'while' '(' expression ')' ';'
    // 'for' '(' condition 'in' expression ')' statement

    DECLARE_LEXER;
    gnode_t        *cond = NULL;
    gnode_t        *stmt = NULL;
    gnode_t        *expr = NULL;

    gtoken_t type = gravity_lexer_next(lexer);
    gtoken_s token = gravity_lexer_token(lexer);
    gtoken_s end_token;
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
    // read current token to extract node total length
    end_token = gravity_lexer_token(lexer);
    
    // return loop node
    return gnode_loop_stat_create(token, cond, stmt, expr, LAST_DECLARATION(), end_token.position + end_token.length - token.position);
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

    gnode_t *expr = NULL;
    if ((type == TOK_KEY_RETURN) && (gravity_lexer_peek(lexer) != TOK_OP_SEMICOLON) && (gravity_lexer_peek(lexer) != TOK_OP_CLOSED_CURLYBRACE)) {
        expr = parse_expression(parser);
    }

    parse_semicolon(parser);
    return gnode_jump_stat_create(token, expr, LAST_DECLARATION());
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
        if (++parser->depth > MAX_RECURSION_DEPTH) {
            REPORT_ERROR(gravity_lexer_token(lexer), "Maximum statement recursion depth reached.");
            return NULL;
        }
        gnode_t *node = parse_statement(parser);
        if (node) gnode_array_push(stmts, node);
        --parser->depth;
    }

    // check and consume TOK_OP_CLOSED_CURLYBRACE
    parse_required(parser, TOK_OP_CLOSED_CURLYBRACE);

	// read current token to extract node total length
	gtoken_s end_token = gravity_lexer_token(lexer);

    return gnode_block_stat_create(NODE_COMPOUND_STAT, token, stmts, LAST_DECLARATION(), end_token.position + end_token.length - token.position);
}

static gnode_t *parse_empty_statement (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_empty_statement");

    // ;

    DECLARE_LEXER;
    gravity_lexer_next(lexer);
    gtoken_s token = gravity_lexer_token(lexer);
    return gnode_empty_stat_create(token, LAST_DECLARATION());
}

static gnode_t *parse_declaration_statement (gravity_parser_t *parser) {
    DEBUG_PARSER("parse_declaration_statement");

    DECLARE_LEXER;
    gtoken_t peek = gravity_lexer_peek(lexer);
    gtoken_t access_specifier = 0;    // 0 means no access specifier set
    gtoken_t storage_specifier = 0;    // 0 means no storage specifier set

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
        REPORT_ERROR(gravity_lexer_token(lexer), "Access or storage specifier cannot be used here.");
    }

    switch (peek) {
        case TOK_MACRO: return parse_macro_statement(parser);
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
    DECLARE_LEXER;

    // import is a syntactic sugar for System.import
    gravity_lexer_next(lexer);
    return NULL;
}

static gnode_t *parse_macro_statement (gravity_parser_t *parser) {
    typedef enum {
        MACRO_UNKNOWN = 0,
        MACRO_UNITEST = 1,
        MACRO_INCLUDE = 2,
        MACRO_PUSH = 3,
        MACRO_POP = 4
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
    else if (string_cmp(macroid, "push") == 0) macro_type = MACRO_PUSH;
    else if (string_cmp(macroid, "pop") == 0) macro_type = MACRO_POP;
    mem_free(macroid);

    gtoken_s token = gravity_lexer_token(lexer);
    token.type = TOK_MACRO;
    PARSER_CALL_CALLBACK(token);

    switch (macro_type) {
        case MACRO_UNITEST:
            return parse_unittest_macro(parser);
            break;

        case MACRO_INCLUDE:
            return parse_include_macro(parser);
            break;

        case MACRO_PUSH:
            break;

        case MACRO_POP:
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
    const char **list = gravity_core_identifiers();

    // for each core identifier create a dummy extern variable node
    gnode_r *decls = gnode_array_create();

    uint32_t i = 0;
    while (list[i]) {
        const char *identifier = list[i];
        gnode_t *node = gnode_variable_create(NO_TOKEN, string_dup(identifier), NULL, NULL, LAST_DECLARATION(), NULL);
        gnode_array_push(decls, node);
        ++i;
    }

    // register a variable declaration node in global statements
    gnode_t *node = gnode_variable_decl_create(NO_TOKEN, TOK_KEY_VAR, 0, TOK_KEY_EXTERN, decls, LAST_DECLARATION());;
    gnode_array_push(parser->statements, (gnode_t *)node);
}

static void parser_register_optional_classes (gravity_parser_t *parser) {
    // for each optional identifier create a dummy extern variable node
    gnode_r *decls = gnode_array_create();

    // compile time optional classes
    const char **list = gravity_optional_identifiers();
    uint32_t i = 0;
    while (list[i]) {
        const char *identifier = list[i];
        gnode_t *decl = gnode_variable_create(NO_TOKEN, string_dup(identifier), NULL, NULL, LAST_DECLARATION(), NULL);
        gnode_array_push(decls, decl);
        ++i;
    }

    // check if optional classes callback is registered (runtime optional classes)
    if (parser->delegate && parser->delegate->optional_classes) {
        list = parser->delegate->optional_classes(parser->delegate->xdata);
        i = 0;
        while (list[i]) {
            const char *identifier = list[i];
            gnode_t *decl_node = gnode_variable_create(NO_TOKEN, string_dup(identifier), NULL, NULL, LAST_DECLARATION(), NULL);
            gnode_array_push(decls, decl_node);
            ++i;
        }
    }
    
    // register a variable declaration node in global statements
    gnode_t *node = gnode_variable_decl_create(NO_TOKEN, TOK_KEY_VAR, 0, TOK_KEY_EXTERN, decls, LAST_DECLARATION());;
    gnode_array_push(parser->statements, (gnode_t *)node);
}

static uint32_t parser_run (gravity_parser_t *parser) {
    DEBUG_PARSER("=== BEGIN PARSING ===");

    // register core and optional classes as extern globals
    parser_register_core_classes(parser);
    parser_register_optional_classes(parser);

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
    gnode_t *node= gnode_block_stat_create(NODE_LIST_STAT, NO_TOKEN, parser->statements, LAST_DECLARATION(), 0);
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

    gravity_parser_t *parser = mem_alloc(NULL, sizeof(gravity_parser_t));
    if (!parser) return NULL;

    gravity_lexer_t *lexer = gravity_lexer_create(source, len, fileid, is_static);
    if (!lexer) goto abort_init;

    parser->lexer = mem_alloc(NULL, sizeof(lexer_r));
    marray_init(*parser->lexer);
    marray_push(gravity_lexer_t*, *parser->lexer, lexer);

    parser->statements = gnode_array_create();
    if (!parser->statements) goto abort_init;

    parser->declarations = gnode_array_create();
    if (!parser->declarations) goto abort_init;

    marray_init(parser->vdecl);
    
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
    if (marray_size(*parser->declarations) > 0) return NULL;

    // return ast
    return gnode_block_stat_create(NODE_LIST_STAT, NO_TOKEN, parser->statements, NULL, 0);
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

    // parser->declarations is used to keep track of nodes hierarchy and it contains pointers to
    // parser->statements nodes so there is no need to free it using node_array_free because nodes
    // are freed when AST (parser->statements) is freed
    if (parser->declarations) {
        marray_destroy(*parser->declarations);
        mem_free(parser->declarations);
    }

    // parser->statements is returned from gravity_parser_run
    // and must be deallocated using gnode_free

    marray_destroy(parser->vdecl);
    mem_free(parser);
}
