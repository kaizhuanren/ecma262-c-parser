#include "js_ast.h"

#include <stdlib.h>
#include <string.h>

static void js_ast_node_list_free_nodes(js_ast_node_list_t *list) {
    if (!list || !list->items) {
        return;
    }
    for (size_t i = 0; i < list->length; ++i) {
        js_ast_free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->length = 0;
    list->capacity = 0;
}

js_ast_node_t *js_ast_node_new(js_ast_kind_t kind, js_source_location_t loc) {
    js_ast_node_t *node = (js_ast_node_t *)calloc(1, sizeof(js_ast_node_t));
    if (!node) {
        return NULL;
    }
    node->kind = kind;
    node->loc = loc;
    return node;
}

char *js_ast_strndup(const char *source, size_t length) {
    if (!source) {
        return NULL;
    }
    char *copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

char *js_ast_strdup(const char *source) {
    if (!source) {
        return NULL;
    }
    return js_ast_strndup(source, strlen(source));
}

js_ast_node_t *js_ast_make_identifier_slice(js_source_location_t loc, const char *start, size_t length) {
    js_ast_node_t *node = js_ast_node_new(JS_AST_IDENTIFIER, loc);
    if (!node) {
        return NULL;
    }
    node->data.identifier.name = js_ast_strndup(start, length);
    if (!node->data.identifier.name) {
        free(node);
        return NULL;
    }
    return node;
}

js_ast_node_t *js_ast_make_identifier(js_source_location_t loc, const char *name) {
    if (!name) {
        return NULL;
    }
    return js_ast_make_identifier_slice(loc, name, strlen(name));
}

js_ast_node_t *js_ast_make_literal_number(js_source_location_t loc, double value) {
    js_ast_node_t *node = js_ast_node_new(JS_AST_LITERAL, loc);
    if (!node) {
        return NULL;
    }
    node->data.literal.kind = JS_AST_LITERAL_NUMBER;
    node->data.literal.value.number = value;
    return node;
}

static char decode_escape(char ch) {
    switch (ch) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        default:
            return ch;
    }
}

js_ast_node_t *js_ast_make_literal_string_slice(js_source_location_t loc, const char *start, size_t length) {
    js_ast_node_t *node = js_ast_node_new(JS_AST_LITERAL, loc);
    if (!node) {
        return NULL;
    }
    node->data.literal.kind = JS_AST_LITERAL_STRING;
    if (!start || length < 2) {
        node->data.literal.value.string = js_ast_strdup("");
        if (!node->data.literal.value.string) {
            free(node);
            return NULL;
        }
        return node;
    }
    size_t capacity = length - 1;
    char *buffer = (char *)malloc(capacity);
    if (!buffer) {
        free(node);
        return NULL;
    }
    size_t out = 0;
    for (size_t i = 1; i + 1 < length; ++i) {
        char ch = start[i];
        if (ch == '\\' && (i + 1) < length - 1) {
            ch = decode_escape(start[++i]);
        }
        buffer[out++] = ch;
    }
    buffer[out] = '\0';
    node->data.literal.value.string = buffer;
    return node;
}

js_ast_node_t *js_ast_make_literal_string(js_source_location_t loc, const char *value) {
    if (!value) {
        return NULL;
    }
    return js_ast_make_literal_string_slice(loc, value, strlen(value));
}

js_ast_node_t *js_ast_make_literal_boolean(js_source_location_t loc, bool value) {
    js_ast_node_t *node = js_ast_node_new(JS_AST_LITERAL, loc);
    if (!node) {
        return NULL;
    }
    node->data.literal.kind = JS_AST_LITERAL_BOOLEAN;
    node->data.literal.value.boolean = value;
    return node;
}

js_ast_node_t *js_ast_make_literal_null(js_source_location_t loc) {
    js_ast_node_t *node = js_ast_node_new(JS_AST_LITERAL, loc);
    if (!node) {
        return NULL;
    }
    node->data.literal.kind = JS_AST_LITERAL_NULL;
    return node;
}

bool js_ast_node_list_append(js_ast_node_list_t *list, js_ast_node_t *node) {
    if (!list) {
        return false;
    }
    if (list->length == list->capacity) {
        size_t new_capacity = list->capacity ? list->capacity * 2 : 4;
        js_ast_node_t **new_items = (js_ast_node_t **)realloc(list->items, new_capacity * sizeof(js_ast_node_t *));
        if (!new_items) {
            return false;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->length++] = node;
    return true;
}

void js_ast_node_list_free(js_ast_node_list_t *list) {
    if (!list) {
        return;
    }
    free(list->items);
    list->items = NULL;
    list->length = 0;
    list->capacity = 0;
}

void js_ast_free(js_ast_node_t *node) {
    if (!node) {
        return;
    }

    switch (node->kind) {
        case JS_AST_PROGRAM:
            js_ast_node_list_free_nodes(&node->data.program.body);
            break;
        case JS_AST_BLOCK_STATEMENT:
            js_ast_node_list_free_nodes(&node->data.block_statement.body);
            break;
        case JS_AST_EXPRESSION_STATEMENT:
            js_ast_free(node->data.expression_statement.expression);
            break;
        case JS_AST_EMPTY_STATEMENT:
            break;
        case JS_AST_RETURN_STATEMENT:
            if (node->data.return_statement.has_argument) {
                js_ast_free(node->data.return_statement.argument);
            }
            break;
        case JS_AST_IF_STATEMENT:
            js_ast_free(node->data.if_statement.test);
            js_ast_free(node->data.if_statement.consequent);
            js_ast_free(node->data.if_statement.alternate);
            break;
        case JS_AST_WHILE_STATEMENT:
            js_ast_free(node->data.while_statement.test);
            js_ast_free(node->data.while_statement.body);
            break;
        case JS_AST_DO_WHILE_STATEMENT:
            js_ast_free(node->data.do_while_statement.body);
            js_ast_free(node->data.do_while_statement.test);
            break;
        case JS_AST_FOR_STATEMENT:
            js_ast_free(node->data.for_statement.init);
            js_ast_free(node->data.for_statement.test);
            js_ast_free(node->data.for_statement.update);
            js_ast_free(node->data.for_statement.body);
            break;
        case JS_AST_BREAK_STATEMENT:
            free(node->data.break_statement.label);
            break;
        case JS_AST_CONTINUE_STATEMENT:
            free(node->data.continue_statement.label);
            break;
        case JS_AST_VARIABLE_DECLARATION:
            js_ast_node_list_free_nodes(&node->data.variable_declaration.declarators);
            break;
        case JS_AST_VARIABLE_DECLARATOR:
            js_ast_free(node->data.variable_declarator.id);
            js_ast_free(node->data.variable_declarator.init);
            break;
        case JS_AST_FUNCTION_DECLARATION:
        case JS_AST_FUNCTION_EXPRESSION:
            js_ast_free(node->data.function.id);
            js_ast_node_list_free_nodes(&node->data.function.params);
            js_ast_free(node->data.function.body);
            break;
        case JS_AST_IDENTIFIER:
            free(node->data.identifier.name);
            break;
        case JS_AST_LITERAL:
            if (node->data.literal.kind == JS_AST_LITERAL_STRING) {
                free(node->data.literal.value.string);
            }
            break;
        case JS_AST_BINARY_EXPRESSION:
            js_ast_free(node->data.binary_expression.left);
            js_ast_free(node->data.binary_expression.right);
            break;
        case JS_AST_ASSIGNMENT_EXPRESSION:
            js_ast_free(node->data.assignment_expression.left);
            js_ast_free(node->data.assignment_expression.right);
            break;
        case JS_AST_UNARY_EXPRESSION:
            js_ast_free(node->data.unary_expression.argument);
            break;
        case JS_AST_UPDATE_EXPRESSION:
            js_ast_free(node->data.update_expression.argument);
            break;
        case JS_AST_CONDITIONAL_EXPRESSION:
            js_ast_free(node->data.conditional_expression.test);
            js_ast_free(node->data.conditional_expression.consequent);
            js_ast_free(node->data.conditional_expression.alternate);
            break;
        case JS_AST_CALL_EXPRESSION:
            js_ast_free(node->data.call_expression.callee);
            js_ast_node_list_free_nodes(&node->data.call_expression.arguments);
            break;
        case JS_AST_MEMBER_EXPRESSION:
            js_ast_free(node->data.member_expression.object);
            js_ast_free(node->data.member_expression.property);
            break;
        case JS_AST_ARRAY_EXPRESSION: {
            js_ast_node_list_t *elements = &node->data.array_expression.elements;
            if (elements->items) {
                for (size_t i = 0; i < elements->length; ++i) {
                    js_ast_free(elements->items[i]);
                }
            }
            js_ast_node_list_free(elements);
            break;
        }
        case JS_AST_OBJECT_EXPRESSION:
            js_ast_node_list_free_nodes(&node->data.object_expression.properties);
            break;
        case JS_AST_PROPERTY:
            free(node->data.property.key);
            js_ast_free(node->data.property.value);
            break;
        case JS_AST_SEQUENCE_EXPRESSION:
            js_ast_node_list_free_nodes(&node->data.sequence_expression.expressions);
            break;
    }

    free(node);
}
