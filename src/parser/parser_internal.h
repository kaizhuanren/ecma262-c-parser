#ifndef JS_PARSER_INTERNAL_H
#define JS_PARSER_INTERNAL_H

#include <stdbool.h>

#include "js_parser_api.h"
#include "js_lexer.h"
#include "js_ast.h"

typedef struct js_parser_context_t {
    js_diagnostic_callback_t callback;
    void *user_data;
    js_lexer_t *lexer;
    js_ast_node_t *result;
    bool had_error;
} js_parser_context_t;

#endif /* JS_PARSER_INTERNAL_H */
