#include "parser_internal.h"

#include <stdlib.h>

extern int js_parser_implparse(js_parser_context_t *ctx);

js_parser_context_t *js_parser_create(js_diagnostic_callback_t callback, void *user_data) {
    js_parser_context_t *ctx = (js_parser_context_t *)calloc(1, sizeof(js_parser_context_t));
    if (!ctx) {
        return NULL;
    }
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->lexer = NULL;
    ctx->result = NULL;
    ctx->had_error = false;
    return ctx;
}

void js_parser_destroy(js_parser_context_t *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->lexer) {
        js_lexer_destroy(ctx->lexer);
    }
    if (ctx->result) {
        js_ast_free(ctx->result);
    }
    free(ctx);
}

bool js_parser_parse(js_parser_context_t *ctx, const char *source, js_ast_node_t **program_out) {
    if (!ctx || !source || !program_out) {
        return false;
    }
    if (!ctx->lexer) {
        ctx->lexer = js_lexer_create(ctx->callback, ctx->user_data);
        if (!ctx->lexer) {
            return false;
        }
    }
    if (!js_lexer_reset(ctx->lexer, source)) {
        return false;
    }

    ctx->had_error = false;
    if (ctx->result) {
        js_ast_free(ctx->result);
        ctx->result = NULL;
    }

    int parse_status = js_parser_implparse(ctx);
    if (parse_status != 0 || ctx->had_error) {
        if (ctx->result) {
            js_ast_free(ctx->result);
            ctx->result = NULL;
        }
        return false;
    }

    *program_out = ctx->result;
    ctx->result = NULL;
    return true;
}
