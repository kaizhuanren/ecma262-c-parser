This directory contains sample JavaScript programs used for manual regression
testing of the parser. Programs are split into valid and invalid suites under
`tests/cases/valid/` and `tests/cases/invalid/`.

Valid samples exercise supported syntax the parser should accept:

- `valid/basic.js` – variable declarations, block statements, and conditionals.
- `valid/asi.js` – statements that rely on Automatic Semicolon Insertion (ASI).
- `valid/loops.js` – `for`, `while`, and `do/while` constructs with expressions.
- `valid/control-flow.js` – nested conditionals plus `break`/`continue` inside loops.
- `valid/expressions.js` – collection of unary, binary, and call/member expressions.

Invalid samples are expected to trigger syntax errors:

- `invalid/missing-closing-brace.js` – unterminated block statement.
- `invalid/for-missing-semicolon.js` – `for` header missing the first semicolon.
- `invalid/dangling-operator.js` – expression ends with a dangling operator.

See `TESTING.md` at the project root for instructions on generating the parser
and running these samples through the command line tool.
