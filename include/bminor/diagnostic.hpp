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
    StringTooLong,
    UnexpectedToken,
    // Kept as a stable code from the earlier partial parser. The selected
    // grammar is now implemented, so normal parsing no longer emits PAR002.
    UnsupportedSyntax,
    ParserNestingLimit
};

struct Diagnostic {
    DiagnosticCode code;
    SourceLocation location;
    // Number of source bytes covered by the error. A bad byte or dangling
    // backslash has length 1; a missing delimiter covers the unfinished token.
    // Parser diagnostics at EOF have length 0 because no source byte follows it.
    std::size_t length;
    std::string message;
};

const char* diagnostic_code_name(DiagnosticCode code);
void write_diagnostic(std::ostream& out, const Diagnostic& diagnostic);

} // namespace bminor

#endif
