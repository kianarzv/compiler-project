# Reading and changing the code

## Follow a run

1. `src/main.cpp` checks arguments. With no display option it shows the original
   source, as in step 1. `--tokens` calls `print_tokens()`; `--ast` calls
   `print_ast()`. The two display modes cannot be combined.
2. `SourceFile::load()` opens the file in binary mode and stores its bytes. File
   errors become exceptions and are reported by `main()` with exit code 1.
3. `Lexer::tokenize()` resets its cursor and diagnostics, then repeatedly calls
   `scan_token()`. Each call consumes at least one byte. An empty file goes
   directly to EOF. The returned `LexResult` owns both tokens and diagnostics.
4. `scan_token()` saves the starting location and dispatches according to the
   next byte. Words, whitespace and digits are consumed directly. Quotes go to
   `scan_quoted()`, comment openers to `scan_comment()`, and remaining symbols
   to `scan_symbol()`. The resulting token gets the complete original slice.
5. `advance()` is the only function that moves the cursor. It updates the byte
   offset, line and column together and treats CRLF as a single line ending.
6. `print_tokens()` optionally hides whitespace, prints each token's location
   and kind, then escapes its lexeme for display. EOF is always visible. After
   the table, diagnostics are printed on stderr; errors make the CLI return 1.

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

`report()` appends an error to the scanner's diagnostic collection. Helpers
choose one primary cause for each invalid token: a missing delimiter is more
useful than several secondary body errors. Error locations can be more precise
than the token start, for example the backslash of an incomplete escape.

Identifier limits count ASCII characters. String limits count content units
inside the quotes, so a backslash pair adds one rather than two. Both allow
256 units. Overlong tokens stay whole, which preserves source offsets and lets
scanning resume after the token instead of interpreting its suffix as code.

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
- **Diagnostic cause:** add a `DiagnosticCode` and its label in
  `src/diagnostic.cpp`, then call `report()` with a source location and span.
  The lexer reports data; `write_diagnostic()` controls the CLI message format.
- **CLI display:** change `print_tokens()` or `print_ast()` in `src/main.cpp`.
  Shared `write_escaped()` lives in `src/text_output.cpp`. Keep display escaping
  separate from stored lexeme text.
- **Regression checks:** add a case in `tests/lexer_tests.cpp` and run CTest.
  Tests use exceptions rather than disabled-in-Release assertions.
  Error behavior belongs in `tests/diagnostic_tests.cpp`; CLI stream/exit-code
  behavior is also checked by `tests/check_lexical_cli.cmake`.

## Ownership and boundaries

`SourceFile` owns the file contents. `Lexer` borrows a reference to that object,
so the source must stay alive while scanning. Each returned `Token` owns copies
of its lexeme and source location, and does not depend on the lexer's lifetime.
`LexResult` owns these tokens and the diagnostic data; both remain valid after
the source/lexer are destroyed or another scan starts.

The CLI does not decide lexical categories. The lexer does not print or format
output. Keeping these responsibilities apart lets future parser and highlighter
code use the same tokens without depending on command-line behavior.

## Follow an AST run

`print_ast()` first uses the same lexer. If that reports lexical errors, they
are shown without starting syntax parsing. Otherwise the EOF-terminated token
vector is passed to `Parser`. Its cursor skips whitespace/comments, retaining
the original token objects and source locations for node creation.

`parse_declaration()` consumes a name and colon before deciding between a
function prototype and a variable. Variable types are read by
`parse_value_type()`, which recurses for nested arrays. Function return types
use `parse_atomic_type(true)` to admit `void`; parameter types additionally
permit unsized arrays. Each parameter has its own name and type child nodes.

`parse_initializer()` currently handles literals and nested braced lists. It
creates `LiteralExpr` leaves or an `ArrayInitExpr` owning the elements. Normal
expression routines will replace the literal-only branch in step 6.

An error creates a structured parser diagnostic and throws a private
`ParseFailure` signal. `parse()` catches it, automatically destroys the
incomplete declaration, and returns any complete earlier declarations. A
`PAR002` message explicitly marks syntax scheduled for a later step rather
than accepting an unparsed expression/body. Panic-mode recovery comes in step 8.

`ParseResult` owns the `Program` and diagnostics. Nodes own their children with
`unique_ptr`; no AST data depends on the input vector's lifetime. All node
locations come from their first significant token. Every node's
`semantic_type` starts null, separately from the `TypeSyntax` tree that records
what was written in the file. `dump_ast()` traverses these nodes for display.

To extend parsing, update the appropriate routine in `src/parser.cpp`, the node
definitions in `include/bminor/ast.hpp`, and the printer in `src/ast.cpp`.
Document grammar choices in `docs/parser-design.md` and test tree shape as well
as successful parsing. The target and current subset are documented separately
so a grammar rule in a future step does not imply it is already implemented.
