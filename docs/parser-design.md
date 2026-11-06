# Parser design and grammar

## Reference and current boundary

The target grammar in [grammar.ebnf](grammar.ebnf) is our formalization of the
selected base dialect, using Douglas Thain's
[B-Minor overview](https://dthain.github.io/books/compiler/chapterB.pdf), especially
sections B.3 through B.6. The supplied project PDF's C grammar is illustrative;
it is not the grammar used by this parser.

The EBNF file describes the target for steps 5 through 7. Step 6 implements
declarations and the complete expression family. Statements/function bodies
remain for step 7. The current declaration subset below uses the expression
rules from `grammar.ebnf`:

```text
program = { declaration }, EOF ;
declaration = IDENTIFIER, ":", (prototype_tail | variable_tail) ;
prototype_tail = "function", return_type, "(", [ parameter_list ], ")", ";" ;
variable_tail = value_type, [ "=", initializer ], ";" ;
parameter_list = parameter, { ",", parameter } ;
parameter = IDENTIFIER, ":", parameter_type ;
atomic_type = "integer" | "boolean" | "char" | "string" ;
return_type = atomic_type | "void" ;
value_type = atomic_type | "array", "[", expression, "]", value_type ;
parameter_type = atomic_type
               | "array", "[", [ expression ], "]", parameter_type ;
initializer = expression | "{", [ initializer, { ",", initializer } ], "}" ;
literal = INTEGER_LITERAL | BOOLEAN_LITERAL | CHARACTER_LITERAL | STRING_LITERAL ;
```

Initializers and array bounds now accept names, arithmetic/logical operations,
assignments, grouping, calls, indexing and postfix increment/decrement. Function
bodies/statements still receive an explicit unsupported-feature diagnostic;
they are never skipped or represented as parsed bodies.

An empty file or declaration fragment is a valid parse input. Checking for a
`main` definition, compatible initializers, duplicate names, array dimensions
or constant expressions belongs to semantic analysis. For example, this parser
can build the syntax tree for `x: integer = true;` without claiming it is typed
correctly. `void` only appears in the return-type grammar, and array variables
require bounds while array parameters may omit them.

## Strategy and lookahead

The implementation is a handwritten recursive-descent parser. Its input is the
lexer's original EOF-terminated vector. `skip_trivia()` ignores whitespace and
comments for decisions while retaining their original source positions.
Functions such as `parse_declaration()`, `parse_value_type()` and
`parse_initializer()` follow the corresponding nonterminals.

The current subset can be selected with one significant token of lookahead:

| Decision | FIRST / boundary tokens | Why it is deterministic |
| --- | --- | --- |
| Next declaration or end | `IDENTIFIER` / `EOF` | Declaration starts and program end do not overlap. |
| Declaration tail after name/colon | `function` / atomic type or `array` | The shared prefix is consumed before choosing a tail. |
| Atomic or array type | atomic keywords / `array` | Distinct terminal sets. |
| Optional parameter list | `IDENTIFIER` / `)` | The empty alternative is selected only at `)`. |
| Optional initializer | `=` / `;` | No overlap between FIRST and the empty alternative's FOLLOW. |
| Expression or braced initializer | expression FIRST set / `{` | Expressions do not start with `{`. |
| Optional parameter bound | expression FIRST set / `]` | A variable bound is required; only parameters have an empty alternative. |
| List continuation | `,` / closing `)` or `}` | A comma always requires another item; trailing commas are rejected. |

There is no left recursion in the subset or target expression grammar. The
usual left-recursive form `additive = additive, "+", multiplicative | ...`
is replaced with a repetition. `parse_left_associative()` folds repetitions into
left-associated ASTs. The right-recursive assignment/power rules instead preserve
right grouping. Expression routines stop naturally at context delimiters such
as `;`, `,`, `)` and `]`; they do not consume those tokens as operators.

The *full* grammar has two additional issues; it is not claimed to be globally
LL(1). A local declaration and an expression statement can both begin with an
identifier: looking at the following `:` distinguishes them, so statement
dispatch will use two significant tokens. The optional `else` has the classic
FIRST/FOLLOW conflict with a containing `if`; a recursive-descent `if` routine
will consume an available `else` before returning. This explicit nearest-`if`
policy resolves the ambiguity without backtracking.

## Expression rules implemented in step 6

The overview is informal; these choices were recorded in step 5 and are now
implemented and checked through expected expression-tree shapes.
The grammar splits relational/equality and logical AND/OR into the usual C
order, rather than treating the grouped rows in the overview as single levels.
Exponentiation is right-associated. Unary negation/not bind more tightly than
exponentiation, matching their order in the overview's table.

| Tightest to loosest | Operators | Grouping |
| --- | --- | --- |
| 1 | calls and indexing, postfix `++` / `--` | successive suffixes |
| 2 | unary `-` / `!` | right |
| 3 | `^` | right |
| 4 | `*` / `/` / `%` | left |
| 5 | `+` / `-` | left |
| 6 | `<` / `<=` / `>` / `>=` | left |
| 7 | `==` / `!=` | left |
| 8 | `&&` | left |
| 9 | `\|\|` | left |
| 10 | `=` | right |

Examples of the implemented grouping: `a-b-c` becomes `(a-b)-c`, `a=b=c` becomes
`a=(b=c)`, `2^3^2` becomes `2^(3^2)`, and `-2^2` becomes `(-2)^2`. Unary plus,
compound assignment, floating point, while statements and nested function
definitions are outside this selected target grammar. These choices can be
revised if an actual course-specific definition becomes available.

Each precedence routine delegates to the next tighter level. The shared
`parse_left_associative()` loop receives a pointer to that operand routine and
the current level's operator set. Assignment and power use right recursion
instead. `parse_unary()` handles prefix `-`/`!`; `parse_postfix()` iteratively
wraps its primary with call, index or postfix nodes. `parse_primary()` handles
literal/name leaves and explicitly grouped expressions.

Parsing does not resolve names, check assignment targets, enforce operand types,
evaluate constants, or decide whether a global initializer is constant. Thus an
expression such as `1=2` has a syntax tree but requires a later semantic error.
The sample `examples/expressions.bminor` is a syntax demonstration fragment,
not a promise that its assignments/calls pass those later checks.

## AST and ownership

`Program` owns declarations. `VarDecl` owns its identifier, written type and
optional initializer. `FuncDecl` currently represents a prototype, owning its
name, return type and parameters. Each `ParamDecl` owns its name and type.
Array `TypeSyntax` owns its bound expression and element type; `ArrayInitExpr`
owns nested initializer expressions. Literal leaves retain exact token text.

Expression names use `IdentifierExpr` leaves. `UnaryExpr` and `PostfixExpr` own
one operand; `BinaryExpr` owns left/right operands and the exact operator token,
including for assignment. `GroupExpr` owns the grouped expression. `CallExpr`
owns its callee and argument vector; `IndexExpr` owns its base and index.

Groups are explicit so the opening `(` can be an expression's first token
without overwriting the location of a name inside it. Unary nodes start at
their operator; binary/call/index/postfix nodes start at their left/base/callee
expression. Operators and opening delimiters retain their separate token
locations. This supports later context-aware highlighting and diagnostics.

Every node inherits a location and a nullable `semantic_type` from `AstNode`.
Declaration/parameter locations point to their name. Type nodes point to the
type keyword, array initializers to `{`, and literal/name leaves to their first
token. `TypeSyntax` is source syntax; it is distinct from a resolved semantic
type. `TypeInfo` is forward-declared for a later semantic phase, and every
annotation starts null.

Children use `std::unique_ptr`, giving them a single owner and automatic cleanup
on success or parse failure. Shared semantic types can later use the existing
`std::shared_ptr<const TypeInfo>` annotation. Returned AST nodes do not borrow
the token vector. The parser itself does borrow that vector, which must outlive
the parser. `dump_ast()` is a separate traversal, using shared display escaping
from `text_output.cpp` rather than teaching parsing routines to print.

## Errors and unfinished features

| Code | Meaning |
| --- | --- |
| `PAR001` | Unexpected token or missing required syntax. |
| `PAR002` | A function body requires step 7. |
| `PAR003` | Active recursive nesting or expression-tree height exceeds the implementation limit of 128. |

An EOF error has a zero-byte span at the cursor after the final byte; other
errors cover the current token. A private `ParseFailure` signal unwinds the
incomplete declaration and is caught inside `parse()`. The result contains one
diagnostic and any declarations completed earlier. This is controlled failure,
not panic-mode recovery; continuing after syntax errors is step 8.

`--ast` first scans the whole file. Lexical errors are returned without starting
the parser, preventing duplicate syntax messages for invalid tokens. Otherwise
it prints the complete or partial tree and any parser diagnostic; an error
returns exit code 1. The nesting guard is restored during unwinding, and a
second `parse()` call resets all cursor/error state.

Recursive edges (grouping, unary operators, right-associated operators, nested
calls/indexes, types and initializer lists) use the nesting guard. Iterative
left-associated/suffix chains are also checked through `Expr::depth`, which
records expression-tree height. Without that second check, a shallow parsing
loop could create a tree too deep to print or destroy safely. Leaves have
depth 1; parents add one to the deepest expression child. Array lists and call
arguments update this value as children are appended. This is a depth limit,
not a width limit: a shallow call with 300 arguments is accepted and tested.
