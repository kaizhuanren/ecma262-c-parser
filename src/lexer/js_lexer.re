#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lexer_internal.h"

static void js_lexer_update_position(js_lexer_t *lexer, const char *start, const char *end) {
    uint32_t line = lexer->state.current_line;
    uint32_t column = lexer->state.current_column;
    const unsigned char *p = (const unsigned char *)start;
    const unsigned char *limit = (const unsigned char *)end;
    while (p < limit) {
        unsigned char ch = *p++;
        if (ch == '\r') {
            if (p < limit && *p == '\n') {
                ++p;
            }
            line += 1;
            column = 1;
            lexer->state.saw_newline = true;
        } else if (ch == '\n') {
            line += 1;
            column = 1;
            lexer->state.saw_newline = true;
        } else {
            column += 1;
        }
    }
    lexer->state.current_line = line;
    lexer->state.current_column = column;
}

static void js_lexer_set_token(js_lexer_t *lexer, js_token_t *out_token, js_token_kind_t kind, const char *start, const char *end) {
    out_token->kind = kind;
    out_token->lexeme = start;
    out_token->length = (size_t)(end - start);
    out_token->location.line = lexer->state.token_line;
    out_token->location.column = lexer->state.token_column;
    out_token->preceded_by_newline = lexer->state.prev_had_newline;
    out_token->number_value = 0.0;
    js_lexer_update_position(lexer, start, end);
}

static void js_lexer_emit_error(js_lexer_t *lexer, const char *message) {
    if (!lexer->callback) {
        return;
    }
    js_diagnostic_t diag;
    diag.level = JS_DIAG_ERROR;
    diag.message = message;
    diag.location.line = lexer->state.token_line;
    diag.location.column = lexer->state.token_column;
    lexer->callback(&diag, lexer->user_data);
}

static js_token_kind_t js_keyword_lookup(const char *lexeme, size_t length) {
    switch (length) {
        case 2:
            if (strncmp(lexeme, "do", 2) == 0) return JS_TOK_KW_DO;
            if (strncmp(lexeme, "if", 2) == 0) return JS_TOK_KW_IF;
            if (strncmp(lexeme, "in", 2) == 0) return JS_TOK_KW_IN;
            break;
        case 3:
            if (strncmp(lexeme, "for", 3) == 0) return JS_TOK_KW_FOR;
            if (strncmp(lexeme, "new", 3) == 0) return JS_TOK_KW_NEW;
            if (strncmp(lexeme, "try", 3) == 0) return JS_TOK_KW_TRY;
            if (strncmp(lexeme, "var", 3) == 0) return JS_TOK_KW_VAR;
            if (strncmp(lexeme, "let", 3) == 0) return JS_TOK_KW_LET;
            break;
        case 4:
            if (strncmp(lexeme, "case", 4) == 0) return JS_TOK_KW_CASE;
            if (strncmp(lexeme, "else", 4) == 0) return JS_TOK_KW_ELSE;
            if (strncmp(lexeme, "null", 4) == 0) return JS_TOK_NULL;
            if (strncmp(lexeme, "this", 4) == 0) return JS_TOK_THIS;
            if (strncmp(lexeme, "true", 4) == 0) return JS_TOK_TRUE;
            if (strncmp(lexeme, "void", 4) == 0) return JS_TOK_KW_VOID;
            if (strncmp(lexeme, "with", 4) == 0) return JS_TOK_KW_WITH;
            break;
        case 5:
            if (strncmp(lexeme, "await", 5) == 0) return JS_TOK_KW_AWAIT;
            if (strncmp(lexeme, "break", 5) == 0) return JS_TOK_KW_BREAK;
            if (strncmp(lexeme, "catch", 5) == 0) return JS_TOK_KW_CATCH;
            if (strncmp(lexeme, "class", 5) == 0) return JS_TOK_KW_CLASS;
            if (strncmp(lexeme, "const", 5) == 0) return JS_TOK_KW_CONST;
            if (strncmp(lexeme, "false", 5) == 0) return JS_TOK_FALSE;
            if (strncmp(lexeme, "super", 5) == 0) return JS_TOK_SUPER;
            if (strncmp(lexeme, "throw", 5) == 0) return JS_TOK_KW_THROW;
            if (strncmp(lexeme, "while", 5) == 0) return JS_TOK_KW_WHILE;
            if (strncmp(lexeme, "yield", 5) == 0) return JS_TOK_KW_YIELD;
            break;
        case 6:
            if (strncmp(lexeme, "delete", 6) == 0) return JS_TOK_KW_DELETE;
            if (strncmp(lexeme, "export", 6) == 0) return JS_TOK_KW_EXPORT;
            if (strncmp(lexeme, "import", 6) == 0) return JS_TOK_KW_IMPORT;
            if (strncmp(lexeme, "return", 6) == 0) return JS_TOK_KW_RETURN;
            if (strncmp(lexeme, "switch", 6) == 0) return JS_TOK_KW_SWITCH;
            if (strncmp(lexeme, "typeof", 6) == 0) return JS_TOK_KW_TYPEOF;
            if (strncmp(lexeme, "static", 6) == 0) return JS_TOK_KW_STATIC;
            break;
        case 7:
            if (strncmp(lexeme, "default", 7) == 0) return JS_TOK_KW_DEFAULT;
            if (strncmp(lexeme, "extends", 7) == 0) return JS_TOK_KW_EXTENDS;
            if (strncmp(lexeme, "finally", 7) == 0) return JS_TOK_KW_FINALLY;
            break;
        case 8:
            if (strncmp(lexeme, "continue", 8) == 0) return JS_TOK_KW_CONTINUE;
            if (strncmp(lexeme, "debugger", 8) == 0) return JS_TOK_KW_DEBUGGER;
            if (strncmp(lexeme, "function", 8) == 0) return JS_TOK_KW_FUNCTION;
            break;
        case 9:
            if (strncmp(lexeme, "interface", 9) == 0) return JS_TOK_IDENTIFIER;
            if (strncmp(lexeme, "protected", 9) == 0) return JS_TOK_IDENTIFIER;
            break;
        case 10:
            if (strncmp(lexeme, "instanceof", 10) == 0) return JS_TOK_KW_INSTANCEOF;
            break;
        default:
            break;
    }
    return JS_TOK_IDENTIFIER;
}

static double js_parse_numeric_literal(const char *lexeme, size_t length) {
    if (length >= 2 && lexeme[0] == '0') {
        if (length >= 3 && (lexeme[1] == 'x' || lexeme[1] == 'X')) {
            double value = 0.0;
            for (size_t i = 2; i < length; ++i) {
                char ch = lexeme[i];
                if (ch >= '0' && ch <= '9') {
                    value = value * 16 + (double)(ch - '0');
                } else if (ch >= 'a' && ch <= 'f') {
                    value = value * 16 + (double)(10 + ch - 'a');
                } else if (ch >= 'A' && ch <= 'F') {
                    value = value * 16 + (double)(10 + ch - 'A');
                } else {
                    break;
                }
            }
            return value;
        }
        if (length >= 3 && (lexeme[1] == 'o' || lexeme[1] == 'O')) {
            double value = 0.0;
            for (size_t i = 2; i < length; ++i) {
                char ch = lexeme[i];
                if (ch >= '0' && ch <= '7') {
                    value = value * 8 + (double)(ch - '0');
                } else {
                    break;
                }
            }
            return value;
        }
        if (length >= 3 && (lexeme[1] == 'b' || lexeme[1] == 'B')) {
            double value = 0.0;
            for (size_t i = 2; i < length; ++i) {
                char ch = lexeme[i];
                if (ch == '0' || ch == '1') {
                    value = value * 2 + (double)(ch - '0');
                } else {
                    break;
                }
            }
            return value;
        }
    }
    char buffer[256];
    size_t copy_len = length < sizeof(buffer) - 1 ? length : sizeof(buffer) - 1;
    memcpy(buffer, lexeme, copy_len);
    buffer[copy_len] = '\0';
    return strtod(buffer, NULL);
}

int js_lexer_scan(js_lexer_t *lexer, js_token_t *out_token) {
    const char *cursor = lexer->state.cursor;
    const char *marker = lexer->state.marker;
    const char *limit = lexer->state.limit;

    for (;;) {
        lexer->state.prev_had_newline = lexer->state.saw_newline;
        lexer->state.saw_newline = false;
        lexer->state.token_start = cursor;
        lexer->state.token_line = lexer->state.current_line;
        lexer->state.token_column = lexer->state.current_column;

        if (cursor >= limit) {
            js_lexer_set_token(lexer, out_token, JS_TOK_EOF, cursor, cursor);
            lexer->state.cursor = cursor;
            lexer->state.marker = marker;
            return 1;
        }

        const char *token_start = cursor;

#define YYCTYPE unsigned char
#define YYCURSOR cursor
#define YYMARKER marker
#define YYLIMIT limit
#define YYFILL(n) ((void)0)

        /*!re2c
            newline          = "\r\n" | "\n" | "\r";
            whitespace       = [ \t\v\f]+;
            line_comment     = "//" [^\r\n]*;
            block_comment    = "/*" ([^*] | "*" [^/])* "*/";
            digit            = [0-9];
            hex              = [0-9a-fA-F];
            oct              = [0-7];
            bin              = [0-1];
            identifier_start = [A-Za-z_$];
            identifier_part  = [A-Za-z0-9_$];
            decimal_int      = "0" | [1-9][0-9]*;
            exponent         = [eE] [+-]? digit+;
            decimal          = decimal_int exponent?
                            | decimal_int "." digit* exponent?
                            | "." digit+ exponent?;
            hex_literal      = "0" [xX] hex+;
            oct_literal      = "0" [oO] oct+;
            bin_literal      = "0" [bB] bin+;
            string_double    = "\"" ( [^"\\\n\r] | "\\" . )* "\"";
            string_single    = "'"  ( [^'\\\n\r] | "\\" . )* "'";
        newline {
            js_lexer_update_position(lexer, token_start, cursor);
            continue;
        }

        whitespace {
            js_lexer_update_position(lexer, token_start, cursor);
            continue;
        }

        line_comment {
            js_lexer_update_position(lexer, token_start, cursor);
            continue;
        }

        block_comment {
            js_lexer_update_position(lexer, token_start, cursor);
            continue;
        }

        "/*" {
            const char *p = cursor;
            while (p < limit) {
                char ch = *p++;
                if (ch == '\n') {
                    lexer->state.saw_newline = true;
                }
            }
            cursor = limit;
            js_lexer_emit_error(lexer, "Unterminated block comment");
            js_lexer_set_token(lexer, out_token, JS_TOK_ERROR, token_start, cursor);
            break;
        }

        "..." {
            js_lexer_set_token(lexer, out_token, JS_TOK_ELLIPSIS, token_start, cursor);
            break;
        }

        "&&=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LOGICAL_AND_ASSIGN, token_start, cursor);
            break;
        }

        "||=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LOGICAL_OR_ASSIGN, token_start, cursor);
            break;
        }

        "?""?=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_NULLISH_ASSIGN, token_start, cursor);
            break;
        }

        "**=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_STAR_STAR_ASSIGN, token_start, cursor);
            break;
        }

        ">>=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_RSHIFT_ASSIGN, token_start, cursor);
            break;
        }

        "<<=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LSHIFT_ASSIGN, token_start, cursor);
            break;
        }

        ">>>=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_URSHIFT_ASSIGN, token_start, cursor);
            break;
        }

        "===" {
            js_lexer_set_token(lexer, out_token, JS_TOK_STRICT_EQUAL, token_start, cursor);
            break;
        }

        "!==" {
            js_lexer_set_token(lexer, out_token, JS_TOK_STRICT_NOT_EQUAL, token_start, cursor);
            break;
        }

        "++" {
            js_lexer_set_token(lexer, out_token, JS_TOK_PLUS_PLUS, token_start, cursor);
            break;
        }

        "--" {
            js_lexer_set_token(lexer, out_token, JS_TOK_MINUS_MINUS, token_start, cursor);
            break;
        }

        "&&" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LOGICAL_AND, token_start, cursor);
            break;
        }

        "||" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LOGICAL_OR, token_start, cursor);
            break;
        }

        "?""?" {
            js_lexer_set_token(lexer, out_token, JS_TOK_NULLISH_COALESCE, token_start, cursor);
            break;
        }

        "**" {
            js_lexer_set_token(lexer, out_token, JS_TOK_STAR_STAR, token_start, cursor);
            break;
        }

        ">>>" {
            js_lexer_set_token(lexer, out_token, JS_TOK_URSHIFT, token_start, cursor);
            break;
        }

        ">>" {
            js_lexer_set_token(lexer, out_token, JS_TOK_RSHIFT, token_start, cursor);
            break;
        }

        "<<" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LSHIFT, token_start, cursor);
            break;
        }

        "=>" {
            js_lexer_set_token(lexer, out_token, JS_TOK_ARROW, token_start, cursor);
            break;
        }

        "+=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_PLUS_ASSIGN, token_start, cursor);
            break;
        }

        "-=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_MINUS_ASSIGN, token_start, cursor);
            break;
        }

        "*=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_STAR_ASSIGN, token_start, cursor);
            break;
        }

        "/=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_SLASH_ASSIGN, token_start, cursor);
            break;
        }

        "%=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_PERCENT_ASSIGN, token_start, cursor);
            break;
        }

        "&=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_BIT_AND_ASSIGN, token_start, cursor);
            break;
        }

        "|=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_BIT_OR_ASSIGN, token_start, cursor);
            break;
        }

        "^=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_BIT_XOR_ASSIGN, token_start, cursor);
            break;
        }

        "==" {
            js_lexer_set_token(lexer, out_token, JS_TOK_EQUAL, token_start, cursor);
            break;
        }

        "!=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_NOT_EQUAL, token_start, cursor);
            break;
        }

        "<=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LE, token_start, cursor);
            break;
        }

        ">=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_GE, token_start, cursor);
            break;
        }

        "+" {
            js_lexer_set_token(lexer, out_token, JS_TOK_PLUS, token_start, cursor);
            break;
        }

        "-" {
            js_lexer_set_token(lexer, out_token, JS_TOK_MINUS, token_start, cursor);
            break;
        }

        "*" {
            js_lexer_set_token(lexer, out_token, JS_TOK_STAR, token_start, cursor);
            break;
        }

        "/" {
            js_lexer_set_token(lexer, out_token, JS_TOK_SLASH, token_start, cursor);
            break;
        }

        "%" {
            js_lexer_set_token(lexer, out_token, JS_TOK_PERCENT, token_start, cursor);
            break;
        }

        "=" {
            js_lexer_set_token(lexer, out_token, JS_TOK_ASSIGN, token_start, cursor);
            break;
        }

        "<" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LT, token_start, cursor);
            break;
        }

        ">" {
            js_lexer_set_token(lexer, out_token, JS_TOK_GT, token_start, cursor);
            break;
        }

        "&" {
            js_lexer_set_token(lexer, out_token, JS_TOK_BIT_AND, token_start, cursor);
            break;
        }

        "|" {
            js_lexer_set_token(lexer, out_token, JS_TOK_BIT_OR, token_start, cursor);
            break;
        }

        "^" {
            js_lexer_set_token(lexer, out_token, JS_TOK_BIT_XOR, token_start, cursor);
            break;
        }

        "!" {
            js_lexer_set_token(lexer, out_token, JS_TOK_NOT, token_start, cursor);
            break;
        }

        "~" {
            js_lexer_set_token(lexer, out_token, JS_TOK_BIT_NOT, token_start, cursor);
            break;
        }

        "?" {
            js_lexer_set_token(lexer, out_token, JS_TOK_QUESTION, token_start, cursor);
            break;
        }

        ":" {
            js_lexer_set_token(lexer, out_token, JS_TOK_COLON, token_start, cursor);
            break;
        }

        ";" {
            js_lexer_set_token(lexer, out_token, JS_TOK_SEMICOLON, token_start, cursor);
            break;
        }

        "," {
            js_lexer_set_token(lexer, out_token, JS_TOK_COMMA, token_start, cursor);
            break;
        }

        "." {
            js_lexer_set_token(lexer, out_token, JS_TOK_DOT, token_start, cursor);
            break;
        }

        "(" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LPAREN, token_start, cursor);
            break;
        }

        ")" {
            js_lexer_set_token(lexer, out_token, JS_TOK_RPAREN, token_start, cursor);
            break;
        }

        "[" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LBRACKET, token_start, cursor);
            break;
        }

        "]" {
            js_lexer_set_token(lexer, out_token, JS_TOK_RBRACKET, token_start, cursor);
            break;
        }

        "{" {
            js_lexer_set_token(lexer, out_token, JS_TOK_LBRACE, token_start, cursor);
            break;
        }

        "}" {
            js_lexer_set_token(lexer, out_token, JS_TOK_RBRACE, token_start, cursor);
            break;
        }

        "`" {
            const char *p = cursor;
            bool closed = false;
            while (p < limit) {
                char ch = *p++;
                if (ch == '`') {
                    closed = true;
                    break;
                }
                if (ch == '\\' && p < limit) {
                    ++p;
                }
                if (ch == '\n') {
                    lexer->state.saw_newline = true;
                }
            }
            if (!closed) {
                cursor = p;
                js_lexer_emit_error(lexer, "Unterminated template literal");
                js_lexer_set_token(lexer, out_token, JS_TOK_ERROR, token_start, p);
                break;
            }
            cursor = p;
            js_lexer_set_token(lexer, out_token, JS_TOK_STRING, token_start, cursor);
            break;
        }

        string_double {
            js_lexer_set_token(lexer, out_token, JS_TOK_STRING, token_start, cursor);
            break;
        }

        string_single {
            js_lexer_set_token(lexer, out_token, JS_TOK_STRING, token_start, cursor);
            break;
        }

        hex_literal {
            js_lexer_set_token(lexer, out_token, JS_TOK_NUMBER, token_start, cursor);
            break;
        }

        oct_literal {
            js_lexer_set_token(lexer, out_token, JS_TOK_NUMBER, token_start, cursor);
            break;
        }

        bin_literal {
            js_lexer_set_token(lexer, out_token, JS_TOK_NUMBER, token_start, cursor);
            break;
        }

        decimal {
            js_lexer_set_token(lexer, out_token, JS_TOK_NUMBER, token_start, cursor);
            break;
        }

        identifier_start identifier_part* {
            js_token_kind_t kind = js_keyword_lookup(token_start, (size_t)(cursor - token_start));
            js_lexer_set_token(lexer, out_token, kind, token_start, cursor);
            break;
        }

        . {
            cursor = token_start + 1;
            js_lexer_emit_error(lexer, "Unexpected character");
            js_lexer_set_token(lexer, out_token, JS_TOK_ERROR, token_start, cursor);
            break;
        }
        */
    }

#undef YYFILL
#undef YYLIMIT
#undef YYMARKER
#undef YYCURSOR
#undef YYCTYPE

    lexer->state.cursor = cursor;
    lexer->state.marker = marker;

    if (out_token->kind == JS_TOK_NUMBER) {
        out_token->number_value = js_parse_numeric_literal(out_token->lexeme, out_token->length);
    }

    return out_token->kind != JS_TOK_ERROR;
}
