//
//  gravity_lexer.c
//  gravity
//
//  Created by Marco Bambini on 30/08/14.
//  Copyright (c) 2014 CreoLabs. All rights reserved.
//

// ASCII Table: http://www.theasciicode.com.ar

#include "gravity_lexer.h"
#include "gravity_token.h"
#include "gravity_utils.h"

struct gravity_lexer_t {
    const char                  *buffer;        // buffer
    uint32_t                    offset;         // current buffer offset (in bytes)
    uint32_t                    position;       // current buffer position (in characters)
    uint32_t                    length;         // buffer length (in bytes)
    uint32_t                    lineno;         // line counter
    uint32_t                    colno;          // column counter
    uint32_t                    fileid;         // current file id

    gtoken_s                    token;          // current token
    bool                        peeking;        // flag to check if a peek operation is in progress
    bool                        is_static;      // flag to check if buffer is static and must not be freed
    gravity_delegate_t          *delegate;      // delegate (if any)
    
    gtoken_t                    cache;
};

typedef enum {
    NUMBER_INTEGER,
    NUMBER_HEX,
    NUMBER_BIN,
    NUMBER_OCT
} gravity_number_type;


// LEXER macros
#define NEXT                    lexer->buffer[lexer->offset++]; ++lexer->position; INC_COL
#define PEEK_CURRENT            ((int)lexer->buffer[lexer->offset])
#define PEEK_NEXT               ((lexer->offset < lexer->length) ? lexer->buffer[lexer->offset+1] : 0)
#define PEEK_NEXT2              ((lexer->offset+1 < lexer->length) ? lexer->buffer[lexer->offset+2] : 0)
#define INC_LINE                ++lexer->lineno; RESET_COL
#define INC_COL                 ++lexer->colno
#define DEC_COL                 --lexer->colno
#define RESET_COL               lexer->colno = 1
#define IS_EOF                  (lexer->offset >= lexer->length)
#define DEC_OFFSET              --lexer->offset; DEC_COL
#define DEC_POSITION            --lexer->position
#define DEC_OFFSET_POSITION     DEC_OFFSET; DEC_POSITION
#define INC_OFFSET              ++lexer->offset; INC_COL
#define INC_POSITION            ++lexer->position
#define INC_OFFSET_POSITION     INC_OFFSET; INC_POSITION

// TOKEN macros
#define TOKEN_RESET             lexer->token = NO_TOKEN; lexer->token.position = lexer->position; lexer->token.value = lexer->buffer + lexer->offset;    \
                                lexer->token.lineno = lexer->lineno; lexer->token.colno = lexer->colno; lexer->token.builtin = 0
#define TOKEN_FINALIZE(t)       lexer->token.type = t; lexer->token.fileid = lexer->fileid
#define INC_TOKBYTES            ++lexer->token.bytes
#define INC_TOKUTF8LEN          ++lexer->token.length
#define INC_TOKLEN              INC_TOKBYTES; INC_TOKUTF8LEN
#define DEC_TOKLEN              --lexer->token.bytes; --lexer->token.length
#define SET_TOKTYPE(t)          lexer->token.type = t

#define LEXER_CALL_CALLBACK()   if ((!lexer->peeking) && (lexer->delegate) && (lexer->delegate->parser_callback)) {    \
                                lexer->delegate->parser_callback(&lexer->token, lexer->delegate->xdata); }

// MARK: -

static inline bool is_whitespace (int c) {
    return ((c == ' ') || (c == '\t') || (c == '\v') || (c == '\f'));
}

static inline bool is_newline (gravity_lexer_t *lexer, int c) {
    // CR: Carriage Return, U+000D (UTF-8 in hex: 0D)
    // LF: Line Feed, U+000A (UTF-8 in hex: 0A)
    // CR+LF: CR (U+000D) followed by LF (U+000A) (UTF-8 in hex: 0D0A)

    // LF
    if (c == 0x0A) return true;

    // CR+LF or CR
    if (c == 0x0D) {
        if (PEEK_CURRENT == 0x0A) {NEXT;}
        return true;
    }

    // UTF-8 cases https://en.wikipedia.org/wiki/Newline#Unicode

    // NEL: Next Line, U+0085 (UTF-8 in hex: C285)
    if ((c == 0xC2) && (PEEK_CURRENT == 0x85)) {
        NEXT;
        return true;
    }

    // LS: Line Separator, U+2028 (UTF-8 in hex: E280A8)
    if ((c == 0xE2) && (PEEK_CURRENT == 0x80) && (PEEK_NEXT == 0xA8)) {
        NEXT; NEXT;
        return true;
    }

    // and probably more not handled here
    return false;
}

static inline bool is_comment (int c1, int c2) {
    return (c1 == '/') && ((c2 == '*') || (c2 == '/'));
}

static inline bool is_semicolon (int c) {
    return (c == ';');
}

static inline bool is_alpha (int c) {
    if (c == '_') return true;
    return isalpha(c);
}

static inline bool is_digit (int c, gravity_number_type ntype) {
    if (ntype == NUMBER_BIN) return (c == '0' || (c == '1'));
    if (ntype == NUMBER_OCT) return (c >= '0' && (c <= '7'));
    if ((ntype == NUMBER_HEX) && ((toupper(c) >= 'A' && toupper(c) <= 'F'))) return true;
    return isdigit(c);
}

static inline bool is_string (int c) {
    return ((c == '"') || (c == '\''));
}

static inline bool is_special (int c) {
    return (c == '@');
}

static inline bool is_builtin_operator (int c) {
    // PARENTHESIS
    // { } [ ] ( )
    // PUNCTUATION
    // . ; : ? ,
    // OPERATORS
    // + - * / < > ! = | & ^ % ~

    return ((c == '+') || (c == '-') || (c == '*') || (c == '/') ||
            (c == '<') || (c == '>') || (c == '!') || (c == '=') ||
            (c == '|') || (c == '&') || (c == '^') || (c == '%') ||
            (c == '~') || (c == '.') || (c == ';') || (c == ':') ||
            (c == '?') || (c == ',') || (c == '{') || (c == '}') ||
            (c == '[') || (c == ']') || (c == '(') || (c == ')') );
}

static inline bool is_preprocessor (int c) {
    return (c == '#');
}

static inline bool is_identifier (int c) {
    // when called I am already sure first character is alpha so next valid characters are alpha, digit and _
    return ((isalpha(c)) || (isdigit(c)) || (c == '_'));
}

// MARK: -

static gtoken_t lexer_error(gravity_lexer_t *lexer, const char *message) {
    if (!IS_EOF) {
        INC_TOKLEN;
        INC_OFFSET_POSITION;
    }
    TOKEN_FINALIZE(TOK_ERROR);

    lexer->token.value = (char *)message;
    lexer->token.bytes = (uint32_t)strlen(message);
    return TOK_ERROR;
}

static inline bool next_utf8(gravity_lexer_t *lexer, int *result) {
    int c = NEXT;
    INC_TOKLEN;

    // the following explicit conversion fixes an issue with big-endian processor (like Motorola 68000, PowerPC, Sun Sparc and IBM S/390)
    // was uint32_t len = utf8_charbytes((const char *)&c, 0);
    const char s = c;
    uint32_t len = utf8_charbytes((const char *)&s, 0);
    if (len == 0) return false;

    switch(len) {
        case 1: break;
        case 2: INC_OFFSET; INC_TOKBYTES; break;
        case 3: INC_OFFSET; INC_OFFSET; INC_TOKBYTES; INC_TOKBYTES; break;
        case 4: INC_OFFSET; INC_OFFSET; INC_OFFSET; INC_TOKBYTES; INC_TOKBYTES; INC_TOKBYTES; INC_POSITION; INC_TOKUTF8LEN; break;
    }

    if (result) *result = c;
    return true;
}

static gtoken_t lexer_scan_comment(gravity_lexer_t *lexer) {
    bool isLineComment = (PEEK_NEXT == '/');

    TOKEN_RESET;
    INC_OFFSET_POSITION;
    INC_OFFSET_POSITION;

    // because I already scanned /* or //
    lexer->token.bytes = lexer->token.length = 2;

    // count variable used only to support nested comments
    int count = 1;
    while (!IS_EOF) {
        int c = 0;
        next_utf8(lexer, &c);

        if (isLineComment){
            if (is_newline(lexer, c)) {INC_LINE; break;}
        } else {
            if (IS_EOF) break;
            int c2 = PEEK_CURRENT;
            if ((c == '/') && (c2 == '*')) ++count;
            if ((c == '*') && (c2 == '/')) {--count; NEXT; INC_TOKLEN; if (count == 0) break;}
            if (is_newline(lexer, c)) {INC_LINE;}
        }
    }

    // comment is from buffer->[nseek] and it is nlen length
    TOKEN_FINALIZE(TOK_COMMENT);

    // comments callback is called directly from the scan function and not from the main scan loop
    if ((lexer->delegate) && (lexer->delegate->parser_callback)) {
        lexer->delegate->parser_callback(&lexer->token, lexer->delegate->xdata);
    }

    DEBUG_LEXEM("Found comment");
    return TOK_COMMENT;
}

static gtoken_t lexer_scan_semicolon(gravity_lexer_t *lexer) {
    TOKEN_RESET;
    INC_TOKLEN;
    INC_OFFSET_POSITION;
    TOKEN_FINALIZE(TOK_OP_SEMICOLON);

    return TOK_OP_SEMICOLON;
}

static gtoken_t lexer_scan_identifier(gravity_lexer_t *lexer) {
    TOKEN_RESET;
    while (is_identifier(PEEK_CURRENT)) {
        INC_OFFSET_POSITION;
        INC_TOKLEN;
    }
    TOKEN_FINALIZE(TOK_IDENTIFIER);

    // check if identifier is a special built-in case
    gtoken_t type = token_special_builtin(&lexer->token);
    // then check if it is a reserved word (otherwise reports it as an identifier)
    if (type == TOK_IDENTIFIER) type = token_keyword(lexer->token.value, lexer->token.bytes);
    SET_TOKTYPE(type);

    #if GRAVITY_LEXEM_DEBUG
    if (type == TOK_IDENTIFIER) DEBUG_LEXEM("Found identifier: %.*s", TOKEN_BYTES(lexer->token), TOKEN_VALUE(lexer->token));
    else DEBUG_LEXEM("Found keyword: %s", token_name(type));
    #endif

    return type;
}

static gtoken_t lexer_scan_number(gravity_lexer_t *lexer) {
    bool        floatAllowed = true;
    bool        expAllowed = true;
    bool        signAllowed = false;
    bool        dotFound = false;
    bool        expFound = false;
    int            c, expChar = 'e', floatChar = '.';
    int            plusSign = '+', minusSign = '-';

    gravity_number_type    ntype = NUMBER_INTEGER;
    if (PEEK_CURRENT == '0') {
        if (toupper(PEEK_NEXT) == 'X') {ntype = NUMBER_HEX; floatAllowed = false; expAllowed = false;}
        else if (toupper(PEEK_NEXT) == 'B') {ntype = NUMBER_BIN; floatAllowed = false; expAllowed = false;}
        else if (toupper(PEEK_NEXT) == 'O') {ntype = NUMBER_OCT; floatAllowed = false; expAllowed = false;}
    }

    TOKEN_RESET;
    if (ntype != NUMBER_INTEGER) {
        // skip first 0* number marker
        INC_TOKLEN;
        INC_TOKLEN;
        INC_OFFSET_POSITION;
        INC_OFFSET_POSITION;
    }

    // supported exp formats:
    // 12345    // decimal
    // 3.1415    // float
    // 1.25e2 = 1.25 * 10^2 = 125.0        // scientific notation
    // 1.25e-2 = 1.25 * 10^-2 = 0.0125    // scientific notation
    // 0xFFFF    // hex
    // 0B0101    // binary
    // 0O7777    // octal

loop:
    c = PEEK_CURRENT;

    // explicitly list all accepted cases
    if (IS_EOF) goto report_token;
    if (is_digit(c, ntype)) goto accept_char;
    if (is_whitespace(c)) goto report_token;
    if (is_newline(lexer, c)) goto report_token;

    if (expAllowed) {
        if ((c == expChar) && (!expFound)) {expFound = true; signAllowed = true; goto accept_char;}
    }
    if (floatAllowed) {
        if ((c == floatChar) && (!is_digit(PEEK_NEXT, ntype))) goto report_token;
        if ((c == floatChar) && (!dotFound))  {dotFound = true; goto accept_char;}
    }
    if (signAllowed) {
        if ((c == plusSign) || (c == minusSign)) {signAllowed = false; goto accept_char;}
    }
    if (is_builtin_operator(c)) goto report_token;
    if (is_semicolon(c)) goto report_token;

    // any other case is an error
    goto report_error;

accept_char:
    INC_TOKLEN;
    INC_OFFSET_POSITION;
    goto loop;

report_token:
    // number is from buffer->[nseek] and it is bytes length
    TOKEN_FINALIZE(TOK_NUMBER);

    DEBUG_LEXEM("Found number: %.*s", TOKEN_BYTES(lexer->token), TOKEN_VALUE(lexer->token));
    return TOK_NUMBER;

report_error:
    return lexer_error(lexer, "Malformed number expression.");
}

static gtoken_t lexer_scan_string(gravity_lexer_t *lexer) {
    int c, c2;

    // no memory allocation here
    c = NEXT;                    // save escaped character
    TOKEN_RESET;                // save offset

    while ((c2 = (unsigned char)PEEK_CURRENT) != c) {
        if (IS_EOF) return lexer_error(lexer, "Unexpected EOF inside a string literal");
        if (is_newline(lexer, c2)) INC_LINE;

        // handle escaped characters
        if (c2 == '\\') {
            INC_OFFSET_POSITION;
            INC_OFFSET_POSITION;
            INC_TOKLEN;
            INC_TOKLEN;

            // sanity check
            if (IS_EOF) return lexer_error(lexer, "Unexpected EOF inside a string literal");
            continue;
        }

        // scan next
        if (!next_utf8(lexer, NULL)) return lexer_error(lexer, "Unknown character inside a string literal");
        if (IS_EOF) return lexer_error(lexer, "Unexpected EOF inside a string literal");
    }

    // skip last escape character
    INC_OFFSET_POSITION;

    // string is from buffer->[nseek] and it is nlen length
    TOKEN_FINALIZE(TOK_STRING);

    DEBUG_LEXEM("Found string: %.*s", TOKEN_BYTES(lexer->token), TOKEN_VALUE(lexer->token));
    return TOK_STRING;
}

static gtoken_t lexer_scan_operator(gravity_lexer_t *lexer) {
    TOKEN_RESET;
    INC_TOKLEN;

    int c = NEXT;
    int c2 = PEEK_CURRENT;
    int tok = 0;

    switch (c) {
        case '=':
            if (c2 == '=') {
                INC_OFFSET_POSITION; INC_TOKLEN; c2 = PEEK_CURRENT;
                if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_ISIDENTICAL;}
                else tok = TOK_OP_ISEQUAL;
            }
            else tok = TOK_OP_ASSIGN;
            break;
        case '+':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_ADD_ASSIGN;}
            else tok = TOK_OP_ADD;
            break;
        case '-':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_SUB_ASSIGN;}
            else tok = TOK_OP_SUB;
            break;
        case '*':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_MUL_ASSIGN;}
            else tok = TOK_OP_MUL;
            break;
        case '/':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_DIV_ASSIGN;}
            else tok = TOK_OP_DIV;
            break;
        case '%':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_REM_ASSIGN;}
            else tok = TOK_OP_REM;
            break;
        case '<':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_LESS_EQUAL;}
            else if (c2 == '<') {
                INC_OFFSET_POSITION; INC_TOKLEN; c2 = PEEK_CURRENT;
                if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_SHIFT_LEFT_ASSIGN;}
                else tok = TOK_OP_SHIFT_LEFT;
            }
            else tok = TOK_OP_LESS;
            break;
        case '>':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_GREATER_EQUAL;}
            else if (c2 == '>') {
                INC_OFFSET_POSITION; INC_TOKLEN; c2 = PEEK_CURRENT;
                if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_SHIFT_RIGHT_ASSIGN;}
                else tok = TOK_OP_SHIFT_RIGHT;
            }
            else tok = TOK_OP_GREATER;
            break;
        case '&':
            if (c2 == '&') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_AND;}
            else if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_BIT_AND_ASSIGN;}
            else tok = TOK_OP_BIT_AND;
            break;
        case '|':
            if (c2 == '|') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_OR;}
            else if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_BIT_OR_ASSIGN;}
            else tok = TOK_OP_BIT_OR;
            break;
        case '.': // check for special .digit case
            if (is_digit(c2, false)) {DEC_OFFSET_POSITION; DEC_TOKLEN; tok = lexer_scan_number(lexer);}
            else if (c2 == '.') {
                // seems a range, now peek c2 again and decide range type
                INC_OFFSET_POSITION; INC_TOKLEN; c2 = PEEK_CURRENT;
                if ((c2 == '<') || (c2 == '.')) {
                    INC_OFFSET_POSITION; INC_TOKLEN;
                    tok = (c2 == '<') ? TOK_OP_RANGE_EXCLUDED : TOK_OP_RANGE_INCLUDED;
                } else {
                    return lexer_error(lexer, "Unrecognized Range operator");
                }
            }
            else tok = TOK_OP_DOT;
            break;
        case ',':
            tok = TOK_OP_COMMA;
            break;
        case '!':
            if (c2 == '=') {
                INC_OFFSET_POSITION; INC_TOKLEN; c2 = PEEK_CURRENT;
                if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_ISNOTIDENTICAL;}
                else tok = TOK_OP_ISNOTEQUAL;
            }
            else tok = TOK_OP_NOT;
            break;
        case '^':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_BIT_XOR_ASSIGN;}
            else tok = TOK_OP_BIT_XOR;
            break;
        case '~':
            if (c2 == '=') {INC_OFFSET_POSITION; INC_TOKLEN; tok = TOK_OP_PATTERN_MATCH;}
            else tok = TOK_OP_BIT_NOT;
            break;
        case ':':
            tok = TOK_OP_COLON;
            break;
        case '{':
            tok = TOK_OP_OPEN_CURLYBRACE;
            break;
        case '}':
            tok = TOK_OP_CLOSED_CURLYBRACE;
            break;
        case '[':
            tok = TOK_OP_OPEN_SQUAREBRACKET;
            break;
        case ']':
            tok = TOK_OP_CLOSED_SQUAREBRACKET;
            break;
        case '(':
            tok = TOK_OP_OPEN_PARENTHESIS;
            break;
        case ')':
            tok = TOK_OP_CLOSED_PARENTHESIS;
            break;
        case '?':
            tok = TOK_OP_TERNARY;
            break;
        default:
            return lexer_error(lexer, "Unrecognized Operator");

    }

    TOKEN_FINALIZE(tok);

    DEBUG_LEXEM("Found operator: %s", token_name(tok));
    return tok;
}

static gtoken_t lexer_scan_special(gravity_lexer_t *lexer) {
    TOKEN_RESET;
    INC_TOKLEN;
    INC_OFFSET_POSITION;
    TOKEN_FINALIZE(TOK_SPECIAL);

    return TOK_SPECIAL;
}

static gtoken_t lexer_scan_preprocessor(gravity_lexer_t *lexer) {
    TOKEN_RESET;
    INC_TOKLEN;
    INC_OFFSET_POSITION;
    TOKEN_FINALIZE(TOK_MACRO);

    return TOK_MACRO;
}

// MARK: -

gravity_lexer_t *gravity_lexer_create (const char *source, size_t len, uint32_t fileid, bool is_static) {
    gravity_lexer_t *lexer = mem_alloc(NULL, sizeof(gravity_lexer_t));
    if (!lexer) return NULL;

    bzero(lexer, sizeof(gravity_lexer_t));
    lexer->is_static = is_static;
    lexer->lineno = 1;
    lexer->buffer = source;
    lexer->length = (uint32_t)len;
    lexer->fileid = fileid;
    lexer->peeking = false;
    lexer->cache = TOK_END;
    
    return lexer;
}

void gravity_lexer_setdelegate (gravity_lexer_t *lexer, gravity_delegate_t *delegate) {
    lexer->delegate = delegate;
}

gtoken_t gravity_lexer_peek (gravity_lexer_t *lexer) {
    if (lexer->cache != TOK_END) return lexer->cache;
    
    lexer->peeking = true;
    gravity_lexer_t saved = *lexer;

    gtoken_t result = gravity_lexer_next(lexer);

    *lexer = saved;
    lexer->peeking = false;

    lexer->cache = result;
    return result;
}

gtoken_t gravity_lexer_next (gravity_lexer_t *lexer) {
    int            c;
    gtoken_t    token;

    // reset cached value
    if (!lexer->peeking) lexer->cache = TOK_END;
    
loop:
    if (IS_EOF) return TOK_EOF;
    c = PEEK_CURRENT;

    if (is_whitespace(c)) {INC_OFFSET_POSITION; goto loop;}
    if (is_newline(lexer, c)) {INC_OFFSET_POSITION; INC_LINE; goto loop;}
    if (is_comment(c, PEEK_NEXT)) {lexer_scan_comment(lexer); goto loop;}

    if (is_semicolon(c)) {token = lexer_scan_semicolon(lexer); goto return_result;}
    if (is_alpha(c)) {token = lexer_scan_identifier(lexer); goto return_result;}
    if (is_digit(c, false)) {token = lexer_scan_number(lexer); goto return_result;}
    if (is_string(c)) {token = lexer_scan_string(lexer); goto return_result;}
    if (is_builtin_operator(c)) {token = lexer_scan_operator(lexer); goto return_result;}
    if (is_special(c)) {token = lexer_scan_special(lexer); goto return_result;}
    if (is_preprocessor(c)) {token = lexer_scan_preprocessor(lexer); goto return_result;}

    return lexer_error(lexer, "Unrecognized token");

return_result:
    LEXER_CALL_CALLBACK();
    return token;
}

void gravity_lexer_free (gravity_lexer_t *lexer) {
    if ((!lexer->is_static) && (lexer->buffer)) mem_free(lexer->buffer);
    mem_free(lexer);
}

gtoken_s gravity_lexer_token (gravity_lexer_t *lexer) {
    return lexer->token;
}

gtoken_s gravity_lexer_token_next (gravity_lexer_t *lexer) {
    gtoken_s token = lexer->token;
    token.lineno = lexer->lineno;
    token.colno = lexer->colno;
    token.position = lexer->position;
    return token;
}

gtoken_t gravity_lexer_token_type (gravity_lexer_t *lexer) {
    return lexer->token.type;
}

void gravity_lexer_skip_line (gravity_lexer_t *lexer) {
    while (!IS_EOF) {
        int c = 0;
        next_utf8(lexer, &c);
        if (is_newline(lexer, c)) {
            INC_LINE;
            break;
        }
    }
}

uint32_t gravity_lexer_lineno (gravity_lexer_t *lexer) {
    return lexer->lineno;
}

// MARK: -

void gravity_lexer_token_dump (gtoken_s token) {
    printf("(%02d, %02d) %s: ", token.lineno, token.colno, token_name(token.type));
    printf("%.*s\t(offset: %d len:%d)\n", token.bytes, token.value, token.position, token.bytes);
}

#if GRAVITY_LEXER_DEGUB
void gravity_lexer_debug (gravity_lexer_t *lexer) {
    static uint32_t offset = UINT32_MAX;
    if (lexer->peeking) return;
    
    gtoken_s token = lexer->token;
    if ((token.lineno == 0) && (token.colno == 0)) return;
    if (offset == token.position) return;
    offset = token.position;
    
    printf("(%02d, %02d) %.*s\n", token.lineno, token.colno, token.bytes, token.value);
    
    //printf("(%02d, %02d) %s: ", token.lineno, token.colno, token_name(token.type));
	//printf("%.*s\t(offset: %d)\n", token.bytes, token.value, token.position);
}
#endif
