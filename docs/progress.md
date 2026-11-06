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

Next review gate: user approval before step 3.
