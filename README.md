# B-Minor Front End

A C++ implementation developed in small, runnable steps for phase one of the
compiler design project. The target language is B-Minor.

## Current state: step 1

The program reads a source file, reports its byte and line counts, and displays
its contents. It reports file errors and invalid command-line arguments.
Lexing, parsing, and syntax highlighting are scheduled for later steps.

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

To build manually:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build
.\build\bminor.exe .\examples\hello.bminor
.\build\bminor.exe --help
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

## Code map

- `src/main.cpp`: command-line handling and output.
- `include/bminor/source_file.hpp`: source-file data and its public interface.
- `src/source_file.cpp`: file reading and line counting, including LF and CRLF.
- `examples/`: small inputs to run after each step.
- `docs/roadmap.md`: the staged plan and observable results.
- `docs/progress.md`: completed work and validation for each step.

Source text is retained without newline conversion, since later output must
preserve its formatting. Generated files stay in `build/` or `output/`.
Reference documents are kept locally and are excluded from Git.
