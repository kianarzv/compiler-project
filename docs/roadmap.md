# Phase-one roadmap

Status: all 12 steps are complete for the approved base dialect. The final
snapshot is `phase1-final`; use `verify.ps1` to reproduce the checks and inspect
the pipeline report. See `submission-guide.md` for the handoff. Later compiler
phases begin only after a new user request.

Each step ends with a runnable result, validation, and a descriptive Git commit
pushed to the same private repository. Work on the next step starts after the
user reviews and approves the completed step. Commits reflect actual work as it
happens; this repository will also be used for later project phases.

| Step | Work | Visible result |
| --- | --- | --- |
| 1 | Build setup, CLI, source-file loading | Run the sample and see its contents and size. |
| 2 | Token model, basic lexical rules, identifiers and keywords | Inspect tokens with their lexemes and source locations. |
| 3 | Complete the lexer: literals, escapes, operators, delimiters and comments | Inspect tokens for a richer B-Minor sample. |
| 4 | Lexical diagnostics and recovery | See multiple lexical errors while scanning continues. |
| 5 | B-Minor EBNF, parser strategy, types and declarations | Inspect an initial AST of declarations. |
| 6 | Expression parsing, precedence and associativity | Inspect expression trees for arithmetic, calls and indexing. |
| 7 | Statement parsing and the complete phase-one AST | Inspect function bodies, branches, loops and return statements. |
| 8 | Panic-mode syntax recovery | See multiple syntax errors and the recovered AST. |
| 9 | AST-based classification of source spans | Inspect the distinct roles of function names and variable identifiers. |
| 10 | ANSI rendering | See highlighted source in a terminal. |
| 11 | Standalone HTML/CSS rendering | Open highlighted source in a browser. |
| 12 | End-to-end checks, regression coverage and submission documentation | Reproduce the full pipeline on valid and invalid examples. |

## Design constraints

- Keep the implementation in standard C++ and comments in English.
- Keep file loading, lexing, parsing, AST traversal and rendering separate.
- Document formal token rules and longest-match/keyword priority decisions.
- Document B-Minor grammar and the selected parsing strategy.
- Give each AST node a source location and a type field for future phases.
- Preserve source formatting in both renderers.
- Build meaningful automated checks alongside the corresponding behavior;
  use the final step to exercise the complete system.

## Specification points to resolve before the dependent steps

The supplied PDF includes an illustrative **C grammar**, not the complete
B-Minor grammar. The EBNF appendix explains notation; it is not a B-Minor
language specification. For step 2, the user approved proceeding with the base
textbook dialect. Its reference and current lexical rules are recorded in
`docs/lexical-rules.md`; step 5 records grammar and parser decisions in
`docs/parser-design.md` and `docs/grammar.ebnf`. Any later course-specific
requirements should be documented there before changing the affected lexer or
parser rules.

The PDF mentions at least one output format in its overview, while the rubric
lists ANSI and HTML/CSS. This plan includes both formats.
