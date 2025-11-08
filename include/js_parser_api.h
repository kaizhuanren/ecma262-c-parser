#ifndef JS_PARSER_API_H
#define JS_PARSER_API_H

#include <stdbool.h>

#include "js_ast.h"
#include "js_diagnostics.h"

typedef struct js_parser_context_t js_parser_context_t;

js_parser_context_t *js_parser_create(js_diagnostic_callback_t callback, void *user_data);
void js_parser_destroy(js_parser_context_t *ctx);
bool js_parser_parse(js_parser_context_t *ctx, const char *source, js_ast_node_t **program_out);
void js_parser_set_debug(bool enabled);

#endif /* JS_PARSER_API_H */
