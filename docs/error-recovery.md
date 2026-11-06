# Panic-mode syntax recovery

## Result and visible behavior

Step 8 changes parser errors from a first-error stop to recovery at declaration
and statement boundaries. `ParseResult` still owns the tree and diagnostics;
any diagnostic means the source is invalid and the CLI returns 1. The tree is
a recovered view of source structure, not a corrected or executable program.

For example, `examples/syntax_errors.bminor` contains five independent errors.
Its AST retains the valid global declarations, the function, a later print,
the surviving else branch and the final return. Discarded text appears as an
`ErrorDecl` or `ErrorStmt` with a starting location and raw byte length.

Run it with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/syntax_errors.bminor -Ast
```

The AST goes to stdout and all five messages to stderr. Exit code 1 is expected.
The older declaration, expression and statement error examples now retain valid
nodes *after* their errors too; they no longer stop at the first diagnostic.

## Where failures are caught

- `parse()` catches a failed top-level declaration, records an `ErrorDecl` and
  continues its declaration loop.
- `parse_block()` uses `parse_recovering_statement()` for each statement. A
  failed statement becomes an `ErrorStmt`, while the surrounding block survives.
- `parse_if()` and `parse_for()` use the same wrapper for their child bodies.
  A bad branch can therefore be explicit without destroying the whole control
  statement. Invalid control headers still discard that malformed construct.

The original `ParseFailure` signal unwinds the locally owned incomplete node.
Successful earlier nodes stay owned by their parent. No arbitrary expressions,
semicolons, conditions or closing tokens are inserted to make the input pass.

## Synchronization rules

`synchronize()` starts with the construct's saved token index and the failure
cursor. It reconstructs delimiter counts from tokens already consumed, then
scans forward. Counts use token kinds, so punctuation inside a string or comment
cannot change the balance.

| Context | Boundary behavior |
| --- | --- |
| Program scope | Consume an ending semicolon or a discarded brace group; preserve a following `IDENTIFIER :` outside header/bracket/brace groups. |
| Ordinary local statement | Consume its semicolon, or stop before a new declaration or `if`, `for`, `return`, `print`. A semicolon can end a broken call/initializer even if a delimiter is missing. |
| Malformed control/function region | Skip header semicolons and enclosed brace bodies rather than exposing their fragments as sibling statements. A malformed if's available else body is discarded with it. |
| Enclosing block | Leave an unowned `}` for `parse_block()` to consume. |
| Expected branch/body | Leave an available `else` for its owning if, preserving nearest-if association. |
| Stray program-scope `}` | Consume it, since no enclosing block can own it. |
| EOF | Stop scanning; never advance beyond the final token. |

At the start of a brace group, stale parenthesis/bracket counts are cleared.
This lets a discarded body provide a boundary after an unclosed header. At
program scope, name/colon pairs inside a parameter list or skipped body are
not treated as new globals. For ordinary statements, keywords/declaration
starts are stronger boundaries than an unfinished expression delimiter.

Recovery is conservative. It may discard uncertain source until a safe point
and does not infer a missing block boundary before a later function. It does
not promise a uniquely repaired AST for ambiguous invalid input. A separate
semantic pass must still reject the diagnostics rather than executing this tree.

## Progress and ownership of delimiters

Before preserving a normal synchronization start, the recovery loop checks that
some input has already been consumed. Otherwise it consumes the offending token.
The exceptions are EOF, a block's closing brace or an if's else: their owning
routine consumes that token or terminates its loop. A missing branch at such
a boundary gets a zero-byte `ErrorStmt`, which records the missing child without
stealing its parent's token. This prevents both infinite loops and lost else/
brace ownership.

Error-node lengths cover the discarded region through the next cursor position,
including skipped terminators and trivia. They can differ between LF and CRLF.
Diagnostic spans remain independent: a missing semicolon's diagnostic points at
the unexpected next token, even when that token is preserved as valid later code.
All positions come from the original lexer tokens, and no source text is changed.

## Unclosed blocks and resource limits

At EOF, `parse_block()` keeps its known statements and sets `incomplete=true`.
The printer displays `BlockStmt incomplete`; no closing brace is fabricated.
Nested unclosed blocks get one missing-closing-brace diagnostic at EOF rather
than a duplicate for every open block. An unrelated missing operand can still
have its own diagnostic. The per-parse flag resets before every `parse()` call.

Nesting/tree-height failures use the existing `PAR003`. Their scopes unwind
through the existing guards, then synchronize like other failures so later
ordinary code can still be parsed. The depth limit remains 128.

## Checks

`tests/recovery_tests.cpp` covers global/local continuation, missing semicolons,
control headers, else ownership, skipped function regions, broken delimiters,
EOF blocks, CRLF spans, resource errors and parser reuse. It also runs 64
deterministic streams of 80 lexically valid tokens, checking bounded diagnostics,
monotonic spans and repeatable AST output. A test timeout detects regressions
that prevent progress. `tests/check_recovery_cli.cmake` verifies the five-error
sample, recovered nodes, stdout/stderr separation and exit status.
