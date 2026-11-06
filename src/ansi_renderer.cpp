#include "bminor/ansi_renderer.hpp"
#include "bminor/render_support.hpp"

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

} // namespace

void render_ansi(std::ostream& out, const SourceFile& source,
                 const std::vector<ClassifiedSpan>& spans) {
    validate_render_spans(source, spans);
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
