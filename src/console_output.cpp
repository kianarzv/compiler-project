#include "bminor/console_output.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <stdexcept>
#endif

namespace bminor {

void prepare_binary_stdout() {
#ifdef _WIN32
    // Text mode expands every LF to CRLF, including an LF already preceded
    // by an original CR. Binary mode prevents that change when stdout is
    // redirected too. Only ANSI mode uses this adapter, before any stdout I/O.
    // The CLI never reopens stdout, whose CRT descriptor is 1. Using that
    // descriptor also works with older MinGW headers that hide _fileno in
    // strict C++11 mode; no compiler-extension mode is needed for this file.
    const int stdout_descriptor = 1;
    if (_setmode(stdout_descriptor, _O_BINARY) == -1) {
        throw std::runtime_error("Could not configure binary stdout for ANSI output.");
    }
#endif
}

} // namespace bminor
