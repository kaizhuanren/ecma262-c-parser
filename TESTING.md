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

This target runs the CLI on every `.js` file located under `tests/cases/`.

## 4. Manual Checks

You can manually inspect additional inputs by invoking the CLI:

```sh
build/bin/jsparser path/to/file.js
```

The program reports diagnostics to `stderr` and prints `Parsing succeeded.` on
success. Non-zero exit codes indicate syntax or lexical errors.
