#include "js_parser_api.h"
#include "js_lexer.h"
#include "js_token.h"
#include "js_ast.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#ifdef DEBUG_TOKENS
#define TRACE_LEX_SUPPORTED 1
#else
#define TRACE_LEX_SUPPORTED 0
#endif

typedef struct {
    bool check;
    bool tokens;
    bool ast;
    bool pretty;
    bool asi;
    bool dot;
    const char *dot_path;
    bool trace_parse;
    bool trace_lex;
    bool help;
    const char *scan_dir;
    const char *log_path;
} cli_options_t;

typedef struct {
    FILE *stream;
    size_t next_id;
} dot_ctx_t;

typedef struct folder_stat_t {
    char *path;
    size_t success;
    size_t failed;
    struct folder_stat_t *next;
} folder_stat_t;

typedef struct {
    bool seen;
    char message[256];
    uint32_t line;
    uint32_t column;
} captured_diag_t;

typedef struct {
    size_t total_js_files;
    size_t processed;
    FILE *log_file;
    folder_stat_t *folders;
    bool had_error;
} scan_context_t;

static size_t dot_emit_node(dot_ctx_t *ctx, const js_ast_node_t *node);
static void dot_connect(dot_ctx_t *ctx, size_t parent, const js_ast_node_t *child);
static void dot_connect_list(dot_ctx_t *ctx, size_t parent, const js_ast_node_list_t *list);

static void print_usage(const char *prog_name);
static void print_diagnostic(const js_diagnostic_t *diag, void *user_data);
static bool parse_arguments(int argc, char **argv, cli_options_t *options, const char **path_out);
static char *read_source_file(const char *path, size_t *size_out);
static bool run_lexer_modes(const char *source, const cli_options_t *options);
static bool run_parser_modes(const char *source, const cli_options_t *options);
static bool run_scan_mode(const cli_options_t *options);
static const char *token_kind_name(js_token_kind_t kind);
static void print_token_line(const char *prefix, const js_token_t *token);
static void print_escaped_lexeme(FILE *out, const js_token_t *token);
static const char *asi_reason_name(js_asi_reason_t reason);
static void print_ast(const js_ast_node_t *node, FILE *out, int indent);
static void print_ast_list(const js_ast_node_list_t *list, FILE *out, int indent);
static const char *ast_kind_name(js_ast_kind_t kind);
static const char *var_kind_keyword(js_ast_var_kind_t kind);
static const char *binary_op_symbol(js_ast_binary_op_t op);
static const char *assignment_op_symbol(js_ast_assignment_op_t op);
static const char *unary_op_symbol(js_ast_unary_op_t op);
static const char *update_op_symbol(js_ast_update_op_t op);
static void print_labeled_node(const char *label, const js_ast_node_t *node, FILE *out, int indent);
static void print_labeled_list(const char *label, const js_ast_node_list_t *list, FILE *out, int indent);
static void pretty_print_program(const js_ast_node_t *program, FILE *out);
static void pretty_print_statement(const js_ast_node_t *node, FILE *out, int indent);
static void pretty_print_expression(const js_ast_node_t *node, FILE *out);
static void pretty_print_expression_list(const js_ast_node_list_t *list, FILE *out);
static void pretty_print_variable_declaration(const js_ast_node_t *node, FILE *out, int indent, bool in_for_header);
static bool write_dot_file(const js_ast_node_t *root, const char *path);
static void free_folder_stats(folder_stat_t *head);
static bool walk_directory(const char *path, bool (*on_file)(const char *, void *), void *user_data);
static bool is_js_file(const char *path);
static folder_stat_t *get_or_create_folder(folder_stat_t **head, const char *dir_path);
static bool parse_file_for_scan(const char *path, captured_diag_t *diag_out);

int main(int argc, char **argv) {
    cli_options_t options = {0};
    const char *path = NULL;

    if (!parse_arguments(argc, argv, &options, &path)) {
        return EXIT_FAILURE;
    }

    if (options.help) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (options.scan_dir) {
        return run_scan_mode(&options) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    char *source = read_source_file(path, NULL);
    if (!source) {
        return EXIT_FAILURE;
    }

    bool ok = true;
    if (!run_lexer_modes(source, &options)) {
        ok = false;
    }

    bool need_parse = options.check || options.ast || options.pretty || options.dot || options.trace_parse;
    if (need_parse) {
        if (!run_parser_modes(source, &options)) {
            ok = false;
        }
    }

    free(source);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [options] <file.js>\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --check          Parse and report success/failure (default when no other actions).\n");
    fprintf(stderr, "  --tokens         Dump the token stream with line/column info.\n");
    fprintf(stderr, "  --asi            List Automatic Semicolon Insertion events.\n");
    fprintf(stderr, "  --ast            Print the AST as S-expressions.\n");
    fprintf(stderr, "  --dot <path>     Write the AST as a Graphviz DOT file.\n");
    fprintf(stderr, "  --pretty         Re-print the program with normalized formatting.\n");
    fprintf(stderr, "  --trace-parse    Enable the Bison parser trace (yydebug).\n");
    fprintf(stderr, "  --trace-lex      Trace lexer output (requires -DDEBUG_TOKENS).\n");
    fprintf(stderr, "  --scan-dir <dir> Recursively parse every .js file under <dir> and report results.\n");
    fprintf(stderr, "  --log <path>     Log scan results to <path> (used with --scan-dir, default: scan.log).\n");
    fprintf(stderr, "  --help           Show this message.\n");
}

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

static bool parse_arguments(int argc, char **argv, cli_options_t *options, const char **path_out) {
    *options = (cli_options_t){0};
    *path_out = NULL;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
            return true;
        } else if (strcmp(arg, "--check") == 0) {
            options->check = true;
        } else if (strcmp(arg, "--tokens") == 0) {
            options->tokens = true;
        } else if (strcmp(arg, "--ast") == 0) {
            options->ast = true;
        } else if (strcmp(arg, "--pretty") == 0) {
            options->pretty = true;
        } else if (strcmp(arg, "--asi") == 0) {
            options->asi = true;
        } else if (strcmp(arg, "--dot") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--dot requires an output path.\n");
                return false;
            }
            options->dot = true;
            options->dot_path = argv[++i];
        } else if (strcmp(arg, "--trace-parse") == 0) {
            options->trace_parse = true;
        } else if (strcmp(arg, "--trace-lex") == 0) {
#if TRACE_LEX_SUPPORTED
            options->trace_lex = true;
#else
            fprintf(stderr, "--trace-lex requires building with -DDEBUG_TOKENS.\n");
            return false;
#endif
        } else if (strcmp(arg, "--scan-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--scan-dir requires a directory path.\n");
                return false;
            }
            options->scan_dir = argv[++i];
        } else if (strcmp(arg, "--log") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--log requires a file path.\n");
                return false;
            }
            options->log_path = argv[++i];
        } else if (arg[0] == '-') {
            fprintf(stderr, "Unknown option '%s'.\n", arg);
            return false;
        } else {
            if (*path_out) {
                fprintf(stderr, "Only one input file can be specified.\n");
                return false;
            }
            *path_out = arg;
        }
    }

    if (options->scan_dir) {
        if (*path_out) {
            fprintf(stderr, "Cannot combine positional input file with --scan-dir.\n");
            return false;
        }
        if (!options->log_path) {
            options->log_path = "scan.log";
        }
        options->check = true;
        return true;
    }

    if (!options->dot && options->dot_path) {
        fprintf(stderr, "--dot path provided without enabling the flag.\n");
        return false;
    }

    if (!*path_out) {
        fprintf(stderr, "Input file is required.\n");
        return false;
    }

    bool parse_requested = options->check || options->ast || options->pretty || options->dot;
    bool lex_requested = options->tokens || options->asi || options->trace_lex;

    if (options->trace_parse && !parse_requested) {
        options->check = true;
        parse_requested = true;
    }

    if (!parse_requested && !lex_requested) {
        options->check = true;
    }

    return true;
}

static char *read_source_file(const char *path, size_t *size_out) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open '%s': %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Failed to seek '%s': %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "Failed to determine size of '%s': %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to rewind '%s': %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer) {
        fprintf(stderr, "Out of memory while reading '%s'.\n", path);
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    if (read != (size_t)length) {
        fprintf(stderr, "Failed to read '%s'.\n", path);
        free(buffer);
        return NULL;
    }
    buffer[length] = '\0';
    if (size_out) {
        *size_out = (size_t)length;
    }
    return buffer;
}

static void print_token_line(const char *prefix, const js_token_t *token) {
    printf("%s %4u:%-3u %-24s ", prefix, token->location.line, token->location.column, token_kind_name(token->kind));
    print_escaped_lexeme(stdout, token);
    if (token->inserted_via_asi) {
        printf(" [ASI:%s]", asi_reason_name(token->asi_reason));
    }
    printf("\n");
}

static bool run_lexer_modes(const char *source, const cli_options_t *options) {
    if (!options->tokens && !options->asi && !options->trace_lex) {
        return true;
    }

    js_lexer_t *lexer = js_lexer_create(print_diagnostic, NULL);
    if (!lexer) {
        fprintf(stderr, "Failed to create lexer.\n");
        return false;
    }
    if (!js_lexer_reset(lexer, source)) {
        fprintf(stderr, "Failed to initialize lexer.\n");
        js_lexer_destroy(lexer);
        return false;
    }

    js_token_t token;
    bool ok = true;
    while (true) {
        if (!js_lexer_next(lexer, &token)) {
            ok = false;
            break;
        }
        if (options->tokens) {
            print_token_line("[token]", &token);
        }
#if TRACE_LEX_SUPPORTED
        if (options->trace_lex) {
            print_token_line("[trace-lex]", &token);
        }
#endif
        if (options->asi && token.inserted_via_asi) {
            printf("[asi] %4u:%-3u reason=%s\n", token.location.line, token.location.column, asi_reason_name(token.asi_reason));
        }
        if (token.kind == JS_TOK_EOF || token.kind == JS_TOK_ERROR) {
            break;
        }
    }

    js_lexer_destroy(lexer);
    return ok;
}

static bool run_parser_modes(const char *source, const cli_options_t *options) {
    js_parser_context_t *ctx = js_parser_create(print_diagnostic, NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to create parser context.\n");
        return false;
    }

    js_parser_set_debug(options->trace_parse);

    js_ast_node_t *program = NULL;
    bool ok = js_parser_parse(ctx, source, &program);

    if (options->check) {
        if (ok) {
            printf("Parsing succeeded.\n");
        } else {
            printf("Parsing failed.\n");
        }
    }

    if (ok) {
        if (options->ast) {
            print_ast(program, stdout, 0);
        }
        if (options->pretty) {
            pretty_print_program(program, stdout);
        }
        if (options->dot && options->dot_path) {
            if (!write_dot_file(program, options->dot_path)) {
                ok = false;
            }
        }
    }

    js_ast_free(program);
    js_parser_destroy(ctx);
    return ok;
}

static void capture_diagnostic_callback(const js_diagnostic_t *diag, void *user_data) {
    captured_diag_t *capture = (captured_diag_t *)user_data;
    if (!capture || !diag) {
        return;
    }
    capture->seen = true;
    capture->line = diag->location.line;
    capture->column = diag->location.column;
    strncpy(capture->message, diag->message ? diag->message : "", sizeof(capture->message) - 1);
    capture->message[sizeof(capture->message) - 1] = '\0';
}

static bool parse_file_for_scan(const char *path, captured_diag_t *diag_out) {
    if (!path) {
        return false;
    }
    captured_diag_t local_diag = {0};
    if (diag_out) {
        *diag_out = (captured_diag_t){0};
    }

    char *source = read_source_file(path, NULL);
    if (!source) {
        if (diag_out) {
            diag_out->seen = true;
            strncpy(diag_out->message, "Failed to read file", sizeof(diag_out->message) - 1);
            diag_out->line = 0;
            diag_out->column = 0;
        }
        return false;
    }

    js_parser_context_t *ctx = js_parser_create(capture_diagnostic_callback, &local_diag);
    if (!ctx) {
        free(source);
        if (diag_out) {
            diag_out->seen = true;
            strncpy(diag_out->message, "Failed to create parser", sizeof(diag_out->message) - 1);
        }
        return false;
    }

    js_ast_node_t *program = NULL;
    bool ok = js_parser_parse(ctx, source, &program);
    js_ast_free(program);
    js_parser_destroy(ctx);
    free(source);

    if (diag_out && local_diag.seen) {
        *diag_out = local_diag;
    }

    if (!ok && diag_out && !diag_out->seen) {
        diag_out->seen = true;
        strncpy(diag_out->message, "Parse failed without diagnostic", sizeof(diag_out->message) - 1);
    }
    return ok;
}

static folder_stat_t *get_or_create_folder(folder_stat_t **head, const char *dir_path) {
    folder_stat_t *node = *head;
    while (node) {
        if (strcmp(node->path, dir_path) == 0) {
            return node;
        }
        node = node->next;
    }
    folder_stat_t *created = (folder_stat_t *)calloc(1, sizeof(folder_stat_t));
    if (!created) {
        return NULL;
    }
    created->path = strdup(dir_path);
    if (!created->path) {
        free(created);
        return NULL;
    }
    created->next = *head;
    *head = created;
    return created;
}

static void free_folder_stats(folder_stat_t *head) {
    while (head) {
        folder_stat_t *next = head->next;
        free(head->path);
        free(head);
        head = next;
    }
}

static const char *dirname_from_path(const char *path, char *buffer, size_t buffer_size) {
    if (!path || !buffer || buffer_size == 0) {
        return ".";
    }
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        strncpy(buffer, ".", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return buffer;
    }
    size_t len = (size_t)(last_slash - path);
    if (len >= buffer_size) {
        len = buffer_size - 1;
    }
    memcpy(buffer, path, len);
    buffer[len] = '\0';
    return buffer;
}

static bool is_js_file(const char *path) {
    if (!path) {
        return false;
    }
    size_t len = strlen(path);
    if (len < 3) {
        return false;
    }
    return (strcmp(path + len - 3, ".js") == 0);
}

static bool walk_directory(const char *path, bool (*on_file)(const char *, void *), void *user_data) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Failed to open directory '%s': %s\n", path, strerror(errno));
        return false;
    }
    struct dirent *entry;
    bool ok = true;
    while (ok && (entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child_path[PATH_MAX];
        int written = snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(child_path)) {
            fprintf(stderr, "Path too long, skipping '%s/%s'\n", path, entry->d_name);
            continue;
        }
        struct stat st;
        if (stat(child_path, &st) != 0) {
            fprintf(stderr, "Failed to stat '%s': %s\n", child_path, strerror(errno));
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            ok = walk_directory(child_path, on_file, user_data);
        } else if (S_ISREG(st.st_mode)) {
            if (is_js_file(child_path)) {
                ok = on_file(child_path, user_data);
            }
        }
    }
    closedir(dir);
    return ok;
}

static bool count_files_callback(const char *path, void *user_data) {
    (void)path;
    scan_context_t *ctx = (scan_context_t *)user_data;
    if (ctx) {
        ctx->total_js_files++;
    }
    return true;
}

static bool parse_file_callback(const char *path, void *user_data) {
    scan_context_t *ctx = (scan_context_t *)user_data;
    if (!ctx) {
        return false;
    }
    captured_diag_t diag = {0};
    bool ok = parse_file_for_scan(path, &diag);
    ctx->processed++;

    const char *status = ok ? "SUCCESS" : "ERROR";
    if (ctx->log_file) {
        if (ok) {
            fprintf(ctx->log_file, "%s %s\n", path, status);
        } else {
            fprintf(ctx->log_file, "%s %s %u:%u %s\n", path, status, diag.line, diag.column, diag.message[0] ? diag.message : "unknown-error");
        }
        fflush(ctx->log_file);
    }

    if (ok) {
        printf("[%zu/%zu] %s -> ok\n", ctx->processed, ctx->total_js_files, path);
    } else {
        printf("[%zu/%zu] %s -> syntax error at %u:%u %s\n", ctx->processed, ctx->total_js_files, path, diag.line, diag.column, diag.message[0] ? diag.message : "unknown-error");
    }

    char dir_buffer[PATH_MAX];
    const char *dir_path = dirname_from_path(path, dir_buffer, sizeof(dir_buffer));
    folder_stat_t *stat = get_or_create_folder(&ctx->folders, dir_path);
    if (stat) {
        if (ok) {
            stat->success++;
        } else {
            stat->failed++;
        }
    }

    if (!ok) {
        ctx->had_error = true;
    }
    return true;
}

static bool run_scan_mode(const cli_options_t *options) {
    if (!options || !options->scan_dir) {
        fprintf(stderr, "Scan mode requires --scan-dir.\n");
        return false;
    }

    scan_context_t ctx = {0};

    if (!walk_directory(options->scan_dir, count_files_callback, &ctx)) {
        fprintf(stderr, "Failed while counting files under '%s'.\n", options->scan_dir);
        return false;
    }

    if (ctx.total_js_files == 0) {
        fprintf(stderr, "No .js files found under '%s'.\n", options->scan_dir);
        return false;
    }

    ctx.log_file = fopen(options->log_path ? options->log_path : "scan.log", "w");
    if (!ctx.log_file) {
        fprintf(stderr, "Failed to open log file '%s': %s\n", options->log_path, strerror(errno));
        return false;
    }

    printf("Parsing %zu JavaScript files under '%s' (logging to %s)\n", ctx.total_js_files, options->scan_dir, options->log_path ? options->log_path : "scan.log");

    bool ok = walk_directory(options->scan_dir, parse_file_callback, &ctx);
    fclose(ctx.log_file);

    printf("\nSummary by directory:\n");
    for (folder_stat_t *it = ctx.folders; it; it = it->next) {
        printf("  %s : success=%zu, failed=%zu\n", it->path, it->success, it->failed);
    }

    free_folder_stats(ctx.folders);
    return ok && !ctx.had_error;
}

static const char *token_kind_name(js_token_kind_t kind) {
#define TOKEN_CASE(name) case name: return #name;
    switch (kind) {
        TOKEN_CASE(JS_TOK_EOF)
        TOKEN_CASE(JS_TOK_ERROR)
        TOKEN_CASE(JS_TOK_IDENTIFIER)
        TOKEN_CASE(JS_TOK_NUMBER)
        TOKEN_CASE(JS_TOK_STRING)
        TOKEN_CASE(JS_TOK_TEMPLATE_HEAD)
        TOKEN_CASE(JS_TOK_TEMPLATE_MIDDLE)
        TOKEN_CASE(JS_TOK_TEMPLATE_TAIL)
        TOKEN_CASE(JS_TOK_REGEX)
        TOKEN_CASE(JS_TOK_TRUE)
        TOKEN_CASE(JS_TOK_FALSE)
        TOKEN_CASE(JS_TOK_NULL)
        TOKEN_CASE(JS_TOK_THIS)
        TOKEN_CASE(JS_TOK_SUPER)
        TOKEN_CASE(JS_TOK_KW_BREAK)
        TOKEN_CASE(JS_TOK_KW_CASE)
        TOKEN_CASE(JS_TOK_KW_CATCH)
        TOKEN_CASE(JS_TOK_KW_CLASS)
        TOKEN_CASE(JS_TOK_KW_CONST)
        TOKEN_CASE(JS_TOK_KW_CONTINUE)
        TOKEN_CASE(JS_TOK_KW_DEBUGGER)
        TOKEN_CASE(JS_TOK_KW_DEFAULT)
        TOKEN_CASE(JS_TOK_KW_DELETE)
        TOKEN_CASE(JS_TOK_KW_DO)
        TOKEN_CASE(JS_TOK_KW_ELSE)
        TOKEN_CASE(JS_TOK_KW_EXPORT)
        TOKEN_CASE(JS_TOK_KW_EXTENDS)
        TOKEN_CASE(JS_TOK_KW_FINALLY)
        TOKEN_CASE(JS_TOK_KW_FOR)
        TOKEN_CASE(JS_TOK_KW_FUNCTION)
        TOKEN_CASE(JS_TOK_KW_IF)
        TOKEN_CASE(JS_TOK_KW_IMPORT)
        TOKEN_CASE(JS_TOK_KW_IN)
        TOKEN_CASE(JS_TOK_KW_INSTANCEOF)
        TOKEN_CASE(JS_TOK_KW_LET)
        TOKEN_CASE(JS_TOK_KW_NEW)
        TOKEN_CASE(JS_TOK_KW_RETURN)
        TOKEN_CASE(JS_TOK_KW_STATIC)
        TOKEN_CASE(JS_TOK_KW_SWITCH)
        TOKEN_CASE(JS_TOK_KW_THROW)
        TOKEN_CASE(JS_TOK_KW_TRY)
        TOKEN_CASE(JS_TOK_KW_TYPEOF)
        TOKEN_CASE(JS_TOK_KW_VAR)
        TOKEN_CASE(JS_TOK_KW_VOID)
        TOKEN_CASE(JS_TOK_KW_WHILE)
        TOKEN_CASE(JS_TOK_KW_WITH)
        TOKEN_CASE(JS_TOK_KW_YIELD)
        TOKEN_CASE(JS_TOK_KW_AWAIT)
        TOKEN_CASE(JS_TOK_LPAREN)
        TOKEN_CASE(JS_TOK_RPAREN)
        TOKEN_CASE(JS_TOK_LBRACE)
        TOKEN_CASE(JS_TOK_RBRACE)
        TOKEN_CASE(JS_TOK_LBRACKET)
        TOKEN_CASE(JS_TOK_RBRACKET)
        TOKEN_CASE(JS_TOK_SEMICOLON)
        TOKEN_CASE(JS_TOK_COMMA)
        TOKEN_CASE(JS_TOK_DOT)
        TOKEN_CASE(JS_TOK_ELLIPSIS)
        TOKEN_CASE(JS_TOK_COLON)
        TOKEN_CASE(JS_TOK_QUESTION)
        TOKEN_CASE(JS_TOK_BACKTICK)
        TOKEN_CASE(JS_TOK_ARROW)
        TOKEN_CASE(JS_TOK_PLUS)
        TOKEN_CASE(JS_TOK_MINUS)
        TOKEN_CASE(JS_TOK_STAR)
        TOKEN_CASE(JS_TOK_SLASH)
        TOKEN_CASE(JS_TOK_PERCENT)
        TOKEN_CASE(JS_TOK_PLUS_PLUS)
        TOKEN_CASE(JS_TOK_MINUS_MINUS)
        TOKEN_CASE(JS_TOK_STAR_STAR)
        TOKEN_CASE(JS_TOK_ASSIGN)
        TOKEN_CASE(JS_TOK_PLUS_ASSIGN)
        TOKEN_CASE(JS_TOK_MINUS_ASSIGN)
        TOKEN_CASE(JS_TOK_STAR_ASSIGN)
        TOKEN_CASE(JS_TOK_SLASH_ASSIGN)
        TOKEN_CASE(JS_TOK_PERCENT_ASSIGN)
        TOKEN_CASE(JS_TOK_LSHIFT_ASSIGN)
        TOKEN_CASE(JS_TOK_RSHIFT_ASSIGN)
        TOKEN_CASE(JS_TOK_URSHIFT_ASSIGN)
        TOKEN_CASE(JS_TOK_BIT_AND_ASSIGN)
        TOKEN_CASE(JS_TOK_BIT_OR_ASSIGN)
        TOKEN_CASE(JS_TOK_BIT_XOR_ASSIGN)
        TOKEN_CASE(JS_TOK_STAR_STAR_ASSIGN)
        TOKEN_CASE(JS_TOK_NULLISH_ASSIGN)
        TOKEN_CASE(JS_TOK_LOGICAL_AND_ASSIGN)
        TOKEN_CASE(JS_TOK_LOGICAL_OR_ASSIGN)
        TOKEN_CASE(JS_TOK_EQUAL)
        TOKEN_CASE(JS_TOK_NOT_EQUAL)
        TOKEN_CASE(JS_TOK_STRICT_EQUAL)
        TOKEN_CASE(JS_TOK_STRICT_NOT_EQUAL)
        TOKEN_CASE(JS_TOK_LT)
        TOKEN_CASE(JS_TOK_GT)
        TOKEN_CASE(JS_TOK_LE)
        TOKEN_CASE(JS_TOK_GE)
        TOKEN_CASE(JS_TOK_BIT_AND)
        TOKEN_CASE(JS_TOK_BIT_OR)
        TOKEN_CASE(JS_TOK_BIT_XOR)
        TOKEN_CASE(JS_TOK_BIT_NOT)
        TOKEN_CASE(JS_TOK_LSHIFT)
        TOKEN_CASE(JS_TOK_RSHIFT)
        TOKEN_CASE(JS_TOK_URSHIFT)
        TOKEN_CASE(JS_TOK_LOGICAL_AND)
        TOKEN_CASE(JS_TOK_LOGICAL_OR)
        TOKEN_CASE(JS_TOK_NULLISH_COALESCE)
        TOKEN_CASE(JS_TOK_NOT)
        default:
            return "<unknown-token>";
    }
#undef TOKEN_CASE
}

static const char *asi_reason_name(js_asi_reason_t reason) {
    switch (reason) {
        case JS_ASI_REASON_LINE_TERMINATOR:
            return "line-terminator";
        case JS_ASI_REASON_CLOSING_BRACE:
            return "closing-brace";
        case JS_ASI_REASON_EOF:
            return "eof";
        default:
            return "none";
    }
}

static void print_escaped_lexeme(FILE *out, const js_token_t *token) {
    fputc('"', out);
    for (size_t i = 0; i < token->length; ++i) {
        unsigned char ch = (unsigned char)token->lexeme[i];
        switch (ch) {
            case '\n':
                fputs("\\n", out);
                break;
            case '\r':
                fputs("\\r", out);
                break;
            case '\t':
                fputs("\\t", out);
                break;
            case '\\':
                fputs("\\\\", out);
                break;
            case '"':
                fputs("\\\"", out);
                break;
            default:
                if (ch < 0x20 || ch > 0x7E) {
                    fprintf(out, "\\x%02X", ch);
                } else {
                    fputc(ch, out);
                }
                break;
        }
    }
    fputc('"', out);
}

static void print_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; ++i) {
        fputc(' ', out);
    }
}

static void print_ast(const js_ast_node_t *node, FILE *out, int indent) {
    if (!node) {
        print_indent(out, indent);
        fprintf(out, "null\n");
        return;
    }

    switch (node->kind) {
        case JS_AST_PROGRAM:
            print_indent(out, indent);
            fprintf(out, "(Program\n");
            print_labeled_list("body", &node->data.program.body, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_BLOCK_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(BlockStatement\n");
            print_labeled_list("body", &node->data.block_statement.body, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_EXPRESSION_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(ExpressionStatement\n");
            print_ast(node->data.expression_statement.expression, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_EMPTY_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(EmptyStatement)\n");
            break;
        case JS_AST_RETURN_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(ReturnStatement\n");
            if (node->data.return_statement.has_argument) {
                print_ast(node->data.return_statement.argument, out, indent + 2);
            } else {
                print_indent(out, indent + 2);
                fprintf(out, "null\n");
            }
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_IF_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(IfStatement\n");
            print_labeled_node("test", node->data.if_statement.test, out, indent + 2);
            print_labeled_node("consequent", node->data.if_statement.consequent, out, indent + 2);
            print_labeled_node("alternate", node->data.if_statement.alternate, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_WHILE_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(WhileStatement\n");
            print_labeled_node("test", node->data.while_statement.test, out, indent + 2);
            print_labeled_node("body", node->data.while_statement.body, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_DO_WHILE_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(DoWhileStatement\n");
            print_labeled_node("body", node->data.do_while_statement.body, out, indent + 2);
            print_labeled_node("test", node->data.do_while_statement.test, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_FOR_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(ForStatement\n");
            print_labeled_node("init", node->data.for_statement.init, out, indent + 2);
            print_labeled_node("test", node->data.for_statement.test, out, indent + 2);
            print_labeled_node("update", node->data.for_statement.update, out, indent + 2);
            print_labeled_node("body", node->data.for_statement.body, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_BREAK_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(BreakStatement)\n");
            break;
        case JS_AST_CONTINUE_STATEMENT:
            print_indent(out, indent);
            fprintf(out, "(ContinueStatement)\n");
            break;
        case JS_AST_VARIABLE_DECLARATION:
            print_indent(out, indent);
            fprintf(out, "(VariableDeclaration kind=%s\n", var_kind_keyword(node->data.variable_declaration.kind));
            print_ast_list(&node->data.variable_declaration.declarators, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_VARIABLE_DECLARATOR:
            print_indent(out, indent);
            fprintf(out, "(VariableDeclarator\n");
            print_ast(node->data.variable_declarator.id, out, indent + 2);
            print_ast(node->data.variable_declarator.init, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_IDENTIFIER:
            print_indent(out, indent);
            fprintf(out, "(Identifier %s)\n", node->data.identifier.name ? node->data.identifier.name : "<anon>");
            break;
        case JS_AST_LITERAL:
            print_indent(out, indent);
            switch (node->data.literal.kind) {
                case JS_AST_LITERAL_NUMBER:
                    fprintf(out, "(Literal number %.17g)\n", node->data.literal.value.number);
                    break;
                case JS_AST_LITERAL_STRING:
                    fprintf(out, "(Literal string ");
                    if (node->data.literal.value.string) {
                        fputs(node->data.literal.value.string, out);
                    }
                    fprintf(out, ")\n");
                    break;
                case JS_AST_LITERAL_BOOLEAN:
                    fprintf(out, "(Literal boolean %s)\n", node->data.literal.value.boolean ? "true" : "false");
                    break;
                case JS_AST_LITERAL_NULL:
                    fprintf(out, "(Literal null)\n");
                    break;
            }
            break;
        case JS_AST_BINARY_EXPRESSION:
            print_indent(out, indent);
            fprintf(out, "(BinaryExpression op=%s\n", binary_op_symbol(node->data.binary_expression.op));
            print_ast(node->data.binary_expression.left, out, indent + 2);
            print_ast(node->data.binary_expression.right, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_ASSIGNMENT_EXPRESSION:
            print_indent(out, indent);
            fprintf(out, "(AssignmentExpression op=%s\n", assignment_op_symbol(node->data.assignment_expression.op));
            print_ast(node->data.assignment_expression.left, out, indent + 2);
            print_ast(node->data.assignment_expression.right, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_UNARY_EXPRESSION:
            print_indent(out, indent);
            fprintf(out, "(UnaryExpression op=%s prefix=%s\n", unary_op_symbol(node->data.unary_expression.op), node->data.unary_expression.prefix ? "true" : "false");
            print_ast(node->data.unary_expression.argument, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_UPDATE_EXPRESSION:
            print_indent(out, indent);
            fprintf(out, "(UpdateExpression op=%s prefix=%s\n", update_op_symbol(node->data.update_expression.op), node->data.update_expression.prefix ? "true" : "false");
            print_ast(node->data.update_expression.argument, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_CONDITIONAL_EXPRESSION:
            print_indent(out, indent);
            fprintf(out, "(ConditionalExpression\n");
            print_labeled_node("test", node->data.conditional_expression.test, out, indent + 2);
            print_labeled_node("consequent", node->data.conditional_expression.consequent, out, indent + 2);
            print_labeled_node("alternate", node->data.conditional_expression.alternate, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_CALL_EXPRESSION:
            print_indent(out, indent);
            fprintf(out, "(CallExpression\n");
            print_labeled_node("callee", node->data.call_expression.callee, out, indent + 2);
            print_labeled_list("arguments", &node->data.call_expression.arguments, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_MEMBER_EXPRESSION:
            print_indent(out, indent);
            fprintf(out, "(MemberExpression computed=%s\n", node->data.member_expression.computed ? "true" : "false");
            print_labeled_node("object", node->data.member_expression.object, out, indent + 2);
            print_labeled_node("property", node->data.member_expression.property, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        case JS_AST_SEQUENCE_EXPRESSION:
            print_indent(out, indent);
            fprintf(out, "(SequenceExpression\n");
            print_ast_list(&node->data.sequence_expression.expressions, out, indent + 2);
            print_indent(out, indent);
            fprintf(out, ")\n");
            break;
        default:
            print_indent(out, indent);
            fprintf(out, "(%s)\n", ast_kind_name(node->kind));
            break;
    }
}

static void print_ast_list(const js_ast_node_list_t *list, FILE *out, int indent) {
    if (!list || !list->items) {
        return;
    }
    for (size_t i = 0; i < list->length; ++i) {
        print_ast(list->items[i], out, indent);
    }
}

static void print_labeled_node(const char *label, const js_ast_node_t *node, FILE *out, int indent) {
    print_indent(out, indent);
    fprintf(out, "(%s\n", label);
    if (node) {
        print_ast(node, out, indent + 2);
    } else {
        print_indent(out, indent + 2);
        fprintf(out, "null\n");
    }
    print_indent(out, indent);
    fprintf(out, ")\n");
}

static void print_labeled_list(const char *label, const js_ast_node_list_t *list, FILE *out, int indent) {
    print_indent(out, indent);
    fprintf(out, "(%s\n", label);
    if (list && list->items && list->length > 0) {
        for (size_t i = 0; i < list->length; ++i) {
            print_ast(list->items[i], out, indent + 2);
        }
    } else {
        print_indent(out, indent + 2);
        fprintf(out, "[]\n");
    }
    print_indent(out, indent);
    fprintf(out, ")\n");
}

static const char *ast_kind_name(js_ast_kind_t kind) {
    switch (kind) {
        case JS_AST_PROGRAM: return "Program";
        case JS_AST_BLOCK_STATEMENT: return "BlockStatement";
        case JS_AST_EXPRESSION_STATEMENT: return "ExpressionStatement";
        case JS_AST_EMPTY_STATEMENT: return "EmptyStatement";
        case JS_AST_RETURN_STATEMENT: return "ReturnStatement";
        case JS_AST_IF_STATEMENT: return "IfStatement";
        case JS_AST_WHILE_STATEMENT: return "WhileStatement";
        case JS_AST_DO_WHILE_STATEMENT: return "DoWhileStatement";
        case JS_AST_FOR_STATEMENT: return "ForStatement";
        case JS_AST_BREAK_STATEMENT: return "BreakStatement";
        case JS_AST_CONTINUE_STATEMENT: return "ContinueStatement";
        case JS_AST_VARIABLE_DECLARATION: return "VariableDeclaration";
        case JS_AST_VARIABLE_DECLARATOR: return "VariableDeclarator";
        case JS_AST_FUNCTION_DECLARATION: return "FunctionDeclaration";
        case JS_AST_FUNCTION_EXPRESSION: return "FunctionExpression";
        case JS_AST_IDENTIFIER: return "Identifier";
        case JS_AST_LITERAL: return "Literal";
        case JS_AST_BINARY_EXPRESSION: return "BinaryExpression";
        case JS_AST_ASSIGNMENT_EXPRESSION: return "AssignmentExpression";
        case JS_AST_UNARY_EXPRESSION: return "UnaryExpression";
        case JS_AST_UPDATE_EXPRESSION: return "UpdateExpression";
        case JS_AST_CONDITIONAL_EXPRESSION: return "ConditionalExpression";
        case JS_AST_CALL_EXPRESSION: return "CallExpression";
        case JS_AST_MEMBER_EXPRESSION: return "MemberExpression";
        case JS_AST_ARRAY_EXPRESSION: return "ArrayExpression";
        case JS_AST_OBJECT_EXPRESSION: return "ObjectExpression";
        case JS_AST_PROPERTY: return "Property";
        case JS_AST_SEQUENCE_EXPRESSION: return "SequenceExpression";
    }
    return "<unknown>";
}

static const char *var_kind_keyword(js_ast_var_kind_t kind) {
    switch (kind) {
        case JS_AST_VAR_VAR:
            return "var";
        case JS_AST_VAR_LET:
            return "let";
        case JS_AST_VAR_CONST:
            return "const";
    }
    return "var";
}

static const char *binary_op_symbol(js_ast_binary_op_t op) {
    switch (op) {
        case JS_AST_BINARY_OR: return "|";
        case JS_AST_BINARY_XOR: return "^";
        case JS_AST_BINARY_AND: return "&";
        case JS_AST_BINARY_EQUAL: return "==";
        case JS_AST_BINARY_NOT_EQUAL: return "!=";
        case JS_AST_BINARY_STRICT_EQUAL: return "===";
        case JS_AST_BINARY_STRICT_NOT_EQUAL: return "!==";
        case JS_AST_BINARY_LT: return "<";
        case JS_AST_BINARY_LE: return "<=";
        case JS_AST_BINARY_GT: return ">";
        case JS_AST_BINARY_GE: return ">=";
        case JS_AST_BINARY_INSTANCEOF: return "instanceof";
        case JS_AST_BINARY_IN: return "in";
        case JS_AST_BINARY_LSHIFT: return "<<";
        case JS_AST_BINARY_RSHIFT: return ">>";
        case JS_AST_BINARY_URSHIFT: return ">>>";
        case JS_AST_BINARY_ADD: return "+";
        case JS_AST_BINARY_SUB: return "-";
        case JS_AST_BINARY_MUL: return "*";
        case JS_AST_BINARY_DIV: return "/";
        case JS_AST_BINARY_MOD: return "%";
        case JS_AST_BINARY_EXP: return "**";
        case JS_AST_BINARY_LOGICAL_OR: return "||";
        case JS_AST_BINARY_LOGICAL_AND: return "&&";
        case JS_AST_BINARY_NULLISH_COALESCING: return "??";
    }
    return "?";
}

static const char *assignment_op_symbol(js_ast_assignment_op_t op) {
    switch (op) {
        case JS_AST_ASSIGN_EQ: return "=";
        case JS_AST_ASSIGN_ADD: return "+=";
        case JS_AST_ASSIGN_SUB: return "-=";
        case JS_AST_ASSIGN_MUL: return "*=";
        case JS_AST_ASSIGN_DIV: return "/=";
        case JS_AST_ASSIGN_MOD: return "%=";
        case JS_AST_ASSIGN_LSHIFT: return "<<=";
        case JS_AST_ASSIGN_RSHIFT: return ">>=";
        case JS_AST_ASSIGN_URSHIFT: return ">>>=";
        case JS_AST_ASSIGN_AND: return "&=";
        case JS_AST_ASSIGN_OR: return "|=";
        case JS_AST_ASSIGN_XOR: return "^=";
        case JS_AST_ASSIGN_EXP: return "**=";
        case JS_AST_ASSIGN_NULLISH: return "\?\?=";
        case JS_AST_ASSIGN_LOGICAL_AND: return "&&=";
        case JS_AST_ASSIGN_LOGICAL_OR: return "||=";
    }
    return "=";
}

static const char *unary_op_symbol(js_ast_unary_op_t op) {
    switch (op) {
        case JS_AST_UNARY_PLUS: return "+";
        case JS_AST_UNARY_MINUS: return "-";
        case JS_AST_UNARY_BIT_NOT: return "~";
        case JS_AST_UNARY_LOGICAL_NOT: return "!";
        case JS_AST_UNARY_TYPEOF: return "typeof";
        case JS_AST_UNARY_VOID: return "void";
        case JS_AST_UNARY_DELETE: return "delete";
    }
    return "?";
}

static const char *update_op_symbol(js_ast_update_op_t op) {
    switch (op) {
        case JS_AST_UPDATE_INCREMENT: return "++";
        case JS_AST_UPDATE_DECREMENT: return "--";
    }
    return "++";
}

static void pretty_print_program(const js_ast_node_t *program, FILE *out) {
    if (!program || program->kind != JS_AST_PROGRAM) {
        fprintf(out, "// <no program>\n");
        return;
    }
    const js_ast_node_list_t *body = &program->data.program.body;
    if (body && body->items) {
        for (size_t i = 0; i < body->length; ++i) {
            pretty_print_statement(body->items[i], out, 0);
        }
    }
}

static void emit_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; ++i) {
        fputc(' ', out);
    }
}

static void pretty_print_statement(const js_ast_node_t *node, FILE *out, int indent) {
    if (!node) {
        return;
    }
    switch (node->kind) {
        case JS_AST_BLOCK_STATEMENT: {
            emit_indent(out, indent);
            fprintf(out, "{\n");
            const js_ast_node_list_t *list = &node->data.block_statement.body;
            if (list && list->items) {
                for (size_t i = 0; i < list->length; ++i) {
                    pretty_print_statement(list->items[i], out, indent + 2);
                }
            }
            emit_indent(out, indent);
            fprintf(out, "}\n");
            break;
        }
        case JS_AST_VARIABLE_DECLARATION:
            pretty_print_variable_declaration(node, out, indent, false);
            fprintf(out, "\n");
            break;
        case JS_AST_EXPRESSION_STATEMENT:
            emit_indent(out, indent);
            pretty_print_expression(node->data.expression_statement.expression, out);
            fprintf(out, ";\n");
            break;
        case JS_AST_EMPTY_STATEMENT:
            emit_indent(out, indent);
            fprintf(out, ";\n");
            break;
        case JS_AST_RETURN_STATEMENT:
            emit_indent(out, indent);
            fprintf(out, "return");
            if (node->data.return_statement.has_argument && node->data.return_statement.argument) {
                fputc(' ', out);
                pretty_print_expression(node->data.return_statement.argument, out);
            }
            fprintf(out, ";\n");
            break;
        case JS_AST_IF_STATEMENT:
            emit_indent(out, indent);
            fprintf(out, "if (");
            pretty_print_expression(node->data.if_statement.test, out);
            fprintf(out, ")\n");
            pretty_print_statement(node->data.if_statement.consequent, out, indent + 2);
            if (node->data.if_statement.alternate) {
                emit_indent(out, indent);
                fprintf(out, "else\n");
                pretty_print_statement(node->data.if_statement.alternate, out, indent + 2);
            }
            break;
        case JS_AST_WHILE_STATEMENT:
            emit_indent(out, indent);
            fprintf(out, "while (");
            pretty_print_expression(node->data.while_statement.test, out);
            fprintf(out, ")\n");
            pretty_print_statement(node->data.while_statement.body, out, indent + 2);
            break;
        case JS_AST_DO_WHILE_STATEMENT:
            emit_indent(out, indent);
            fprintf(out, "do\n");
            pretty_print_statement(node->data.do_while_statement.body, out, indent + 2);
            emit_indent(out, indent);
            fprintf(out, "while (");
            pretty_print_expression(node->data.do_while_statement.test, out);
            fprintf(out, ");\n");
            break;
        case JS_AST_FOR_STATEMENT:
            emit_indent(out, indent);
            fprintf(out, "for (");
            if (node->data.for_statement.init) {
                if (node->data.for_statement.init->kind == JS_AST_VARIABLE_DECLARATION) {
                    pretty_print_variable_declaration(node->data.for_statement.init, out, 0, true);
                } else {
                    pretty_print_expression(node->data.for_statement.init, out);
                }
            }
            fprintf(out, "; ");
            if (node->data.for_statement.test) {
                pretty_print_expression(node->data.for_statement.test, out);
            }
            fprintf(out, "; ");
            if (node->data.for_statement.update) {
                pretty_print_expression(node->data.for_statement.update, out);
            }
            fprintf(out, ")\n");
            pretty_print_statement(node->data.for_statement.body, out, indent + 2);
            break;
        case JS_AST_BREAK_STATEMENT:
            emit_indent(out, indent);
            fprintf(out, "break;\n");
            break;
        case JS_AST_CONTINUE_STATEMENT:
            emit_indent(out, indent);
            fprintf(out, "continue;\n");
            break;
        default:
            emit_indent(out, indent);
            fprintf(out, "/* unsupported node %s */\n", ast_kind_name(node->kind));
            break;
    }
}

static void pretty_print_variable_declaration(const js_ast_node_t *node, FILE *out, int indent, bool in_for_header) {
    if (!node || node->kind != JS_AST_VARIABLE_DECLARATION) {
        return;
    }
    if (!in_for_header) {
        emit_indent(out, indent);
    }
    fprintf(out, "%s ", var_kind_keyword(node->data.variable_declaration.kind));
    const js_ast_node_list_t *decls = &node->data.variable_declaration.declarators;
    if (decls && decls->items) {
        for (size_t i = 0; i < decls->length; ++i) {
            const js_ast_node_t *decl = decls->items[i];
            if (!decl || decl->kind != JS_AST_VARIABLE_DECLARATOR) {
                continue;
            }
            if (i > 0) {
                fprintf(out, ", ");
            }
            if (decl->data.variable_declarator.id) {
                pretty_print_expression(decl->data.variable_declarator.id, out);
            }
            if (decl->data.variable_declarator.init) {
                fprintf(out, " = ");
                pretty_print_expression(decl->data.variable_declarator.init, out);
            }
        }
    }
    if (!in_for_header) {
        fprintf(out, ";");
    }
}

static void pretty_print_expression(const js_ast_node_t *node, FILE *out) {
    if (!node) {
        fprintf(out, "null");
        return;
    }
    switch (node->kind) {
        case JS_AST_IDENTIFIER:
            fprintf(out, "%s", node->data.identifier.name ? node->data.identifier.name : "<anon>");
            break;
        case JS_AST_LITERAL:
            switch (node->data.literal.kind) {
                case JS_AST_LITERAL_NUMBER:
                    fprintf(out, "%.17g", node->data.literal.value.number);
                    break;
                case JS_AST_LITERAL_STRING:
                    fprintf(out, "\"");
                    if (node->data.literal.value.string) {
                        fputs(node->data.literal.value.string, out);
                    }
                    fprintf(out, "\"");
                    break;
                case JS_AST_LITERAL_BOOLEAN:
                    fprintf(out, "%s", node->data.literal.value.boolean ? "true" : "false");
                    break;
                case JS_AST_LITERAL_NULL:
                    fprintf(out, "null");
                    break;
            }
            break;
        case JS_AST_BINARY_EXPRESSION:
            fprintf(out, "(");
            pretty_print_expression(node->data.binary_expression.left, out);
            fprintf(out, " %s ", binary_op_symbol(node->data.binary_expression.op));
            pretty_print_expression(node->data.binary_expression.right, out);
            fprintf(out, ")");
            break;
        case JS_AST_ASSIGNMENT_EXPRESSION:
            fprintf(out, "(");
            pretty_print_expression(node->data.assignment_expression.left, out);
            fprintf(out, " %s ", assignment_op_symbol(node->data.assignment_expression.op));
            pretty_print_expression(node->data.assignment_expression.right, out);
            fprintf(out, ")");
            break;
        case JS_AST_UNARY_EXPRESSION:
            if (node->data.unary_expression.prefix) {
                fprintf(out, "%s", unary_op_symbol(node->data.unary_expression.op));
                if (node->data.unary_expression.op == JS_AST_UNARY_TYPEOF ||
                    node->data.unary_expression.op == JS_AST_UNARY_VOID ||
                    node->data.unary_expression.op == JS_AST_UNARY_DELETE) {
                    fputc(' ', out);
                }
                pretty_print_expression(node->data.unary_expression.argument, out);
            } else {
                pretty_print_expression(node->data.unary_expression.argument, out);
                fprintf(out, "%s", unary_op_symbol(node->data.unary_expression.op));
            }
            break;
        case JS_AST_UPDATE_EXPRESSION:
            if (node->data.update_expression.prefix) {
                fprintf(out, "%s", update_op_symbol(node->data.update_expression.op));
                pretty_print_expression(node->data.update_expression.argument, out);
            } else {
                pretty_print_expression(node->data.update_expression.argument, out);
                fprintf(out, "%s", update_op_symbol(node->data.update_expression.op));
            }
            break;
        case JS_AST_CONDITIONAL_EXPRESSION:
            fprintf(out, "(");
            pretty_print_expression(node->data.conditional_expression.test, out);
            fprintf(out, " ? ");
            pretty_print_expression(node->data.conditional_expression.consequent, out);
            fprintf(out, " : ");
            pretty_print_expression(node->data.conditional_expression.alternate, out);
            fprintf(out, ")");
            break;
        case JS_AST_CALL_EXPRESSION:
            pretty_print_expression(node->data.call_expression.callee, out);
            fputc('(', out);
            pretty_print_expression_list(&node->data.call_expression.arguments, out);
            fputc(')', out);
            break;
        case JS_AST_MEMBER_EXPRESSION:
            pretty_print_expression(node->data.member_expression.object, out);
            if (node->data.member_expression.computed) {
                fputc('[', out);
                pretty_print_expression(node->data.member_expression.property, out);
                fputc(']', out);
            } else {
                fputc('.', out);
                pretty_print_expression(node->data.member_expression.property, out);
            }
            break;
        case JS_AST_SEQUENCE_EXPRESSION:
            fputc('(', out);
            pretty_print_expression_list(&node->data.sequence_expression.expressions, out);
            fputc(')', out);
            break;
        default:
            fprintf(out, "/* %s */", ast_kind_name(node->kind));
            break;
    }
}

static void pretty_print_expression_list(const js_ast_node_list_t *list, FILE *out) {
    if (!list || !list->items || list->length == 0) {
        return;
    }
    for (size_t i = 0; i < list->length; ++i) {
        if (i > 0) {
            fprintf(out, ", ");
        }
        pretty_print_expression(list->items[i], out);
    }
}

static size_t dot_emit_node(dot_ctx_t *ctx, const js_ast_node_t *node) {
    size_t id = ctx->next_id++;
    if (!node) {
        fprintf(ctx->stream, "  n%zu [label=\"null\", shape=box, style=dashed];\n", id);
        return id;
    }

    fprintf(ctx->stream, "  n%zu [label=\"%s\"];\n", id, ast_kind_name(node->kind));

    switch (node->kind) {
        case JS_AST_PROGRAM:
            dot_connect_list(ctx, id, &node->data.program.body);
            break;
        case JS_AST_BLOCK_STATEMENT:
            dot_connect_list(ctx, id, &node->data.block_statement.body);
            break;
        case JS_AST_EXPRESSION_STATEMENT:
            dot_connect(ctx, id, node->data.expression_statement.expression);
            break;
        case JS_AST_RETURN_STATEMENT:
            dot_connect(ctx, id, node->data.return_statement.argument);
            break;
        case JS_AST_IF_STATEMENT:
            dot_connect(ctx, id, node->data.if_statement.test);
            dot_connect(ctx, id, node->data.if_statement.consequent);
            dot_connect(ctx, id, node->data.if_statement.alternate);
            break;
        case JS_AST_WHILE_STATEMENT:
            dot_connect(ctx, id, node->data.while_statement.test);
            dot_connect(ctx, id, node->data.while_statement.body);
            break;
        case JS_AST_DO_WHILE_STATEMENT:
            dot_connect(ctx, id, node->data.do_while_statement.body);
            dot_connect(ctx, id, node->data.do_while_statement.test);
            break;
        case JS_AST_FOR_STATEMENT:
            dot_connect(ctx, id, node->data.for_statement.init);
            dot_connect(ctx, id, node->data.for_statement.test);
            dot_connect(ctx, id, node->data.for_statement.update);
            dot_connect(ctx, id, node->data.for_statement.body);
            break;
        case JS_AST_VARIABLE_DECLARATION:
            dot_connect_list(ctx, id, &node->data.variable_declaration.declarators);
            break;
        case JS_AST_VARIABLE_DECLARATOR:
            dot_connect(ctx, id, node->data.variable_declarator.id);
            dot_connect(ctx, id, node->data.variable_declarator.init);
            break;
        case JS_AST_BINARY_EXPRESSION:
            dot_connect(ctx, id, node->data.binary_expression.left);
            dot_connect(ctx, id, node->data.binary_expression.right);
            break;
        case JS_AST_ASSIGNMENT_EXPRESSION:
            dot_connect(ctx, id, node->data.assignment_expression.left);
            dot_connect(ctx, id, node->data.assignment_expression.right);
            break;
        case JS_AST_UNARY_EXPRESSION:
            dot_connect(ctx, id, node->data.unary_expression.argument);
            break;
        case JS_AST_UPDATE_EXPRESSION:
            dot_connect(ctx, id, node->data.update_expression.argument);
            break;
        case JS_AST_CONDITIONAL_EXPRESSION:
            dot_connect(ctx, id, node->data.conditional_expression.test);
            dot_connect(ctx, id, node->data.conditional_expression.consequent);
            dot_connect(ctx, id, node->data.conditional_expression.alternate);
            break;
        case JS_AST_CALL_EXPRESSION:
            dot_connect(ctx, id, node->data.call_expression.callee);
            dot_connect_list(ctx, id, &node->data.call_expression.arguments);
            break;
        case JS_AST_MEMBER_EXPRESSION:
            dot_connect(ctx, id, node->data.member_expression.object);
            dot_connect(ctx, id, node->data.member_expression.property);
            break;
        case JS_AST_SEQUENCE_EXPRESSION:
            dot_connect_list(ctx, id, &node->data.sequence_expression.expressions);
            break;
        default:
            break;
    }

    return id;
}

static void dot_connect(dot_ctx_t *ctx, size_t parent, const js_ast_node_t *child) {
    size_t child_id = dot_emit_node(ctx, child);
    fprintf(ctx->stream, "  n%zu -> n%zu;\n", parent, child_id);
}

static void dot_connect_list(dot_ctx_t *ctx, size_t parent, const js_ast_node_list_t *list) {
    if (!list || !list->items) {
        return;
    }
    for (size_t i = 0; i < list->length; ++i) {
        dot_connect(ctx, parent, list->items[i]);
    }
}

static bool write_dot_file(const js_ast_node_t *root, const char *path) {
    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open '%s' for writing: %s\n", path, strerror(errno));
        return false;
    }

    dot_ctx_t ctx = { out, 0 };
    fprintf(out, "digraph AST {\n");
    dot_emit_node(&ctx, root);
    fprintf(out, "}\n");
    fclose(out);
    return true;
}
