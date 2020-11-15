//
//  gravity_ast.h
//  gravity
//
//  Created by Marco Bambini on 02/09/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#ifndef __GRAVITY_AST__
#define __GRAVITY_AST__

#include "debug_macros.h"
#include "gravity_array.h"
#include "gravity_token.h"

/*
    AST can be uniform (the same data struct is used for all expressions/statements/declarations) or
    non-uniform. I choosed a non-uniform AST node implementation with a common base struct.
    It requires more work but design and usage is much more cleaner and we benefit from static check.
 */

typedef enum {
    // statements: 7
    NODE_LIST_STAT, NODE_COMPOUND_STAT, NODE_LABEL_STAT, NODE_FLOW_STAT, NODE_JUMP_STAT, NODE_LOOP_STAT, NODE_EMPTY_STAT,

    // declarations: 6
    NODE_ENUM_DECL, NODE_FUNCTION_DECL, NODE_VARIABLE_DECL, NODE_CLASS_DECL, NODE_MODULE_DECL, NODE_VARIABLE,

    // expressions: 8
    NODE_BINARY_EXPR, NODE_UNARY_EXPR, NODE_FILE_EXPR, NODE_LIST_EXPR, NODE_LITERAL_EXPR, NODE_IDENTIFIER_EXPR,
    NODE_POSTFIX_EXPR, NODE_KEYWORD_EXPR,

    // postfix subexpression type
    NODE_CALL_EXPR, NODE_SUBSCRIPT_EXPR, NODE_ACCESS_EXPR
} gnode_n;

typedef enum {
    LOCATION_LOCAL,
    LOCATION_GLOBAL,
    LOCATION_UPVALUE,
    LOCATION_CLASS_IVAR_SAME,
    LOCATION_CLASS_IVAR_OUTER
} gnode_location_type;

// BASE NODE
typedef struct {
    gnode_n     tag;                        // node type from gnode_n enum
    uint32_t    refcount;                   // reference count to manage duplicated nodes
    uint32_t    block_length;               // total length in bytes of the block (used in autocompletion)
    gtoken_s    token;                      // token type and location
    bool        is_assignment;              // flag to check if it is an assignment node
    void        *decl;                      // enclosing declaration node
} gnode_t;

// UPVALUE STRUCT
typedef struct {
    gnode_t     *node;                      // reference to the original var node
    uint32_t    index;                      // can be an index in the stack or in the upvalue list (depending on the is_direct flag)
    uint32_t    selfindex;                  // always index inside uplist
    bool        is_direct;                  // flag to check if var is local to the direct enclosing func
} gupvalue_t;

// shortcut for array of common structs
typedef marray_t(gnode_t *)                 gnode_r;
typedef marray_t(gupvalue_t *)              gupvalue_r;

#ifndef GRAVITY_SYMBOLTABLE_DEFINED
#define GRAVITY_SYMBOLTABLE_DEFINED
typedef struct symboltable_t                symboltable_t;
#endif

// LOCATION
typedef struct {
    gnode_location_type type;               // location type
    uint16_t            index;              // symbol index
    uint16_t            nup;                // upvalue index or outer index
} gnode_location_t;

// STATEMENTS
typedef struct {
    gnode_t             base;               // NODE_LIST_STAT | NODE_COMPOUND_STAT
    symboltable_t       *symtable;          // node internal symbol table
    gnode_r             *stmts;             // array of statements node
    uint32_t            nclose;             // initialized to UINT32_MAX
} gnode_compound_stmt_t;
typedef gnode_compound_stmt_t gnode_list_stmt_t;

typedef struct {
    gnode_t             base;               // CASE or DEFAULT
    gnode_t             *expr;              // expression in case of CASE
    gnode_t             *stmt;              // common statement
    uint32_t            label_case;         // for switch to jump
} gnode_label_stmt_t;

typedef struct {
    gnode_t             base;               // IF, SWITCH, TOK_OP_TERNARY
    gnode_t             *cond;              // common condition (it's an expression)
    gnode_t             *stmt;              // common statement
    gnode_t             *elsestmt;          // optional else statement in case of IF
} gnode_flow_stmt_t;

typedef struct {
    gnode_t             base;               // WHILE, REPEAT or FOR
    gnode_t             *cond;              // used in WHILE and FOR
    gnode_t             *stmt;              // common statement
    gnode_t             *expr;              // used in REPEAT and FOR
    uint32_t            nclose;             // initialized to UINT32_MAX
} gnode_loop_stmt_t;

typedef struct {
    gnode_t             base;               // BREAK, CONTINUE or RETURN
    gnode_t             *expr;              // optional expression in case of RETURN
} gnode_jump_stmt_t;

// DECLARATIONS
typedef struct {
    gnode_t             base;               // FUNCTION_DECL or FUNCTION_EXPR
    gnode_t             *env;               // shortcut to node where function is declared
    gtoken_t            access;             // TOK_KEY_PRIVATE | TOK_KEY_INTERNAL | TOK_KEY_PUBLIC
    gtoken_t            storage;            // TOK_KEY_STATIC | TOK_KEY_EXTERN
    symboltable_t       *symtable;          // function internal symbol table
    const char          *identifier;        // function name
    gnode_r             *params;            // function params
    gnode_compound_stmt_t *block;           // internal function statements
    uint16_t            nlocals;            // locals counter
    uint16_t            nparams;            // formal parameters counter
    bool                has_defaults;       // flag set if parmas has default values
    bool                is_closure;         // flag to check if function is a closure
    gupvalue_r          *uplist;            // list of upvalues used in function (can be empty)
} gnode_function_decl_t;
typedef gnode_function_decl_t gnode_function_expr_t;

typedef struct {
    gnode_t             base;               // VARIABLE_DECL
    gtoken_t            type;               // TOK_KEY_VAR | TOK_KEY_CONST
    gtoken_t            access;             // TOK_KEY_PRIVATE | TOK_KEY_INTERNAL | TOK_KEY_PUBLIC
    gtoken_t            storage;            // TOK_KEY_STATIC | TOK_KEY_EXTERN
    gnode_r             *decls;             // variable declarations list (gnode_var_t)
} gnode_variable_decl_t;

typedef struct {
    gnode_t             base;               // VARIABLE
    gnode_t             *env;               // shortcut to node where variable is declared
    const char          *identifier;        // variable name
    const char          *annotation_type;   // optional annotation type
    gnode_t             *expr;              // optional assignment expression/declaration
    gtoken_t            access;             // optional access token (duplicated value from its gnode_variable_decl_t)
    uint16_t            index;              // local variable index (if local)
    bool                upvalue;            // flag set if this variable is used as an upvalue
    bool                iscomputed;         // flag set is variable must not be backed
    gnode_variable_decl_t   *vdecl;         // reference to enclosing variable declaration (in order to be able to have access to storage and access fields)
} gnode_var_t;

typedef struct {
    gnode_t             base;               // ENUM_DECL
    gnode_t             *env;               // shortcut to node where enum is declared
    gtoken_t            access;             // TOK_KEY_PRIVATE | TOK_KEY_INTERNAL | TOK_KEY_PUBLIC
    gtoken_t            storage;            // TOK_KEY_STATIC | TOK_KEY_EXTERN
    symboltable_t       *symtable;          // enum internal hash table
    const char          *identifier;        // enum name
} gnode_enum_decl_t;

typedef struct {
    gnode_t             base;               // CLASS_DECL
    bool                bridge;             // flag to check of a bridged class
    bool                is_struct;          // flag to mark the class as a struct
    gnode_t             *env;               // shortcut to node where class is declared
    gtoken_t            access;             // TOK_KEY_PRIVATE | TOK_KEY_INTERNAL | TOK_KEY_PUBLIC
    gtoken_t            storage;            // TOK_KEY_STATIC | TOK_KEY_EXTERN
    const char          *identifier;        // class name
    gnode_t             *superclass;        // super class ptr
    bool                super_extern;       // flag set when a superclass is declared as extern
    gnode_r             *protocols;         // array of protocols (currently unused)
    gnode_r             *decls;             // class declarations list
    symboltable_t       *symtable;          // class internal symbol table
    void                *data;              // used to keep track of super classes
    uint32_t            nivar;              // instance variables counter
    uint32_t            nsvar;              // static variables counter
} gnode_class_decl_t;

typedef struct {
    gnode_t             base;               // MODULE_DECL
    gnode_t             *env;               // shortcut to node where module is declared
    gtoken_t            access;             // TOK_KEY_PRIVATE | TOK_KEY_INTERNAL | TOK_KEY_PUBLIC
    gtoken_t            storage;            // TOK_KEY_STATIC | TOK_KEY_EXTERN
    const char          *identifier;        // module name
    gnode_r             *decls;             // module declarations list
    symboltable_t       *symtable;          // module internal symbol table
} gnode_module_decl_t;

// EXPRESSIONS
typedef struct {
    gnode_t             base;               // BINARY_EXPR
    gtoken_t            op;                 // operation
    gnode_t             *left;              // left node
    gnode_t             *right;             // right node
} gnode_binary_expr_t;

typedef struct {
    gnode_t             base;               // UNARY_EXPR
    gtoken_t            op;                 // operation
    gnode_t             *expr;              // node
} gnode_unary_expr_t;

typedef struct {
    gnode_t             base;               // FILE
    cstring_r           *identifiers;       // identifier name
    gnode_location_t    location;           // identifier location
} gnode_file_expr_t;

typedef struct {
    gnode_t             base;               // LITERAL
    gliteral_t          type;               // LITERAL_STRING, LITERAL_FLOAT, LITERAL_INT, LITERAL_BOOL, LITERAL_INTERPOLATION
    uint32_t            len;                // used only for TYPE_STRING
    union {
        char            *str;               // LITERAL_STRING
        double          d;                  // LITERAL_FLOAT
        int64_t         n64;                // LITERAL_INT or LITERAL_BOOL
        gnode_r         *r;                 // LITERAL_STRING_INTERPOLATED
    } value;
} gnode_literal_expr_t;

typedef struct {
    gnode_t             base;               // IDENTIFIER or ID
    const char          *value;             // identifier name
    const char          *value2;            // NULL for IDENTIFIER (check if just one value or an array)
    gnode_t             *symbol;            // pointer to identifier declaration (if any)
    gnode_location_t    location;           // location coordinates
    gupvalue_t          *upvalue;           // upvalue location reference
} gnode_identifier_expr_t;

typedef struct {
    gnode_t             base;               // KEYWORD token
} gnode_keyword_expr_t;

typedef gnode_keyword_expr_t gnode_empty_stmt_t;
typedef gnode_keyword_expr_t gnode_base_t;

typedef struct {
    gnode_t             base;               // NODE_CALLFUNC_EXPR, NODE_SUBSCRIPT_EXPR, NODE_ACCESS_EXPR
    gnode_t             *id;                // id(...) or id[...] or id.
    gnode_r             *list;              // list of postfix_subexpr
} gnode_postfix_expr_t;

typedef struct {
    gnode_t             base;               // NODE_CALLFUNC_EXPR, NODE_SUBSCRIPT_EXPR, NODE_ACCESS_EXPR
    union {
        gnode_t         *expr;              // used in case of NODE_SUBSCRIPT_EXPR or NODE_ACCESS_EXPR
        gnode_r         *args;              // used in case of NODE_CALLFUNC_EXPR
    };
} gnode_postfix_subexpr_t;

typedef struct {
    gnode_t             base;               // LIST_EXPR
    bool                ismap;              // flag to check if the node represents a map (otherwise it is a list)
    gnode_r             *list1;             // node items (cannot use a symtable here because order is mandatory in array)
    gnode_r             *list2;             // used only in case of map
} gnode_list_expr_t;

gnode_t *gnode_jump_stat_create (gtoken_s token, gnode_t *expr, gnode_t *decl);
gnode_t *gnode_label_stat_create (gtoken_s token, gnode_t *expr, gnode_t *stmt, gnode_t *decl);
gnode_t *gnode_flow_stat_create (gtoken_s token, gnode_t *cond, gnode_t *stmt1, gnode_t *stmt2, gnode_t *decl, uint32_t block_length);
gnode_t *gnode_loop_stat_create (gtoken_s token, gnode_t *cond, gnode_t *stmt, gnode_t *expr, gnode_t *decl, uint32_t block_length);
gnode_t *gnode_block_stat_create (gnode_n type, gtoken_s token, gnode_r *stmts, gnode_t *decl, uint32_t block_length);
gnode_t *gnode_empty_stat_create (gtoken_s token, gnode_t *decl);

gnode_t *gnode_enum_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, symboltable_t *symtable, gnode_t *decl);
gnode_t *gnode_class_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_t *superclass, gnode_r *protocols, gnode_r *declarations, bool is_struct, gnode_t *decl);
gnode_t *gnode_module_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *declarations, gnode_t *decl);
gnode_t *gnode_variable_decl_create (gtoken_s token, gtoken_t type, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *declarations, gnode_t *decl);
gnode_t *gnode_variable_create (gtoken_s token, const char *identifier, const char *annotation_type, gnode_t *expr, gnode_t *decl, gnode_variable_decl_t *vdecl);

gnode_t *gnode_function_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *params, gnode_compound_stmt_t *block, gnode_t *decl);

gnode_t *gnode_binary_expr_create (gtoken_t op, gnode_t *left, gnode_t *right, gnode_t *decl);
gnode_t *gnode_unary_expr_create (gtoken_t op, gnode_t *expr, gnode_t *decl);
gnode_t *gnode_file_expr_create (gtoken_s token, cstring_r *list, gnode_t *decl);
gnode_t *gnode_identifier_expr_create (gtoken_s token, const char *identifier, const char *identifier2, gnode_t *decl);
gnode_t *gnode_string_interpolation_create (gtoken_s token, gnode_r *r, gnode_t *decl);
gnode_t *gnode_literal_string_expr_create (gtoken_s token, char *s, uint32_t len, bool allocated, gnode_t *decl);
gnode_t *gnode_literal_float_expr_create (gtoken_s token, double f, gnode_t *decl);
gnode_t *gnode_literal_int_expr_create (gtoken_s token, int64_t n, gnode_t *decl);
gnode_t *gnode_literal_bool_expr_create (gtoken_s token, int32_t n, gnode_t *decl);
gnode_t *gnode_keyword_expr_create (gtoken_s token, gnode_t *decl);
gnode_t *gnode_postfix_subexpr_create (gtoken_s token, gnode_n type, gnode_t *expr, gnode_r *list, gnode_t *decl);
gnode_t *gnode_postfix_expr_create (gtoken_s token, gnode_t *id, gnode_r *list, gnode_t *decl);
gnode_t *gnode_list_expr_create (gtoken_s token, gnode_r *list1, gnode_r *list2, bool ismap, gnode_t *decl);

gnode_t *gnode_duplicate (gnode_t *node, bool deep);
gnode_r *gnode_array_create (void);
gnode_r *gnode_array_remove_byindex(gnode_r *list, size_t index);
gupvalue_t *gnode_function_add_upvalue(gnode_function_decl_t *f, gnode_var_t *symbol, uint16_t n);
cstring_r  *cstring_array_create (void);
void_r  *void_array_create (void);
void    gnode_array_sethead(gnode_r *list, gnode_t *node);
gnode_t *gnode2class (gnode_t *node, bool *isextern);

bool    gnode_is_equal (gnode_t *node1, gnode_t *node2);
bool    gnode_is_expression (gnode_t *node);
bool    gnode_is_literal (gnode_t *node);
bool    gnode_is_literal_int (gnode_t *node);
bool    gnode_is_literal_number (gnode_t *node);
bool    gnode_is_literal_string (gnode_t *node);
void    gnode_literal_dump (gnode_literal_expr_t *node, char *buffer, int buffersize);
void    gnode_free (gnode_t *node);

// MARK: -

#define gnode_array_init(r)                 marray_init(*r)
#define gnode_array_size(r)                 ((r) ? marray_size(*r) : 0)
#define gnode_array_push(r, node)           marray_push(gnode_t*,*r,node)
#define gnode_array_pop(r)                  (marray_size(*r) ? marray_pop(*r) : NULL)
#define gnode_array_get(r, i)               (((i) >= 0 && (i) < marray_size(*r)) ? marray_get(*r, (i)) : NULL)
#define gnode_array_free(r)                 do {marray_destroy(*r); mem_free((void*)r);} while (0)
#define gtype_array_each(r, block, type)    {   size_t _len = gnode_array_size(r);                \
                                                for (size_t _i=0; _i<_len; ++_i) {                \
                                                    type val = (type)gnode_array_get(r, _i);      \
                                                    block;} \
                                            }
#define gnode_array_each(r, block)          gtype_array_each(r, block, gnode_t*)
#define gnode_array_eachbase(r, block)      gtype_array_each(r, block, gnode_base_t*)

#define cstring_array_free(r)               marray_destroy(*r)
#define cstring_array_push(r, s)            marray_push(const char*,*r,s)
#define cstring_array_each(r, block)        gtype_array_each(r, block, const char*)

#define NODE_TOKEN_TYPE(_node)              _node->base.token.type
#define NODE_TAG(_node)                     ((gnode_base_t *)_node)->base.tag
#define NODE_ISA(_node,_tag)                ((_node) && NODE_TAG(_node) == _tag)
#define NODE_ISA_FUNCTION(_node)            (NODE_ISA(_node, NODE_FUNCTION_DECL))
#define NODE_ISA_CLASS(_node)               (NODE_ISA(_node, NODE_CLASS_DECL))

#define NODE_SET_ENCLOSING(_node,_enc)      (((gnode_base_t *)_node)->base.enclosing = _enc)
#define NODE_GET_ENCLOSING(_node)           ((gnode_base_t *)_node)->base.enclosing

#endif
