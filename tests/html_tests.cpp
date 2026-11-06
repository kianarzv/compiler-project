#include "bminor/html_renderer.hpp"
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
    bminor::render_html(out, source, spans);
    return out.str();
}

std::string render(const std::string& text) {
    const bminor::SourceFile source{"html.bminor", text};
    return render(source, bminor::classify_source(source).spans);
}

std::string source_markup(const std::string& document) {
    const std::string opening = "<code id=\"source\">";
    const auto start = document.find(opening);
    require(start != std::string::npos, "Missing source code container.");
    const auto end = document.find("</code>", start);
    require(end != std::string::npos, "Unclosed code container.");
    return document.substr(start + opening.size(), end - start - opening.size());
}

std::string decoded_source(const std::string& document) {
    const auto markup = source_markup(document);
    const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&#39;", "'"}, {"&#13;", "\r"}
    };
    std::string text;
    for (std::size_t i = 0; i < markup.size();) {
        if (markup[i] == '<') {
            require(markup.compare(i, 13, "<span class=\"") == 0
                        || markup.compare(i, 7, "</span>") == 0,
                    "Source text injected unexpected markup.");
            const auto end = markup.find('>', i);
            require(end != std::string::npos, "Incomplete generated span.");
            i = end + 1;
        } else if (markup[i] == '&') {
            bool found = false;
            for (const auto& entity : entities) {
                const std::string spelling = entity.first;
                if (markup.compare(i, spelling.size(), spelling) == 0) {
                    text += entity.second;
                    i += spelling.size();
                    found = true;
                    break;
                }
            }
            require(found, "Unexpected or unescaped source entity.");
        } else { text += markup[i++]; }
    }
    return text;
}

std::size_t occurrences(const std::string& text, const std::string& needle) {
    std::size_t offset = 0, count = 0;
    while ((offset = text.find(needle, offset)) != std::string::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

void standalone_document() {
    const auto document = render("value: integer = 42;");
    require(document.find("<!DOCTYPE html>\n<html lang=\"en\">") == 0
                && document.find("<meta charset=\"utf-8\">") != std::string::npos
                && document.find("</html>\n") == document.size() - 8,
            "Output is not a complete UTF-8 HTML document.");
    require(occurrences(document, "<style>") == 1 && occurrences(document, "<pre ") == 1
                && document.find("<script") == std::string::npos
                && document.find("<link") == std::string::npos
                && document.find(" src=") == std::string::npos,
            "Document must have embedded CSS and no scripts or external assets.");
    for (const std::string rule : {"#60a5fa", "#5eead4", "#facc15", "#fb923c", "#86efac",
                                   "#cbd5e1", "font-style: italic", "underline wavy",
                                   "white-space: pre", "tab-size: 4", "overflow: auto"}) {
        require(document.find(rule) != std::string::npos, "Missing palette/formatting rule: " + rule);
    }
}

void source_and_metadata_escaping() {
    const bminor::SourceFile source{"a<&\"'>.bminor",
        "payload: string = \"</code></pre><script>alert('x')</script>&amp;&lt;\";\n"
        "// </style><img src=x onerror=alert(1)> & \"quoted\"\n"};
    const auto document = render(source, bminor::classify_source(source).spans);
    require(decoded_source(document) == source.text, "HTML escaping changed literal source spelling.");
    require(document.find("<script>") == std::string::npos && document.find("<img ") == std::string::npos
                && occurrences(document, "</code>") == 1 && occurrences(document, "</style>") == 1,
            "Source-authored tags escaped their code container.");
    require(occurrences(document, "a&lt;&amp;&quot;&#39;&gt;.bminor") == 2,
            "Filename must be escaped in both title and heading.");
    require(document.find("&amp;amp;&amp;lt;") != std::string::npos,
            "Source-authored entities must be escaped exactly once before browser decoding.");
}

void ast_identifier_roles() {
    const std::string source = "work: function integer (); main: function integer () = { "
        "copy: integer = work; (work)(); { work: integer = 1; print work; } return 0; }";
    const auto document = render(source);
    const auto markup = source_markup(document);
    require(occurrences(markup, "class=\"fn-decl\">work</span>") == 1
                && occurrences(markup, "class=\"fn-call\">work</span>") == 1
                && occurrences(markup, "class=\"var-ref\">work</span>") == 2
                && occurrences(markup, "class=\"var-decl\">work</span>") == 1,
            "Repeated names lost their AST-derived occurrence roles.");
    require(decoded_source(document) == source, "Name highlighting changed source text.");
}

void lexical_categories() {
    const bminor::SourceFile source{"html.bminor",
        "name if integer 42 true \"text\" 'x' + ; // comment\n"};
    bminor::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    const auto document = render(source, bminor::classify_tokens(tokens.tokens));
    const auto markup = source_markup(document);
    for (const std::string category : {"identifier", "kw", "type", "lit-int", "lit-bool",
                                      "lit-string", "lit-char", "op", "punct", "comment"}) {
        require(markup.find("class=\"" + category + "\"") != std::string::npos,
                "Missing lexical CSS class: " + category);
    }
    require(decoded_source(document) == source.text, "Lexical rendering lost trivia.");
}

void formatting_and_empty_input() {
    const std::string cases[] = {"", "\n", "\r\n\t  \r\n", " \t\r  ",
        "\n/* first\r\n\tsecond */\rvalue:\tinteger = 1;  \n", "value: integer = 1;"};
    for (const auto& text : cases) {
        const auto document = render(text);
        require(decoded_source(document) == text, "Leading/trailing source whitespace changed.");
        require(source_markup(document).find('\r') == std::string::npos,
                "Raw CR would be normalized by the HTML parser; use a character reference.");
        require(document.find("<pre aria-label=\"Highlighted B-Minor source\"><code") != std::string::npos,
                "Template newline before code would interfere with initial source newlines.");
    }
    require(source_markup(render("")).empty(), "Empty source gained template whitespace.");
}

void error_classes() {
    const bminor::SourceFile source{"html.bminor", "integer \t@"};
    bminor::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    auto spans = bminor::classify_tokens(tokens.tokens);
    spans[0].has_error = true;
    spans[1].has_error = true;
    spans[2].has_error = false;
    require(source_markup(render(source, spans)) ==
                "<span class=\"type error\">integer</span> \t<span class=\"invalid error\">@</span>",
            "Error class must preserve base role, override its color and leave whitespace alone.");
}

void recovered_syntax() {
    const bminor::SourceFile source{"html.bminor", "broken integer; "
        "work: function integer (); main: function integer () = { work(); return 0; }"};
    const auto classified = bminor::classify_source(source);
    require(classified.diagnostics.size() == 1, "Expected one syntax error.");
    const auto document = render(source, classified.spans);
    require(source_markup(document).find("class=\"identifier error\">broken</span>") != std::string::npos
                && source_markup(document).find("class=\"fn-call\">work</span>") != std::string::npos,
            "Recovered output lost error decoration or later AST-derived names.");
    require(decoded_source(document) == source.text, "Recovery dropped invalid or later source.");
}

void lexical_fallback() {
    const bminor::SourceFile source{"html.bminor", "work: function integer () = { return @; }"};
    const auto classified = bminor::classify_source(source);
    require(classified.diagnostics.size() == 1, "Expected a lexical error.");
    const auto document = render(source, classified.spans);
    const auto markup = source_markup(document);
    require(markup.find("class=\"invalid error\">@</span>") != std::string::npos
                && markup.find("class=\"identifier\">work</span>") != std::string::npos
                && markup.find("class=\"fn-decl\"") == std::string::npos,
            "Lexical fallback invented an AST role or lost invalid styling.");
    require(decoded_source(document) == source.text, "Lexical fallback omitted source text.");
}

void utf8_and_invalid_bytes() {
    const bminor::SourceFile source{u8"\u0633\u0644\u0627\u0645.bminor",
        u8"// \u0633\u0644\u0627\u0645 \U0001F600\nvalue: integer = 1;"};
    const auto document = render(source, bminor::classify_source(source).spans);
    require(decoded_source(document) == source.text && occurrences(document, source.path) == 2,
            "Valid UTF-8 in comments or metadata was corrupted.");

    const bminor::SourceFile bad{"html.bminor",
        std::string("\0\x1b\x7f\xff\xc0\x80\xed\xa0\x80\xf4\x90\x80\x80\xe2", 14)};
    const std::vector<bminor::ClassifiedSpan> spans{
        {{bad.path, 1, 1, 0}, bad.text.size(), bminor::SyntaxRole::Invalid, true}
    };
    require(decoded_source(render(bad, spans)) ==
                "\\x00\\x1B\\x7F\\xFF\\xC0\\x80\\xED\\xA0\\x80\\xF4\\x90\\x80\\x80\\xE2",
            "Control, overlong, surrogate, out-of-range or truncated UTF-8 bytes were not made visible.");
}

void invalid_span_contract() {
    const bminor::SourceFile source{"html.bminor", "value: integer = 1;"};
    const auto valid = bminor::classify_source(source).spans;
    std::vector<std::vector<bminor::ClassifiedSpan>> invalid;
    auto changed = valid; changed[0].location.offset = 1; invalid.push_back(changed);
    changed = valid; changed[1].location.offset = 0; invalid.push_back(changed);
    changed = valid; changed.back().location.filename = "other.bminor"; invalid.push_back(changed);
    changed = valid; changed[0].length = 0; invalid.push_back(changed);
    changed = valid; changed.back().length = std::numeric_limits<std::size_t>::max(); invalid.push_back(changed);
    changed = valid; changed.pop_back(); invalid.push_back(changed);
    invalid.push_back({});
    for (const auto& spans : invalid) {
        std::ostringstream out;
        out << "existing output";
        bool rejected = false;
        try { bminor::render_html(out, source, spans); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected && out.str() == "existing output", "Invalid spans emitted a partial document.");
    }
}

void output_failures_and_repeatability() {
    const bminor::SourceFile source{"html.bminor", "value: integer = 1;"};
    const auto spans = bminor::classify_source(source).spans;
    std::ostringstream out;
    out.setstate(std::ios::badbit);
    bool rejected = false;
    try { bminor::render_html(out, source, spans); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "A failed stream was reported as successful output.");
    require(render(source, spans) == render(source, spans), "HTML rendering retained mutable state.");
}

} // namespace

int main() {
    struct TestCase { const char* name; void (*run)(); };
    const TestCase cases[] = {
        {"standalone document and palette", standalone_document},
        {"source and metadata escaping", source_and_metadata_escaping},
        {"AST-derived identifier roles", ast_identifier_roles},
        {"lexical CSS categories", lexical_categories},
        {"formatting and empty input", formatting_and_empty_input},
        {"error classes and precedence", error_classes},
        {"recovered syntax", recovered_syntax},
        {"lexical fallback", lexical_fallback},
        {"UTF-8 and invalid bytes", utf8_and_invalid_bytes},
        {"invalid span contract", invalid_span_contract},
        {"output failures and repeatability", output_failures_and_repeatability}
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
