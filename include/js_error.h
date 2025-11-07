#ifndef JS_ERROR_H
#define JS_ERROR_H

#include "js_diagnostics.h"

typedef enum {
    JS_ERROR_NONE = 0,
    JS_ERROR_LEXICAL,
    JS_ERROR_SYNTACTIC,
    JS_ERROR_SEMANTIC,
    JS_ERROR_RUNTIME
} js_error_code_t;

typedef struct {
    js_error_code_t code;
    js_diagnostic_t diagnostic;
} js_error_t;

#endif /* JS_ERROR_H */
