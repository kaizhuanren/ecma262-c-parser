# Build and Test Guide

The steps below assume `re2c`, `bison`, `make`, and a C17 compiler are
available in your environment. All commands are relative to the repository
root.

## 1. Generate Sources

```sh
re2c -o build/generated/js_lexer.c src/lexer/js_lexer.re

bison --defines=build/generated/js_parser.h --output=build/generated/js_parser.c src/parser/js_parser.y

```

## 2. Compile

Use the provided `Makefile` to build the parser and CLI frontend:

```sh
make
```

The executable will be created at `build/bin/jsparser`.

## 3. Run Regression Samples

Execute the parser against the bundled test cases:

```sh
make test
```

This target runs the CLI on every `.js` file located under
`tests/cases/valid/` and expects each run to succeed. It also executes the
parser against every sample in `tests/cases/invalid/` and fails the test suite
if any of those files parse without reporting an error.

## 4. Manual Checks

You can manually inspect additional inputs by invoking the CLI:

```sh
build/bin/jsparser path/to/file.js
```

The program reports diagnostics to `stderr` and prints `Parsing succeeded.` on
success. Non-zero exit codes indicate syntax or lexical errors. Combine the
supported flags to explore different parser facets, for example:

- `--tokens` – view the discovered token stream (line/column + lexeme).
- `--asi` – list Automatic Semicolon Insertion points and reasons.
- `--ast` or `--dot out.dot` – inspect the generated AST directly.
- `--pretty` – output a normalized version of the input with semicolons emitted.
- `--trace-parse` / `--trace-lex` – enable parser or lexer tracing (the latter
  requires compiling with `-DDEBUG_TOKENS`).
