#include "bminor/lexer.hpp"

#include <exception>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using bminor::Token;
using bminor::TokenKind;

// Throw instead of using assert: checks must still run in Release builds,
// where the compiler may disable assertions through NDEBUG.
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<Token> scan(const std::string& text, bool keep_whitespace = false) {
    const bminor::SourceFile source{"test.bminor", text};
    bminor::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    std::vector<Token> result;
    for (const auto& token : tokens) {
        if (keep_whitespace || token.kind != TokenKind::Whitespace) {
            result.push_back(token);
        }
    }
    return result;
}

void expect_kinds(const std::vector<Token>& tokens,
                  std::initializer_list<TokenKind> expected) {
    require(tokens.size() == expected.size(), "Unexpected token count.");
    std::size_t index = 0;
    for (TokenKind kind : expected) {
        require(tokens[index].kind == kind,
                "Unexpected kind at token " + std::to_string(index));
        ++index;
    }
}

void keyword_boundaries() {
    const auto tokens = scan("if ifx return returnValue true trueValue");
    expect_kinds(tokens, {TokenKind::KwIf, TokenKind::Identifier,
                         TokenKind::KwReturn, TokenKind::Identifier,
                         TokenKind::BooleanLiteral, TokenKind::Identifier,
                         TokenKind::EndOfFile});
    require(tokens[1].lexeme == "ifx", "Keyword prefix split an identifier.");
    require(tokens[3].lexeme == "returnValue", "Lost part of an identifier.");
    require(tokens[4].lexeme == "true", "Boolean literal lost its value.");
}

void reserved_words() {
    const auto tokens = scan("array boolean char else false for function if "
                             "integer print return string true void while");
    expect_kinds(tokens, {TokenKind::KwArray, TokenKind::KwBoolean,
                         TokenKind::KwChar, TokenKind::KwElse,
                         TokenKind::BooleanLiteral, TokenKind::KwFor,
                         TokenKind::KwFunction, TokenKind::KwIf,
                         TokenKind::KwInteger, TokenKind::KwPrint,
                         TokenKind::KwReturn, TokenKind::KwString,
                         TokenKind::BooleanLiteral, TokenKind::KwVoid,
                         TokenKind::KwWhile, TokenKind::EndOfFile});
    require(tokens[4].lexeme == "false", "False literal lost its value.");
}

void identifier_rules() {
    const auto tokens = scan("_ _count x1 if1 if_ IF False auto float double");
    require(tokens.size() == 11, "Unexpected identifier count.");
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        require(tokens[i].kind == TokenKind::Identifier,
                "A case-sensitive identifier was treated as a keyword.");
    }
}

void source_locations() {
    const auto tokens = scan("a\tb\r\nc\rd\ne");
    const std::size_t lines[] = {1, 1, 2, 3, 4, 4};
    const std::size_t columns[] = {1, 3, 1, 1, 1, 2};
    const std::size_t offsets[] = {0, 2, 5, 7, 9, 10};
    require(tokens.size() == 6, "Unexpected location sample token count.");
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto& location = tokens[i].location;
        require(location.filename == "test.bminor", "Missing source filename.");
        require(location.line == lines[i] && location.column == columns[i]
                    && location.offset == offsets[i],
                "Wrong source location at token " + std::to_string(i));
    }
}

void source_coverage() {
    // Include CRLF, a null and a non-ASCII byte. Even bytes that the partial
    // lexer does not understand must survive in contiguous source slices.
    std::string text = "if \t\r\nname@";
    text.push_back('\0');
    text.push_back(static_cast<char>(0xFF));
    const auto tokens = scan(text, true);
    std::string reconstructed;
    std::size_t next_offset = 0;
    for (const auto& token : tokens) {
        require(token.location.offset == next_offset, "Gap in token offsets.");
        require(token.kind == TokenKind::EndOfFile || !token.lexeme.empty(),
                "A non-EOF token did not consume input.");
        reconstructed += token.lexeme;
        next_offset += token.lexeme.size();
    }
    require(reconstructed == text, "Tokenization changed or lost source bytes.");
    require(tokens.back().kind == TokenKind::EndOfFile, "Missing EOF token.");
    require(next_offset == text.size(), "EOF does not follow the final byte.");
}

void unsupported_input() {
    const auto tokens = scan("@if$9");
    expect_kinds(tokens, {TokenKind::Invalid, TokenKind::KwIf,
                         TokenKind::Invalid, TokenKind::Invalid,
                         TokenKind::EndOfFile});
    require(tokens[0].lexeme == "@" && tokens[2].lexeme == "$"
                && tokens[3].lexeme == "9", "Unsupported bytes were dropped.");
}

void empty_input_and_reuse() {
    const auto empty = scan("");
    expect_kinds(empty, {TokenKind::EndOfFile});
    require(empty[0].lexeme.empty() && empty[0].location.offset == 0
                && empty[0].location.line == 1 && empty[0].location.column == 1,
            "Incorrect EOF location for empty input.");

    const bminor::SourceFile source{"repeat.bminor", "if\r\n"};
    bminor::Lexer lexer(source);
    const auto first = lexer.tokenize();
    const auto second = lexer.tokenize();
    require(first.size() == second.size(), "Repeated scanning changed count.");
    for (std::size_t i = 0; i < first.size(); ++i) {
        require(first[i].kind == second[i].kind
                    && first[i].lexeme == second[i].lexeme
                    && first[i].location.offset == second[i].location.offset
                    && first[i].location.line == second[i].location.line
                    && first[i].location.column == second[i].location.column,
                "Lexer cursor was not reset before a repeated scan.");
    }
    require(second.back().location.line == 2
                && second.back().location.column == 1,
            "EOF after CRLF must start the next line.");
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*run)();
    };
    const TestCase cases[] = {
        {"keyword boundaries", keyword_boundaries},
        {"reserved words", reserved_words},
        {"identifier rules", identifier_rules},
        {"source locations", source_locations},
        {"source coverage", source_coverage},
        {"unsupported input", unsupported_input},
        {"empty input and repeated scanning", empty_input_and_reuse}
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
