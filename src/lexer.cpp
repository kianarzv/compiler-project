#include "bminor/lexer.hpp"

#include <string>
#include <unordered_map>

namespace bminor {
namespace {

// Explicit ASCII checks follow the documented identifier rule. Locale-aware
// character functions could otherwise accept letters outside the language.
bool is_identifier_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_identifier_part(char c) {
    return is_identifier_start(c) || (c >= '0' && c <= '9');
}

bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

TokenKind classify_word(const std::string& word) {
    // Look up only a COMPLETE word. For example, returnValue stays a single
    // identifier instead of becoming the keyword return followed by Value.
    static const std::unordered_map<std::string, TokenKind> keywords = {
        {"array", TokenKind::KwArray},
        {"boolean", TokenKind::KwBoolean},
        {"char", TokenKind::KwChar},
        {"else", TokenKind::KwElse},
        {"false", TokenKind::BooleanLiteral},
        {"for", TokenKind::KwFor},
        {"function", TokenKind::KwFunction},
        {"if", TokenKind::KwIf},
        {"integer", TokenKind::KwInteger},
        {"print", TokenKind::KwPrint},
        {"return", TokenKind::KwReturn},
        {"string", TokenKind::KwString},
        {"true", TokenKind::BooleanLiteral},
        {"void", TokenKind::KwVoid},
        {"while", TokenKind::KwWhile}
    };
    const auto found = keywords.find(word);
    return found == keywords.end() ? TokenKind::Identifier : found->second;
}

} // namespace

Lexer::Lexer(const SourceFile& source) : source_(source) {}

std::vector<Token> Lexer::tokenize() {
    // Resetting here makes a second call behave like the first one. Cursor
    // changes are confined to advance(), keeping location tracking consistent.
    offset_ = 0;
    line_ = 1;
    column_ = 1;

    std::vector<Token> tokens;
    while (!at_end()) {
        tokens.push_back(scan_token());
    }
    tokens.push_back(Token{TokenKind::EndOfFile, "", current_location()});
    return tokens;
}

bool Lexer::at_end() const {
    return offset_ >= source_.text.size();
}

char Lexer::peek() const {
    return at_end() ? '\0' : source_.text[offset_];
}

void Lexer::advance() {
    if (at_end()) {
        return;
    }

    const char c = source_.text[offset_++];
    if (c == '\r') {
        // Consume CRLF together: two bytes in the lexeme, one logical newline.
        // Standalone CR and LF also move to column 1 of the next line.
        if (!at_end() && peek() == '\n') {
            ++offset_;
        }
        ++line_;
        column_ = 1;
    } else if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
}

SourceLocation Lexer::current_location() const {
    return SourceLocation{source_.path, line_, column_, offset_};
}

Token Lexer::scan_token() {
    const SourceLocation start = current_location();
    TokenKind kind = TokenKind::Invalid;

    if (is_identifier_start(peek())) {
        // Maximal munch: keep letters, digits and underscores until the whole
        // identifier is consumed; only then decide if it is a reserved word.
        do {
            advance();
        } while (!at_end() && is_identifier_part(peek()));
        kind = TokenKind::Identifier;
    } else if (is_whitespace(peek())) {
        do {
            advance();
        } while (!at_end() && is_whitespace(peek()));
        kind = TokenKind::Whitespace;
    } else {
        // Always consume something, even if this step does not recognize it.
        // This prevents unsupported input from trapping the lexer in a loop.
        advance();
    }

    const std::string lexeme = source_.text.substr(start.offset,
                                                  offset_ - start.offset);
    if (kind == TokenKind::Identifier) {
        kind = classify_word(lexeme);
    }
    return Token{kind, lexeme, start};
}

} // namespace bminor
