#ifndef BMINOR_LEXER_HPP
#define BMINOR_LEXER_HPP

#include "bminor/diagnostic.hpp"
#include "bminor/source_file.hpp"
#include "bminor/token.hpp"

#include <cstddef>
#include <vector>

namespace bminor {

// Tokens and diagnostics belong to the caller. Keeping both in one result lets
// later phases inspect recovered tokens even when the source contains errors.
struct LexResult {
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;

    bool has_errors() const { return !diagnostics.empty(); }
};

// Scans the source from left to right, retaining whitespace and comments.
// The source is borrowed: it must outlive this lexer. Returned tokens own their
// text and locations, so they remain valid after the lexer is destroyed.
class Lexer {
public:
    explicit Lexer(const SourceFile& source);

    // Reset the cursor and scan the entire file, ending with exactly one EOF.
    // Unknown bytes and malformed literals/comments become INVALID tokens.
    // Each INVALID token has one primary diagnostic, and scanning still advances.
    LexResult tokenize();

private:
    bool at_end() const;
    char peek(std::size_t lookahead = 0) const;
    bool match(char expected);
    void advance();
    SourceLocation current_location() const;
    Token scan_token();
    TokenKind scan_quoted(char quote, const SourceLocation& start);
    TokenKind scan_comment(const SourceLocation& start);
    TokenKind scan_symbol();
    void report(DiagnosticCode code, const SourceLocation& location,
                std::size_t length, const std::string& message);

    const SourceFile& source_;
    std::vector<Diagnostic> diagnostics_;
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

} // namespace bminor

#endif
