#include "bminor/diagnostic.hpp"

#include <ostream>

namespace bminor {

const char* diagnostic_code_name(DiagnosticCode code) {
    switch (code) {
    case DiagnosticCode::UnknownByte:              return "LEX001";
    case DiagnosticCode::UnterminatedString:       return "LEX002";
    case DiagnosticCode::UnterminatedCharacter:    return "LEX003";
    case DiagnosticCode::UnterminatedBlockComment: return "LEX004";
    case DiagnosticCode::InvalidCharacterLength:   return "LEX005";
    case DiagnosticCode::InvalidLiteralByte:       return "LEX006";
    case DiagnosticCode::IncompleteEscape:         return "LEX007";
    case DiagnosticCode::IdentifierTooLong:        return "LEX008";
    case DiagnosticCode::StringTooLong:            return "LEX009";
    case DiagnosticCode::UnexpectedToken:          return "PAR001";
    case DiagnosticCode::UnsupportedSyntax:        return "PAR002";
    case DiagnosticCode::ParserNestingLimit:       return "PAR003";
    }
    return "UNKNOWN";
}

void write_diagnostic(std::ostream& out, const Diagnostic& diagnostic) {
    // Rendering stays outside the lexer, which can also be used without a CLI.
    // Byte values in messages are escaped by the scanner before reaching here.
    out << diagnostic.location.filename << ':' << diagnostic.location.line
        << ':' << diagnostic.location.column << ": error ["
        << diagnostic_code_name(diagnostic.code) << "]: "
        << diagnostic.message << '\n';
}

} // namespace bminor
