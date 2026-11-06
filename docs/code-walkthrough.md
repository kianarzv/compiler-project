# Reading and changing the code

## Follow a run

1. `src/main.cpp` checks arguments. With no display option it shows the original
   source, as in step 1. With `--tokens` it calls `print_tokens()` instead.
2. `SourceFile::load()` opens the file in binary mode and stores its bytes. File
   errors become exceptions and are reported by `main()` with exit code 1.
3. `Lexer::tokenize()` resets its cursor and repeatedly calls `scan_token()`.
   Each call consumes at least one byte. An empty file goes directly to EOF.
4. `scan_token()` saves the starting location and dispatches according to the
   next byte. Words, whitespace and digits are consumed directly. Quotes go to
   `scan_quoted()`, comment openers to `scan_comment()`, and remaining symbols
   to `scan_symbol()`. The resulting token gets the complete original slice.
5. `advance()` is the only function that moves the cursor. It updates the byte
   offset, line and column together and treats CRLF as a single line ending.
6. `print_tokens()` optionally hides whitespace, prints each token's location
   and kind, then escapes its lexeme for display. EOF is always visible.

For `if ifx`, the first word is a keyword, the space is a retained whitespace
token, and the second word is an identifier. The keyword lookup does not run
halfway through the second word, which prevents splitting it into `if` and `x`.

For `x-12`, the lexer emits an identifier, `MINUS` and an integer. Attaching a
sign to the number would make subtraction depend on spaces. The later parser
will decide whether the same minus token means subtraction or unary negation.

Inside a quoted literal, an escape pair is consumed together, so an escaped
quote does not end the literal. The raw spelling is kept rather than decoded.
Character literals count such a pair as one character. A physical newline or
EOF without a closing quote produces one `INVALID` token.

The comment branch comes before the division operator branch. Line comments
end before the physical newline, while block comments include their closing
`*/` and may cross lines. Neither kind interprets quotes or operators as code.

## Where to make a change

- **Keyword spelling or kind:** update `classify_word()` in `src/lexer.cpp`,
  `TokenKind` in `include/bminor/token.hpp`, and the display label in
  `src/token.cpp`. Update the documented dialect and a boundary test too.
- **New lexical category:** add its kind and a branch in `scan_token()`.
  Consume bytes through `advance()` so all paths share location handling.
- **Operator:** update `scan_symbol()` and its kind/label. Try a supported
  two-character spelling before accepting a one-character prefix.
- **Literal or comment boundary:** update `scan_quoted()` or `scan_comment()`.
  Check a malformed or EOF case too; every call must consume source bytes.
- **CLI display:** change `print_tokens()` or `write_escaped()` in `src/main.cpp`.
  Keep display escaping separate from the token's original lexeme.
- **Regression checks:** add a case in `tests/lexer_tests.cpp` and run CTest.
  Tests use exceptions rather than disabled-in-Release assertions.

## Ownership and boundaries

`SourceFile` owns the file contents. `Lexer` borrows a reference to that object,
so the source must stay alive while scanning. Each returned `Token` owns copies
of its lexeme and source location, and does not depend on the lexer's lifetime.

The CLI does not decide lexical categories. The lexer does not print or format
output. Keeping these responsibilities apart lets future parser and highlighter
code use the same tokens without depending on command-line behavior.
