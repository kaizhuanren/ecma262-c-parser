#ifndef JS_DIAGNOSTICS_H
#define JS_DIAGNOSTICS_H

#include <stddef.h>

#include "js_source.h"

typedef enum {
    JS_DIAG_ERROR = 0,
    JS_DIAG_WARNING,
    JS_DIAG_NOTE
} js_diag_level_t;

typedef struct {
    js_diag_level_t level;
    const char *message;
    js_source_location_t location;
} js_diagnostic_t;

typedef void (*js_diagnostic_callback_t)(const js_diagnostic_t *diag, void *user_data);

#endif /* JS_DIAGNOSTICS_H */
