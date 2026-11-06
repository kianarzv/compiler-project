# Reading and changing the code

## Follow a run

1. `src/main.cpp` checks arguments. With no display option it shows the original
   source, as in step 1. `--tokens` calls `print_tokens()`; `--ast` calls
   `print_ast()`. `--classify` calls `print_classification()` and `--ansi` calls
   `print_ansi()`. Analysis modes
   cannot be combined; the `OutputMode` enum stores the single selected mode.
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

After a function signature, `;` leaves the body's pointer null for a prototype.
`=` calls `parse_block()`, which requires braces and parses every statement.
The printer distinguishes `FuncDecl prototype` and `FuncDecl definition`.

`parse_statement()` dispatches keywords to `parse_if()`, `parse_for()`,
`parse_return()` and `parse_print()`. A leading `{` parses a nested block. For
an identifier, `lookahead(1)` checks the next significant token: `:` means a
local declaration, while a call/assignment/name remains an expression statement.
The global and local paths share `parse_variable_declaration()` after consuming
the name/colon. `DeclStmt` owns the resulting local variable node.

`parse_if()` parses its child statement before looking for an `else`. An inner
if therefore consumes its own else first, giving the documented nearest-if
association. `parse_for()` preserves each missing header expression as null,
then parses an ordinary statement for the body. `return;` has a null value,
and `;` is a real `ExprStmt` with a null expression. Print requires at least
one value, with comma-separated expressions followed by a semicolon.

`parse_initializer()` handles nested braced lists or delegates to
`parse_expression()`. Array bounds use the same expression entry point. From
there, `parse_assignment()` calls progressively tighter precedence routines.
Each left-associative level supplies its operator set and the next operand
routine to `parse_left_associative()`, whose loop builds an accumulated left
child. Power and assignment instead recurse on the right.

The `OperandParser` alias is a pointer to a `Parser` member function.
`&Parser::parse_multiplicative` selects one such function;
`(this->*operand_parser)()` invokes it on this parser. This shares the folding
loop while keeping named functions corresponding to the grammar levels.

`parse_unary()` handles prefix minus/not. `parse_postfix()` starts from a
primary and wraps it for every following call, index or increment/decrement.
Call arguments and index expressions recurse through `parse_expression()`;
the surrounding routine owns the comma or closing delimiter. `parse_primary()`
creates name/literal leaves or a `GroupExpr` retaining an opening parenthesis.

Each new expression constructor calculates its structural depth from its
children. The parser checks this after construction, including left-associated
and postfix chains built by loops. A separate nesting guard limits actual
recursive parsing. Both protect the later AST printer and automatic cleanup.

An error creates a structured diagnostic and throws a private `ParseFailure`
signal. `parse()` catches declaration failures, while
`parse_recovering_statement()` catches statement/child-body failures. The local
incomplete node is destroyed, `synchronize()` skips to a safe boundary, and an
`ErrorDecl`/`ErrorStmt` records the discarded span. Parent blocks/functions and
valid later nodes survive. `parse_if()` and `parse_for()` use the child wrapper
so an errored branch need not discard its surrounding control structure.

`synchronize()` rebuilds delimiter counts from the construct's saved start,
which keeps for-header semicolons from looking like statement endings. It leaves
an owning brace/else for its parent and consumes stray tokens to ensure progress.
At EOF a block can return with `incomplete=true`, preserving known contents and
reporting the missing closing brace once. See `docs/error-recovery.md` for the
rules and limits; the AST printer exposes all error markers.

`ParseResult` owns the `Program` and diagnostics. Nodes own their children with
`unique_ptr`; no AST data depends on the input vector's lifetime. All node
locations come from their first significant token. Every node's
`semantic_type` starts null, separately from the `TypeSyntax` tree that records
what was written in the file. `dump_ast()` traverses these nodes for display.

To extend parsing, update the appropriate routine in `src/parser.cpp`, the node
definitions in `include/bminor/ast.hpp`, and the printer in `src/ast.cpp`.
Document grammar choices in `docs/parser-design.md` and test tree shape as well
as successful parsing. `tests/statement_tests.cpp` covers statement structure,
nearest-else association, optional for headers, positions and nesting limits;
`tests/check_statement_cli.cmake` exercises full program files and failed bodies.
`tests/recovery_tests.cpp` checks synchronization, progress, spans and parser reuse.
The selected syntax grammar and panic-mode recovery are implemented; semantic
analysis remains separate and must respect diagnostics in a recovered result.

## Follow a classification run

`print_classification()` calls `classify_source()`. The function first builds
lexical categories from every non-EOF token. With clean lexing, it parses the
same tokens and passes the recovered `Program` to `classify_tokens()`.
Lexical errors instead retain the complete fallback table and diagnostics.

`AstClassifier::visit()` walks the AST without modifying it. Declaration and
parameter parents mark their identifier leaves; expression names become
references. A `CallExpr` visits its callee/arguments first, then `mark_callee()`
overrides a directly named callee as a function call, unwrapping group nodes.
This ordering prevents the normal identifier visit from erasing the call role.
Indexed/call-result callees keep their children classified by their own roles.

`mark_name()` finds the exact token span by offset, filename and length using
binary search. It does not look up a name globally, so repeated spellings and
shadowing do not contaminate unrelated occurrences. `mark_error()` marks the
discarded regions represented by error nodes while preserving each base role;
whitespace and zero-byte missing nodes are not decorated.

The classification table reads each span's text from the original source and
escapes it using the existing display helper. The ANSI renderer uses the same
spans and raw bytes with terminal styles. Read `docs/syntax-classification.md` before changing role
or error precedence; `tests/classifier_tests.cpp` checks expected context and
that source/AST data remain unchanged.

## Follow an ANSI preview

`print_ansi()` calls the same `classify_source()` entry point as the table. It
passes the result to `render_ansi()` and leaves diagnostics on stderr. Even a
failed analysis returns complete spans, so the whole file remains visible.
The boolean return value determines exit status after rendering and diagnostics.

`render_ansi()` first checks source identity, contiguous offsets, nonempty
lengths and complete coverage. This catches a renderer/classifier integration
mistake before any partial document is written. For each span, `style_for()`
chooses a sequence based on role/error status. The renderer writes that sequence,
copies the original source bytes and immediately writes a reset. Ordinary names
and whitespace have no sequence. No AST pretty-printer or debug escaping is used.

The small `prepare_binary_stdout()` CLI adapter runs before writing ANSI text.
Windows CRT text mode would otherwise translate LF into CRLF; binary mode keeps
the stored source endings intact, including when redirected. This adapter is
compiled into the executable, while the renderer stays in the portable core
library and accepts any `std::ostream`. File callers should use binary mode too.

To change colors, edit `style_for()` in `src/ansi_renderer.cpp`. To change which
occurrence counts as a call or declaration, edit the classifier instead. The
English comments explain error precedence, reset boundaries and byte copying;
`docs/ansi-rendering.md` lists the full palette and stream contract. The unit
suite checks formatting/styles and the CLI suite captures actual binary output
to catch newline translation that string-stream tests would miss.
