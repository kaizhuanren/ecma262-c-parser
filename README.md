# JS Parser Project

This repository contains a JavaScript syntax parser written in C using **re2c** for
lexical analysis and **Bison** for grammar parsing. The project aims to implement
ECMAScript compliant syntax checking with Automatic Semicolon Insertion (ASI) support.

## Directory Layout

- `include/` – Public headers shared across compilation units.
- `src/lexer/` – re2c lexer specification and generated scanner glue code.
- `src/parser/` – Bison grammar and parser driver logic.
- `src/ast/` – Abstract Syntax Tree types and helpers.
- `src/cli/` – Command line interface implementation.
- `tests/` – Sample JavaScript inputs and regression cases.

## Build Requirements

- ISO C17 compatible compiler (e.g. `gcc`, `clang`, or MSVC).
- `re2c` (≥ 3.1) for lexer generation.
- `bison` (≥ 3.8) for parser generation.
- `make` for the provided build script.

See `TESTING.md` for step-by-step build and regression instructions.

## CLI Usage

After running `make`, the CLI lives at `build/bin/jsparser`. The tool accepts a
JavaScript file plus optional demonstration flags:

```
build/bin/jsparser [options] <file.js>
```

- `--check` – parse and report success/failure (default when no other action is selected).
- `--tokens` – dump the token stream with line/column metadata.
- `--asi` – show Automatic Semicolon Insertion events (line, column, reason).
- `--ast` – print the AST as a human-readable S-expression tree.
- `--dot out.dot` – serialize the AST to Graphviz DOT for visualization.
- `--pretty` – pretty-print the program with explicit semicolons (ASI materialized).
- `--trace-parse` – enable Bison’s `yydebug` trace.
- `--trace-lex` – stream lexer activity (requires building with `-DDEBUG_TOKENS`).

Multiple flags can be combined to explore different aspects of the parser output
without re-running the build.
