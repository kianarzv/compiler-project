# B-Minor Front End

A C++ implementation developed in small, runnable steps for phase one of the
compiler design project. The target language is B-Minor.

## Current state: step 6

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
and function prototypes. Expressions include arithmetic, comparisons, logical
operators, assignment, unary minus/not, groups, calls, indexing and postfix
increment/decrement. Array sizes also accept expressions. The AST retains source
locations and null semantic-type annotations. Function bodies remain for step 7;
syntax parsing currently stops after its first error, retaining completed declarations.

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
`-Ast` and `-Tokens` are alternative display modes; whitespace display applies
only to token mode.

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
It reports `PAR001` at the missing argument's closing parenthesis and retains
the completed `ready` declaration. `PAR003` reports excessive recursive nesting
or expression-tree height; the current implementation limit is 128.

For a partial AST and a syntax diagnostic, use
`-Source examples/declaration_error.bminor -Ast`. It retains `ready`, reports
`PAR001` for the missing colon, and stops before `unreached`. The full B-Minor
programs in `hello.bminor` and `lexer_full.bminor` can still be tokenized, but
their function bodies produce `PAR002` in AST mode until step 7.

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

Exit codes: `0` for success or help, `1` for file/lexical/parser errors, and `2`
for incorrect command-line usage. Lexical errors affect token and AST modes;
parser errors affect AST mode. Byte counts depend on the
file's line endings. Default source-display mode only reads and shows the file;
it does not perform lexical validation.

## Code map

- `src/main.cpp`: command-line handling and output.
- `include/bminor/source_file.hpp`: source-file data and its public interface.
- `src/source_file.cpp`: file reading and line counting, including LF and CRLF.
- `include/bminor/source_location.hpp`: filename and cursor coordinates.
- `include/bminor/token.hpp` and `src/token.cpp`: token kinds, data and labels.
- `include/bminor/lexer.hpp` and `src/lexer.cpp`: scanning and word classification.
- `include/bminor/diagnostic.hpp` and `src/diagnostic.cpp`: diagnostic data,
  stable error codes and message rendering.
- `include/bminor/ast.hpp` and `src/ast.cpp`: owned syntax nodes and tree output.
- `include/bminor/parser.hpp` and `src/parser.cpp`: declaration/type/expression parsing.
- `include/bminor/text_output.hpp` and `src/text_output.cpp`: shared escaped text display.
- `tests/lexer_tests.cpp`: 16 regression groups for words, literals, operators,
  comments, malformed input, positions and complete source coverage.
- `tests/diagnostic_tests.cpp`: 10 regression groups for error causes, spans,
  length boundaries, recovery and owned scan results.
- `tests/check_lexical_cli.cmake`: real CLI output streams and error exit status.
- `tests/parser_tests.cpp`: 12 groups for declaration structure, positions,
  syntax errors, unfinished features, nesting limits and AST ownership.
- `tests/check_ast_cli.cmake`: valid/partial AST output and lexical-error gating.
- `tests/expression_tests.cpp`: 10 groups checking exact expression shapes,
  precedence, associativity, calls/indexing, positions, errors and depth limits.
- `tests/check_expression_cli.cmake`: expression AST output and malformed-call reporting.
- `examples/`: small inputs to run after each step.
- `docs/roadmap.md`: the staged plan and observable results.
- `docs/progress.md`: completed work and validation for each step.
- `docs/lexical-rules.md`: selected dialect, formal rules and current limits.
- `docs/code-walkthrough.md`: execution flow and where to make future changes.
- `docs/grammar.ebnf`: the target B-Minor grammar for steps 5 through 7.
- `docs/parser-design.md`: implemented subset, FIRST/FOLLOW decisions, precedence,
  AST ownership and current parser limitations.

Source text is retained without newline conversion, since later output must
preserve its formatting. Generated files stay in `build/` or `output/`.
Reference documents are kept locally and are excluded from Git.
The `.local/` directory contains untracked development checks; use `examples/`
for the sample files maintained with the project.
