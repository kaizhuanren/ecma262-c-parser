This directory contains sample JavaScript programs used for manual regression
testing of the parser. The `cases/` subdirectory groups representative inputs:

- `basic.js` – variable declarations, block statements, and conditionals.
- `asi.js` – statements that rely on Automatic Semicolon Insertion (ASI).
- `loops.js` – `for`, `while`, and `do/while` constructs with expressions.

See `TESTING.md` at the project root for instructions on generating the parser
and running these samples through the command line tool.
