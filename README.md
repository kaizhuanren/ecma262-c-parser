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
