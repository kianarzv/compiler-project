# ANSI syntax rendering

Step 10 adds a terminal preview using the source roles from step 9. The renderer
does not tokenize, parse or infer names itself. Function names receive their
color because classification visited the corresponding AST nodes.

## Run

From PowerShell in the project directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/statements.bminor -Ansi
```

Use `examples/classification.bminor` to compare the different occurrences of
`work`: function declarations/calls are yellow, while variable declarations and
references use the terminal's default color. Try `examples/syntax_errors.bminor`
for recovered syntax and red underlines; five diagnostics and exit code 1 are
expected. `examples/lexical_errors.bminor` shows lexical fallback with six errors.

The direct executable emits only the highlighted source on stdout:

```powershell
.\build\bminor.exe --ansi examples/classification.bminor
.\build\bminor.exe --ansi examples/syntax_errors.bminor
```

`--ansi` is an explicit choice, so it emits escape codes even when redirected.
Use a terminal that supports ANSI SGR, 256 colors and italic text, such as the
VS Code integrated terminal. Actual shades depend on the terminal theme; some
terminals may display comments without italics. This mode does not enable
Windows console flags or change the terminal's configuration.

To save the native stream after building, use `cmd` redirection from PowerShell:

```powershell
New-Item -ItemType Directory -Force output | Out-Null
cmd /c ".\build\bminor.exe --ansi examples/classification.bminor > output\classification.ansi"
```

The saved file contains ANSI sequences; a plain text editor may show them
literally. `run.ps1` includes build messages, so use the executable for a clean
saved stream. PowerShell text redirection can decode/re-encode native output;
the `cmd` command above redirects the native bytes directly.

## Palette

This follows the suggested table on page 14 of the supplied phase-one PDF.
`ESC` below is the byte `0x1B`, written as `\x1b` in C++ string literals.

| Source role | Appearance | SGR |
| --- | --- | --- |
| Keywords | Bold blue | `ESC[1;34m` |
| Type names, including array and void | Cyan | `ESC[36m` |
| Function declarations and named calls | Yellow | `ESC[33m` |
| Integer and boolean literals | Orange | `ESC[38;5;208m` |
| String and character literals | Green | `ESC[32m` |
| Operators and delimiters | Light gray | `ESC[37m` |
| Comments | Gray, italic | `ESC[3;90m` |
| Ordinary identifiers, variable/parameter names and references | Default | No added style |
| Invalid tokens or spans marked as errors | Red, underlined | `ESC[4;31m` |
| Whitespace | Unchanged | No added style |

Error styling overrides the base category for visible spans. Every styled span
ends with `ESC[0m`, including the final token, to keep its style from leaking
into the next token or shell prompt. Whitespace remains untouched even between
error spans. A missing token at EOF has no bytes to underline; its diagnostic
still appears on stderr.

## Source and stream contract

`render_ansi(out, source, spans)` reads original bytes using each span's offset
and length. It adds SGR sequences around tokens without adding headings, line
numbers, escaped debug text or a final newline. Tabs, quotes, literal backslash
escapes, comments, LF/CRLF/CR and trailing spaces are copied exactly. Empty input
produces empty output. Multiline comments keep a single style around their full
original text. The source, spans and AST are never modified.

Spans must be an ordered, complete partition of the matching source, without
zero-length/EOF entries. The renderer validates this before writing anything;
invalid spans throw `std::invalid_argument`. Stream failures throw
`std::runtime_error` (or the stream's own exception if enabled). The CLI also
checks its final flush, so a buffered write failure produces exit code 1.

The rendering core uses standard C++11 streams. File callers should open their
output stream with `std::ios::binary`. For stdout, the CLI has a small
Windows-only CRT adapter in `src/console_output.cpp`: `_setmode` disables newline
translation before any ANSI output. Without binary mode, Windows text output
expands LF into CRLF, including LF already preceded by an original CR. See the
[Microsoft CRT documentation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setmode?view=msvc-170).
Other platforms need no adapter operation. Core rendering and tests do not
depend on Windows headers or console APIs.

Raw invalid bytes, including source-authored terminal escape sequences, are
also preserved. Such sequences may affect terminal display; this is a source
preview, with a byte-preservation contract rather than control-character
escaping. Use the escaped `--tokens`/`--classify` tables to inspect their spelling.
Removing SGR sequences reconstructs ordinary source fixtures exactly, but cannot
distinguish generated sequences from identical sequences present in the input.

## Errors and extension points

`classify_source()` supplies a recovered AST for syntax errors, or complete
lexical categories when lexical errors prevent parsing. Either result still
renders the entire file. Diagnostics stay unstyled on stderr; valid analysis
returns 0, analysis/file/output failures return 1 and conflicting mode options
return 2. `--show-whitespace` is only for the inspection tables; ANSI output
always includes whitespace.

Change `style_for()` in `src/ansi_renderer.cpp` to adjust the palette. Change
the classifier when a source role needs different AST handling. The planned
HTML renderer will reuse the same classification with its own escaping/styles.
It will not need terminal or parser changes.

`tests/ansi_tests.cpp` checks the palette, context-sensitive names, resets,
formatting, error precedence, fallback/recovery, raw bytes, invalid spans and
stream failures. `tests/check_ansi_cli.cmake` checks real output/status streams,
option conflicts, empty input and binary mixed-newline capture in a spaced path.
