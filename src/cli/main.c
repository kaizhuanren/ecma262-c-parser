#include "js_parser_api.h"

#include <stdio.h>
#include <stdlib.h>

static void print_diagnostic(const js_diagnostic_t *diag, void *user_data) {
    (void)user_data;
    const char *level = "error";
    switch (diag->level) {
        case JS_DIAG_WARNING:
            level = "warning";
            break;
        case JS_DIAG_NOTE:
            level = "note";
            break;
        default:
            break;
    }
    fprintf(stderr, "%s:%u:%u: %s\n", level, diag->location.line, diag->location.column, diag->message);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.js>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *path = argv[1];
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return EXIT_FAILURE;
    }

    long size = ftell(f);
    if (size < 0) {
        perror("ftell");
        fclose(f);
        return EXIT_FAILURE;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(f);
        return EXIT_FAILURE;
    }

    char *source = (char *)malloc((size_t)size + 1);
    if (!source) {
        perror("malloc");
        fclose(f);
        return EXIT_FAILURE;
    }

    size_t read = fread(source, 1, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size) {
        fprintf(stderr, "Failed to read file '%s'\n", path);
        free(source);
        return EXIT_FAILURE;
    }
    source[size] = '\0';

    js_parser_context_t *ctx = js_parser_create(print_diagnostic, NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to create parser context\n");
        free(source);
        return EXIT_FAILURE;
    }

    js_ast_node_t *program = NULL;
    bool ok = js_parser_parse(ctx, source, &program);

    if (!ok) {
        fprintf(stderr, "Parsing failed.\n");
    } else {
        printf("Parsing succeeded.\n");
    }

    js_ast_free(program);
    js_parser_destroy(ctx);
    free(source);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
