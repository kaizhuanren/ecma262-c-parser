#ifndef JS_LEXER_H
#define JS_LEXER_H

#include <stdbool.h>
#include <stddef.h>

#include "js_token.h"
#include "js_diagnostics.h"

typedef struct js_lexer_t js_lexer_t;

js_lexer_t *js_lexer_create(js_diagnostic_callback_t callback, void *user_data);
void js_lexer_destroy(js_lexer_t *lexer);
bool js_lexer_reset(js_lexer_t *lexer, const char *source);
bool js_lexer_next(js_lexer_t *lexer, js_token_t *out_token);

#endif /* JS_LEXER_H */
