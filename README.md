# B-Minor Front End

A C++ implementation developed in small, runnable steps for phase one of the
compiler design project. The target language is B-Minor.

## Current state: step 10

The program can display a source file or tokenize base B-Minor words, decimal
integers, booleans, strings, characters, operators, delimiters and comments.
Tokens retain their exact text, filename, line, column and byte offset. Source
display from step 1 still works.

Use `examples/lexer_full.bminor` for the new complete-program sample, or
`examples/tokens_basic.bminor` for the earlier word-boundary demonstration.
Malformed literals, unfinished block comments, overlong words/strings and
unknown bytes produce `INVALID` rows and structured diagnostics. Scanning
continues to EOF. Token mode prints diagnostics on stderr and returns exit
code 1 when any lexical error is present.

`--ast` parses declarations, nested array types, expression/braced initializers
and function prototypes/definitions. Expressions include arithmetic, comparisons, logical
operators, assignment, unary minus/not, groups, calls, indexing and postfix
increment/decrement. Array sizes also accept expressions. The AST retains source
locations and null semantic-type annotations. Function bodies support local
declarations, blocks, if/else, for, return, print and expression/empty statements.
Panic-mode syntax recovery now reports multiple errors and retains valid later
statements/declarations. Discarded regions appear as `ErrorStmt`/`ErrorDecl`;
unclosed blocks at EOF are marked incomplete. Errors still cause exit code 1.

`--classify` now combines lexical categories with AST-derived identifier roles,
producing source spans for the color renderers. Function declarations,
calls, variable declarations/references and parameter declarations are distinct.
The table retains comments, exact byte offsets and optional whitespace rows.

`--ansi` displays the complete source with terminal colors based on those roles.
It preserves original formatting and underlines invalid/discarded source in red.
Diagnostics remain on stderr, including when a recovered preview is available.

## Build and run on Windows

Requirements: a C++11 compiler, CMake 3.15 or newer, and Ninja on `PATH`.
The initial build was checked with MinGW GCC 6.3.0 and CMake 3.29.2.

From PowerShell in the project directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1
```

To load your own file:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source .\examples\hello.bminor
```

The script builds the executable before running it. Paths passed to the script
are relative to the project directory, or can be absolute.

To inspect a valid complete-program sample:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/lexer_full.bminor -Tokens
```

To inspect a declaration tree:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/declarations.bminor -Ast
```

The start of its output is:

```text
Program @examples/declarations.bminor:2:1 type=null
  VarDecl @examples/declarations.bminor:2:1 type=null
    Identifier "count" @examples/declarations.bminor:2:1 type=null
    Type integer @examples/declarations.bminor:2:8 type=null
    Literal INTEGER_LITERAL "42" @examples/declarations.bminor:2:18 type=null
```

`Type integer` records source syntax; `type=null` is the separate semantic
annotation reserved for a later project phase. The sample also contains nested
array initializers and functions with ordinary/unsized-array parameters.
`-Ast`, `-Tokens`, `-Classify` and `-Ansi` are alternative display modes. Whitespace
display applies to token and classification tables.

To inspect the new AST-based source roles:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/classification.bminor -Classify
```

The same spelling `work` appears as `FUNCTION_DECLARATION`, `FUNCTION_CALL`,
`VARIABLE_DECLARATION` or `VARIABLE_REFERENCE`, depending on its AST node.
`(work)(value)` is also recognized as a call. Add `-ShowWhitespace` to include
spaces and line endings. Each row shows location, offset, length, role, an error
flag and the original escaped text. See `docs/syntax-classification.md` for all
roles and rules.

With syntax errors, classification uses the recovered AST and marks discarded
regions without losing their base categories. With lexical errors, it returns
complete lexical fallback spans and leaves uncertain names as `IDENTIFIER`.
Diagnostics still go to stderr and cause exit code 1 in either case.

To see highlighted source in an ANSI-capable terminal:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/statements.bminor -Ansi
```

Keywords are bold blue, types cyan, function names yellow, literals orange/green
and comments gray/italic. Ordinary variable names retain the default color.
Use `examples/classification.bminor` to compare function/variable occurrences of
`work`, or `examples/syntax_errors.bminor` to see red underlines and continued
rendering after errors. Whitespace is always included in ANSI output.

The script prints build messages before the preview. For a pure highlighted
stream, run `.\build\bminor.exe --ansi examples/statements.bminor` directly.
See `docs/ansi-rendering.md` for the palette, binary-safe saving and terminal
requirements. Standalone HTML/CSS output is planned for step 11.

To inspect a complete program with function bodies and statements:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/statements.bminor -Ast
```

This sample includes three functions, local arrays, calls, nested blocks,
branches, loops with optional header fields, a bare `return;`, and an empty
statement. The tree shows `FuncDecl definition`, `BlockStmt`, `DeclStmt`,
`IfStmt`, `ForStmt`, `ReturnStmt`, `PrintStmt` and `ExprStmt` nodes. A prototype
still appears as `FuncDecl prototype`. An available `else` attaches to the
nearest unmatched `if`. `ForStmt` labels show which optional header fields exist.

The earlier `hello.bminor` and `lexer_full.bminor` samples now work with `-Ast`
too. This tool displays the program structure; semantic analysis and execution
are separate from this phase-one parser.

To inspect the new expression trees:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/expressions.bminor -Ast
```

In `priority: integer = 1 + 2 * 3;`, the root is `BinaryExpr "+"` and its right
child is `BinaryExpr "*"`. `2^3^2` groups to the right; `20-5-2` groups to the
left. Parentheses produce `GroupExpr` nodes. Calls and indexing appear as
`CallExpr` and `IndexExpr`, retaining their callee/base and argument/index trees.
The sample demonstrates syntax; name binding, assignment-target validity and
constant/type checks belong to later semantic analysis.

For a malformed call, run `-Source examples/expression_error.bminor -Ast`.
It reports `PAR001` at the missing argument's closing parenthesis, records an
`ErrorDecl`, and keeps both surrounding declarations. `PAR003` reports excessive recursive nesting
or expression-tree height; the current implementation limit is 128.

For a partial AST and a syntax diagnostic, use
`-Source examples/declaration_error.bminor -Ast`. It retains `ready`, reports
`PAR001` for the missing colon, adds an `ErrorDecl`, and continues to `unreached`.

For an error inside a function, use `-Source examples/statement_error.bminor -Ast`.
The missing semicolon after `return 0` is reported at line 5, column 1. The
earlier global `version`, the function and its valid earlier print, and the
later global declaration remain in the tree. An `ErrorStmt` replaces the
unfinished return.

To inspect multiple independent syntax errors in one run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/syntax_errors.bminor -Ast
```

This sample intentionally reports **five errors**. The tree still contains the
surviving else branch, final return and global `after`. The malformed loop is
one discarded region, with its header/body represented by an error node. Exit
code 1 is expected. Error-node byte lengths include discarded source/trivia;
missing children may have a zero-length placeholder at an owning else/brace.
See `docs/error-recovery.md` for synchronization rules and recovery limits.

Add `-ShowWhitespace` to include escaped spaces and line endings in the table.
Comments are shown even when whitespace is hidden. To compare with step 2,
run `examples/tokens_basic.bminor`; its first line still produces:

```text
examples/tokens_basic.bminor:1:1    KW_IF          "if"
examples/tokens_basic.bminor:1:4    IDENTIFIER     "ifx"
examples/tokens_basic.bminor:1:8    KW_RETURN      "return"
examples/tokens_basic.bminor:1:15   IDENTIFIER     "returnValue"
```

Columns in the actual output are separated by tabs. Lines and columns start at
1, and the table always ends with EOF. Tabs count as one source column.

To see error reporting and continued scanning:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/lexical_errors.bminor -Tokens
```

This sample intentionally contains six lexical errors. Its token table still
includes the `print`, `return` and EOF tokens, followed by messages such as:

```text
examples/lexical_errors.bminor:2:24: error [LEX001]: Unexpected byte 0x40.
examples/lexical_errors.bminor:5:20: error [LEX002]: Unterminated string literal; expected a closing double quote.
```

An exit code of 1 is expected for this example. `run.ps1` forwards that status.
Add `-ShowWhitespace` to either example to show every original source slice.
See `docs/lexical-rules.md` for all diagnostic codes and recovery boundaries.

To build manually:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build
.\build\bminor.exe .\examples\hello.bminor
.\build\bminor.exe --help
.\build\bminor.exe --tokens examples/tokens_basic.bminor
.\build\bminor.exe --tokens --show-whitespace examples/tokens_basic.bminor
.\build\bminor.exe --tokens examples/lexer_full.bminor
.\build\bminor.exe --ast examples/declarations.bminor
.\build\bminor.exe --ast examples/expressions.bminor
.\build\bminor.exe --ast examples/statements.bminor
.\build\bminor.exe --ast examples/syntax_errors.bminor
.\build\bminor.exe --classify examples/classification.bminor
.\build\bminor.exe --classify --show-whitespace examples/classification.bminor
.\build\bminor.exe --ansi examples/statements.bminor
.\build\bminor.exe --ansi examples/syntax_errors.bminor
ctest --test-dir build --output-on-failure
```

Expected output for the sample file:

```text
Loaded: examples/hello.bminor
Bytes: 77
Lines: 4

main: function integer () = {
    print "Hello, B-Minor!\n";
    return 0;
}
```

Exit codes: `0` for success or help, `1` for file/output/lexical/parser errors, and `2`
for incorrect command-line usage. Lexical errors affect token, AST and
classification and ANSI modes; parser errors affect AST, classification and ANSI modes.
Byte counts depend on the file's line endings. Default source-display mode only reads and shows the file;
it does not perform lexical validation.

## Code map

- `src/main.cpp`: mutually exclusive output modes, argument handling and display.
- `include/bminor/source_file.hpp`: source-file data and its public interface.
- `src/source_file.cpp`: file reading and line counting, including LF and CRLF.
- `include/bminor/source_location.hpp`: filename and cursor coordinates.
- `include/bminor/token.hpp` and `src/token.cpp`: token kinds, data and labels.
- `include/bminor/lexer.hpp` and `src/lexer.cpp`: scanning and word classification.
- `include/bminor/diagnostic.hpp` and `src/diagnostic.cpp`: diagnostic data,
  stable error codes and message rendering.
- `include/bminor/ast.hpp` and `src/ast.cpp`: owned syntax nodes and tree output.
- `include/bminor/parser.hpp` and `src/parser.cpp`: declarations, types, expressions and statements.
- `include/bminor/text_output.hpp` and `src/text_output.cpp`: shared escaped text display.
- `include/bminor/syntax_classifier.hpp` and `src/syntax_classifier.cpp`: lexical
  categories, AST-based source roles and independent error decorations.
- `include/bminor/ansi_renderer.hpp` and `src/ansi_renderer.cpp`: terminal palette,
  validated source spans and original-byte rendering with per-token resets.
- `include/bminor/console_output.hpp` and `src/console_output.cpp`: CLI-only
  Windows binary-stdout adapter to preserve line endings.
- `tests/lexer_tests.cpp`: 16 regression groups for words, literals, operators,
  comments, malformed input, positions and complete source coverage.
- `tests/diagnostic_tests.cpp`: 10 regression groups for error causes, spans,
  length boundaries, recovery and owned scan results.
- `tests/check_lexical_cli.cmake`: real CLI output streams and error exit status.
- `tests/parser_tests.cpp`: 12 groups for declaration/function structure,
  positions, syntax errors, nesting limits and AST ownership.
- `tests/check_ast_cli.cmake`: valid/partial AST output and lexical-error gating.
- `tests/expression_tests.cpp`: 10 groups checking exact expression shapes,
  precedence, associativity, calls/indexing, positions, errors and depth limits.
- `tests/check_expression_cli.cmake`: expression AST output and malformed-call reporting.
- `tests/statement_tests.cpp`: 12 groups for bodies, local declarations, nearest
  else, optional for headers, locations, errors and bounded statement nesting.
- `tests/check_statement_cli.cmake`: complete programs and failed-function output.
- `tests/recovery_tests.cpp`: 12 groups for continuation, boundaries, spans,
  EOF blocks, resource failures, reuse and deterministic progress checks.
- `tests/check_recovery_cli.cmake`: multiple diagnostics and a recovered CLI tree.
- `tests/classifier_tests.cpp`: 12 groups for roles, traversal, AST dependence,
  error fallback, immutability and complete source coverage.
- `tests/check_classification_cli.cmake`: classification output modes and error status.
- `tests/ansi_tests.cpp`: 11 groups for styles, AST names, exact source bytes,
  error precedence, recovery, invalid spans and output failure handling.
- `tests/check_ansi_cli.cmake`: colored CLI output, diagnostics/status, conflicts
  and byte-exact mixed-newline capture.
- `examples/`: small inputs to run after each step.
- `docs/roadmap.md`: the staged plan and observable results.
- `docs/progress.md`: completed work and validation for each step.
- `docs/lexical-rules.md`: selected dialect, formal rules and current limits.
- `docs/code-walkthrough.md`: execution flow and where to make future changes.
- `docs/grammar.ebnf`: the implemented B-Minor syntax grammar.
- `docs/parser-design.md`: supported syntax, FIRST/FOLLOW decisions, precedence,
  AST ownership and current parser limitations.
- `docs/error-recovery.md`: panic-mode synchronization, error nodes and progress rules.
- `docs/syntax-classification.md`: source-span roles and the shared renderer contract.
- `docs/ansi-rendering.md`: terminal rendering, palette, stream contract and usage.

Source loading and ANSI output retain original bytes and line endings.
Generated files stay in `build/` or `output/`.
Reference documents are kept locally and are excluded from Git.
The `.local/` directory contains untracked development checks; use `examples/`
for the sample files maintained with the project.
