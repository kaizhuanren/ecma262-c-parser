#ifndef JS_TOKEN_H
#define JS_TOKEN_H

#include <stdbool.h>
#include <stddef.h>

#include "js_source.h"

typedef enum {
    JS_ASI_REASON_NONE = 0,
    JS_ASI_REASON_LINE_TERMINATOR,
    JS_ASI_REASON_CLOSING_BRACE,
    JS_ASI_REASON_EOF
} js_asi_reason_t;

typedef enum {
    JS_TOK_EOF = 0,
    JS_TOK_ERROR,

    JS_TOK_IDENTIFIER,
    JS_TOK_NUMBER,
    JS_TOK_STRING,
    JS_TOK_TEMPLATE_HEAD,
    JS_TOK_TEMPLATE_MIDDLE,
    JS_TOK_TEMPLATE_TAIL,
    JS_TOK_REGEX,

    JS_TOK_TRUE,
    JS_TOK_FALSE,
    JS_TOK_NULL,
    JS_TOK_THIS,
    JS_TOK_SUPER,

    /* Keywords */
    JS_TOK_KW_BREAK,
    JS_TOK_KW_CASE,
    JS_TOK_KW_CATCH,
    JS_TOK_KW_CLASS,
    JS_TOK_KW_CONST,
    JS_TOK_KW_CONTINUE,
    JS_TOK_KW_DEBUGGER,
    JS_TOK_KW_DEFAULT,
    JS_TOK_KW_DELETE,
    JS_TOK_KW_DO,
    JS_TOK_KW_ELSE,
    JS_TOK_KW_EXPORT,
    JS_TOK_KW_EXTENDS,
    JS_TOK_KW_FINALLY,
    JS_TOK_KW_FOR,
    JS_TOK_KW_FUNCTION,
    JS_TOK_KW_IF,
    JS_TOK_KW_IMPORT,
    JS_TOK_KW_IN,
    JS_TOK_KW_INSTANCEOF,
    JS_TOK_KW_LET,
    JS_TOK_KW_NEW,
    JS_TOK_KW_RETURN,
    JS_TOK_KW_STATIC,
    JS_TOK_KW_SWITCH,
    JS_TOK_KW_THROW,
    JS_TOK_KW_TRY,
    JS_TOK_KW_TYPEOF,
    JS_TOK_KW_VAR,
    JS_TOK_KW_VOID,
    JS_TOK_KW_WHILE,
    JS_TOK_KW_WITH,
    JS_TOK_KW_YIELD,
    JS_TOK_KW_AWAIT,

    /* Punctuators */
    JS_TOK_LPAREN,
    JS_TOK_RPAREN,
    JS_TOK_LBRACE,
    JS_TOK_RBRACE,
    JS_TOK_LBRACKET,
    JS_TOK_RBRACKET,
    JS_TOK_SEMICOLON,
    JS_TOK_COMMA,
    JS_TOK_DOT,
    JS_TOK_ELLIPSIS,
    JS_TOK_COLON,
    JS_TOK_QUESTION,
    JS_TOK_BACKTICK,
    JS_TOK_ARROW,

    JS_TOK_PLUS,
    JS_TOK_MINUS,
    JS_TOK_STAR,
    JS_TOK_SLASH,
    JS_TOK_PERCENT,
    JS_TOK_PLUS_PLUS,
    JS_TOK_MINUS_MINUS,
    JS_TOK_STAR_STAR,

    JS_TOK_ASSIGN,
    JS_TOK_PLUS_ASSIGN,
    JS_TOK_MINUS_ASSIGN,
    JS_TOK_STAR_ASSIGN,
    JS_TOK_SLASH_ASSIGN,
    JS_TOK_PERCENT_ASSIGN,
    JS_TOK_LSHIFT_ASSIGN,
    JS_TOK_RSHIFT_ASSIGN,
    JS_TOK_URSHIFT_ASSIGN,
    JS_TOK_BIT_AND_ASSIGN,
    JS_TOK_BIT_OR_ASSIGN,
    JS_TOK_BIT_XOR_ASSIGN,
    JS_TOK_STAR_STAR_ASSIGN,
    JS_TOK_NULLISH_ASSIGN,
    JS_TOK_LOGICAL_AND_ASSIGN,
    JS_TOK_LOGICAL_OR_ASSIGN,

    JS_TOK_EQUAL,
    JS_TOK_NOT_EQUAL,
    JS_TOK_STRICT_EQUAL,
    JS_TOK_STRICT_NOT_EQUAL,
    JS_TOK_LT,
    JS_TOK_GT,
    JS_TOK_LE,
    JS_TOK_GE,

    JS_TOK_BIT_AND,
    JS_TOK_BIT_OR,
    JS_TOK_BIT_XOR,
    JS_TOK_BIT_NOT,
    JS_TOK_LSHIFT,
    JS_TOK_RSHIFT,
    JS_TOK_URSHIFT,
    JS_TOK_LOGICAL_AND,
    JS_TOK_LOGICAL_OR,
    JS_TOK_NULLISH_COALESCE,
    JS_TOK_NOT
} js_token_kind_t;

typedef struct {
    js_token_kind_t kind;
    const char *lexeme;
    size_t length;
    js_source_location_t location;
    bool preceded_by_newline;
    double number_value;
    bool inserted_via_asi;
    js_asi_reason_t asi_reason;
} js_token_t;

#endif /* JS_TOKEN_H */
