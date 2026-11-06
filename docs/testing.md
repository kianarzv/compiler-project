# Verification and regression checks

## Run the complete check

Requirements are the same as the compiler build: a C++11 compiler, CMake 3.15+
and Ninja. The Windows wrapper selects `g++` from PATH. No Python packages,
browser, network access or external C++ libraries are needed for CTest.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\verify.ps1
```

The script configures a Release build with testing enabled in `build/verify`,
builds all targets and runs CTest. It returns 0 only if every command succeeds.
It can be called from outside the project; paths resolve from the script's
directory. Existing build directories are reused without being deleted.
To choose another build directory, add `-BuildDirectory "build/my checks"`.

The equivalent generator-independent test invocation, after configuring and
building a chosen directory, is `cmake -E chdir <build-directory> ctest --output-on-failure`.
For the checked MinGW/Ninja setup:

```powershell
cmake -S . -B build/verify -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/verify
cmake -E chdir build/verify ctest --output-on-failure
```

## Coverage

CTest has 18 targets: nine C++ unit suites and nine native CLI checks. The unit
suites contain 106 named groups; a group may check several inputs/boundaries.

| Unit suite | Groups | Main checks |
| --- | --- | --- |
| `lexer_tests` | 16 | Tokens, longest match, comments, raw text and source locations. |
| `diagnostic_tests` | 10 | Lexical causes, spans, length limits and continued scanning. |
| `parser_tests` | 12 | Types, variables, function declarations, ownership and errors. |
| `expression_tests` | 10 | Exact tree shapes, precedence, associativity, calls/indexing and depth/width. |
| `statement_tests` | 12 | Bodies, nearest else, optional for fields and statement nesting. |
| `recovery_tests` | 12 | Synchronization, preserved siblings/owners, EOF blocks, progress and parser reuse. |
| `classifier_tests` | 12 | AST occurrence roles, traversal, fallback, immutability and byte coverage. |
| `ansi_tests` | 11 | Colors/resets, raw bytes, error priority and failed streams/spans. |
| `html_tests` | 11 | Escaping, document/CSS classes, formatting, UTF-8 and failed streams/spans. |

The first eight CLI targets check their corresponding lexical, declaration,
expression, statement, recovery, classification, ANSI and HTML behavior. They
also cover option conflicts, stream separation, file export failures and source
protection. The ninth target, `phase1_pipeline_cli`, exercises the complete CLI
against the same source in every mode, rather than testing one feature at a time.

## Pipeline matrix and inspectable output

`tests/check_pipeline_cli.cmake` runs **19 cases in six modes: 114 processes**.
Every process has a ten-second timeout; the entire matrix has a 120-second limit.
It includes every maintained example:

- Seven syntax-valid examples: hello, lexer_full, declarations, expressions,
  statements, classification and html_preview.
- Four one-error syntax inputs: declaration_error, expression_error,
  statement_error and the earlier lexer-only tokens_basic demonstration.
- The five-error syntax sample and the six-error lexical sample.
- Six generated cases: empty input, whitespace only, true CRLF with a spaced
  filename, a leading-dash/spaced filename without a final newline, excessive
  expression nesting with a surviving declaration, and 400 shallow statements.

For each input, the matrix checks source-display byte counts, complete token
output, lexical-error AST gating, recovered ASTs, classification output,
diagnostic counts/categories, mode-specific exit status, and unchanged input.
AST/classification/ANSI/HTML diagnostic text must agree. ANSI is compared with
the original bytes after removing generated SGR codes. HTML code content is
compared with independently escaped original fixture bytes after removing span
wrappers, preserving CR/LF, whitespace and literal HTML characters.

Matrix inputs are ASCII with tab/CR/LF formatting. Arbitrary bytes, malformed
UTF-8, source-authored terminal escapes and Unicode comments/metadata are checked
by the more focused unit suites. The renderer documentation describes their
different byte-display contracts.

After a successful default verification, open:

- `build/verify/phase1-review/summary.md`: all cases, diagnostic counts and the
  six actual exit codes, with links to their highlighted pages.
- `build/verify/phase1-review/html_preview/highlighted.html`: the valid browser demo.
- `build/verify/phase1-review/syntax_errors/highlighted.html`: recovered syntax.
- `build/verify/phase1-review/lexical_errors/highlighted.html`: lexical fallback.

Each case directory also retains its input, `source.out`, `tokens.out`,
`ast.out`, `classify.out`, `terminal.ansi` and a separate `.err` file per mode.
An empty `.err` is expected on successful analysis. The report is reset on each
matrix run, and the final PASS marker is written only after all cases succeed.
Artifacts remain local under the build directory and are not committed.

## Expected failure is different from a failing test

| Input | Source display | Tokens | AST / classification / ANSI / HTML |
| --- | --- | --- | --- |
| Valid syntax, including empty input | 0 | 0 | 0 |
| Syntax error with clean lexing | 0 | 0 | 1, with recovered structure/output |
| Lexical error | 0 | 1 | 1; AST stdout is empty, classification/renderers use lexical fallback |
| Incorrect CLI usage | 2 | 2 | 2 |
| Missing source or failed output write | 1 | 1 | 1 |

Diagnostic locations/counts are assertions, not incidental text to ignore. A
test succeeds only when an intentionally invalid input returns the expected
status and messages while keeping the required later output. `PAR003` is an
implementation resource limit, not a semantic type error.

## Recorded environment and reproducibility

Final verification used Windows, MinGW GCC 6.3.0, CMake 3.29.2 and Ninja 1.12.0,
with strict C++11 and `-Wall -Wextra -Wpedantic`. A fresh Release build passed
all 18 targets without compiler warnings. A separate `BUILD_TESTING=OFF` build
also produced a working CLI. The source ZIP was rebuilt and tested after
extraction into a directory containing spaces.

Step 11 separately inspected 15 generated pages in headless Chrome for exact DOM
text, computed colors/font styles, underlines, whitespace and absence of injected
elements/external assets. Browser tooling is not part of the shipped test setup.
Other operating systems/compiler versions have not been executed in this
workspace; the core uses standard C++11 and the Windows stdout adapter is
conditionally compiled.

Future semantic-phase tests should be added as new suites. Keep these syntax
checks: syntax-valid examples deliberately do not prove name binding, type
compatibility, lvalue correctness, constant bounds or executable behavior.
