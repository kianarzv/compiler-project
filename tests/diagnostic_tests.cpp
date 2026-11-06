#include "bminor/lexer.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using bminor::DiagnosticCode;
using bminor::LexResult;
using bminor::TokenKind;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LexResult scan(const std::string& text) {
    const bminor::SourceFile source{"errors.bminor", text};
    bminor::Lexer lexer(source);
    return lexer.tokenize();
}

void unknown_bytes() {
    std::string text = "@ if $ return & | ";
    text.push_back('\0');
    text.push_back(static_cast<char>(0xFF));
    const auto result = scan(text);
    const std::size_t offsets[] = {0, 5, 14, 16, 18, 19};
    require(result.has_errors() && result.diagnostics.size() == 6,
            "Expected one diagnostic for each unknown byte.");
    for (std::size_t i = 0; i < result.diagnostics.size(); ++i) {
        const auto& error = result.diagnostics[i];
        require(error.code == DiagnosticCode::UnknownByte && error.length == 1,
                "Unknown byte has the wrong code or length.");
        require(error.location.filename == "errors.bminor"
                    && error.location.offset == offsets[i]
                    && error.location.line == 1
                    && error.location.column == offsets[i] + 1,
                "Unknown byte was reported at the wrong location.");
    }
    require(result.diagnostics[2].message.find("&&") != std::string::npos,
            "Missing suggestion for a single ampersand.");
    require(result.diagnostics[4].message.find("0x00") != std::string::npos
                && result.diagnostics[5].message.find("0xFF") != std::string::npos,
            "Non-printable bytes are not represented safely.");
    require(result.tokens.back().kind == TokenKind::EndOfFile,
            "Scanning stopped before EOF after an unknown byte.");
}

void unterminated_forms() {
    const auto quoted = scan("\"open\r\nreturn 'x");
    require(quoted.diagnostics.size() == 2, "Expected both unfinished literals.");
    require(quoted.diagnostics[0].code == DiagnosticCode::UnterminatedString
                && quoted.diagnostics[0].location.offset == 0
                && quoted.diagnostics[0].length == 5,
            "Unfinished string must cover the opening quote through its text.");
    const auto& character = quoted.diagnostics[1];
    require(character.code == DiagnosticCode::UnterminatedCharacter
                && character.location.line == 2 && character.location.column == 8
                && character.location.offset == 14 && character.length == 2,
            "Wrong location or span for an unfinished character literal.");
    bool saw_return = false;
    for (const auto& token : quoted.tokens) {
        if (token.kind == TokenKind::KwReturn) {
            saw_return = token.location.line == 2 && token.location.column == 1;
        }
    }
    require(saw_return, "Code after the unfinished string was not scanned.");

    const std::string text = "if /* one\r\ntwo";
    const auto block = scan(text);
    require(block.diagnostics.size() == 1
                && block.diagnostics[0].code == DiagnosticCode::UnterminatedBlockComment
                && block.diagnostics[0].location.offset == 3
                && block.diagnostics[0].length == text.size() - 3,
            "Unfinished block comment must cover its opener through EOF.");
}

void incomplete_escape() {
    for (const std::string ending : {"", "\nif", "\r\nif"}) {
        const auto result = scan(std::string("\"abc\\") + ending);
        require(result.diagnostics.size() == 1, "Escape produced duplicate errors.");
        const auto& error = result.diagnostics[0];
        require(error.code == DiagnosticCode::IncompleteEscape
                    && error.location.offset == 4 && error.location.line == 1
                    && error.location.column == 5 && error.length == 1,
                "Incomplete escape must point to its backslash.");
        require(result.tokens.back().kind == TokenKind::EndOfFile,
                "Unfinished escape prevented completion of the scan.");
    }
    const auto character = scan(std::string("'\\"));
    require(character.diagnostics.size() == 1
                && character.diagnostics[0].code == DiagnosticCode::IncompleteEscape,
            "Unfinished character escape needs the same diagnostic.");
}

void character_width() {
    const auto result = scan(R"bminor('' 'ab' '\q')bminor");
    require(result.diagnostics.size() == 2, "Valid escape was counted as two characters.");
    require(result.diagnostics[0].code == DiagnosticCode::InvalidCharacterLength
                && result.diagnostics[0].length == 2
                && result.diagnostics[0].message.find("found 0") != std::string::npos,
            "Empty character literal needs a specific error.");
    require(result.diagnostics[1].code == DiagnosticCode::InvalidCharacterLength
                && result.diagnostics[1].location.offset == 3
                && result.diagnostics[1].length == 4
                && result.diagnostics[1].message.find("found 2") != std::string::npos,
            "Multi-character literal needs a specific error.");
}

void invalid_literal_bytes_and_priority() {
    std::string text = "\"a\tb\" '";
    text.push_back(static_cast<char>(0xFF));
    text += "'";
    const auto result = scan(text);
    require(result.diagnostics.size() == 2, "Expected one error per malformed literal.");
    const std::size_t offsets[] = {2, 7};
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& error = result.diagnostics[i];
        require(error.code == DiagnosticCode::InvalidLiteralByte
                    && error.location.offset == offsets[i] && error.length == 1,
                "Literal byte error must point to the first invalid byte.");
    }

    // Report the structural failure when there is no closing quote, rather
    // than adding several causes for the same incomplete token.
    const auto unfinished = scan("\"a\t");
    require(unfinished.diagnostics.size() == 1
                && unfinished.diagnostics[0].code == DiagnosticCode::UnterminatedString,
            "Unclosed literal should prioritize its missing delimiter.");
    const auto several_bytes = scan("\"\t\t\"");
    require(several_bytes.diagnostics.size() == 1
                && several_bytes.diagnostics[0].location.offset == 1,
            "Closed literal should identify only its first bad byte.");
}

void identifier_length_boundary() {
    for (std::size_t length : {255u, 256u, 257u}) {
        const std::string text(length, 'a');
        const auto result = scan(text + " return");
        const bool too_long = length > 256;
        require(result.has_errors() == too_long, "Wrong identifier length boundary.");
        require(result.tokens[0].lexeme == text
                    && result.tokens[0].kind == (too_long ? TokenKind::Invalid
                                                         : TokenKind::Identifier),
                "Overlong identifier was split or lost its text.");
        if (too_long) {
            require(result.diagnostics.size() == 1
                        && result.diagnostics[0].code == DiagnosticCode::IdentifierTooLong
                        && result.diagnostics[0].length == length,
                    "Overlong identifier has the wrong diagnostic span.");
        }
        require(result.tokens[2].kind == TokenKind::KwReturn,
                "Scanning did not continue after an overlong identifier.");
    }
}

void string_length_boundary() {
    for (std::size_t length : {255u, 256u, 257u}) {
        for (bool escaped : {false, true}) {
            std::string body;
            for (std::size_t i = 0; i < length; ++i) {
                body += escaped ? "\\n" : "a";
            }
            const std::string text = "\"" + body + "\"";
            const auto result = scan(text + " return");
            const bool too_long = length > 256;
            require(result.has_errors() == too_long, "Wrong string unit-count boundary.");
            require(result.tokens[0].lexeme == text
                        && result.tokens[0].kind == (too_long ? TokenKind::Invalid
                                                             : TokenKind::StringLiteral),
                    "String limit must count content units without changing its text.");
            if (too_long) {
                require(result.diagnostics.size() == 1
                            && result.diagnostics[0].code == DiagnosticCode::StringTooLong
                            && result.diagnostics[0].length == text.size(),
                        "Overlong string diagnostic must cover its raw source span.");
            }
            require(result.tokens[2].kind == TokenKind::KwReturn,
                    "Scanning did not continue after an overlong string.");
        }
    }
}

void result_ownership_and_reset() {
    bminor::SourceFile source{"repeat.bminor", "@"};
    bminor::Lexer lexer(source);
    const auto first = lexer.tokenize();
    const auto second = lexer.tokenize();
    require(first.diagnostics.size() == 1 && second.diagnostics.size() == 1,
            "Repeated scanning accumulated or lost diagnostics.");
    source.text = "if";
    const auto clean = lexer.tokenize();
    require(!clean.has_errors() && clean.tokens[0].kind == TokenKind::KwIf,
            "A clean scan inherited diagnostics or cursor state.");
    require(first.tokens[0].lexeme == "@" && first.diagnostics[0].location.offset == 0,
            "Scanning again changed the previously returned result.");
}

void valid_input_and_rendering() {
    const auto valid = scan(R"bminor(true "\q\n\0" '\'' // tail
/* closed */ 123456789012345678901234567890)bminor");
    require(!valid.has_errors(), "Valid base-dialect spellings produced diagnostics.");
    const auto bad = scan("@");
    std::ostringstream output;
    bminor::write_diagnostic(output, bad.diagnostics[0]);
    require(output.str().find("errors.bminor:1:1: error [LEX001]:") == 0,
            "CLI diagnostic format lost the filename, position or stable code.");
    require(output.str().find("0x40") != std::string::npos
                && output.str().back() == '\n',
            "Diagnostic text is incomplete.");
}

void diagnostic_token_consistency() {
    for (unsigned int seed = 0; seed < 16; ++seed) {
        std::string text;
        for (unsigned int i = 0; i < 256; ++i) {
            text.push_back(static_cast<char>((i * 73 + seed * 19) & 255));
        }
        const auto result = scan(text);
        std::size_t error_index = 0;
        for (const auto& token : result.tokens) {
            if (token.kind != TokenKind::Invalid) {
                continue;
            }
            require(error_index < result.diagnostics.size(), "INVALID has no diagnostic.");
            const auto& error = result.diagnostics[error_index++];
            require(error.location.offset >= token.location.offset && error.length > 0
                        && error.location.offset + error.length
                               <= token.location.offset + token.lexeme.size(),
                    "Diagnostic span lies outside its invalid token.");
            require(!error.message.empty()
                        && std::string(bminor::diagnostic_code_name(error.code)) != "UNKNOWN",
                    "Diagnostic is missing a message or stable code.");
        }
        require(error_index == result.diagnostics.size(), "Unexpected duplicate diagnostic.");
    }
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*run)();
    };
    const TestCase cases[] = {
        {"unknown bytes and recovery", unknown_bytes},
        {"unfinished literals and comments", unterminated_forms},
        {"incomplete escapes", incomplete_escape},
        {"character literal width", character_width},
        {"invalid literal bytes and error priority", invalid_literal_bytes_and_priority},
        {"identifier length boundary", identifier_length_boundary},
        {"string length boundary", string_length_boundary},
        {"result ownership and reset", result_ownership_and_reset},
        {"valid input and diagnostic format", valid_input_and_rendering},
        {"diagnostic/token consistency", diagnostic_token_consistency}
    };
    for (const auto& test : cases) {
        try {
            test.run();
            std::cout << "PASS: " << test.name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
