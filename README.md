# B-Minor Front End

A C++ implementation developed in small, runnable steps for phase one of the
compiler design project. The target language is B-Minor.

## Current state: step 2

The program can display a source file or tokenize identifiers, base B-Minor
keywords, boolean literals and whitespace. Tokens include their original lexeme,
filename, line, column and byte offset. Source display from step 1 still works.

This is a partial lexer: numbers, strings, characters, punctuation, operators and
comments are scheduled for step 3 and currently produce `INVALID` rows. For this
step, use `examples/tokens_basic.bminor` to see the supported categories. This
file is a lexical sample, not a complete B-Minor program.

## Build and run on Windows

Requirements: a C++11 compiler, CMake 3.15 or newer, and Ninja on `PATH`.
The initial build was checked with MinGW GCC 6.3.0 and CMake 3.29.2.

From PowerShell in the project directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1
```

To load your own file:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source .\examples\hello.bminor
```

The script builds the executable before running it. Paths passed to the script
are relative to the project directory, or can be absolute.

To inspect the step-2 tokens:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Source examples/tokens_basic.bminor -Tokens
```

Add `-ShowWhitespace` to include escaped spaces and line endings in the table.
For example, the first line of the sample produces these non-whitespace tokens:

```text
examples/tokens_basic.bminor:1:1    KW_IF          "if"
examples/tokens_basic.bminor:1:4    IDENTIFIER     "ifx"
examples/tokens_basic.bminor:1:8    KW_RETURN      "return"
examples/tokens_basic.bminor:1:15   IDENTIFIER     "returnValue"
```

Columns in the actual output are separated by tabs. Lines and columns start at
1, and the table always ends with EOF. Tabs count as one source column.

To build manually:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build
.\build\bminor.exe .\examples\hello.bminor
.\build\bminor.exe --help
.\build\bminor.exe --tokens examples/tokens_basic.bminor
.\build\bminor.exe --tokens --show-whitespace examples/tokens_basic.bminor
ctest --test-dir build --output-on-failure
```

Expected output for the sample file:

```text
Loaded: examples/hello.bminor
Bytes: 77
Lines: 4

main: function integer () = {
    print "Hello, B-Minor!\n";
    return 0;
}
```

Exit codes: `0` for success or help, `1` for file errors, and `2` for incorrect
command-line usage. Byte counts depend on the file's line endings.
In this intermediate build, `INVALID` token rows do not change the exit code;
full lexical diagnostics are scheduled for step 4.

## Code map

- `src/main.cpp`: command-line handling and output.
- `include/bminor/source_file.hpp`: source-file data and its public interface.
- `src/source_file.cpp`: file reading and line counting, including LF and CRLF.
- `include/bminor/source_location.hpp`: filename and cursor coordinates.
- `include/bminor/token.hpp` and `src/token.cpp`: token kinds, data and labels.
- `include/bminor/lexer.hpp` and `src/lexer.cpp`: scanning and word classification.
- `tests/lexer_tests.cpp`: keyword boundaries, positions, source coverage and EOF.
- `examples/`: small inputs to run after each step.
- `docs/roadmap.md`: the staged plan and observable results.
- `docs/progress.md`: completed work and validation for each step.
- `docs/lexical-rules.md`: selected dialect, formal rules and current limits.
- `docs/code-walkthrough.md`: execution flow and where to make future changes.

Source text is retained without newline conversion, since later output must
preserve its formatting. Generated files stay in `build/` or `output/`.
Reference documents are kept locally and are excluded from Git.
