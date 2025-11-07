#ifndef JS_SOURCE_H
#define JS_SOURCE_H

#include <stdint.h>

typedef struct {
    uint32_t line;
    uint32_t column;
} js_source_location_t;

typedef struct {
    js_source_location_t start;
    js_source_location_t end;
} js_source_range_t;

#endif /* JS_SOURCE_H */
