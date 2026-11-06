#include "bminor/ansi_renderer.hpp"
#include "bminor/lexer.hpp"

#include <exception>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

std::string render(const bminor::SourceFile& source,
                   const std::vector<bminor::ClassifiedSpan>& spans) {
    std::ostringstream out;
    bminor::render_ansi(out, source, spans);
    return out.str();
}

std::string render(const std::string& text) {
    const bminor::SourceFile source{"ansi.bminor", text};
    return render(source, bminor::classify_source(source).spans);
}

std::string strip_sgr(const std::string& text) {
    // Fixtures using this helper have no source-authored SGR sequences. The
    // separate raw-control test compares bytes directly to avoid that ambiguity.
    std::string plain;
    for (std::size_t i = 0; i < text.size();) {
        if (text.compare(i, 2, "\x1b[") == 0) {
            std::size_t end = i + 2;
            while (end < text.size() && ((text[end] >= '0' && text[end] <= '9')
                                         || text[end] == ';')) { ++end; }
            require(end < text.size() && text[end] == 'm', "Malformed generated SGR sequence.");
            i = end + 1;
        } else { plain += text[i++]; }
    }
    return plain;
}

std::size_t occurrences(const std::string& text, const std::string& needle) {
    std::size_t count = 0, offset = 0;
    while ((offset = text.find(needle, offset)) != std::string::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

void lexical_palette() {
    const std::pair<const char*, const char*> cases[] = {
        {"return", "1;34"}, {"function", "1;34"}, {"integer", "36"},
        {"array", "36"}, {"void", "36"}, {"42", "38;5;208"},
        {"true", "38;5;208"}, {"false", "38;5;208"},
        {"\"a\\n\"", "32"}, {"'x'", "32"}, {"!=", "37"}, {";", "37"},
        {"// comment", "3;90"}, {"/* comment */", "3;90"}, {"@", "4;31"}
    };
    for (const auto& item : cases) {
        const bminor::SourceFile source{"ansi.bminor", item.first};
        bminor::Lexer lexer(source);
        const auto lexed = lexer.tokenize();
        const std::string expected = std::string("\x1b[") + item.second + "m" + item.first + "\x1b[0m";
        require(render(source, bminor::classify_tokens(lexed.tokens)) == expected,
                std::string("Wrong palette or missing reset for ") + item.first);
    }
}

void ast_names() {
    const std::string source = "work: function integer (); main: function integer () = { "
        "copy: integer = work; (work)(); { work: integer = 1; print work; } return 0; }";
    const std::string output = render(source);
    require(occurrences(output, "\x1b[33mwork\x1b[0m") == 2,
            "Only the function declaration and grouped call should be yellow.");
    require(occurrences(output, " work\x1b[37m") == 3,
            "Value, local declaration and local reference must use default color.");
    require(strip_sgr(output) == source, "AST-based rendering changed source text.");
}

void adjacent_styles_reset() {
    const bminor::SourceFile source{"ansi.bminor", "if x=3;"};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    require(render(source, bminor::classify_tokens(lexed.tokens)) ==
            "\x1b[1;34mif\x1b[0m x\x1b[37m=\x1b[0m\x1b[38;5;208m3\x1b[0m\x1b[37m;\x1b[0m",
            "Colors leaked across unstyled names, spaces or adjacent punctuation.");
}

void empty_and_whitespace() {
    require(render("").empty(), "Empty source gained a heading, reset or newline.");
    const std::string spaces = " \t\r\n\t\n\r  ";
    require(render(spaces) == spaces, "Whitespace-only source gained styles or changed bytes.");
}

void original_formatting() {
    const std::string cases[] = {
        "main: function integer () = {\n\tprint \"a\\n\";  \n\treturn 0;\n}\n",
        "/* first\r\n\tsecond */\r\nvalue:\tinteger = 1;  \r\n",
        "/* first\nsecond */\rvalue: integer = 1;",
        "value: integer = 1;"
    };
    for (const auto& source : cases) {
        const auto output = render(source);
        require(strip_sgr(output) == source, "Newlines, tabs, escapes or final newline changed.");
        require(occurrences(output, "\x1b[0m") > 0, "Fixture was not actually colored.");
    }
}

void error_precedence() {
    const bminor::SourceFile source{"ansi.bminor", "integer \t@"};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    auto spans = bminor::classify_tokens(lexed.tokens);
    spans[0].has_error = true;
    spans[1].has_error = true; // Error flags must never decorate whitespace.
    spans[2].has_error = false; // Invalid role alone still requires error styling.
    require(render(source, spans) == "\x1b[4;31minteger\x1b[0m \t\x1b[4;31m@\x1b[0m",
            "Error priority, invalid-role fallback or whitespace exception is wrong.");
}

void recovered_syntax() {
    const bminor::SourceFile source{"ansi.bminor",
        "broken integer; work: function integer (); "
        "main: function integer () = { work(); return 0; } after: integer = 42;"};
    const auto classified = bminor::classify_source(source);
    require(classified.diagnostics.size() == 1, "Recovery fixture should have one syntax error.");
    const auto output = render(source, classified.spans);
    require(output.find("\x1b[4;31mbroken\x1b[0m") != std::string::npos,
            "Discarded syntax was not underlined red.");
    require(occurrences(output, "\x1b[33mwork\x1b[0m") == 2
                && output.find("\x1b[1;34mreturn\x1b[0m") != std::string::npos,
            "Recovered declarations, calls or later statements lost normal styles.");
    require(strip_sgr(output) == source.text, "Recovery omitted or duplicated source.");
}

void lexical_fallback() {
    const bminor::SourceFile source{"ansi.bminor", "work: function integer () = { return @; }"};
    const auto classified = bminor::classify_source(source);
    require(classified.diagnostics.size() == 1, "Expected a single lexical error.");
    const auto output = render(source, classified.spans);
    require(output.find("\x1b[4;31m@\x1b[0m") != std::string::npos
                && output.find("\x1b[33mwork") == std::string::npos,
            "Lexical fallback lost the error style or invented an AST role.");
    require(strip_sgr(output) == source.text, "Lexical fallback lost original source.");
}

void raw_control_bytes() {
    std::string bytes;
    for (unsigned int value = 0; value < 256; ++value) { bytes += static_cast<char>(value); }
    require(strip_sgr(render(bytes)) == bytes, "Arbitrary bytes were escaped, truncated or replaced.");

    // A genuine source-authored escape sequence is copied literally too. A
    // terminal may interpret it; the renderer promises byte preservation only.
    const bminor::SourceFile source{"ansi.bminor", std::string("\0\x1b[31m\xff", 7)};
    const std::vector<bminor::ClassifiedSpan> spans{
        {{source.path, 1, 1, 0}, source.text.size(), bminor::SyntaxRole::Invalid, true}
    };
    require(render(source, spans) == std::string("\x1b[4;31m") + source.text + "\x1b[0m",
            "Source-authored control bytes must remain verbatim inside the generated style.");
}

void reject_invalid_spans_before_output() {
    const bminor::SourceFile source{"ansi.bminor", "value: integer = 1;"};
    const auto valid = bminor::classify_source(source).spans;
    std::vector<std::vector<bminor::ClassifiedSpan>> invalid;
    auto changed = valid; changed[0].location.offset = 1; invalid.push_back(changed);
    changed = valid; changed[1].location.offset = 0; invalid.push_back(changed);
    changed = valid; changed[0].location.filename = "other.bminor"; invalid.push_back(changed);
    changed = valid; changed[0].length = 0; invalid.push_back(changed);
    changed = valid; changed.back().length = std::numeric_limits<std::size_t>::max(); invalid.push_back(changed);
    changed = valid; changed.pop_back(); invalid.push_back(changed);
    invalid.push_back({});
    for (const auto& spans : invalid) {
        std::ostringstream out;
        out << "existing output";
        bool rejected = false;
        try { bminor::render_ansi(out, source, spans); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected && out.str() == "existing output",
                "Invalid spans must be rejected before writing any bytes.");
    }
}

void output_failures() {
    const bminor::SourceFile source{"ansi.bminor", "value: integer = 1;"};
    const auto spans = bminor::classify_source(source).spans;
    std::ostringstream out;
    out.setstate(std::ios::badbit);
    bool rejected = false;
    try { bminor::render_ansi(out, source, spans); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "A failed stream must not be reported as successful output.");
    require(render(source, spans) == render(source, spans), "Rendering retained mutable style state.");
}

} // namespace

int main() {
    struct TestCase { const char* name; void (*run)(); };
    const TestCase cases[] = {
        {"lexical palette", lexical_palette},
        {"AST-derived function and variable colors", ast_names},
        {"adjacent styles reset", adjacent_styles_reset},
        {"empty and whitespace-only output", empty_and_whitespace},
        {"original source formatting", original_formatting},
        {"error precedence", error_precedence},
        {"recovered syntax", recovered_syntax},
        {"lexical fallback", lexical_fallback},
        {"raw control bytes", raw_control_bytes},
        {"invalid span contract", reject_invalid_spans_before_output},
        {"output failures and repeatability", output_failures}
    };
    for (const auto& test : cases) {
        try { test.run(); std::cout << "PASS: " << test.name << '\n'; }
        catch (const std::exception& error) {
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
