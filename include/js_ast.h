#ifndef JS_AST_H
#define JS_AST_H

#include <stdbool.h>
#include <stddef.h>

#include "js_source.h"

typedef enum {
    JS_AST_PROGRAM = 0,
    JS_AST_BLOCK_STATEMENT,
    JS_AST_EXPRESSION_STATEMENT,
    JS_AST_EMPTY_STATEMENT,
    JS_AST_RETURN_STATEMENT,
    JS_AST_IF_STATEMENT,
    JS_AST_WHILE_STATEMENT,
    JS_AST_DO_WHILE_STATEMENT,
    JS_AST_FOR_STATEMENT,
    JS_AST_BREAK_STATEMENT,
    JS_AST_CONTINUE_STATEMENT,
    JS_AST_VARIABLE_DECLARATION,
    JS_AST_VARIABLE_DECLARATOR,
    JS_AST_FUNCTION_DECLARATION,
    JS_AST_FUNCTION_EXPRESSION,
    JS_AST_IDENTIFIER,
    JS_AST_LITERAL,
    JS_AST_BINARY_EXPRESSION,
    JS_AST_ASSIGNMENT_EXPRESSION,
    JS_AST_UNARY_EXPRESSION,
    JS_AST_UPDATE_EXPRESSION,
    JS_AST_CONDITIONAL_EXPRESSION,
    JS_AST_CALL_EXPRESSION,
    JS_AST_MEMBER_EXPRESSION,
    JS_AST_ARRAY_EXPRESSION,
    JS_AST_OBJECT_EXPRESSION,
    JS_AST_PROPERTY,
    JS_AST_SEQUENCE_EXPRESSION
} js_ast_kind_t;

typedef enum {
    JS_AST_VAR_VAR = 0,
    JS_AST_VAR_LET,
    JS_AST_VAR_CONST
} js_ast_var_kind_t;

typedef enum {
    JS_AST_LITERAL_NUMBER = 0,
    JS_AST_LITERAL_STRING,
    JS_AST_LITERAL_BOOLEAN,
    JS_AST_LITERAL_NULL
} js_ast_literal_kind_t;

typedef enum {
    JS_AST_BINARY_OR = 0,
    JS_AST_BINARY_XOR,
    JS_AST_BINARY_AND,
    JS_AST_BINARY_EQUAL,
    JS_AST_BINARY_NOT_EQUAL,
    JS_AST_BINARY_STRICT_EQUAL,
    JS_AST_BINARY_STRICT_NOT_EQUAL,
    JS_AST_BINARY_LT,
    JS_AST_BINARY_LE,
    JS_AST_BINARY_GT,
    JS_AST_BINARY_GE,
    JS_AST_BINARY_INSTANCEOF,
    JS_AST_BINARY_IN,
    JS_AST_BINARY_LSHIFT,
    JS_AST_BINARY_RSHIFT,
    JS_AST_BINARY_URSHIFT,
    JS_AST_BINARY_ADD,
    JS_AST_BINARY_SUB,
    JS_AST_BINARY_MUL,
    JS_AST_BINARY_DIV,
    JS_AST_BINARY_MOD,
    JS_AST_BINARY_EXP,
    JS_AST_BINARY_LOGICAL_OR,
    JS_AST_BINARY_LOGICAL_AND,
    JS_AST_BINARY_NULLISH_COALESCING
} js_ast_binary_op_t;

typedef enum {
    JS_AST_ASSIGN_EQ = 0,
    JS_AST_ASSIGN_ADD,
    JS_AST_ASSIGN_SUB,
    JS_AST_ASSIGN_MUL,
    JS_AST_ASSIGN_DIV,
    JS_AST_ASSIGN_MOD,
    JS_AST_ASSIGN_LSHIFT,
    JS_AST_ASSIGN_RSHIFT,
    JS_AST_ASSIGN_URSHIFT,
    JS_AST_ASSIGN_AND,
    JS_AST_ASSIGN_OR,
    JS_AST_ASSIGN_XOR,
    JS_AST_ASSIGN_EXP,
    JS_AST_ASSIGN_NULLISH,
    JS_AST_ASSIGN_LOGICAL_AND,
    JS_AST_ASSIGN_LOGICAL_OR
} js_ast_assignment_op_t;

typedef enum {
    JS_AST_UNARY_PLUS = 0,
    JS_AST_UNARY_MINUS,
    JS_AST_UNARY_BIT_NOT,
    JS_AST_UNARY_LOGICAL_NOT,
    JS_AST_UNARY_TYPEOF,
    JS_AST_UNARY_VOID,
    JS_AST_UNARY_DELETE
} js_ast_unary_op_t;

typedef enum {
    JS_AST_UPDATE_INCREMENT = 0,
    JS_AST_UPDATE_DECREMENT
} js_ast_update_op_t;

typedef enum {
    JS_AST_PROPERTY_DATA = 0
} js_ast_property_kind_t;

struct js_ast_node_t;
typedef struct js_ast_node_t js_ast_node_t;

typedef struct {
    struct js_ast_node_t **items;
    size_t length;
    size_t capacity;
} js_ast_node_list_t;

typedef struct {
    char *name;
} js_ast_identifier_t;

typedef struct {
    js_ast_literal_kind_t kind;
    union {
        double number;
        bool boolean;
        char *string;
    } value;
} js_ast_literal_t;

typedef struct {
    js_ast_node_list_t body;
} js_ast_program_t;

typedef struct {
    js_ast_node_list_t body;
} js_ast_block_statement_t;

typedef struct {
    struct js_ast_node_t *expression;
} js_ast_expression_statement_t;

typedef struct {
    js_ast_node_t *argument;
    bool has_argument;
} js_ast_return_statement_t;

typedef struct {
    js_ast_node_t *test;
    js_ast_node_t *consequent;
    js_ast_node_t *alternate;
} js_ast_if_statement_t;

typedef struct {
    js_ast_node_t *test;
    js_ast_node_t *body;
} js_ast_while_statement_t;

typedef struct {
    js_ast_node_t *body;
    js_ast_node_t *test;
} js_ast_do_while_statement_t;

typedef struct {
    js_ast_node_t *init;
    js_ast_node_t *test;
    js_ast_node_t *update;
    js_ast_node_t *body;
} js_ast_for_statement_t;

typedef struct {
    char *label;
} js_ast_break_statement_t;

typedef struct {
    char *label;
} js_ast_continue_statement_t;

typedef struct {
    js_ast_var_kind_t kind;
    js_ast_node_list_t declarators;
} js_ast_variable_declaration_t;

typedef struct {
    js_ast_node_t *id;
    js_ast_node_t *init;
} js_ast_variable_declarator_t;

typedef struct {
    js_ast_node_t *id;
    js_ast_node_list_t params;
    js_ast_node_t *body;
    bool is_async;
    bool is_generator;
    bool is_expression;
} js_ast_function_t;

typedef struct {
    js_ast_binary_op_t op;
    js_ast_node_t *left;
    js_ast_node_t *right;
} js_ast_binary_expression_t;

typedef struct {
    js_ast_assignment_op_t op;
    js_ast_node_t *left;
    js_ast_node_t *right;
} js_ast_assignment_expression_t;

typedef struct {
    js_ast_unary_op_t op;
    js_ast_node_t *argument;
    bool prefix;
} js_ast_unary_expression_t;

typedef struct {
    js_ast_update_op_t op;
    js_ast_node_t *argument;
    bool prefix;
} js_ast_update_expression_t;

typedef struct {
    js_ast_node_t *test;
    js_ast_node_t *consequent;
    js_ast_node_t *alternate;
} js_ast_conditional_expression_t;

typedef struct {
    js_ast_node_t *callee;
    js_ast_node_list_t arguments;
    bool is_new;
} js_ast_call_expression_t;

typedef struct {
    js_ast_node_t *object;
    js_ast_node_t *property;
    bool computed;
} js_ast_member_expression_t;

typedef struct {
    js_ast_node_list_t elements;
} js_ast_array_expression_t;

typedef struct {
    js_ast_node_list_t properties;
} js_ast_object_expression_t;

typedef struct {
    js_ast_property_kind_t kind;
    char *key;
    js_ast_node_t *value;
    bool computed;
    bool shorthand;
} js_ast_property_t;

typedef struct {
    js_ast_node_list_t expressions;
} js_ast_sequence_expression_t;

typedef struct js_ast_node_t {
    js_ast_kind_t kind;
    js_source_location_t loc;
    union {
        js_ast_program_t program;
        js_ast_block_statement_t block_statement;
        js_ast_expression_statement_t expression_statement;
        js_ast_return_statement_t return_statement;
        js_ast_if_statement_t if_statement;
        js_ast_while_statement_t while_statement;
        js_ast_do_while_statement_t do_while_statement;
        js_ast_for_statement_t for_statement;
        js_ast_break_statement_t break_statement;
        js_ast_continue_statement_t continue_statement;
        js_ast_variable_declaration_t variable_declaration;
        js_ast_variable_declarator_t variable_declarator;
        js_ast_function_t function;
        js_ast_identifier_t identifier;
        js_ast_literal_t literal;
        js_ast_binary_expression_t binary_expression;
        js_ast_assignment_expression_t assignment_expression;
        js_ast_unary_expression_t unary_expression;
        js_ast_update_expression_t update_expression;
        js_ast_conditional_expression_t conditional_expression;
        js_ast_call_expression_t call_expression;
        js_ast_member_expression_t member_expression;
        js_ast_array_expression_t array_expression;
        js_ast_object_expression_t object_expression;
        js_ast_property_t property;
        js_ast_sequence_expression_t sequence_expression;
    } data;
} js_ast_node_t;

js_ast_node_t *js_ast_node_new(js_ast_kind_t kind, js_source_location_t loc);
js_ast_node_t *js_ast_make_identifier(js_source_location_t loc, const char *name);
js_ast_node_t *js_ast_make_literal_number(js_source_location_t loc, double value);
js_ast_node_t *js_ast_make_literal_string(js_source_location_t loc, const char *value);
js_ast_node_t *js_ast_make_literal_boolean(js_source_location_t loc, bool value);
js_ast_node_t *js_ast_make_literal_null(js_source_location_t loc);

js_ast_node_t *js_ast_make_identifier_slice(js_source_location_t loc, const char *start, size_t length);
js_ast_node_t *js_ast_make_literal_string_slice(js_source_location_t loc, const char *start, size_t length);
bool js_ast_node_list_append(js_ast_node_list_t *list, js_ast_node_t *node);
void js_ast_node_list_free(js_ast_node_list_t *list);

char *js_ast_strdup(const char *source);
char *js_ast_strndup(const char *source, size_t length);

void js_ast_free(js_ast_node_t *node);

#endif /* JS_AST_H */
