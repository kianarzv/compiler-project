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
    const auto tokens = lexer.tokenize().tokens;
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
    // Include CRLF, a null and a non-ASCII byte. Even invalid input must survive
    // in contiguous slices so highlighting can reproduce the original source.
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
                         TokenKind::Invalid, TokenKind::IntegerLiteral,
                         TokenKind::EndOfFile});
    require(tokens[0].lexeme == "@" && tokens[2].lexeme == "$"
                && tokens[3].lexeme == "9", "Input bytes were dropped.");
}

void decimal_literals_and_signs() {
    const auto numbers = scan("0 007 123456789012345678901234567890");
    expect_kinds(numbers, {TokenKind::IntegerLiteral, TokenKind::IntegerLiteral,
                          TokenKind::IntegerLiteral, TokenKind::EndOfFile});
    require(numbers[1].lexeme == "007", "Leading zeros were changed.");
    require(numbers[2].lexeme == "123456789012345678901234567890",
            "A large literal was converted or truncated during scanning.");

    const auto signs = scan("a-12 -42 +7");
    expect_kinds(signs, {TokenKind::Identifier, TokenKind::Minus,
                        TokenKind::IntegerLiteral, TokenKind::Minus,
                        TokenKind::IntegerLiteral, TokenKind::Plus,
                        TokenKind::IntegerLiteral, TokenKind::EndOfFile});
    const auto spaced = scan("a - 12 - 42 + 7");
    require(signs.size() == spaced.size(), "Spacing changed arithmetic tokens.");
    for (std::size_t i = 0; i < signs.size(); ++i) {
        require(signs[i].kind == spaced[i].kind
                    && signs[i].lexeme == spaced[i].lexeme,
                "A sign was incorrectly attached to an integer.");
    }
}

void operator_vocabulary() {
    const auto tokens = scan("+ - * / % ^ ! < > = ++ -- <= >= == != && ||");
    expect_kinds(tokens, {TokenKind::Plus, TokenKind::Minus, TokenKind::Star,
                         TokenKind::Slash, TokenKind::Percent, TokenKind::Caret,
                         TokenKind::Bang, TokenKind::Less, TokenKind::Greater,
                         TokenKind::Assign, TokenKind::PlusPlus,
                         TokenKind::MinusMinus, TokenKind::LessEqual,
                         TokenKind::GreaterEqual, TokenKind::EqualEqual,
                         TokenKind::BangEqual, TokenKind::AndAnd,
                         TokenKind::OrOr, TokenKind::EndOfFile});
    for (const auto& token : tokens) {
        require(std::string(bminor::token_kind_name(token.kind)) != "UNKNOWN",
                "An operator has no display label.");
    }
}

void longest_operator_match() {
    const auto tokens = scan("a+++b--->=c===d&&&e|||f");
    expect_kinds(tokens, {TokenKind::Identifier, TokenKind::PlusPlus,
                         TokenKind::Plus, TokenKind::Identifier,
                         TokenKind::MinusMinus, TokenKind::Minus,
                         TokenKind::GreaterEqual, TokenKind::Identifier,
                         TokenKind::EqualEqual, TokenKind::Assign,
                         TokenKind::Identifier, TokenKind::AndAnd,
                         TokenKind::Invalid, TokenKind::Identifier,
                         TokenKind::OrOr, TokenKind::Invalid,
                         TokenKind::Identifier, TokenKind::EndOfFile});
    require(tokens[6].lexeme == ">=", "Split a two-byte comparison.");
    require(tokens[12].lexeme == "&" && tokens[15].lexeme == "|",
            "Single bitwise operators must not become logical operators.");
}

void delimiters() {
    const auto tokens = scan("(){}[],:;");
    expect_kinds(tokens, {TokenKind::LeftParen, TokenKind::RightParen,
                         TokenKind::LeftBrace, TokenKind::RightBrace,
                         TokenKind::LeftBracket, TokenKind::RightBracket,
                         TokenKind::Comma, TokenKind::Colon,
                         TokenKind::Semicolon, TokenKind::EndOfFile});
}

void quoted_literals_and_escapes() {
    const auto tokens = scan(
        R"bminor("" "if // /*" "say \"hi\"" "\\" "\n\0\q" 'a' '\n' '\0' '\'' '\\' '\q')bminor");
    expect_kinds(tokens, {TokenKind::StringLiteral, TokenKind::StringLiteral,
                         TokenKind::StringLiteral, TokenKind::StringLiteral,
                         TokenKind::StringLiteral, TokenKind::CharacterLiteral,
                         TokenKind::CharacterLiteral, TokenKind::CharacterLiteral,
                         TokenKind::CharacterLiteral, TokenKind::CharacterLiteral,
                         TokenKind::CharacterLiteral, TokenKind::EndOfFile});
    require(tokens[1].lexeme == R"bminor("if // /*")bminor",
            "A comment opener inside a string started a comment.");
    require(tokens[2].lexeme == R"bminor("say \"hi\"")bminor",
            "An escaped quote ended a string too early.");
    require(tokens[4].lexeme == R"bminor("\n\0\q")bminor",
            "Escape sequences must retain their original spelling.");
    require(tokens[8].lexeme == R"bminor('\'')bminor",
            "Escaped character delimiter was not retained.");

    const auto boundary = scan(R"bminor("a\\" next "a\"still")bminor");
    expect_kinds(boundary, {TokenKind::StringLiteral, TokenKind::Identifier,
                           TokenKind::StringLiteral, TokenKind::EndOfFile});
    require(boundary[1].lexeme == "next", "Backslash parity hid a closing quote.");
}

void malformed_quoted_literals() {
    const auto characters = scan(R"bminor('' 'ab' '\ab' 'z')bminor");
    expect_kinds(characters, {TokenKind::Invalid, TokenKind::Invalid,
                             TokenKind::Invalid, TokenKind::CharacterLiteral,
                             TokenKind::EndOfFile});
    const auto newline = scan("\"open\r\nreturn 1;");
    expect_kinds(newline, {TokenKind::Invalid, TokenKind::KwReturn,
                          TokenKind::IntegerLiteral, TokenKind::Semicolon,
                          TokenKind::EndOfFile});
    require(newline[0].lexeme == "\"open", "Partial string consumed the newline.");
    require(newline[1].location.line == 2 && newline[1].location.column == 1,
            "Failed to resume after a malformed string.");

    const std::string dangling = std::string("\"tail") + '\\';
    const auto eof = scan(dangling);
    expect_kinds(eof, {TokenKind::Invalid, TokenKind::EndOfFile});
    require(eof[0].lexeme == dangling, "Trailing backslash was lost at EOF.");
    const auto continuation = scan(std::string("\"tail\\") + "\r\nif");
    expect_kinds(continuation, {TokenKind::Invalid, TokenKind::KwIf,
                               TokenKind::EndOfFile});

    std::string invalid_bytes = "\"a";
    invalid_bytes.push_back('\0');
    invalid_bytes.push_back(static_cast<char>(0xFF));
    invalid_bytes += "b\"";
    const auto bytes = scan(invalid_bytes);
    expect_kinds(bytes, {TokenKind::Invalid, TokenKind::EndOfFile});
    require(bytes[0].lexeme == invalid_bytes, "Invalid string bytes were lost.");
}

void comments_and_locations() {
    const auto tokens = scan("a/b// <= \"fake\"\r\nc/**/d/* one\n two */e");
    expect_kinds(tokens, {TokenKind::Identifier, TokenKind::Slash,
                         TokenKind::Identifier, TokenKind::LineComment,
                         TokenKind::Identifier, TokenKind::BlockComment,
                         TokenKind::Identifier, TokenKind::BlockComment,
                         TokenKind::Identifier, TokenKind::EndOfFile});
    require(tokens[3].lexeme == "// <= \"fake\"", "Line comment parsed as code.");
    require(tokens[4].location.line == 2 && tokens[4].location.column == 1,
            "Line comment included or miscounted its following newline.");
    require(tokens[6].location.column == 6, "Empty block comment lost its span.");
    require(tokens[7].lexeme == "/* one\n two */", "Block comment was changed.");
    require(tokens[8].location.line == 3 && tokens[8].location.column == 8,
            "Wrong location after a multiline block comment.");
}

void comment_boundaries() {
    expect_kinds(scan("// tail"), {TokenKind::LineComment, TokenKind::EndOfFile});
    expect_kinds(scan("/**/ /***/"), {TokenKind::BlockComment,
                                     TokenKind::BlockComment, TokenKind::EndOfFile});
    const auto nested = scan("/* outer /* inner */tail");
    expect_kinds(nested, {TokenKind::BlockComment, TokenKind::Identifier,
                         TokenKind::EndOfFile});
    require(nested[1].lexeme == "tail", "Block comments must stop at first */.");
    for (const std::string text : {"/*", "/*/", "/* never closed\r\n"}) {
        const auto tokens = scan(text);
        expect_kinds(tokens, {TokenKind::Invalid, TokenKind::EndOfFile});
        require(tokens[0].lexeme == text, "Unclosed block comment lost bytes.");
    }
    expect_kinds(scan("/"), {TokenKind::Slash, TokenKind::EndOfFile});
}

void arbitrary_byte_coverage() {
    // This checks invariants rather than expecting a particular tokenization:
    // no gaps, no zero-length tokens, correct positions, and exactly one EOF.
    // The deterministic data exercises every byte and many delimiter boundaries.
    for (unsigned int seed = 0; seed < 32; ++seed) {
        std::string text;
        for (unsigned int i = 0; i < 256; ++i) {
            text.push_back(static_cast<char>((i * 73 + seed * 19) & 255));
        }
        const auto tokens = scan(text, true);
        require(tokens.size() <= text.size() + 1, "Scanner did not progress.");
        std::size_t offset = 0;
        std::size_t line = 1;
        std::size_t column = 1;
        std::size_t eof_count = 0;
        for (const auto& token : tokens) {
            require(token.location.offset == offset && token.location.line == line
                        && token.location.column == column,
                    "Arbitrary input produced an incorrect token location.");
            require(token.lexeme == text.substr(offset, token.lexeme.size()),
                    "Arbitrary input changed a source slice.");
            if (token.kind == TokenKind::EndOfFile) {
                ++eof_count;
                require(token.lexeme.empty(), "EOF lexeme must be empty.");
            } else {
                require(!token.lexeme.empty(), "A token consumed no input.");
            }
            for (char c : token.lexeme) {
                // Independent byte-wise location calculation: a CR counts the
                // newline, and an immediately following LF must not count again.
                if (c == '\r') {
                    ++line;
                    column = 1;
                } else if (c == '\n') {
                    if (offset == 0 || text[offset - 1] != '\r') {
                        ++line;
                    }
                    column = 1;
                } else {
                    ++column;
                }
                ++offset;
            }
        }
        require(offset == text.size() && eof_count == 1
                    && tokens.back().kind == TokenKind::EndOfFile,
                "Arbitrary input was not completely covered before EOF.");
    }
}

void empty_input_and_reuse() {
    const auto empty = scan("");
    expect_kinds(empty, {TokenKind::EndOfFile});
    require(empty[0].lexeme.empty() && empty[0].location.offset == 0
                && empty[0].location.line == 1 && empty[0].location.column == 1,
            "Incorrect EOF location for empty input.");

    const bminor::SourceFile source{"repeat.bminor", "if\r\n"};
    bminor::Lexer lexer(source);
    const auto first = lexer.tokenize().tokens;
    const auto second = lexer.tokenize().tokens;
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
        {"decimal literals and signs", decimal_literals_and_signs},
        {"operator vocabulary", operator_vocabulary},
        {"longest operator match", longest_operator_match},
        {"delimiters", delimiters},
        {"quoted literals and escapes", quoted_literals_and_escapes},
        {"malformed quoted literals", malformed_quoted_literals},
        {"comments and locations", comments_and_locations},
        {"comment boundaries", comment_boundaries},
        {"arbitrary byte coverage", arbitrary_byte_coverage},
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
