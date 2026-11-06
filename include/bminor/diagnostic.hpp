#ifndef BMINOR_DIAGNOSTIC_HPP
#define BMINOR_DIAGNOSTIC_HPP

#include "bminor/source_location.hpp"

#include <cstddef>
#include <iosfwd>
#include <string>

namespace bminor {

// Codes identify the cause without requiring callers to parse English messages.
// All diagnostics currently represent errors; later phases can add new codes.
enum class DiagnosticCode {
    UnknownByte,
    UnterminatedString,
    UnterminatedCharacter,
    UnterminatedBlockComment,
    InvalidCharacterLength,
    InvalidLiteralByte,
    IncompleteEscape,
    IdentifierTooLong,
    StringTooLong
};

struct Diagnostic {
    DiagnosticCode code;
    SourceLocation location;
    // Number of source bytes covered by the error. A bad byte or dangling
    // backslash has length 1; a missing delimiter covers the unfinished token.
    std::size_t length;
    std::string message;
};

const char* diagnostic_code_name(DiagnosticCode code);
void write_diagnostic(std::ostream& out, const Diagnostic& diagnostic);

} // namespace bminor

#endif
