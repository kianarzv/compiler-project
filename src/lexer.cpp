#include "bminor/lexer.hpp"

#include <string>
#include <unordered_map>
#include <utility>

namespace bminor {
namespace {

const std::size_t max_identifier_length = 256;
const std::size_t max_string_length = 256;

// Hex notation keeps control bytes, embedded nulls and non-ASCII input readable
// in errors without inserting those bytes into a terminal message.
std::string byte_name(char c) {
    const char hex[] = "0123456789ABCDEF";
    const unsigned char byte = static_cast<unsigned char>(c);
    std::string result = "0x";
    result += hex[byte >> 4];
    result += hex[byte & 15];
    return result;
}

// Explicit ASCII checks follow the documented identifier rule. Locale-aware
// character functions could otherwise accept letters outside the language.
bool is_identifier_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_identifier_part(char c) {
    return is_identifier_start(c) || is_digit(c);
}

bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_line_ending(char c) {
    return c == '\r' || c == '\n';
}

bool is_printable_ascii(char c) {
    const unsigned char byte = static_cast<unsigned char>(c);
    return byte >= 32 && byte <= 126;
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

LexResult Lexer::tokenize() {
    // Resetting here makes a second call behave like the first one. Cursor
    // changes are confined to advance(), keeping location tracking consistent.
    offset_ = 0;
    line_ = 1;
    column_ = 1;
    diagnostics_.clear();

    std::vector<Token> tokens;
    while (!at_end()) {
        tokens.push_back(scan_token());
    }
    tokens.push_back(Token{TokenKind::EndOfFile, "", current_location()});
    // Transfer ownership rather than copying all lexemes and messages. A later
    // call starts with fresh cursor/diagnostic state and cannot change this result.
    return LexResult{std::move(tokens), std::move(diagnostics_)};
}

bool Lexer::at_end() const {
    return offset_ >= source_.text.size();
}

char Lexer::peek(std::size_t lookahead) const {
    // Lookahead is used for two-character operators and comment openers.
    // Check remaining bytes before indexing, including at the end of a file.
    return lookahead < source_.text.size() - offset_
        ? source_.text[offset_ + lookahead] : '\0';
}

bool Lexer::match(char expected) {
    if (at_end() || peek() != expected) {
        return false;
    }
    advance();
    return true;
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
    } else if (is_digit(peek())) {
        // A sign is an operator, not part of this token. This makes x-12 and
        // x - 12 produce the same tokens; unary negation belongs to the parser.
        do {
            advance();
        } while (!at_end() && is_digit(peek()));
        kind = TokenKind::IntegerLiteral;
    } else if (peek() == '"' || peek() == '\'') {
        kind = scan_quoted(peek(), start);
    } else if (peek() == '/' && (peek(1) == '/' || peek(1) == '*')) {
        // A comment opener must win over the single-character division token.
        kind = scan_comment(start);
    } else {
        kind = scan_symbol();
        if (kind == TokenKind::Invalid) {
            std::string message = "Unexpected byte "
                + byte_name(source_.text[start.offset]) + ".";
            if (source_.text[start.offset] == '&') {
                message += " Use '&&' for logical AND.";
            } else if (source_.text[start.offset] == '|') {
                message += " Use '||' for logical OR.";
            }
            report(DiagnosticCode::UnknownByte, start, 1, message);
        }
    }

    const std::string lexeme = source_.text.substr(start.offset,
                                                  offset_ - start.offset);
    if (kind == TokenKind::Identifier) {
        kind = classify_word(lexeme);
        if (lexeme.size() > max_identifier_length) {
            report(DiagnosticCode::IdentifierTooLong, start, lexeme.size(),
                   "Identifier exceeds the 256-character limit.");
            kind = TokenKind::Invalid;
        }
    }
    return Token{kind, lexeme, start};
}

TokenKind Lexer::scan_quoted(char quote, const SourceLocation& start) {
    advance(); // Opening quote; the final lexeme will include both delimiters.
    std::size_t characters = 0;
    bool has_invalid_byte = false;
    SourceLocation invalid_byte = start;

    while (!at_end() && !is_line_ending(peek())) {
        if (peek() == quote) {
            advance();
            // An escape pair represents one character. Thus '\n' is a valid
            // character literal, while '' and 'ab' are not.
            if (has_invalid_byte) {
                report(DiagnosticCode::InvalidLiteralByte, invalid_byte, 1,
                       "Byte " + byte_name(source_.text[invalid_byte.offset])
                       + " is not printable ASCII; use an escape for control characters.");
                return TokenKind::Invalid;
            }
            if (quote == '\'' && characters != 1) {
                report(DiagnosticCode::InvalidCharacterLength, start,
                       offset_ - start.offset,
                       "Character literal must contain exactly one character; found "
                       + std::to_string(characters) + ".");
                return TokenKind::Invalid;
            }
            if (quote == '"' && characters > max_string_length) {
                report(DiagnosticCode::StringTooLong, start, offset_ - start.offset,
                       "String literal exceeds the 256-character limit.");
                return TokenKind::Invalid;
            }
            return quote == '"' ? TokenKind::StringLiteral
                                : TokenKind::CharacterLiteral;
        }

        if (peek() == '\\') {
            const SourceLocation escape = current_location();
            advance();
            // A trailing backslash cannot escape EOF or a physical newline.
            // Leave a newline for the whitespace scanner and resume afterward.
            if (at_end() || is_line_ending(peek())) {
                report(DiagnosticCode::IncompleteEscape, escape, 1,
                       "Incomplete escape: expected a character after the backslash.");
                return TokenKind::Invalid;
            }
        }

        // The base dialect permits a backslash followed by a printable ASCII
        // character. Keep the spelling unchanged; value decoding is a separate
        // concern. Unescaped control/non-ASCII bytes invalidate the whole token.
        if (!is_printable_ascii(peek()) && !has_invalid_byte) {
            has_invalid_byte = true;
            invalid_byte = current_location();
        }
        advance();
        ++characters;
    }

    // EOF or a physical newline before the closing quote: keep the partial
    // literal as one INVALID token instead of interpreting its contents as code.
    report(quote == '"' ? DiagnosticCode::UnterminatedString
                        : DiagnosticCode::UnterminatedCharacter,
           start, offset_ - start.offset,
           quote == '"' ? "Unterminated string literal; expected a closing double quote."
                        : "Unterminated character literal; expected a closing single quote.");
    return TokenKind::Invalid;
}

TokenKind Lexer::scan_comment(const SourceLocation& start) {
    advance(); // The caller already confirmed that the first byte is '/'.
    if (match('/')) {
        while (!at_end() && !is_line_ending(peek())) {
            advance();
        }
        return TokenKind::LineComment;
    }

    advance(); // Consume '*' from the confirmed block-comment opener.
    while (!at_end()) {
        if (peek() == '*' && peek(1) == '/') {
            advance();
            advance();
            return TokenKind::BlockComment;
        }
        advance();
    }
    // Block comments do not nest. If the first closing marker never appears,
    // the INVALID token covers the opener and all remaining source bytes.
    report(DiagnosticCode::UnterminatedBlockComment, start, offset_ - start.offset,
           "Unterminated block comment; expected '*/' before end of file.");
    return TokenKind::Invalid;
}

TokenKind Lexer::scan_symbol() {
    // Consume the first byte unconditionally, then try a supported longer
    // spelling. Even an unknown symbol must move the cursor to guarantee progress.
    const char symbol = peek();
    advance();
    switch (symbol) {
    case '+': return match('+') ? TokenKind::PlusPlus : TokenKind::Plus;
    case '-': return match('-') ? TokenKind::MinusMinus : TokenKind::Minus;
    case '*': return TokenKind::Star;
    case '/': return TokenKind::Slash;
    case '%': return TokenKind::Percent;
    case '^': return TokenKind::Caret;
    case '!': return match('=') ? TokenKind::BangEqual : TokenKind::Bang;
    case '<': return match('=') ? TokenKind::LessEqual : TokenKind::Less;
    case '>': return match('=') ? TokenKind::GreaterEqual : TokenKind::Greater;
    case '=': return match('=') ? TokenKind::EqualEqual : TokenKind::Assign;
    case '&': return match('&') ? TokenKind::AndAnd : TokenKind::Invalid;
    case '|': return match('|') ? TokenKind::OrOr : TokenKind::Invalid;
    case '(': return TokenKind::LeftParen;
    case ')': return TokenKind::RightParen;
    case '{': return TokenKind::LeftBrace;
    case '}': return TokenKind::RightBrace;
    case '[': return TokenKind::LeftBracket;
    case ']': return TokenKind::RightBracket;
    case ',': return TokenKind::Comma;
    case ';': return TokenKind::Semicolon;
    case ':': return TokenKind::Colon;
    default:  return TokenKind::Invalid;
    }
}

void Lexer::report(DiagnosticCode code, const SourceLocation& location,
                   std::size_t length, const std::string& message) {
    // Scanning helpers report data, never print or throw for malformed input.
    // The CLI decides how to render errors and which process exit code to use.
    diagnostics_.push_back(Diagnostic{code, location, length, message});
}

} // namespace bminor
