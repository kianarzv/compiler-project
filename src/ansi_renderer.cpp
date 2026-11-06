#include "bminor/ansi_renderer.hpp"

#include <ostream>
#include <stdexcept>

namespace bminor {
namespace {

const char* const reset_style = "\x1b[0m";

const char* style_for(const ClassifiedSpan& span) {
    // Whitespace stays untouched even if a caller supplies an error flag.
    // For visible tokens, error decoration takes priority over the base role.
    if (span.role == SyntaxRole::Whitespace) { return ""; }
    if (span.has_error || span.role == SyntaxRole::Invalid) { return "\x1b[4;31m"; }

    switch (span.role) {
    case SyntaxRole::Keyword: return "\x1b[1;34m";
    case SyntaxRole::TypeName: return "\x1b[36m";
    case SyntaxRole::FunctionDeclaration:
    case SyntaxRole::FunctionCall: return "\x1b[33m";
    case SyntaxRole::IntegerLiteral:
    case SyntaxRole::BooleanLiteral: return "\x1b[38;5;208m";
    case SyntaxRole::StringLiteral:
    case SyntaxRole::CharacterLiteral: return "\x1b[32m";
    case SyntaxRole::Operator:
    case SyntaxRole::Delimiter: return "\x1b[37m";
    case SyntaxRole::Comment: return "\x1b[3;90m";
    default: return ""; // Ordinary identifiers use the terminal's default color.
    }
}

void validate_spans(const SourceFile& source, const std::vector<ClassifiedSpan>& spans) {
    std::size_t next_offset = 0;
    for (const auto& span : spans) {
        // Check with subtraction rather than offset + length, which could
        // overflow for a malformed span. Validate the whole input first so
        // mistakes cannot leave a partly styled document on the output stream.
        if (span.location.filename != source.path || span.location.offset != next_offset
            || span.length == 0 || span.length > source.text.size() - next_offset) {
            throw std::invalid_argument("ANSI spans must partition the matching source file.");
        }
        next_offset += span.length;
    }
    if (next_offset != source.text.size()) {
        throw std::invalid_argument("ANSI spans must cover the complete source file.");
    }
}

} // namespace

void render_ansi(std::ostream& out, const SourceFile& source,
                 const std::vector<ClassifiedSpan>& spans) {
    validate_spans(source, spans);
    for (const auto& span : spans) {
        const char* const style = style_for(span);
        if (*style != '\0') { out << style; }
        // Offsets refer to the original bytes, including tabs, CRLF and any
        // malformed token text. AST pretty-printing would lose that formatting.
        out.write(source.text.data() + span.location.offset,
                  static_cast<std::streamsize>(span.length));
        // End each styled span immediately. Following identifiers, whitespace
        // and the shell prompt must not inherit the previous token's color.
        if (*style != '\0') { out << reset_style; }
    }
    if (!out) { throw std::runtime_error("Could not write ANSI output."); }
}

} // namespace bminor
