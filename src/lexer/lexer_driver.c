#include "lexer_internal.h"

#include <stdlib.h>
#include <string.h>

bool js_token_allows_asi(js_token_kind_t kind) {
    switch (kind) {
        case JS_TOK_IDENTIFIER:
        case JS_TOK_NUMBER:
        case JS_TOK_STRING:
        case JS_TOK_TEMPLATE_TAIL:
        case JS_TOK_TRUE:
        case JS_TOK_FALSE:
        case JS_TOK_NULL:
        case JS_TOK_THIS:
        case JS_TOK_SUPER:
        case JS_TOK_RPAREN:
        case JS_TOK_RBRACKET:
        case JS_TOK_PLUS_PLUS:
        case JS_TOK_MINUS_MINUS:
        case JS_TOK_REGEX:
        case JS_TOK_KW_BREAK:
        case JS_TOK_KW_CONTINUE:
        case JS_TOK_KW_RETURN:
        case JS_TOK_KW_THROW:
        case JS_TOK_KW_YIELD:
            return true;
        default:
            return false;
    }
}

static bool token_disallows_asi_before(js_token_kind_t kind) {
    switch (kind) {
        case JS_TOK_COMMA:
        case JS_TOK_COLON:
        case JS_TOK_DOT:
        case JS_TOK_ELLIPSIS:
        case JS_TOK_LPAREN:
        case JS_TOK_LBRACKET:
        case JS_TOK_PLUS:
        case JS_TOK_MINUS:
        case JS_TOK_PLUS_PLUS:
        case JS_TOK_MINUS_MINUS:
        case JS_TOK_STAR:
        case JS_TOK_SLASH:
        case JS_TOK_PERCENT:
        case JS_TOK_ASSIGN:
        case JS_TOK_PLUS_ASSIGN:
        case JS_TOK_MINUS_ASSIGN:
        case JS_TOK_STAR_ASSIGN:
        case JS_TOK_SLASH_ASSIGN:
        case JS_TOK_PERCENT_ASSIGN:
        case JS_TOK_LSHIFT_ASSIGN:
        case JS_TOK_RSHIFT_ASSIGN:
        case JS_TOK_URSHIFT_ASSIGN:
        case JS_TOK_BIT_AND_ASSIGN:
        case JS_TOK_BIT_OR_ASSIGN:
        case JS_TOK_BIT_XOR_ASSIGN:
        case JS_TOK_STAR_STAR_ASSIGN:
        case JS_TOK_NULLISH_ASSIGN:
        case JS_TOK_LOGICAL_AND_ASSIGN:
        case JS_TOK_LOGICAL_OR_ASSIGN:
        case JS_TOK_EQUAL:
        case JS_TOK_NOT_EQUAL:
        case JS_TOK_STRICT_EQUAL:
        case JS_TOK_STRICT_NOT_EQUAL:
        case JS_TOK_LT:
        case JS_TOK_GT:
        case JS_TOK_LE:
        case JS_TOK_GE:
        case JS_TOK_LOGICAL_AND:
        case JS_TOK_LOGICAL_OR:
        case JS_TOK_NULLISH_COALESCE:
        case JS_TOK_ARROW:
        case JS_TOK_BIT_AND:
        case JS_TOK_BIT_OR:
        case JS_TOK_BIT_XOR:
        case JS_TOK_LSHIFT:
        case JS_TOK_RSHIFT:
        case JS_TOK_URSHIFT:
        case JS_TOK_KW_INSTANCEOF:
        case JS_TOK_KW_IN:
        case JS_TOK_KW_ELSE:
        case JS_TOK_KW_WHILE:
        case JS_TOK_KW_CATCH:
        case JS_TOK_KW_FINALLY:
            return true;
        default:
            return false;
    }
}

static bool keyword_requires_line_terminator_guard(js_token_kind_t kind) {
    switch (kind) {
        case JS_TOK_KW_RETURN:
        case JS_TOK_KW_THROW:
        case JS_TOK_KW_CONTINUE:
        case JS_TOK_KW_BREAK:
        case JS_TOK_KW_YIELD:
            return true;
        default:
            return false;
    }
}

js_lexer_t *js_lexer_create(js_diagnostic_callback_t callback, void *user_data) {
    js_lexer_t *lexer = (js_lexer_t *)calloc(1, sizeof(js_lexer_t));
    if (!lexer) {
        return NULL;
    }
    lexer->callback = callback;
    lexer->user_data = user_data;
    lexer->previous_token_kind = JS_TOK_EOF;
    return lexer;
}

void js_lexer_destroy(js_lexer_t *lexer) {
    free(lexer);
}

bool js_lexer_reset(js_lexer_t *lexer, const char *source) {
    if (!lexer || !source) {
        return false;
    }
    size_t length = strlen(source);
    lexer->source = source;
    lexer->source_length = length;
    lexer->state.cursor = source;
    lexer->state.marker = source;
    lexer->state.limit = source + length;
    lexer->state.token_start = source;
    lexer->state.current_line = 1;
    lexer->state.current_column = 1;
    lexer->state.token_line = 1;
    lexer->state.token_column = 1;
    lexer->state.saw_newline = false;
    lexer->state.prev_had_newline = false;
    lexer->has_lookahead = false;
    lexer->force_semi_after_keyword = false;
    lexer->previous_token_kind = JS_TOK_EOF;
    return true;
}

static void make_semicolon_token(js_lexer_t *lexer, js_token_t *out_token, js_asi_reason_t reason) {
    out_token->kind = JS_TOK_SEMICOLON;
    out_token->lexeme = ";";
    out_token->length = 1;
    out_token->location.line = lexer->state.current_line;
    out_token->location.column = lexer->state.current_column;
    out_token->preceded_by_newline = false;
    out_token->number_value = 0.0;
    out_token->inserted_via_asi = true;
    out_token->asi_reason = reason;
}

bool js_lexer_next(js_lexer_t *lexer, js_token_t *out_token) {
    if (!lexer || !out_token) {
        return false;
    }

    if (lexer->has_lookahead) {
        *out_token = lexer->lookahead_token;
        lexer->has_lookahead = false;
        lexer->previous_token_kind = out_token->kind;
        if (keyword_requires_line_terminator_guard(out_token->kind)) {
            lexer->force_semi_after_keyword = true;
        } else if (out_token->kind == JS_TOK_SEMICOLON) {
            lexer->force_semi_after_keyword = false;
        } else if (!out_token->preceded_by_newline) {
            lexer->force_semi_after_keyword = false;
        }
        return true;
    }

    if (lexer->previous_token_kind == JS_TOK_SEMICOLON) {
        lexer->force_semi_after_keyword = false;
    }

    js_token_t token;
    if (!js_lexer_scan(lexer, &token)) {
        lexer->previous_token_kind = JS_TOK_ERROR;
        *out_token = token;
        return true;
    }

    bool newline = token.preceded_by_newline || lexer->state.prev_had_newline;

    bool should_insert = false;
    if (lexer->previous_token_kind != JS_TOK_EOF &&
        lexer->previous_token_kind != JS_TOK_SEMICOLON &&
        js_token_allows_asi(lexer->previous_token_kind)) {
        if (lexer->force_semi_after_keyword) {
            if (newline) {
                should_insert = true;
            } else if (token.kind == JS_TOK_EOF || token.kind == JS_TOK_RBRACE) {
                should_insert = true;
            }
        } else if (newline &&
                   token.kind != JS_TOK_SEMICOLON &&
                   token.kind != JS_TOK_EOF &&
                   !token_disallows_asi_before(token.kind)) {
            should_insert = true;
        } else if (newline && (token.kind == JS_TOK_EOF || token.kind == JS_TOK_RBRACE)) {
            should_insert = true;
        }
    } else if (lexer->force_semi_after_keyword && (newline || token.kind == JS_TOK_EOF || token.kind == JS_TOK_RBRACE)) {
        should_insert = true;
    }

    if (should_insert) {
        lexer->lookahead_token = token;
        lexer->has_lookahead = true;
        lexer->force_semi_after_keyword = false;
        js_asi_reason_t reason = JS_ASI_REASON_LINE_TERMINATOR;
        if (token.kind == JS_TOK_RBRACE) {
            reason = JS_ASI_REASON_CLOSING_BRACE;
        } else if (token.kind == JS_TOK_EOF) {
            reason = JS_ASI_REASON_EOF;
        }
        make_semicolon_token(lexer, out_token, reason);
        out_token->preceded_by_newline = newline;
        lexer->previous_token_kind = out_token->kind;
        return true;
    }

    *out_token = token;
    lexer->previous_token_kind = out_token->kind;

    if (keyword_requires_line_terminator_guard(out_token->kind)) {
        lexer->force_semi_after_keyword = true;
    } else if (out_token->kind == JS_TOK_SEMICOLON) {
        lexer->force_semi_after_keyword = false;
    } else if (!newline) {
        lexer->force_semi_after_keyword = false;
    }

    return true;
}
