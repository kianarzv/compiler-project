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

Next review gate: user approval before step 2.
