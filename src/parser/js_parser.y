
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"
#include "js_token.h"

#define JS_PARSER_OOM_MESSAGE "Out of memory while building AST"

typedef struct YYLTYPE YYLTYPE;

static void parser_report(js_parser_context_t *ctx, const YYLTYPE *loc, const char *message);
static js_source_location_t loc_from_yy(const YYLTYPE *loc);
static void free_node_list(js_ast_node_list_t *list);
static bool append_node(js_parser_context_t *ctx, js_ast_node_list_t *list, js_ast_node_t *node, const YYLTYPE *loc);
static js_ast_node_t *make_binary(js_parser_context_t *ctx, js_ast_binary_op_t op, js_ast_node_t *left, js_ast_node_t *right, const YYLTYPE *loc);
static js_ast_node_t *make_unary(js_parser_context_t *ctx, js_ast_unary_op_t op, js_ast_node_t *argument, const YYLTYPE *loc);
static js_ast_node_t *make_update(js_parser_context_t *ctx, js_ast_update_op_t op, js_ast_node_t *argument, bool prefix, const YYLTYPE *loc);
static js_ast_node_t *make_member_expression(js_parser_context_t *ctx, js_ast_node_t *object, js_ast_node_t *property, bool computed, const YYLTYPE *loc);
static js_ast_node_t *make_call_expression(js_parser_context_t *ctx, js_ast_node_t *callee, js_ast_node_list_t args, const YYLTYPE *loc);
static js_ast_node_t *make_sequence(js_parser_context_t *ctx, js_ast_node_t *left, js_ast_node_t *right, const YYLTYPE *loc);
static js_ast_node_t *make_identifier_from_token(js_parser_context_t *ctx, const js_token_t *token, const YYLTYPE *loc);
static js_ast_node_t *make_number_from_token(js_parser_context_t *ctx, const js_token_t *token, const YYLTYPE *loc);
static js_ast_node_t *make_string_from_token(js_parser_context_t *ctx, const js_token_t *token, const YYLTYPE *loc);
static js_ast_node_t *make_function(js_parser_context_t *ctx, js_ast_node_t *id, js_ast_node_list_t params, js_ast_node_t *body, bool is_expression, const YYLTYPE *loc);
static void set_token_location(YYLTYPE *yylloc, const js_token_t *token);
static int js_parser_impllex(YYSTYPE *yylval, YYLTYPE *yylloc, js_parser_context_t *ctx);
static void js_parser_implerror(YYLTYPE *loc, js_parser_context_t *ctx, const char *msg);
%}

%code top {
    typedef union JS_PARSER_IMPLSTYPE JS_PARSER_IMPLSTYPE;
    typedef struct JS_PARSER_IMPLLTYPE JS_PARSER_IMPLLTYPE;
}

%code requires {
    #include <stdbool.h>
    #include <stddef.h>
    #include "js_ast.h"
    #include "js_token.h"
    #include "js_source.h"
}

%define api.pure full
%define api.prefix {js_parser_impl}
%define parse.error verbose
%define parse.trace
%locations

%parse-param { js_parser_context_t *ctx }
%lex-param   { js_parser_context_t *ctx }

%union {
    js_ast_node_t *node;
    js_ast_node_list_t node_list;
    js_ast_var_kind_t var_kind;
    js_token_t token;
}

%token <token> T_IDENTIFIER
%token <token> T_NUMBER
%token <token> T_STRING

%token T_TRUE
%token T_FALSE
%token T_NULL
%token T_THIS
%token T_SUPER

%token T_VAR
%token T_LET
%token T_CONST
%token T_IF
%token T_ELSE
%token T_RETURN
%token T_WHILE
%token T_DO
%token T_FOR
%token T_BREAK
%token T_CONTINUE
%token T_FUNCTION

%token T_TYPEOF
%token T_VOID
%token T_DELETE

%token T_EQUAL_EQUAL
%token T_NOT_EQUAL
%token T_STRICT_EQUAL
%token T_STRICT_NOT_EQUAL

%token T_LOGICAL_OR
%token T_LOGICAL_AND

%token T_LSHIFT
%token T_RSHIFT
%token T_URSHIFT

%token T_LE
%token T_GE

%token T_PLUS_PLUS
%token T_MINUS_MINUS

%token T_INSTANCEOF
%token T_IN

%token T_EXP

%right '='
%right '?'
%left T_LOGICAL_OR
%left T_LOGICAL_AND
%left '|' 
%left '^'
%left '&'
%nonassoc T_EQUAL_EQUAL T_NOT_EQUAL T_STRICT_EQUAL T_STRICT_NOT_EQUAL
%nonassoc '<' '>' T_LE T_GE T_INSTANCEOF T_IN
%left T_LSHIFT T_RSHIFT T_URSHIFT
%left '+' '-'
%left '*' '/' '%'
%right T_EXP
%right T_UNARY
%left T_PLUS_PLUS T_MINUS_MINUS
%nonassoc T_IF_NO_ELSE
%nonassoc T_ELSE

%type <node> program
%type <node> statement
%type <node> block
%type <node> variable_statement
%type <node> variable_declaration
%type <node> variable_declarator
%type <node> expression_statement
%type <node> expression_opt
%type <node> return_statement
%type <node> break_statement
%type <node> continue_statement
%type <node> function_declaration
%type <node> if_statement
%type <node> while_statement
%type <node> do_while_statement
%type <node> for_statement
%type <node> function_expression
%type <node> expression
%type <node> assignment_expression
%type <node> conditional_expression
%type <node> logical_or_expression
%type <node> logical_and_expression
%type <node> bitwise_or_expression
%type <node> bitwise_xor_expression
%type <node> bitwise_and_expression
%type <node> equality_expression
%type <node> relational_expression
%type <node> shift_expression
%type <node> additive_expression
%type <node> multiplicative_expression
%type <node> exponent_expression
%type <node> unary_expression
%type <node> postfix_expression
%type <node> left_hand_side_expression
%type <node> call_expression
%type <node> member_expression
%type <node> primary_expression
%type <node> initializer_opt
%type <node> binding_identifier
%type <node> for_init_opt
%type <node> for_test_opt
%type <node> for_update_opt

%type <node_list> statement_list_opt
%type <node_list> variable_declarator_list
%type <node_list> argument_list
%type <node_list> argument_list_opt
%type <node_list> arguments
%type <node_list> parameter_list
%type <node_list> parameter_list_opt

%type <var_kind> variable_declaration_kind

%%
program
    : statement_list_opt
      {
          js_source_location_t loc = loc_from_yy(&@$);
          js_ast_node_t *node = js_ast_node_new(JS_AST_PROGRAM, loc);
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          node->data.program.body = $1;
          ctx->result = node;
          $$ = node;
      }
;

statement_list_opt
    : /* empty */
      {
          $$ = (js_ast_node_list_t){0};
      }
    | statement_list_opt statement
      {
          js_ast_node_list_t list = $1;
          if ($2) {
              if (!append_node(ctx, &list, $2, &@2)) {
                  YYABORT;
              }
          }
          $$ = list;
      }
;

statement
    : block
    | variable_statement
    | function_declaration
    | expression_statement
    | return_statement
    | if_statement
    | while_statement
    | do_while_statement
    | for_statement
    | break_statement
    | continue_statement
    | ';'
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_EMPTY_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          $$ = node;
      }
;

block
    : '{' statement_list_opt '}'
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_BLOCK_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          node->data.block_statement.body = $2;
          $$ = node;
      }
;

variable_statement
    : variable_declaration ';'
      {
          $$ = $1;
      }
;

variable_declaration
    : variable_declaration_kind variable_declarator_list
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_VARIABLE_DECLARATION, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              free_node_list(&$2);
              YYABORT;
          }
          node->data.variable_declaration.kind = $1;
          node->data.variable_declaration.declarators = $2;
          $$ = node;
      }
;

variable_declaration_kind
    : T_VAR
      { $$ = JS_AST_VAR_VAR; }
    | T_LET
      { $$ = JS_AST_VAR_LET; }
    | T_CONST
      { $$ = JS_AST_VAR_CONST; }
;

variable_declarator_list
    : variable_declarator
      {
          js_ast_node_list_t list = {0};
          if ($1 && !append_node(ctx, &list, $1, &@1)) {
              YYABORT;
          }
          $$ = list;
      }
    | variable_declarator_list ',' variable_declarator
      {
          js_ast_node_list_t list = $1;
          if ($3 && !append_node(ctx, &list, $3, &@3)) {
              YYABORT;
          }
          $$ = list;
      }
;

variable_declarator
    : binding_identifier initializer_opt
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_VARIABLE_DECLARATOR, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($1);
              js_ast_free($2);
              YYABORT;
          }
          node->data.variable_declarator.id = $1;
          node->data.variable_declarator.init = $2;
          $$ = node;
      }
;

binding_identifier
    : T_IDENTIFIER
      {
          js_ast_node_t *node = make_identifier_from_token(ctx, &$1, &@1);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

initializer_opt
    : '=' assignment_expression
      { $$ = $2; }
    | /* empty */
      { $$ = NULL; }
;

expression_statement
    : expression ';'
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_EXPRESSION_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($1);
              YYABORT;
          }
          node->data.expression_statement.expression = $1;
          $$ = node;
      }
;

expression_opt
    : expression
      { $$ = $1; }
    | /* empty */
      { $$ = NULL; }
;

return_statement
    : T_RETURN expression_opt ';'
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_RETURN_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($2);
              YYABORT;
          }
          node->data.return_statement.argument = $2;
          node->data.return_statement.has_argument = ($2 != NULL);
          $$ = node;
      }
;

break_statement
    : T_BREAK ';'
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_BREAK_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          node->data.break_statement.label = NULL;
          $$ = node;
      }
;

continue_statement
    : T_CONTINUE ';'
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_CONTINUE_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          node->data.continue_statement.label = NULL;
          $$ = node;
      }
;

function_declaration
    : T_FUNCTION binding_identifier '(' parameter_list_opt ')' block
      {
          js_ast_node_t *node = make_function(ctx, $2, $4, $6, false, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;
if_statement
    : T_IF '(' expression ')' statement T_ELSE statement
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_IF_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($3);
              js_ast_free($5);
              js_ast_free($7);
              YYABORT;
          }
          node->data.if_statement.test = $3;
          node->data.if_statement.consequent = $5;
          node->data.if_statement.alternate = $7;
          $$ = node;
      }
    | T_IF '(' expression ')' statement %prec T_IF_NO_ELSE
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_IF_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($3);
              js_ast_free($5);
              YYABORT;
          }
          node->data.if_statement.test = $3;
          node->data.if_statement.consequent = $5;
          node->data.if_statement.alternate = NULL;
          $$ = node;
      }
;

while_statement
    : T_WHILE '(' expression ')' statement
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_WHILE_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($3);
              js_ast_free($5);
              YYABORT;
          }
          node->data.while_statement.test = $3;
          node->data.while_statement.body = $5;
          $$ = node;
      }
;

do_while_statement
    : T_DO statement T_WHILE '(' expression ')' ';'
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_DO_WHILE_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($2);
              js_ast_free($5);
              YYABORT;
          }
          node->data.do_while_statement.body = $2;
          node->data.do_while_statement.test = $5;
          $$ = node;
      }
;

for_statement
    : T_FOR '(' for_init_opt ';' for_test_opt ';' for_update_opt ')' statement
      {
          js_ast_node_t *node = js_ast_node_new(JS_AST_FOR_STATEMENT, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($3);
              js_ast_free($5);
              js_ast_free($7);
              js_ast_free($9);
              YYABORT;
          }
          node->data.for_statement.init = $3;
          node->data.for_statement.test = $5;
          node->data.for_statement.update = $7;
          node->data.for_statement.body = $9;
          $$ = node;
      }
;

for_init_opt
    : variable_declaration
      { $$ = $1; }
    | expression
      { $$ = $1; }
    | /* empty */
      { $$ = NULL; }
;

for_test_opt
    : expression
      { $$ = $1; }
    | /* empty */
      { $$ = NULL; }
;

for_update_opt
    : expression
      { $$ = $1; }
    | /* empty */
      { $$ = NULL; }
;

expression
    : assignment_expression
      { $$ = $1; }
    | expression ',' assignment_expression
      {
          js_ast_node_t *node = make_sequence(ctx, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

assignment_expression
    : conditional_expression
      { $$ = $1; }
    | left_hand_side_expression '=' assignment_expression
      {
          if (!$1 || !$3) {
              js_ast_free($1);
              js_ast_free($3);
              ctx->had_error = true;
              YYABORT;
          }
          js_ast_node_t *node = js_ast_node_new(JS_AST_ASSIGNMENT_EXPRESSION, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($1);
              js_ast_free($3);
              YYABORT;
          }
          node->data.assignment_expression.op = JS_AST_ASSIGN_EQ;
          node->data.assignment_expression.left = $1;
          node->data.assignment_expression.right = $3;
          $$ = node;
      }
;

conditional_expression
    : logical_or_expression
      { $$ = $1; }
    | logical_or_expression '?' assignment_expression ':' assignment_expression
      {
          if (!$1 || !$3 || !$5) {
              js_ast_free($1);
              js_ast_free($3);
              js_ast_free($5);
              ctx->had_error = true;
              YYABORT;
          }
          js_ast_node_t *node = js_ast_node_new(JS_AST_CONDITIONAL_EXPRESSION, loc_from_yy(&@$));
          if (!node) {
              parser_report(ctx, &@$, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              js_ast_free($1);
              js_ast_free($3);
              js_ast_free($5);
              YYABORT;
          }
          node->data.conditional_expression.test = $1;
          node->data.conditional_expression.consequent = $3;
          node->data.conditional_expression.alternate = $5;
          $$ = node;
      }
;

logical_or_expression
    : logical_and_expression
      { $$ = $1; }
    | logical_or_expression T_LOGICAL_OR logical_and_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_LOGICAL_OR, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

logical_and_expression
    : bitwise_or_expression
      { $$ = $1; }
    | logical_and_expression T_LOGICAL_AND bitwise_or_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_LOGICAL_AND, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

bitwise_or_expression
    : bitwise_xor_expression
      { $$ = $1; }
    | bitwise_or_expression '|' bitwise_xor_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_OR, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

bitwise_xor_expression
    : bitwise_and_expression
      { $$ = $1; }
    | bitwise_xor_expression '^' bitwise_and_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_XOR, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

bitwise_and_expression
    : equality_expression
      { $$ = $1; }
    | bitwise_and_expression '&' equality_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_AND, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

equality_expression
    : relational_expression
      { $$ = $1; }
    | equality_expression T_EQUAL_EQUAL relational_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_EQUAL, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | equality_expression T_NOT_EQUAL relational_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_NOT_EQUAL, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | equality_expression T_STRICT_EQUAL relational_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_STRICT_EQUAL, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | equality_expression T_STRICT_NOT_EQUAL relational_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_STRICT_NOT_EQUAL, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

relational_expression
    : shift_expression
      { $$ = $1; }
    | relational_expression '<' shift_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_LT, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | relational_expression '>' shift_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_GT, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | relational_expression T_LE shift_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_LE, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | relational_expression T_GE shift_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_GE, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | relational_expression T_INSTANCEOF shift_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_INSTANCEOF, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | relational_expression T_IN shift_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_IN, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

shift_expression
    : additive_expression
      { $$ = $1; }
    | shift_expression T_LSHIFT additive_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_LSHIFT, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | shift_expression T_RSHIFT additive_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_RSHIFT, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | shift_expression T_URSHIFT additive_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_URSHIFT, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

additive_expression
    : multiplicative_expression
      { $$ = $1; }
    | additive_expression '+' multiplicative_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_ADD, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | additive_expression '-' multiplicative_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_SUB, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

multiplicative_expression
    : exponent_expression
      { $$ = $1; }
    | multiplicative_expression '*' exponent_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_MUL, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | multiplicative_expression '/' exponent_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_DIV, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | multiplicative_expression '%' exponent_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_MOD, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

exponent_expression
    : unary_expression
      { $$ = $1; }
    | unary_expression T_EXP exponent_expression
      {
          js_ast_node_t *node = make_binary(ctx, JS_AST_BINARY_EXP, $1, $3, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

unary_expression
    : postfix_expression
      { $$ = $1; }
    | T_PLUS_PLUS unary_expression
      {
          js_ast_node_t *node = make_update(ctx, JS_AST_UPDATE_INCREMENT, $2, true, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | T_MINUS_MINUS unary_expression
      {
          js_ast_node_t *node = make_update(ctx, JS_AST_UPDATE_DECREMENT, $2, true, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | '+' unary_expression %prec T_UNARY
      {
          js_ast_node_t *node = make_unary(ctx, JS_AST_UNARY_PLUS, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | '-' unary_expression %prec T_UNARY
      {
          js_ast_node_t *node = make_unary(ctx, JS_AST_UNARY_MINUS, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | '!' unary_expression %prec T_UNARY
      {
          js_ast_node_t *node = make_unary(ctx, JS_AST_UNARY_LOGICAL_NOT, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | '~' unary_expression %prec T_UNARY
      {
          js_ast_node_t *node = make_unary(ctx, JS_AST_UNARY_BIT_NOT, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | T_TYPEOF unary_expression %prec T_UNARY
      {
          js_ast_node_t *node = make_unary(ctx, JS_AST_UNARY_TYPEOF, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | T_VOID unary_expression %prec T_UNARY
      {
          js_ast_node_t *node = make_unary(ctx, JS_AST_UNARY_VOID, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | T_DELETE unary_expression %prec T_UNARY
      {
          js_ast_node_t *node = make_unary(ctx, JS_AST_UNARY_DELETE, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

postfix_expression
    : left_hand_side_expression
      { $$ = $1; }
    | left_hand_side_expression T_PLUS_PLUS
      {
          js_ast_node_t *node = make_update(ctx, JS_AST_UPDATE_INCREMENT, $1, false, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | left_hand_side_expression T_MINUS_MINUS
      {
          js_ast_node_t *node = make_update(ctx, JS_AST_UPDATE_DECREMENT, $1, false, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

left_hand_side_expression
    : call_expression
      { $$ = $1; }
    | member_expression
      { $$ = $1; }
;

call_expression
    : member_expression arguments
      {
          js_ast_node_t *node = make_call_expression(ctx, $1, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | call_expression arguments
      {
          js_ast_node_t *node = make_call_expression(ctx, $1, $2, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | call_expression '.' T_IDENTIFIER
      {
          js_ast_node_t *property = make_identifier_from_token(ctx, &$3, &@3);
          if (!property) {
              js_ast_free($1);
              YYABORT;
          }
          js_ast_node_t *node = make_member_expression(ctx, $1, property, false, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | call_expression '[' expression ']'
      {
          js_ast_node_t *node = make_member_expression(ctx, $1, $3, true, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

member_expression
    : primary_expression
      { $$ = $1; }
    | member_expression '.' T_IDENTIFIER
      {
          js_ast_node_t *property = make_identifier_from_token(ctx, &$3, &@3);
          if (!property) {
              js_ast_free($1);
              YYABORT;
          }
          js_ast_node_t *node = make_member_expression(ctx, $1, property, false, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | member_expression '[' expression ']'
      {
          js_ast_node_t *node = make_member_expression(ctx, $1, $3, true, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

function_expression
    : T_FUNCTION binding_identifier '(' parameter_list_opt ')' block
      {
          js_ast_node_t *node = make_function(ctx, $2, $4, $6, true, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | T_FUNCTION '(' parameter_list_opt ')' block
      {
          js_ast_node_t *node = make_function(ctx, NULL, $3, $5, true, &@$);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
;

primary_expression
    : T_IDENTIFIER
      {
          js_ast_node_t *node = make_identifier_from_token(ctx, &$1, &@1);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | T_NUMBER
      {
          js_ast_node_t *node = make_number_from_token(ctx, &$1, &@1);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | T_STRING
      {
          js_ast_node_t *node = make_string_from_token(ctx, &$1, &@1);
          if (!node) {
              YYABORT;
          }
          $$ = node;
      }
    | T_TRUE
      {
          js_ast_node_t *node = js_ast_make_literal_boolean(loc_from_yy(&@1), true);
          if (!node) {
              parser_report(ctx, &@1, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          $$ = node;
      }
    | T_FALSE
      {
          js_ast_node_t *node = js_ast_make_literal_boolean(loc_from_yy(&@1), false);
          if (!node) {
              parser_report(ctx, &@1, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          $$ = node;
      }
    | T_NULL
      {
          js_ast_node_t *node = js_ast_make_literal_null(loc_from_yy(&@1));
          if (!node) {
              parser_report(ctx, &@1, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          $$ = node;
      }
    | T_THIS
      {
          js_ast_node_t *node = js_ast_make_identifier(loc_from_yy(&@1), "this");
          if (!node) {
              parser_report(ctx, &@1, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          $$ = node;
      }
    | T_SUPER
      {
          js_ast_node_t *node = js_ast_make_identifier(loc_from_yy(&@1), "super");
          if (!node) {
              parser_report(ctx, &@1, JS_PARSER_OOM_MESSAGE);
              ctx->had_error = true;
              YYABORT;
          }
          $$ = node;
      }
    | '(' expression ')'
      { $$ = $2; }
    | function_expression
;

arguments
    : '(' argument_list_opt ')'
      { $$ = $2; }
;

argument_list_opt
    : /* empty */
      { $$ = (js_ast_node_list_t){0}; }
    | argument_list
      { $$ = $1; }
;

argument_list
    : assignment_expression
      {
          js_ast_node_list_t list = {0};
          if ($1 && !append_node(ctx, &list, $1, &@1)) {
              YYABORT;
          }
          $$ = list;
      }
    | argument_list ',' assignment_expression
      {
          js_ast_node_list_t list = $1;
          if ($3 && !append_node(ctx, &list, $3, &@3)) {
              YYABORT;
          }
          $$ = list;
      }
;

parameter_list_opt
    : /* empty */
      { $$ = (js_ast_node_list_t){0}; }
    | parameter_list
      { $$ = $1; }
;

parameter_list
    : binding_identifier
      {
          js_ast_node_list_t list = {0};
          if (!append_node(ctx, &list, $1, &@1)) {
              YYABORT;
          }
          $$ = list;
      }
    | parameter_list ',' binding_identifier
      {
          js_ast_node_list_t list = $1;
          if (!append_node(ctx, &list, $3, &@3)) {
              YYABORT;
          }
          $$ = list;
      }
;
%%
static void parser_report(js_parser_context_t *ctx, const YYLTYPE *loc, const char *message) {
    if (!ctx) {
        return;
    }
    ctx->had_error = true;
    if (!ctx->callback) {
        return;
    }
    js_diagnostic_t diag;
    diag.level = JS_DIAG_ERROR;
    diag.message = message;
    if (loc) {
        diag.location.line = (uint32_t)loc->first_line;
        diag.location.column = (uint32_t)loc->first_column;
    } else {
        diag.location.line = 0;
        diag.location.column = 0;
    }
    ctx->callback(&diag, ctx->user_data);
}

static js_source_location_t loc_from_yy(const YYLTYPE *loc) {
    js_source_location_t result;
    if (!loc) {
        result.line = 0;
        result.column = 0;
        return result;
    }
    result.line = (uint32_t)loc->first_line;
    result.column = (uint32_t)loc->first_column;
    return result;
}

static void free_node_list(js_ast_node_list_t *list) {
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

static bool append_node(js_parser_context_t *ctx, js_ast_node_list_t *list, js_ast_node_t *node, const YYLTYPE *loc) {
    if (!node) {
        ctx->had_error = true;
        return false;
    }
    if (!js_ast_node_list_append(list, node)) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
        js_ast_free(node);
        return false;
    }
    return true;
}

static js_ast_node_t *make_binary(js_parser_context_t *ctx, js_ast_binary_op_t op, js_ast_node_t *left, js_ast_node_t *right, const YYLTYPE *loc) {
    if (!left || !right) {
        js_ast_free(left);
        js_ast_free(right);
        ctx->had_error = true;
        return NULL;
    }
    js_ast_node_t *node = js_ast_node_new(JS_AST_BINARY_EXPRESSION, loc_from_yy(loc));
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
        js_ast_free(left);
        js_ast_free(right);
        return NULL;
    }
    node->data.binary_expression.op = op;
    node->data.binary_expression.left = left;
    node->data.binary_expression.right = right;
    return node;
}

static js_ast_node_t *make_unary(js_parser_context_t *ctx, js_ast_unary_op_t op, js_ast_node_t *argument, const YYLTYPE *loc) {
    if (!argument) {
        ctx->had_error = true;
        return NULL;
    }
    js_ast_node_t *node = js_ast_node_new(JS_AST_UNARY_EXPRESSION, loc_from_yy(loc));
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
        js_ast_free(argument);
        return NULL;
    }
    node->data.unary_expression.op = op;
    node->data.unary_expression.argument = argument;
    node->data.unary_expression.prefix = true;
    return node;
}

static js_ast_node_t *make_function(js_parser_context_t *ctx, js_ast_node_t *id, js_ast_node_list_t params, js_ast_node_t *body, bool is_expression, const YYLTYPE *loc) {
    if (!body) {
        js_ast_free(id);
        free_node_list(&params);
        ctx->had_error = true;
        return NULL;
    }
    js_ast_node_t *node = js_ast_node_new(is_expression ? JS_AST_FUNCTION_EXPRESSION : JS_AST_FUNCTION_DECLARATION, loc_from_yy(loc));
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
        js_ast_free(id);
        free_node_list(&params);
        js_ast_free(body);
        return NULL;
    }
    node->data.function.id = id;
    node->data.function.params = params;
    node->data.function.body = body;
    node->data.function.is_async = false;
    node->data.function.is_generator = false;
    node->data.function.is_expression = is_expression;
    return node;
}

static js_ast_node_t *make_update(js_parser_context_t *ctx, js_ast_update_op_t op, js_ast_node_t *argument, bool prefix, const YYLTYPE *loc) {
    if (!argument) {
        ctx->had_error = true;
        return NULL;
    }
    js_ast_node_t *node = js_ast_node_new(JS_AST_UPDATE_EXPRESSION, loc_from_yy(loc));
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
        js_ast_free(argument);
        return NULL;
    }
    node->data.update_expression.op = op;
    node->data.update_expression.argument = argument;
    node->data.update_expression.prefix = prefix;
    return node;
}

static js_ast_node_t *make_member_expression(js_parser_context_t *ctx, js_ast_node_t *object, js_ast_node_t *property, bool computed, const YYLTYPE *loc) {
    if (!object || !property) {
        js_ast_free(object);
        js_ast_free(property);
        ctx->had_error = true;
        return NULL;
    }
    js_ast_node_t *node = js_ast_node_new(JS_AST_MEMBER_EXPRESSION, loc_from_yy(loc));
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
        js_ast_free(object);
        js_ast_free(property);
        return NULL;
    }
    node->data.member_expression.object = object;
    node->data.member_expression.property = property;
    node->data.member_expression.computed = computed;
    return node;
}

static js_ast_node_t *make_call_expression(js_parser_context_t *ctx, js_ast_node_t *callee, js_ast_node_list_t args, const YYLTYPE *loc) {
    if (!callee) {
        free_node_list(&args);
        ctx->had_error = true;
        return NULL;
    }
    js_ast_node_t *node = js_ast_node_new(JS_AST_CALL_EXPRESSION, loc_from_yy(loc));
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
        js_ast_free(callee);
        free_node_list(&args);
        return NULL;
    }
    node->data.call_expression.callee = callee;
    node->data.call_expression.arguments = args;
    node->data.call_expression.is_new = false;
    return node;
}

static js_ast_node_t *make_sequence(js_parser_context_t *ctx, js_ast_node_t *left, js_ast_node_t *right, const YYLTYPE *loc) {
    if (!left || !right) {
        js_ast_free(left);
        js_ast_free(right);
        ctx->had_error = true;
        return NULL;
    }
    js_ast_node_t *node = left;
    if (left->kind != JS_AST_SEQUENCE_EXPRESSION) {
        node = js_ast_node_new(JS_AST_SEQUENCE_EXPRESSION, loc_from_yy(loc));
        if (!node) {
            parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
            js_ast_free(left);
            js_ast_free(right);
            return NULL;
        }
        if (!js_ast_node_list_append(&node->data.sequence_expression.expressions, left)) {
            parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
            js_ast_free(left);
            js_ast_free(right);
            free(node);
            return NULL;
        }
    }
    if (!js_ast_node_list_append(&node->data.sequence_expression.expressions, right)) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
        js_ast_free(right);
        if (node != left) {
            free_node_list(&node->data.sequence_expression.expressions);
            free(node);
        }
        return NULL;
    }
    return node;
}

static js_ast_node_t *make_identifier_from_token(js_parser_context_t *ctx, const js_token_t *token, const YYLTYPE *loc) {
    js_ast_node_t *node = js_ast_make_identifier_slice(loc_from_yy(loc), token->lexeme, token->length);
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
    }
    return node;
}

static js_ast_node_t *make_number_from_token(js_parser_context_t *ctx, const js_token_t *token, const YYLTYPE *loc) {
    js_ast_node_t *node = js_ast_make_literal_number(loc_from_yy(loc), token->number_value);
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
    }
    return node;
}

static js_ast_node_t *make_string_from_token(js_parser_context_t *ctx, const js_token_t *token, const YYLTYPE *loc) {
    js_ast_node_t *node = js_ast_make_literal_string_slice(loc_from_yy(loc), token->lexeme, token->length);
    if (!node) {
        parser_report(ctx, loc, JS_PARSER_OOM_MESSAGE);
    }
    return node;
}

static void set_token_location(YYLTYPE *yylloc, const js_token_t *token) {
    if (!yylloc || !token) {
        return;
    }
    yylloc->first_line = (int)token->location.line;
    yylloc->first_column = (int)token->location.column;
    yylloc->last_line = (int)token->location.line;
    yylloc->last_column = (int)(token->location.column + token->length);
}

static int js_parser_impllex(YYSTYPE *yylval, YYLTYPE *yylloc, js_parser_context_t *ctx) {
    js_token_t token;
    if (!ctx || !ctx->lexer) {
        return 0;
    }
    if (!js_lexer_next(ctx->lexer, &token)) {
        ctx->had_error = true;
        return 0;
    }
    set_token_location(yylloc, &token);
    switch (token.kind) {
        case JS_TOK_EOF:
            return 0;
        case JS_TOK_IDENTIFIER:
            yylval->token = token;
            return T_IDENTIFIER;
        case JS_TOK_NUMBER:
            yylval->token = token;
            return T_NUMBER;
        case JS_TOK_STRING:
        case JS_TOK_TEMPLATE_HEAD:
        case JS_TOK_TEMPLATE_MIDDLE:
        case JS_TOK_TEMPLATE_TAIL:
            yylval->token = token;
            return T_STRING;
        case JS_TOK_TRUE:
            return T_TRUE;
        case JS_TOK_FALSE:
            return T_FALSE;
        case JS_TOK_NULL:
            return T_NULL;
        case JS_TOK_THIS:
            return T_THIS;
        case JS_TOK_SUPER:
            return T_SUPER;
        case JS_TOK_KW_VAR:
            return T_VAR;
        case JS_TOK_KW_LET:
            return T_LET;
        case JS_TOK_KW_CONST:
            return T_CONST;
        case JS_TOK_KW_IF:
            return T_IF;
        case JS_TOK_KW_ELSE:
            return T_ELSE;
        case JS_TOK_KW_RETURN:
            return T_RETURN;
        case JS_TOK_KW_WHILE:
            return T_WHILE;
        case JS_TOK_KW_DO:
            return T_DO;
        case JS_TOK_KW_FOR:
            return T_FOR;
        case JS_TOK_KW_BREAK:
            return T_BREAK;
        case JS_TOK_KW_CONTINUE:
            return T_CONTINUE;
        case JS_TOK_KW_FUNCTION:
            return T_FUNCTION;
        case JS_TOK_KW_TYPEOF:
            return T_TYPEOF;
        case JS_TOK_KW_VOID:
            return T_VOID;
        case JS_TOK_KW_DELETE:
            return T_DELETE;
        case JS_TOK_KW_INSTANCEOF:
            return T_INSTANCEOF;
        case JS_TOK_KW_IN:
            return T_IN;
        case JS_TOK_SEMICOLON:
            return ';';
        case JS_TOK_COMMA:
            return ',';
        case JS_TOK_LPAREN:
            return '(';
        case JS_TOK_RPAREN:
            return ')';
        case JS_TOK_LBRACE:
            return '{';
        case JS_TOK_RBRACE:
            return '}';
        case JS_TOK_LBRACKET:
            return '[';
        case JS_TOK_RBRACKET:
            return ']';
        case JS_TOK_DOT:
            return '.';
        case JS_TOK_PLUS:
            return '+';
        case JS_TOK_MINUS:
            return '-';
        case JS_TOK_STAR:
            return '*';
        case JS_TOK_SLASH:
            return '/';
        case JS_TOK_PERCENT:
            return '%';
        case JS_TOK_STAR_STAR:
            return T_EXP;
        case JS_TOK_ASSIGN:
            return '=';
        case JS_TOK_LT:
            return '<';
        case JS_TOK_GT:
            return '>';
        case JS_TOK_LE:
            return T_LE;
        case JS_TOK_GE:
            return T_GE;
        case JS_TOK_EQUAL:
            return T_EQUAL_EQUAL;
        case JS_TOK_NOT_EQUAL:
            return T_NOT_EQUAL;
        case JS_TOK_STRICT_EQUAL:
            return T_STRICT_EQUAL;
        case JS_TOK_STRICT_NOT_EQUAL:
            return T_STRICT_NOT_EQUAL;
        case JS_TOK_LOGICAL_OR:
            return T_LOGICAL_OR;
        case JS_TOK_LOGICAL_AND:
            return T_LOGICAL_AND;
        case JS_TOK_BIT_OR:
            return '|';
        case JS_TOK_BIT_XOR:
            return '^';
        case JS_TOK_BIT_AND:
            return '&';
        case JS_TOK_LSHIFT:
            return T_LSHIFT;
        case JS_TOK_RSHIFT:
            return T_RSHIFT;
        case JS_TOK_URSHIFT:
            return T_URSHIFT;
        case JS_TOK_PLUS_PLUS:
            return T_PLUS_PLUS;
        case JS_TOK_MINUS_MINUS:
            return T_MINUS_MINUS;
        case JS_TOK_NOT:
            return '!';
        case JS_TOK_BIT_NOT:
            return '~';
        case JS_TOK_QUESTION:
            return '?';
        case JS_TOK_COLON:
            return ':';
        default:
            parser_report(ctx, yylloc, "Unsupported token in grammar");
            return 0;
    }
}

static void js_parser_implerror(YYLTYPE *loc, js_parser_context_t *ctx, const char *msg) {
    parser_report(ctx, loc, msg);
}
