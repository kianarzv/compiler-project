# AST-based source classification

Step 9 creates the source categories that the ANSI and HTML renderers will use.
It deliberately prints a plain table for inspection; the colors arrive in
steps 10 and 11. Classification is based on tokens plus the AST, not regular
expressions or a global list of known function spellings.

## Run the sample

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/classification.bminor -Classify
```

Add `-ShowWhitespace` for every source slice. Direct executable options are
`--classify` and `--show-whitespace`. The table contains filename/line/column,
byte offset, byte length, role, error flag and escaped original source text.
It omits EOF because EOF has no bytes. Hiding whitespace changes only display
rows; the library's spans always include it.

The sample intentionally shows a function name used as a value and a local
declaration reusing its spelling. It is a syntax demonstration, not a claim
that every initializer passes future semantic type checks.

## Roles

| Role | Meaning |
| --- | --- |
| `FUNCTION_DECLARATION` | Name of a function prototype or definition. |
| `FUNCTION_CALL` | An identifier used as a directly named callee, possibly through groups. |
| `VARIABLE_DECLARATION` | Name of a global or local variable declaration. |
| `PARAMETER_DECLARATION` | Name written in a function parameter declaration. |
| `VARIABLE_REFERENCE` | An identifier expression that is not a directly named callee. |
| `IDENTIFIER` | An identifier whose role is unknown without a usable AST node. |
| `TYPE_NAME` | Atomic type keywords, `void` and the array type constructor. |
| `KEYWORD` | Other reserved keywords, including `function`, control flow and print. |
| `INTEGER_LITERAL` | An integer token. |
| `BOOLEAN_LITERAL` | `true` or `false`. |
| `STRING_LITERAL` | A quoted string token. |
| `CHARACTER_LITERAL` | A quoted character token. |
| `OPERATOR` | An operator token. |
| `DELIMITER` | Source punctuation such as brackets, braces, colon or semicolon. |
| `COMMENT` | A line or block comment. |
| `WHITESPACE` | The original spaces, tabs and line endings. |
| `INVALID` | A lexical invalid token. |

The reference-role label is syntactic. There is no symbol-table lookup here,
so using the name of a known function as a value still gives that occurrence
`VARIABLE_REFERENCE`. Likewise, `unknown()` has a function-call role even if
semantic analysis later finds the name unbound or not callable. Declaring the
same spelling in another block does not change classifications elsewhere.

## Pipeline and AST refinement

`classify_source()` runs the existing lexer and creates one nonempty span per
non-EOF token. Literal/keyword/type/operator/comment categories are lexical.
Identifier tokens start as `IDENTIFIER`.

If lexing is clean, the parser supplies a complete or recovered `Program`.
`AstClassifier` visits all structural children, including array bounds, nested
initializers, function parameters, statements, loop headers and call arguments.
It refines names by exact filename, byte offset and length:

- `FuncDecl` names become function declarations.
- `VarDecl` names become variable declarations.
- `ParamDecl` names become parameter declarations.
- `IdentifierExpr` leaves become references.
- After visiting a call's children, its directly named callee becomes a call.

The call override is applied last so it is not overwritten by the generic
identifier-expression visit. Groups are unwrapped: `(work)(value)` classifies
`work` as a call even though the following token is `)`. For `handlers[i](x)`,
the array and index remain references; the indexed result is not a directly
named function. For `factory()(x)`, the inner `factory` call retains its role.
Strings/comments that happen to contain `work(x)` are never inspected as code.

The lower-level `classify_tokens(tokens, program)` also accepts a null AST.
Tests use this to prove that lexical categories alone do not infer function
roles. The classifier accepts a const AST and never changes its nodes, type
annotations, tokens or source contents.

## Error behavior

`ClassifiedSpan::has_error` is independent of the base role. `ErrorDecl` and
`ErrorStmt` spans mark overlapping non-whitespace tokens, so a discarded string
or keyword keeps its category while a renderer can add an error decoration.
Discarded identifier fragments remain `IDENTIFIER`; the classifier does not
invent function/variable roles for unparsed code. Valid later AST nodes are
still refined normally.

Diagnostic locations are not automatically treated as error regions. For
example, a missing semicolon can point to a valid following `return` that the
parser preserves. Marking that return as erroneous would misrepresent the
recovered tree. A zero-byte missing child or closing brace also has no source
bytes to decorate; its diagnostic still appears and causes exit code 1.

If lexical errors exist, AST-mode parsing remains disabled, as in earlier steps.
Classification still returns a complete lexical fallback table, with invalid
tokens marked as errors and identifiers left uncertain. All lexer diagnostics
are retained. Clean lexing with syntax errors instead uses the recovered AST
and all parser diagnostics. No analysis error is converted into success.

## Source contract and later renderers

Spans are ordered, nonoverlapping and cover the complete input without gaps.
They are never merged, shortened or expanded during refinement. Concatenating
`source.text.substr(span.location.offset, span.length)` reconstructs the original
bytes, including CRLF, tabs, comments and invalid bytes. Line/column conventions
are inherited unchanged from the lexer.

Results own their coordinate/role/diagnostic data. They do not own a second copy
of the source text or borrow the temporary AST/token vector. A renderer uses
the same original `SourceFile` to retrieve bytes and chooses its own palette.
Keeping categories and rendering separate lets both output formats reuse this
classification instead of reinterpreting code independently.

Name refinement uses binary search over the ordered spans. Error ranges start
with binary search and visit only overlapping spans. This avoids scanning the
entire source for every AST name or searching by repeated spellings.

## Validation

`tests/classifier_tests.cpp` checks all lexical categories, declarations and
parameters, repeated spellings, AST dependence/immutability, grouped/composite
callees, complete child traversal, recovered regions, lexical fallback, CRLF,
empty input, owned results and exact byte coverage. Coverage includes 16
deterministic all-byte inputs. `tests/check_classification_cli.cmake` checks
real tables, whitespace display, error messages/status and conflicting modes.
