# Lexical rules

## Reference and scope

The user approved using the base textbook dialect while the course-specific
definition is unavailable. The reference is Douglas Thain's
[Appendix B: The B-Minor Language](https://dthain.github.io/books/compiler/chapterB.pdf),
section B.2. We do not add extensions from other annual course variants.

Step 2 implements only the rules below. Numeric, string and character literals,
operators, delimiters and comments belong to step 3. In particular, digits at
the beginning of a token are still unsupported here. Their appearance as
`INVALID` is a limitation of this intermediate build, not a final language rule.

## Recognized forms

| Form | Rule | Result |
| --- | --- | --- |
| Identifier candidate | `[a-zA-Z_][a-zA-Z0-9_]*` | An entire ASCII word, then checked against the reserved-word table |
| Reserved word | Exact, case-sensitive table lookup | A keyword-specific kind or `BOOLEAN_LITERAL` |
| Whitespace | `[ \t\r\n]+` | One `WHITESPACE` token retaining the original bytes |
| Unsupported byte | One byte not recognized by the rules above | `INVALID`, then continue scanning |
| End of input | The cursor has reached the source size | One `EOF` token with an empty lexeme |

The reserved words are:

```text
array boolean char else false for function if
integer print return string true void while
```

`true` and `false` receive the `BOOLEAN_LITERAL` kind; their lexemes distinguish
the values. `while` is reserved at the lexical level; this alone does not imply
that the base grammar accepts while statements. `auto`, `float` and `double`
are ordinary identifiers in the selected base dialect.

## Longest match and priority

The lexer consumes the complete identifier candidate before checking keywords.
Thus `returnValue` is one identifier, `return` is a keyword, and `Return` is an
identifier. Whitespace is also consumed as a complete run.

The textbook's identifier length limit will be checked with lexical diagnostics
in step 4. The current scanner retains a long word as a single token.

## Locations and source preservation

Every token owns its lexeme and filename, plus a one-based line and column and
a zero-based byte offset. A tab advances one column; columns are byte counts,
not visual tab stops or Unicode character counts. Non-ASCII bytes are currently
unsupported. CRLF advances the cursor by two bytes and one line; standalone CR
and LF each advance one line. The first token starts at line 1, column 1.

EOF points immediately after the last byte. For an empty file its location is
line 1, column 1, offset 0. This cursor convention is separate from
`SourceFile::line_count()`, which reports zero lines for an empty file.

Whitespace is kept in the token vector even when hidden in the CLI. Concatenating
all lexemes reproduces the original input, and token offsets have no gaps. The
CLI escapes control characters for readable table rows without changing tokens.

`--tokens` currently returns success after scanning even if it prints `INVALID`
rows. Step 4 will add lexical diagnostics and the corresponding failure status.
