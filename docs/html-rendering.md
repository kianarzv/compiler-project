# Standalone HTML/CSS rendering

Step 11 adds an offline browser preview using the same AST-refined spans as
the ANSI renderer. The document contains its own CSS, one preformatted code
block and an escaped filename heading. It needs no JavaScript, server, fonts
or other downloaded assets.

## Build, export and open

From PowerShell in the project directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/html_preview.bminor -Html
```

Open `output/highlight.html` in a browser. The script creates the parent
directory and the native executable writes the file directly, avoiding
PowerShell text-pipeline encoding. The example includes function declarations/
calls, ordinary variables, literals, control flow and HTML-looking string text.
`<title>Source text</title>` remains visible code rather than becoming markup.

Choose a different destination with `-Output`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/classification.bminor -Html -Output "output/function roles.html"
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/syntax_errors.bminor -Html -Output output/errors.html
```

The error example saves a complete preview and reports five syntax diagnostics
on stderr with exit code 1. `examples/lexical_errors.bminor` similarly exports a
lexical fallback with six diagnostics. Missing tokens have no bytes to underline;
their messages still appear on stderr. Detailed diagnostic messages are not
inserted into the displayed source.

After building, equivalent native commands are:

```powershell
New-Item -ItemType Directory -Force output | Out-Null
.\build\bminor.exe --html --output output/highlight.html examples/html_preview.bminor
.\build\bminor.exe --html examples/html_preview.bminor
```

The native CLI requires an existing parent directory. `--html` without
`--output` emits only the document on stdout; with `--output`, stdout instead
reports `Saved HTML: <path>`. Output files are opened in binary mode and may
replace an earlier export. Before writing, the CLI refuses any existing
destination whose complete contents equal the loaded source, including ordinary
path aliases and hard links. This conservative check also rejects a separate
identical copy (or an existing empty file when the source is empty). It compares
contents rather than providing a filesystem identity or concurrent-write lock.

`--html` is mutually exclusive with source-analysis display modes. `--output`
requires HTML mode, accepts one destination and can appear before the source.
`--show-whitespace` remains exclusive to token/classification tables. Paths with
spaces should be quoted; use `--` before a source beginning with a dash, and
`./` before an output filename beginning with a dash.

## Classes and appearance

The palette follows the supplied PDF's suggested colors and the ANSI renderer.
Separate classes retain the more precise syntax roles even when colors match.

| Classes | Appearance |
| --- | --- |
| `kw` | Bold blue |
| `type` | Cyan |
| `fn-decl`, `fn-call` | Yellow |
| `lit-int`, `lit-bool` | Orange |
| `lit-string`, `lit-char` | Green |
| `op`, `punct` | Light gray |
| `comment` | Gray, italic |
| `identifier`, `var-decl`, `param`, `var-ref` | Default light text |
| Additional `error` class, including `invalid error` | Red with a wavy underline |

Whitespace has no span wrapper. Visible spans retain their base class when an
error class is added; the last CSS rule overrides their color. Function-call
classification comes from the AST, including grouped names such as `(work)()`.
A matching word inside a comment or string does not become a function name.
Name roles remain syntactic; this phase does not resolve symbols or check types.

## Text and formatting

`render_html(out, source, spans)` validates the complete source partition before
writing anything. Both renderers share `validate_render_spans()` in
`src/render_support.cpp`. The source, AST and classified spans stay unchanged.
The renderer reads each original source slice and escapes it as HTML text:

- `&`, `<`, `>`, quotes and apostrophes become character references. Existing
  source entities are escaped once, so literal `&amp;` still displays as `&amp;`.
- Spaces, tabs and LF remain literal. The code block uses `white-space: pre`,
  four-column tab stops and horizontal scrolling instead of wrapping long lines.
- CR is encoded as `&#13;`, preserving CR/CRLF in DOM text instead of losing it
  during the HTML parser's [newline normalization](https://html.spec.whatwg.org/multipage/parsing.html#preprocessing-the-input-stream).
- `<pre><code id="source">` has no template whitespace before source content.
  The intervening code element prevents the [special leading-LF removal for pre](https://html.spec.whatwg.org/multipage/grouping-content.html#the-pre-element).
- Closing tags follow the final source byte immediately. An empty source has an
  empty code element; no final source newline is added.

UTF-8 sequences wholly contained in a span are retained, including Unicode
comments and metadata. ASCII control bytes other than tab/LF/CR, DEL and invalid
UTF-8 bytes display as visible `\xHH` text. This avoids silent browser replacement
of malformed bytes and keeps the exported document valid UTF-8. UTF-8 sequences
split between spans are displayed bytewise; the base ASCII lexer can produce
such spans for unsupported non-ASCII identifiers. This display policy does not
extend the accepted B-Minor dialect or change diagnostics/source byte offsets.
HTML is a preview, not a lossless archive of arbitrary invalid byte streams;
use original files or ANSI's raw-byte stream for that purpose.

## Code path and checks

`print_html()` obtains the existing classification result, then calls
`render_html()`. Without an output path it configures binary stdout and checks
the final flush. For a file, it finishes rendering in memory before opening the
destination, checks source protection, writes binary bytes and checks close.
Source/read/render failures therefore occur before truncating a destination.
An actual disk write failure may leave a partial output file and returns 1.
Analysis errors still permit a complete recovered preview and return 1.
Invalid CLI combinations return 2; successful analysis/export returns 0.

To change colors or page spacing, edit the embedded stylesheet in
`src/html_renderer.cpp`. To change role-to-class mapping, edit `css_class()`.
Keep role inference in `src/syntax_classifier.cpp`, rather than duplicating
parser logic in either renderer. `write_html_text()` is the shared escaping path
for source slices, the heading and title; none of those strings are trusted as
markup. Renderer stream failures throw `std::runtime_error` (or the stream's
configured exception); invalid spans throw `std::invalid_argument` before output.

`tests/html_tests.cpp` contains 11 groups for complete documents, escaping,
AST context, lexical classes, formatting, error handling, UTF-8/invalid bytes,
invalid spans and stream failures. `tests/check_html_cli.cmake` covers native
file/stdout parity, repeated/error exports, diagnostics, source protection,
file failures and option validation. The step was also checked in headless
Chrome for DOM text, computed styles and absence of source-injected elements.
Browser tooling is used only for development; the compiler and CTest need no
browser, Python package or external C++ dependency.
