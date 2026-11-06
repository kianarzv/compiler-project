# Lexical rules

## Reference and scope

The user approved using the base textbook dialect while the course-specific
definition is unavailable. The reference is Douglas Thain's
[Appendix B: The B-Minor Language](https://dthain.github.io/books/compiler/chapterB.pdf),
sections B.2 through B.4. We do not add extensions from annual course variants.
Step 3 extends the original word scanner with literals, symbols and comments.
The detailed diagnostic system remains scheduled for step 4.

## Recognized forms

| Form | Rule | Result |
| --- | --- | --- |
| Identifier candidate | `[a-zA-Z_][a-zA-Z0-9_]*` | An entire ASCII word, then checked against the reserved-word table |
| Reserved word | Exact, case-sensitive table lookup | A keyword-specific kind or `BOOLEAN_LITERAL` |
| Decimal integer | `[0-9]+` | `INTEGER_LITERAL`, without converting or changing the digits |
| String | See quoted-literal rules below | `STRING_LITERAL` |
| Character | Exactly one quoted unit, described below | `CHARACTER_LITERAL` |
| Operators | `+ - * / % ^ ++ -- ! < <= > >= == != && \|\| =` | A distinct kind for each spelling |
| Delimiters | `(` `)` `{` `}` `[` `]` `,` `;` `:` | A distinct kind for each delimiter |
| Line comment | `//` followed by bytes up to CR, LF or EOF | `LINE_COMMENT`, excluding the line ending |
| Block comment | `/*` followed by bytes through the first `*/` | `BLOCK_COMMENT`, including both markers |
| Whitespace | `[ \t\r\n]+` | One `WHITESPACE` token retaining the original bytes |
| Unknown byte or malformed form | See boundary handling below | `INVALID`, then continue scanning |
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
identifier. Whitespace and decimal digits are also consumed as complete runs.

Supported two-byte operators win over their one-byte prefixes: `>=` is one
token, and `===` is `==` followed by `=`. `//` and `/*` start comments before
`/` can be treated as division. A solitary `&` or `|` is invalid.

Signs remain separate operators: `x-12` and `x - 12` have identical token kinds
and lexemes. Leading zeros stay decimal. The scanner does not convert integer
values or check their range. Numeric range analysis belongs to later stages.
Adjacent categories can be scanned separately, such as `123abc` becoming an
integer followed by an identifier; whether that sequence is legal is a parser
decision. This base dialect does not add floating-point or hexadecimal literals.

## Quoted literals and escapes

The following expressions describe valid spellings. Hex ranges refer to raw
source bytes; the printable ASCII policy is an explicit implementation choice
for details the informal reference does not fully specify.

```text
escape          = \\[\x20-\x7E]
string_unit     = [\x20-\x21\x23-\x5B\x5D-\x7E] | escape
character_unit  = [\x20-\x26\x28-\x5B\x5D-\x7E] | escape
string_literal  = '"' string_unit* '"'
character_literal = "'" character_unit "'"
```

A unit is one printable ASCII character excluding its delimiter/backslash,
or a backslash plus one printable ASCII character. This accepts empty strings,
escaped delimiters and backslashes; a character literal requires exactly one
unit. Comment markers inside a literal remain literal text.

The base reference defines `\n` and `\0` specially, and other escaped characters
as the following character. For example, `\q` is a permitted spelling; we do
not silently adopt C's escape list. This lexer keeps the raw spelling and does
not decode values, so `\t` is not converted into a tab during tokenization.

Raw tabs, other control bytes and non-ASCII bytes inside quoted literals make
the token invalid. A physical newline cannot be escaped to continue a literal.

## Comment and invalid-token boundaries

Comments retain all their bytes, including arbitrary text. Block comments do
not nest: the first `*/` closes the comment, even if another `/*` appeared.
Unterminated block comments form one `INVALID` token through EOF.

Malformed quoted literals are grouped through an unescaped closing delimiter,
or stop just before the first physical newline/EOF if no closing delimiter is
found. Empty/multiple-unit character literals are also invalid. The next scan
can still process whitespace and subsequent code. Unknown standalone bytes
each produce one `INVALID` token; none are silently discarded.

Identifier and string length limits will be checked with lexical diagnostics in
step 4. The current scanner retains long words and validly closed strings as
single tokens. Detailed causes and messages for invalid forms also come then.

## Locations and source preservation

Every token owns its lexeme and filename, plus a one-based line and column and
a zero-based byte offset. A tab advances one column; columns are byte counts,
not visual tab stops or Unicode character counts. Non-ASCII bytes outside
comments are invalid. CRLF advances the cursor by two bytes and one line;
standalone CR and LF each advance one line. The first token starts at line 1, column 1.

EOF points immediately after the last byte. For an empty file its location is
line 1, column 1, offset 0. This cursor convention is separate from
`SourceFile::line_count()`, which reports zero lines for an empty file.

Whitespace and comments are kept in the token vector. Whitespace can be hidden
in the CLI, but comments are always shown. Concatenating all lexemes reproduces
the original input, and token offsets have no gaps. The CLI escapes control
characters for readable table rows without changing tokens.

`--tokens` currently returns success after scanning even if it prints `INVALID`
rows. Step 4 will add lexical diagnostics and the corresponding failure status.
