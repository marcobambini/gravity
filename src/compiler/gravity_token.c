//
//  gravity_token.c
//  gravity
//
//  Created by Marco Bambini on 31/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

#include "gravity_token.h"
#include "gravity_utils.h"

const char *token_string (gtoken_s token, uint32_t *len) {
    if (len) *len = token.bytes;
    return token.value;
}

const char *token_name (gtoken_t token) {
    switch (token) {
        case TOK_EOF: return "EOF";
        case TOK_ERROR: return "ERROR";
        case TOK_COMMENT: return "COMMENT";
        case TOK_STRING: return "STRING";
        case TOK_NUMBER: return "NUMBER";
        case TOK_IDENTIFIER: return "IDENTIFIER";
        case TOK_SPECIAL: return "SPECIAL";
        case TOK_MACRO: return "MACRO";

        // keywords
        case TOK_KEY_FILE: return "file";
        case TOK_KEY_FUNC: return "func";
        case TOK_KEY_SUPER: return "super";
        case TOK_KEY_DEFAULT: return "default";
        case TOK_KEY_TRUE: return "true";
        case TOK_KEY_FALSE: return "false";
        case TOK_KEY_IF: return "if";
        case TOK_KEY_ELSE: return "else";
        case TOK_KEY_SWITCH: return "switch";
        case TOK_KEY_BREAK: return "break";
        case TOK_KEY_CONTINUE: return "continue";
        case TOK_KEY_RETURN: return "return";
        case TOK_KEY_WHILE: return "while";
        case TOK_KEY_REPEAT: return "repeat";
        case TOK_KEY_FOR: return "for";
        case TOK_KEY_IN: return "in";
        case TOK_KEY_ENUM: return "enum";
        case TOK_KEY_CLASS: return "class";
        case TOK_KEY_STRUCT: return "struct";
        case TOK_KEY_PRIVATE: return "private";
        case TOK_KEY_INTERNAL: return "internal";
        case TOK_KEY_PUBLIC: return "public";
        case TOK_KEY_STATIC: return "static";
        case TOK_KEY_EXTERN: return "extern";
        case TOK_KEY_LAZY: return "lazy";
        case TOK_KEY_CONST: return "const";
        case TOK_KEY_VAR: return "var";
        case TOK_KEY_MODULE: return "module";
        case TOK_KEY_IMPORT: return "import";
        case TOK_KEY_CASE: return "case";
        case TOK_KEY_EVENT: return "event";
        case TOK_KEY_NULL: return "null";
        case TOK_KEY_UNDEFINED: return "undefined";
        case TOK_KEY_ISA: return "is";
        case TOK_KEY_CURRARGS: return "_args";
        case TOK_KEY_CURRFUNC: return "_func";

        // operators
        case TOK_OP_ADD: return "+";
        case TOK_OP_SUB: return "-";
        case TOK_OP_DIV: return "/";
        case TOK_OP_MUL: return "*";
        case TOK_OP_REM: return "%";
        case TOK_OP_ASSIGN: return "=";
        case TOK_OP_LESS: return "<";
        case TOK_OP_GREATER: return ">";
        case TOK_OP_LESS_EQUAL: return "<=";
        case TOK_OP_GREATER_EQUAL: return ">=";
        case TOK_OP_ADD_ASSIGN: return "+=";
        case TOK_OP_SUB_ASSIGN: return "-=";
        case TOK_OP_DIV_ASSIGN: return "/=";
        case TOK_OP_MUL_ASSIGN: return "*=";
        case TOK_OP_REM_ASSIGN: return "%=";
        case TOK_OP_NOT: return "!";
        case TOK_OP_AND: return "&&";
        case TOK_OP_OR: return "||";
        case TOK_OP_ISEQUAL: return "==";
        case TOK_OP_ISNOTEQUAL: return "!=";
        case TOK_OP_RANGE_INCLUDED: return "...";
        case TOK_OP_RANGE_EXCLUDED: return "..<";
        case TOK_OP_TERNARY: return "?";
        case TOK_OP_SHIFT_LEFT: return "<<";
        case TOK_OP_SHIFT_RIGHT: return ">>";
        case TOK_OP_BIT_AND: return "&";
        case TOK_OP_BIT_OR: return "|";
        case TOK_OP_BIT_XOR: return "^";
        case TOK_OP_BIT_NOT: return "~";
        case TOK_OP_ISIDENTICAL: return "===";
        case TOK_OP_ISNOTIDENTICAL: return "!==";
        case TOK_OP_PATTERN_MATCH: return "~=";
        case TOK_OP_SHIFT_LEFT_ASSIGN: return "<<=";
        case TOK_OP_SHIFT_RIGHT_ASSIGN: return ">>=";
        case TOK_OP_BIT_AND_ASSIGN: return "&=";
        case TOK_OP_BIT_OR_ASSIGN: return "|=";
        case TOK_OP_BIT_XOR_ASSIGN: return "^=";

        case TOK_OP_OPEN_PARENTHESIS: return "(";
        case TOK_OP_CLOSED_PARENTHESIS: return ")";
        case TOK_OP_OPEN_SQUAREBRACKET: return "[";
        case TOK_OP_CLOSED_SQUAREBRACKET: return "]";
        case TOK_OP_OPEN_CURLYBRACE: return "{";
        case TOK_OP_CLOSED_CURLYBRACE: return "}";
        case TOK_OP_SEMICOLON: return ";";
        case TOK_OP_COLON: return ":";
        case TOK_OP_COMMA: return ",";
        case TOK_OP_DOT: return ".";

        case TOK_END: return "";
    }

    // should never reach this point
    return "UNRECOGNIZED TOKEN";
}

void token_keywords_indexes (uint32_t *idx_start, uint32_t *idx_end) {
    *idx_start = (uint32_t)TOK_KEY_FUNC;
    *idx_end = (uint32_t)TOK_KEY_CURRARGS;
};

gtoken_t token_special_builtin(gtoken_s *token) {
    const char *buffer = token->value;
    int32_t len = token->bytes;

    switch (len) {
        case 8:
            if (string_casencmp(buffer, "__LINE__", len) == 0) {
                token->builtin = BUILTIN_LINE;
                return TOK_NUMBER;
            }
            if (string_casencmp(buffer, "__FILE__", len) == 0) {
                token->builtin = BUILTIN_FILE;
                return TOK_STRING;
            }
            break;

        case 9:
            if (string_casencmp(buffer, "__CLASS__", len) == 0) {
                token->builtin = BUILTIN_CLASS;
                return TOK_STRING;
            }
            break;

        case 10:
            if (string_casencmp(buffer, "__COLUMN__", len) == 0) {
                token->builtin = BUILTIN_COLUMN;
                return TOK_NUMBER;
            }
            break;

        case 12:
            if (string_casencmp(buffer, "__FUNCTION__", len) == 0) {
                token->builtin = BUILTIN_FUNC;
                return TOK_STRING;
            }
            break;
    }

    return TOK_IDENTIFIER;
}

gtoken_t token_keyword (const char *buffer, int32_t len) {
    switch (len) {
        case 2:
            if (string_casencmp(buffer, "if", len) == 0) return TOK_KEY_IF;
            if (string_casencmp(buffer, "in", len) == 0) return TOK_KEY_IN;
            if (string_casencmp(buffer, "or", len) == 0) return TOK_OP_OR;
            if (string_casencmp(buffer, "is", len) == 0) return TOK_KEY_ISA;
            break;

        case 3:
            if (string_casencmp(buffer, "for", len) == 0) return TOK_KEY_FOR;
            if (string_casencmp(buffer, "var", len) == 0) return TOK_KEY_VAR;
            if (string_casencmp(buffer, "and", len) == 0) return TOK_OP_AND;
            if (string_casencmp(buffer, "not", len) == 0) return TOK_OP_NOT;
            break;

        case 4:
            if (string_casencmp(buffer, "func", len) == 0) return TOK_KEY_FUNC;
            if (string_casencmp(buffer, "else", len) == 0) return TOK_KEY_ELSE;
            if (string_casencmp(buffer, "true", len) == 0) return TOK_KEY_TRUE;
            if (string_casencmp(buffer, "enum", len) == 0) return TOK_KEY_ENUM;
            if (string_casencmp(buffer, "case", len) == 0) return TOK_KEY_CASE;
            if (string_casencmp(buffer, "null", len) == 0) return TOK_KEY_NULL;
            if (string_casencmp(buffer, "NULL", len) == 0) return TOK_KEY_NULL;
            if (string_casencmp(buffer, "file", len) == 0) return TOK_KEY_FILE;
            if (string_casencmp(buffer, "lazy", len) == 0) return TOK_KEY_LAZY;
            break;

        case 5:
            if (string_casencmp(buffer, "super", len) == 0) return TOK_KEY_SUPER;
            if (string_casencmp(buffer, "false", len) == 0) return TOK_KEY_FALSE;
            if (string_casencmp(buffer, "break", len) == 0) return TOK_KEY_BREAK;
            if (string_casencmp(buffer, "while", len) == 0) return TOK_KEY_WHILE;
            if (string_casencmp(buffer, "class", len) == 0) return TOK_KEY_CLASS;
            if (string_casencmp(buffer, "const", len) == 0) return TOK_KEY_CONST;
            if (string_casencmp(buffer, "event", len) == 0) return TOK_KEY_EVENT;
            if (string_casencmp(buffer, "_func", len) == 0) return TOK_KEY_CURRFUNC;
            if (string_casencmp(buffer, "_args", len) == 0) return TOK_KEY_CURRARGS;
            break;

        case 6:
            if (string_casencmp(buffer, "struct", len) == 0) return TOK_KEY_STRUCT;
            if (string_casencmp(buffer, "repeat", len) == 0) return TOK_KEY_REPEAT;
            if (string_casencmp(buffer, "switch", len) == 0) return TOK_KEY_SWITCH;
            if (string_casencmp(buffer, "return", len) == 0) return TOK_KEY_RETURN;
            if (string_casencmp(buffer, "public", len) == 0) return TOK_KEY_PUBLIC;
            if (string_casencmp(buffer, "static", len) == 0) return TOK_KEY_STATIC;
            if (string_casencmp(buffer, "extern", len) == 0) return TOK_KEY_EXTERN;
            if (string_casencmp(buffer, "import", len) == 0) return TOK_KEY_IMPORT;
            if (string_casencmp(buffer, "module", len) == 0) return TOK_KEY_MODULE;
            break;

        case 7:
            if (string_casencmp(buffer, "default", len) == 0) return TOK_KEY_DEFAULT;
            if (string_casencmp(buffer, "private", len) == 0) return TOK_KEY_PRIVATE;
            break;

        case 8:
            if (string_casencmp(buffer, "continue", len) == 0) return TOK_KEY_CONTINUE;
            if (string_casencmp(buffer, "internal", len) == 0) return TOK_KEY_INTERNAL;
            break;

        case 9:
            if (string_casencmp(buffer, "undefined", len) == 0) return TOK_KEY_UNDEFINED;
            break;
    }

    return TOK_IDENTIFIER;
}

const char *token_literal_name (gliteral_t value) {
    if (value == LITERAL_STRING) return "STRING";
    else if (value == LITERAL_FLOAT) return "FLOAT";
    else if (value == LITERAL_INT) return "INTEGER";
    else if (value == LITERAL_BOOL) return "BOOLEAN";
    else if (value == LITERAL_STRING_INTERPOLATED) return "STRING INTERPOLATED";
    return "N/A";
}

// MARK: -

bool token_isidentifier (gtoken_t token) {
    return (token == TOK_IDENTIFIER);
}

bool token_isvariable_declaration (gtoken_t token) {
    return ((token == TOK_KEY_CONST) || (token == TOK_KEY_VAR));
}

bool token_isstatement (gtoken_t token) {
    if (token == TOK_EOF) return false;

    // label_statement (case, default)
    // expression_statement ('+' | '-' | '!' | 'not' | new | raise | file | isPrimaryExpression)
    // flow_statement (if, select)
    // loop_statement (while, loop, for)
    // jump_statement (break, continue, return)
    // compound_statement ({)
    // declaration_statement (isDeclarationStatement)
    // empty_statement (;)
    // import_statement (import)

    return (token_islabel_statement(token) || token_isexpression_statement(token) || token_isflow_statement(token) ||
            token_isloop_statement(token) || token_isjump_statement(token) || token_iscompound_statement(token) ||
            token_isdeclaration_statement(token) || token_isempty_statement(token) || token_isimport_statement(token) ||
            token_ismacro(token));
}

bool token_isassignment (gtoken_t token) {
    return ((token == TOK_OP_ASSIGN) || (token == TOK_OP_MUL_ASSIGN) || (token == TOK_OP_DIV_ASSIGN) ||
            (token == TOK_OP_REM_ASSIGN) || (token == TOK_OP_ADD_ASSIGN) || (token == TOK_OP_SUB_ASSIGN) ||
            (token == TOK_OP_SHIFT_LEFT_ASSIGN) || (token == TOK_OP_SHIFT_RIGHT_ASSIGN) ||
            (token == TOK_OP_BIT_AND_ASSIGN) || (token == TOK_OP_BIT_OR_ASSIGN) || (token == TOK_OP_BIT_XOR_ASSIGN));
}

bool token_isvariable_assignment (gtoken_t token) {
    return (token == TOK_OP_ASSIGN);
}

bool token_isaccess_specifier (gtoken_t token) {
    return ((token == TOK_KEY_PRIVATE) || (token == TOK_KEY_INTERNAL) || (token == TOK_KEY_PUBLIC));
}

bool token_isstorage_specifier (gtoken_t token) {
    return ((token == TOK_KEY_STATIC) || (token == TOK_KEY_EXTERN) || (token == TOK_KEY_LAZY));
}

bool token_isprimary_expression (gtoken_t token) {
    // literal (number, string)
    // true, false
    // IDENTIFIER
    // 'nil'
    // 'super'
    // 'func'
    // 'undefined'
    // 'file'
    // '(' expression ')'
    // function_expression
    // list_expression
    // map_expression

    return ((token == TOK_NUMBER) || (token == TOK_STRING) || (token == TOK_KEY_TRUE) ||
            (token == TOK_KEY_FALSE) || (token == TOK_IDENTIFIER) || (token == TOK_KEY_NULL) ||
            (token == TOK_KEY_SUPER) || (token == TOK_KEY_FUNC) || (token == TOK_KEY_UNDEFINED) ||
            (token == TOK_OP_OPEN_PARENTHESIS) || (token == TOK_OP_OPEN_SQUAREBRACKET) ||
            (token == TOK_OP_OPEN_CURLYBRACE) || (token == TOK_KEY_FILE));

}

bool token_isexpression_statement (gtoken_t token) {
    // reduced to check for unary_expression
    // postfix_expression: primary_expression | 'module' (was file)
    // unary_operator: '+' | '-' | '!' | 'not'
    // raise_expression: 'raise'

    return (token_isprimary_expression(token) || (token == TOK_OP_ADD) || (token == TOK_OP_SUB) ||
            (token == TOK_OP_NOT) || (token == TOK_KEY_CURRARGS) || (token == TOK_KEY_CURRFUNC));
}

bool token_islabel_statement (gtoken_t token) {
    return ((token == TOK_KEY_CASE) || (token == TOK_KEY_DEFAULT));
}

bool token_isflow_statement (gtoken_t token) {
    return ((token == TOK_KEY_IF) || (token == TOK_KEY_SWITCH));
}

bool token_isloop_statement (gtoken_t token) {
    return ((token == TOK_KEY_WHILE) || (token == TOK_KEY_REPEAT)  || (token == TOK_KEY_FOR));
}

bool token_isjump_statement (gtoken_t token) {
    return ((token == TOK_KEY_BREAK) || (token == TOK_KEY_CONTINUE) || (token == TOK_KEY_RETURN));
}

bool token_iscompound_statement (gtoken_t token) {
    return (token == TOK_OP_OPEN_CURLYBRACE);
}

bool token_isdeclaration_statement (gtoken_t token) {
    // variable_declaration_statement (CONST, VAR)
    // function_declaration (FUNC)
    // class_declaration (CLASS | STRUCT)
    // enum_declaration (ENUM)
    // module_declaration (MODULE)
    // event_declaration_statement (EVENT)
    // empty_declaration (;)

    return ((token_isaccess_specifier(token) || token_isstorage_specifier(token) || token_isvariable_declaration(token) ||
            (token == TOK_KEY_FUNC)    || (token == TOK_KEY_CLASS) || (token == TOK_KEY_STRUCT) || (token == TOK_KEY_ENUM) ||
            (token == TOK_KEY_MODULE) || (token == TOK_KEY_EVENT)  || (token == TOK_OP_SEMICOLON)));
}

bool token_isempty_statement (gtoken_t token) {
    return (token == TOK_OP_SEMICOLON);
}

bool token_isimport_statement (gtoken_t token) {
    return (token == TOK_KEY_IMPORT);
}

bool token_isspecial_statement (gtoken_t token) {
    return (token == TOK_SPECIAL);
}

bool token_isoperator (gtoken_t token) {
    return ((token >= TOK_OP_SHIFT_LEFT) && (token <= TOK_OP_NOT));
}

bool token_ismacro (gtoken_t token) {
    return (token == TOK_MACRO);
}

bool token_iserror (gtoken_t token) {
    return (token == TOK_ERROR);
}

bool token_iseof (gtoken_t token) {
    return (token == TOK_EOF);
}
