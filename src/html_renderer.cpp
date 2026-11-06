#include "bminor/html_renderer.hpp"
#include "bminor/render_support.hpp"

#include <ostream>
#include <stdexcept>

namespace bminor {
namespace {

const char* css_class(SyntaxRole role) {
    switch (role) {
    case SyntaxRole::VariableDeclaration: return "var-decl";
    case SyntaxRole::ParameterDeclaration: return "param";
    case SyntaxRole::VariableReference: return "var-ref";
    case SyntaxRole::FunctionDeclaration: return "fn-decl";
    case SyntaxRole::FunctionCall: return "fn-call";
    case SyntaxRole::Keyword: return "kw";
    case SyntaxRole::TypeName: return "type";
    case SyntaxRole::IntegerLiteral: return "lit-int";
    case SyntaxRole::BooleanLiteral: return "lit-bool";
    case SyntaxRole::StringLiteral: return "lit-string";
    case SyntaxRole::CharacterLiteral: return "lit-char";
    case SyntaxRole::Operator: return "op";
    case SyntaxRole::Delimiter: return "punct";
    case SyntaxRole::Comment: return "comment";
    case SyntaxRole::Invalid: return "invalid";
    default: return "identifier";
    }
}

// Return the length of one complete UTF-8 scalar, or zero for malformed input.
// end is the current span boundary: never read into a separately styled span.
std::size_t utf8_length(const std::string& text, std::size_t offset, std::size_t end) {
    const unsigned char first = static_cast<unsigned char>(text[offset]);
    std::size_t length = 0;
    unsigned int value = 0, minimum = 0;
    if (first >= 0xC2 && first <= 0xDF) { length = 2; value = first & 0x1F; minimum = 0x80; }
    else if (first >= 0xE0 && first <= 0xEF) { length = 3; value = first & 0x0F; minimum = 0x800; }
    else if (first >= 0xF0 && first <= 0xF4) { length = 4; value = first & 0x07; minimum = 0x10000; }
    else { return 0; }
    if (length > end - offset) { return 0; }
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char next = static_cast<unsigned char>(text[offset + i]);
        if ((next & 0xC0) != 0x80) { return 0; }
        value = (value << 6) | (next & 0x3F);
    }
    // Reject overlong encodings, surrogate code points and values above the
    // Unicode limit. A malformed sequence is shown one byte at a time instead
    // of allowing the browser to silently replace or reinterpret it.
    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) { return 0; }
    return length;
}

void write_html_text(std::ostream& out, const std::string& text,
                     std::size_t offset, std::size_t length) {
    const char hex[] = "0123456789ABCDEF";
    const std::size_t end = offset + length;
    while (offset < end) {
        const unsigned char byte = static_cast<unsigned char>(text[offset]);
        switch (byte) {
        case '&': out << "&amp;"; break;
        case '<': out << "&lt;"; break;
        case '>': out << "&gt;"; break;
        case '"': out << "&quot;"; break;
        case '\'': out << "&#39;"; break;
        case '\r': out << "&#13;"; break;
        default:
            if (byte >= 0x80) {
                const std::size_t count = utf8_length(text, offset, end);
                if (count != 0) {
                    out.write(text.data() + offset, static_cast<std::streamsize>(count));
                    offset += count;
                    continue;
                }
            }
            if ((byte < 0x20 && byte != '\n' && byte != '\t') || byte >= 0x7F) {
                out << "\\x" << hex[byte >> 4] << hex[byte & 0x0F];
            } else { out.put(static_cast<char>(byte)); }
        }
        ++offset;
    }
}

} // namespace

void render_html(std::ostream& out, const SourceFile& source,
                 const std::vector<ClassifiedSpan>& spans) {
    validate_render_spans(source, spans);
    out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
        << "<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        << "<title>";
    write_html_text(out, source.path, 0, source.path.size());
    // A raw string keeps the self-contained theme readable as normal CSS.
    // Source-derived text is written separately through write_html_text().
    out << R"HTML( - B-Minor</title>
<style>
:root { color-scheme: dark; }
* { box-sizing: border-box; }
body { margin: 0; background: #101318; color: #e5e7eb; font-family: system-ui, sans-serif; }
.page { max-width: 1200px; margin: 0 auto; padding: 32px 20px; }
header { margin-bottom: 24px; }
.eyebrow { color: #94a3b8; font-size: 12px; letter-spacing: .16em; }
h1 { margin: 8px 0; font-size: 22px; font-weight: 600; overflow-wrap: anywhere; }
pre {
    margin: 0;
    padding: 24px;
    overflow: auto;
    border: 1px solid #303844;
    border-radius: 10px;
    background: #171c24;
    white-space: pre;
    tab-size: 4;
    direction: ltr;
    unicode-bidi: isolate;
}
code { font: 14px/1.65 Consolas, 'Liberation Mono', monospace; }
.kw { color: #60a5fa; font-weight: 700; }
.type { color: #5eead4; }
.fn-decl, .fn-call { color: #facc15; }
.lit-int, .lit-bool { color: #fb923c; }
.lit-string, .lit-char { color: #86efac; }
.op, .punct { color: #cbd5e1; }
.comment { color: #94a3b8; font-style: italic; }
.error { color: #f87171; text-decoration: underline wavy; text-underline-offset: 3px; }
</style>
</head>
<body>
<div class="page">
<header>
<p class="eyebrow">B-MINOR SOURCE</p>
<h1>)HTML";
    write_html_text(out, source.path, 0, source.path.size());
    out << "</h1>\n</header>\n<main><pre aria-label=\"Highlighted B-Minor source\"><code id=\"source\">";
    // No template whitespace belongs inside code. The code child also keeps
    // HTML's special initial-newline stripping for pre from eating a source LF.
    for (const auto& span : spans) {
        const bool whitespace = span.role == SyntaxRole::Whitespace;
        if (!whitespace) {
            out << "<span class=\"" << css_class(span.role);
            if (span.has_error || span.role == SyntaxRole::Invalid) { out << " error"; }
            out << "\">";
        }
        // Escape token text and whitespace through the same path, including
        // CR references. This preserves source formatting without making any
        // source-authored tag, entity or closing style/script into active HTML.
        write_html_text(out, source.text, span.location.offset, span.length);
        if (!whitespace) { out << "</span>"; }
    }
    out << "</code></pre></main>\n</div>\n</body>\n</html>\n";
    if (!out) { throw std::runtime_error("Could not write HTML output."); }
}

} // namespace bminor
