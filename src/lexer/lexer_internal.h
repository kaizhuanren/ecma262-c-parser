#ifndef JS_LEXER_INTERNAL_H
#define JS_LEXER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "js_lexer.h"

typedef struct {
    const char *cursor;
    const char *marker;
    const char *limit;
    const char *token_start;
    uint32_t current_line;
    uint32_t current_column;
    uint32_t token_line;
    uint32_t token_column;
    bool saw_newline;
    bool prev_had_newline;
} js_lexer_state_t;

struct js_lexer_t {
    js_diagnostic_callback_t callback;
    void *user_data;
    const char *source;
    size_t source_length;
    js_lexer_state_t state;
    js_token_t lookahead_token;
    bool has_lookahead;
    bool force_semi_after_keyword;
    js_token_kind_t previous_token_kind;
};

int js_lexer_scan(struct js_lexer_t *lexer, js_token_t *out_token);
bool js_token_allows_asi(js_token_kind_t kind);

#endif /* JS_LEXER_INTERNAL_H */
