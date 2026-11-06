# Development progress

## Step 1: runnable project foundation

Implemented:

- A CMake build with compiler warnings and a C++11 baseline.
- A CLI with help, source display and explicit exit codes.
- Source-file loading that retains the original bytes.
- Line counting for LF, CRLF and CR line endings.
- A small sample and a PowerShell build-and-run script.
- A roadmap for the remaining phase-one work.

Validation passed:

- Configured and compiled with MinGW GCC 6.3.0, CMake 3.29.2 and Ninja;
  compiler warnings were enabled and no warnings were reported.
- Ran `run.ps1` and displayed the sample: 77 bytes and 4 lines.
- Checked eight input cases, covering empty files, paths with spaces, LF,
  CRLF, CR, trailing newlines and a final line without a newline.
- Checked help, missing/extra arguments, unknown options and a missing file;
  all returned the documented exit codes.

The source loader preserves file bytes. Terminal display uses the platform's
standard output newline handling; later file renderers must write explicitly
in binary mode to preserve original line endings.

Step 2 was approved by the user after reviewing this foundation.

## Step 2: token model and basic lexer

Implemented:

- More detailed English comments in the source loader, CLI and new lexer code.
- Token kinds, exact lexemes, source filenames, line/column and byte offsets.
- ASCII identifiers, the base dialect's reserved words and boolean literals.
- Longest-word matching and keyword classification after the full word.
- Retained whitespace, consistent CR/LF/CRLF handling and a final EOF token.
- A one-byte fallback for currently unsupported input, ensuring forward progress.
- `--tokens` and optional `--show-whitespace`, also available in `run.ps1`.
- A lexical sample, formal rules and a code-reading guide.
- CTest integration and seven lexer regression groups.

The user approved the base textbook dialect because the course-specific
definition was unavailable. Later extensions must be documented before use.

Validation passed:

- Built with MinGW GCC 6.3.0 and the configured warning flags, with no warnings.
- CTest passed all seven lexer regression groups: keyword boundaries, all
  reserved words, identifier rules, source locations, complete byte coverage,
  unsupported input, and empty/repeated scans.
- Exercised both token display modes and the original source display.
- Verified escaped control/non-ASCII bytes, EOF coordinates and CRLF handling.
- Checked missing/extra arguments, unsupported options and a missing file.
- Checked paths with spaces, a filename starting with a dash through `--`, and
  PowerShell forwarding of the new switches.

This step intentionally leaves numbers, strings, characters, operators,
punctuation and comments for step 3. Their bytes appear as `INVALID` for now;
diagnostics and lexical error exit codes are scheduled for step 4.

Step 3 was approved by the user after reviewing these tokens.

## Step 3: literals, operators, delimiters and comments

Implemented:

- Decimal integer tokens, with separate sign operators and unchanged digits.
- Strings and one-unit character literals, including escaped quotes/backslashes.
- All base operators and delimiters, with longest matching for two-byte symbols.
- Line/block comments retained as tokens, with comment openers checked before `/`.
- Boundaries for malformed quoted literals and unfinished block comments, which
  produce `INVALID` tokens and retain their source bytes.
- A complete-program sample in `examples/lexer_full.bminor`.
- Updated English comments, formal rules and the code-reading guide.
- Nine new regression groups, bringing the lexer suite to 16 groups.

Validation passed:

- Built with MinGW GCC 6.3.0 and warning flags enabled; no warnings were emitted.
- All 16 lexer regression groups passed through CTest, including escaped quotes,
  sign boundaries, longest-match operators, comment boundaries and malformed input.
- Verified complete byte coverage and source locations for 32 deterministic
  inputs, each containing all 256 byte values in a different order.
- Tokenized all three maintained examples without any `INVALID` tokens. The new
  complete-program sample has 118 visible tokens, including its comments.
- Reconstructed each example exactly from the CLI's escaped lexemes, including
  a CRLF copy in a path with spaces; hiding whitespace changed only display rows.
- Confirmed malformed input continues to EOF and code after an unfinished string
  is still scanned. Detailed error messages remain scheduled for step 4.
- Checked original source display, CLI usage/file errors and the PowerShell
  build-and-run command with the full CRLF sample.

The scanner now recognizes the categories needed by the base dialect. Detailed
error messages, length checks and lexical error exit codes remain for step 4.
There is no parser or B-Minor program execution yet.

Next review gate: user approval before step 4.
