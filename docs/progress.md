# Development progress

## Step 1: runnable project foundation

Implemented:

- A CMake build with compiler warnings and a C++11 baseline.
- A CLI with help, source display and explicit exit codes.
- Source-file loading that retains the original bytes.
- Line counting for LF, CRLF and CR line endings.
- A small sample and a PowerShell build-and-run script.
- A roadmap for the remaining phase-one work.

Validation passed:

- Configured and compiled with MinGW GCC 6.3.0, CMake 3.29.2 and Ninja;
  compiler warnings were enabled and no warnings were reported.
- Ran `run.ps1` and displayed the sample: 77 bytes and 4 lines.
- Checked eight input cases, covering empty files, paths with spaces, LF,
  CRLF, CR, trailing newlines and a final line without a newline.
- Checked help, missing/extra arguments, unknown options and a missing file;
  all returned the documented exit codes.

The source loader preserves file bytes. Terminal display uses the platform's
standard output newline handling; later file renderers must write explicitly
in binary mode to preserve original line endings.

Step 2 was approved by the user after reviewing this foundation.

## Step 2: token model and basic lexer

Implemented:

- More detailed English comments in the source loader, CLI and new lexer code.
- Token kinds, exact lexemes, source filenames, line/column and byte offsets.
- ASCII identifiers, the base dialect's reserved words and boolean literals.
- Longest-word matching and keyword classification after the full word.
- Retained whitespace, consistent CR/LF/CRLF handling and a final EOF token.
- A one-byte fallback for currently unsupported input, ensuring forward progress.
- `--tokens` and optional `--show-whitespace`, also available in `run.ps1`.
- A lexical sample, formal rules and a code-reading guide.
- CTest integration and seven lexer regression groups.

The user approved the base textbook dialect because the course-specific
definition was unavailable. Later extensions must be documented before use.

Validation passed:

- Built with MinGW GCC 6.3.0 and the configured warning flags, with no warnings.
- CTest passed all seven lexer regression groups: keyword boundaries, all
  reserved words, identifier rules, source locations, complete byte coverage,
  unsupported input, and empty/repeated scans.
- Exercised both token display modes and the original source display.
- Verified escaped control/non-ASCII bytes, EOF coordinates and CRLF handling.
- Checked missing/extra arguments, unsupported options and a missing file.
- Checked paths with spaces, a filename starting with a dash through `--`, and
  PowerShell forwarding of the new switches.

This step intentionally leaves numbers, strings, characters, operators,
punctuation and comments for step 3. Their bytes appear as `INVALID` for now;
diagnostics and lexical error exit codes are scheduled for step 4.

Step 3 was approved by the user after reviewing these tokens.

## Step 3: literals, operators, delimiters and comments

Implemented:

- Decimal integer tokens, with separate sign operators and unchanged digits.
- Strings and one-unit character literals, including escaped quotes/backslashes.
- All base operators and delimiters, with longest matching for two-byte symbols.
- Line/block comments retained as tokens, with comment openers checked before `/`.
- Boundaries for malformed quoted literals and unfinished block comments, which
  produce `INVALID` tokens and retain their source bytes.
- A complete-program sample in `examples/lexer_full.bminor`.
- Updated English comments, formal rules and the code-reading guide.
- Nine new regression groups, bringing the lexer suite to 16 groups.

Validation passed:

- Built with MinGW GCC 6.3.0 and warning flags enabled; no warnings were emitted.
- All 16 lexer regression groups passed through CTest, including escaped quotes,
  sign boundaries, longest-match operators, comment boundaries and malformed input.
- Verified complete byte coverage and source locations for 32 deterministic
  inputs, each containing all 256 byte values in a different order.
- Tokenized all three maintained examples without any `INVALID` tokens. The new
  complete-program sample has 118 visible tokens, including its comments.
- Reconstructed each example exactly from the CLI's escaped lexemes, including
  a CRLF copy in a path with spaces; hiding whitespace changed only display rows.
- Confirmed malformed input continues to EOF and code after an unfinished string
  is still scanned. Detailed error messages remain scheduled for step 4.
- Checked original source display, CLI usage/file errors and the PowerShell
  build-and-run command with the full CRLF sample.

The scanner now recognizes the categories needed by the base dialect. Detailed
error messages, length checks and lexical error exit codes remain for step 4.
There is no parser or B-Minor program execution yet.

Step 4 was approved by the user after reviewing the complete token scanner.

## Step 4: lexical diagnostics and recovery

Implemented:

- A structured `Diagnostic` with a stable code, message, precise location and span.
- A `LexResult` that owns recovered tokens and diagnostics together.
- Nine lexical error codes covering unknown bytes, unfinished literals/comments,
  character width, invalid literal bytes, unfinished escapes and length limits.
- A 256-character identifier limit and 256-content-unit string limit, with an
  escape pair counting as one unit and the raw source spelling retained.
- One primary cause per invalid token, documented recovery boundaries and no
  exceptions for malformed source input.
- Diagnostics on stderr and lexical failure exit code 1 in `--tokens` mode.
- A six-error sample in `examples/lexical_errors.bminor` and updated reading/rule docs.
- Ten diagnostic regression groups and a real CLI recovery test, alongside the
  existing 16 lexer regression groups.

Validation passed:

- Built with MinGW GCC 6.3.0 and enabled warning flags, without warnings.
- All three CTest targets passed: 16 lexer groups, 10 diagnostic groups and
  the real CLI test for recovered output, stderr separation and exit code 1.
- Checked identifier/string lengths at 255, 256 and 257 units, including
  256 escaped units occupying more than 256 raw source bytes.
- Verified diagnostic/token span consistency on 16 deterministic all-byte inputs,
  while the existing 32 all-byte source-coverage cases remained valid.
- Scanned all four maintained examples and reconstructed each file exactly from
  the CLI token lexemes. Only the intentional error sample returned errors.
- The error sample reported six diagnostics while retaining later `return` and
  EOF tokens. Locations also matched a CRLF copy in a path containing spaces.
- `run.ps1` forwarded all six diagnostics and exit code 1 for that CRLF copy;
  original source-display mode still returned success without lexical validation.

Step 5 was approved by the user after reviewing lexical diagnostics.

## Step 5: grammar, declaration parser and initial AST

Implemented:

- A documented target EBNF and a separately documented implemented subset.
- Recursive-descent strategy analysis: factored declaration prefixes, disjoint
  FIRST sets, list boundaries, absence of left recursion, and planned handling
  of identifier/colon lookahead and dangling else in later statement parsing.
- Explicit operator precedence/associativity choices for the upcoming expression parser.
- Variable declarations, atomic/nested array types, literals and braced initializers.
- Function prototypes with atomic/void return types and typed parameters,
  including unsized array parameters.
- Owned AST nodes with source positions and nullable semantic-type annotations.
- A separate AST printer and shared source-spelling display escaping.
- `--ast` / `run.ps1 -Ast`, a declaration sample and a partial-tree error sample.
- Parser diagnostic codes for unexpected tokens, unfinished later-step features
  and bounded nesting, with a controlled stop at the first syntax error.
- Twelve parser regression groups and a CLI test covering valid/partial ASTs
  and lexical-error gating. Existing lexer/diagnostic checks remain enabled.

Validation passed:

- Built with MinGW GCC 6.3.0 and the configured warnings, without compiler warnings.
- All five CTest targets passed: the existing lexer/diagnostic groups and lexical
  CLI test, 12 new parser groups, and the declaration AST CLI test.
- The maintained declaration sample produced seven variable declarations, three
  function prototypes and four parameters with null semantic annotations.
- Compared the full AST and source coordinates against a CRLF copy in a path
  containing spaces; they matched after normalizing only the filename.
- Checked partial AST output at the first syntax error, EOF diagnostics,
  unimplemented body/expression diagnostics, bounded recursion and AST ownership.
- Confirmed lexical errors suppress parser startup and do not duplicate messages.
- Checked conflicting display flags, old source display, shared token escaping
  and exact source reconstruction from existing token-mode samples.
- `run.ps1 -Ast` forwarded valid AST output and syntax-error exit status for
  both normal and path-with-spaces inputs.

This is an initial declaration parser. General expressions and array bounds
arrive in step 6, function bodies/statements in step 7, and panic-mode recovery
in step 8. Semantic checks are reserved for later project phases.

Step 6 was approved by the user after reviewing declaration parsing.

## Step 6: expression parsing and precedence

Implemented:

- Recursive-descent expression routines matching the documented precedence levels.
- A shared left-fold loop for left-associated operators and right recursion for
  assignment/power, preserving the choices recorded in step 5.
- Identifier, unary, binary, group, call, index and postfix AST nodes with owned
  children, exact operator/delimiter tokens and nullable semantic annotations.
- Expression-valued initializers, nested initializer elements and array bounds.
- Explicit group nodes that preserve opening-parenthesis locations without
  overwriting the source positions of their contained leaves.
- Active-recursion and expression-height checks, including iterative chains.
- An expression sample, a malformed-call sample, updated English comments and
  documentation, ten expression regression groups and an expression CLI test.

Validation passed:

- Built with MinGW GCC 6.3.0 and configured warning flags, without warnings.
- All seven CTest targets passed: the existing lexer/diagnostic/parser checks,
  ten expression groups, and the lexical/declaration/expression CLI tests.
- Checked exact expected tree shapes for arithmetic, comparisons, logic,
  assignment, exponentiation, prefix/postfix operations, grouping, calls and indices.
- Checked expression-valued array bounds and nested initializer elements, while
  preserving original name/operator/delimiter source locations and null types.
- Checked malformed operands, commas, parentheses, calls and indices, including
  missing operands at EOF and a diagnostic after a skipped block comment.
- Deep unary, grouped, assignment, power, left-fold, suffix and nested-call inputs
  returned PAR003. A left-associated tree of height 128 and a shallow call with
  300 arguments were accepted; height 129 was rejected safely.
- The expression sample produced 12 variable declarations and one prototype.
  Its full AST and coordinates matched a CRLF copy in a path with spaces.
- Checked the malformed-call sample's single PAR001 and retained earlier
  declaration, with function bodies still explicitly reserved for step 7.
- Confirmed old source display and exact token-mode source reconstruction.
- `run.ps1 -Ast` forwarded the new AST output and expected error exit status.

Function bodies/statements remain for step 7 and panic-mode syntax recovery for
step 8. Parsing still leaves name, type, constant and assignment-target checks
to semantic analysis.

Next review gate: user approval before step 7 (statements and function bodies).
