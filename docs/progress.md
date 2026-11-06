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

Step 7 was approved by the user after reviewing expression parsing.

## Step 7: statements and complete function bodies

Implemented:

- Function definitions with owned block bodies, retaining prototypes as null bodies.
- Blocks, local variable declarations, expression/empty statements, if/else,
  for, return and print, using the existing expression parser.
- Seven statement node kinds with original source positions and null semantic types.
- Significant-token lookahead to distinguish local declarations from identifier-led
  expressions, including when comments/whitespace appear before the colon.
- Nearest-unmatched-if association and all optional for header combinations.
- Shared global/local variable declaration tails and explicit rejection of nested
  function declarations and other syntax outside the selected base grammar.
- Recursion guards for nested statements, preserving support for wide shallow blocks.
- A complete-program sample and an intentional failed-body sample, updated English
  comments/readme/grammar guidance, 12 statement groups and a full-program CLI test.

Validation passed:

- Built with MinGW GCC 6.3.0 and enabled compiler warnings, without warnings.
- All nine CTest targets passed: five unit suites (including 12 new statement
  groups) and lexical/declaration/expression/statement CLI coverage.
- Parsed the new sample's three complete definitions and both earlier complete
  programs (`hello.bminor` and `lexer_full.bminor`) with no parser diagnostics.
- Verified nearest-else association with unbraced, braced and double-else forms,
  all eight optional for-header combinations, name/colon lookahead across comments,
  block structure, empty statements, print value order and optional returns.
- Verified exact CRLF keyword/block/empty-statement source locations, null semantic
  annotations and AST labels that distinguish prototypes from empty definitions.
- Checked malformed statements, missing braces at EOF, forbidden nested functions,
  top-level syntax boundaries and continued separation of semantic checks.
- Deep blocks/branches/loops returned PAR003, while 400 shallow statements parsed.
- Checked first-error cleanup and parser reuse; an incomplete function was dropped
  while preceding global declarations remained and later declarations were not parsed.
- The complete sample's AST matched a CRLF copy in a path containing spaces.
  Token-mode source reconstruction and the earlier source-display mode still passed.
- `run.ps1 -Ast` forwarded complete AST output and the failed-body sample's single
  PAR001 with exit code 1; lexical-error gating still avoided duplicate messages.

The selected syntax grammar is implemented. Syntax recovery remains for step 8,
and semantic checks remain for later project phases. A body error currently
discards the incomplete function and stops before later top-level declarations.

Step 8 was approved by the user after reviewing function/statement parsing.

## Step 8: panic-mode syntax recovery

Implemented:

- Recovery at top-level declaration, block statement and nested child-body boundaries.
- Delimiter-aware synchronization using the already-consumed construct prefix,
  with semicolon, declaration/statement starts, owning else/brace and EOF rules.
- Explicit `ErrorDecl`/`ErrorStmt` source spans rather than silently dropping
  malformed regions or constructing fake valid expressions.
- Preservation of valid earlier/later statements, surrounding functions and
  later globals, including recovery after malformed control headers.
- Unfinished-block markers at EOF with one missing-brace message per parse.
- Retained nesting limits and error exit status; valid input behavior is unchanged.
- A five-error example, a dedicated recovery guide and updated English comments.
- Twelve recovery groups and a CLI test, with prior error-expectation tests
  updated to require later-node preservation instead of first-error stopping.

Validation passed:

- Built with MinGW GCC 6.3.0 and configured warning flags, without warnings.
- All 11 CTest targets passed: six unit suites and five CLI checks, including
  12 new recovery groups. Earlier error tests now verify continuation explicitly.
- The new sample reports five PAR001 diagnostics, with one ErrorDecl and four
  ErrorStmt markers, while preserving the else branch, final return and global after.
- Checked global/local synchronization, missing semicolons, malformed control
  headers, parameter/body isolation, broken call/initializer delimiters and
  zero-byte missing children at owning else/brace boundaries.
- Checked unfinished nested blocks at EOF, deduplicated missing-brace reporting,
  repeated parsing state reset, resource-error recovery and continued progress.
- Ran 64 deterministic streams of 80 lexically valid tokens twice each, verifying
  bounded diagnostics, monotonic source spans and repeatable printable trees.
- A CRLF copy in a path with spaces retained the same diagnostic lines/columns
  and tree structure; error-span byte lengths correctly included the extra CRs.
- All five maintained valid syntax samples still produced clean trees. The three
  older parser-error examples now retained their following declarations.
- Confirmed lexical-error gating, source display and exact token reconstruction.
- `run.ps1 -Ast` forwarded the recovered tree, all five messages and expected
  exit code 1 without shell error wrappers.

Recovery produces a partial structural view, not a corrected or semantically
valid program. Lexical errors still prevent AST-mode parser startup, while
token mode continues to report every lexical error as before.

Step 9 was approved by the user after reviewing syntax recovery.

## Step 9: AST-based source-span classification

Implemented:

- A `SyntaxRole` model and one classified span per non-EOF token, retaining all
  source coordinates, byte lengths, comments and whitespace.
- Lexical categories refined by a const AST traversal for function declarations/
  calls, variable declarations/references and parameter declarations.
- Exact occurrence matching rather than name-based or next-parenthesis guesses,
  including grouped callees and composite/indexed callee behavior.
- Traversal through every expression, type-bound, initializer and statement field.
- Error decorations independent of base roles, with recovered syntax support and
  a complete lexical fallback when parser startup is blocked by lexical errors.
- `--classify` / `run.ps1 -Classify`, optional whitespace rows and a role demo.
- A single output-mode enum for CLI exclusivity, preparing the later renderers.
- Updated English comments, a classification guide, 12 classifier groups and
  a real CLI classification test.

Validation passed:

- Built with MinGW GCC 6.3.0 and configured warnings, without warnings.
- All 13 CTest targets passed: seven unit suites and six CLI checks, including
  12 classifier groups checking expected roles and source-coverage invariants.
- Proved that omitting the AST keeps identifiers generic while supplying it
  distinguishes declaration/call roles; token and AST snapshots were unchanged.
- Checked repeated spellings, parameter/value roles, local declarations, grouped
  callees, calls through indexed/call results and every existing AST child field.
- Checked recovery decorations independently from categories, preserved later
  valid names, lexical fallback and zero-byte missing-node behavior.
- Verified exact byte coverage for empty/whitespace/CRLF input and 16 deterministic
  inputs containing every byte value. No EOF or synthetic spans were added.
- The maintained sample showed all seven work occurrences with their expected
  declaration/call/value/local roles; a CRLF copy retained line/column/category
  results in a path with spaces. Hiding whitespace changed only displayed rows.
- Reconstructed complete source bytes from CLI offsets/escaped lexemes for
  maintained programs, the five-error sample and an all-256-bytes file.
- Checked error exit codes, conflicting output modes, original source display,
  recovered classification and unchanged token/AST behavior.
- `run.ps1 -Classify -ShowWhitespace` forwarded both switches, complete output
  and all five syntax messages with expected exit code 1 for the error example.

Roles describe syntax rather than resolved symbols or types. This step produces
inspectable spans; ANSI and HTML rendering remain for steps 10 and 11.

Step 10 was approved by the user after reviewing source-span classification.

## Step 10: ANSI syntax rendering

Implemented:

- An ostream-based renderer consuming the existing AST-refined source spans.
- The suggested PDF palette for keywords, types, functions, literals, comments,
  operators and errors, with default-colored variables/parameters.
- Red underline error precedence, unchanged whitespace and immediate per-span
  style resets so later tokens/the prompt do not inherit the previous style.
- Exact original-byte copying, full pre-output span validation and write/flush
  failure reporting; no headings or synthetic final newline in ANSI stdout.
- `--ansi` / `run.ps1 -Ansi`, mutually exclusive with other display modes.
- A small Windows-only CRT adapter to suppress stdout newline translation,
  separate from the standard C++11 rendering core.
- Explanatory English comments, a rendering guide and updated usage/walkthrough.
- Eleven renderer unit groups and a real CLI suite including binary capture.

Validation passed:

- Built with MinGW GCC 6.3.0, strict C++11 and configured warnings.
- All 15 CTest targets passed, including the existing lexer/parser/classifier
  suites, 11 new rendering groups and the ANSI CLI checks.
- Checked the exact palette, resets, AST-derived function/default-variable
  distinction, grouped calls, recovered syntax and lexical fallback.
- Verified tabs, spaces, multiline comments, raw literals, arbitrary byte values,
  source-authored escape bytes, empty input and absent final newline.
- Captured the native CLI stream to a file and compared bytes after removing
  generated SGR; mixed CRLF/CR/LF and a spaced filename were preserved exactly.
- Checked five syntax and six lexical messages on stderr, full source on stdout,
  expected exit code 1 and usage status 2 for incompatible options.
- Passed 15 additional native-byte/PowerShell smoke checks across all six valid
  syntax samples, both error samples, CRLF/spaced paths, empty/whitespace input,
  no-final-newline input and all byte values. `run.ps1 -Ansi` forwarded normal
  output and the recovered preview with their expected statuses.

ANSI display depends on terminal support for SGR/256-color/italic styling. Roles
still describe syntax; semantic name/type checking remains for later phases.

Next review gate: user approval before step 11 (standalone HTML/CSS rendering).
