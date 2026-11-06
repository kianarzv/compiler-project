#ifndef BMINOR_LEXER_HPP
#define BMINOR_LEXER_HPP

#include "bminor/source_file.hpp"
#include "bminor/token.hpp"

#include <cstddef>
#include <vector>

namespace bminor {

// Scans the source from left to right, retaining whitespace and comments.
// The source is borrowed: it must outlive this lexer. Returned tokens own their
// text and locations, so they remain valid after the lexer is destroyed.
class Lexer {
public:
    explicit Lexer(const SourceFile& source);

    // Reset the cursor and scan the entire file, ending with exactly one EOF.
    // Unknown bytes and malformed literals/comments become INVALID tokens.
    // Scanning still advances; detailed diagnostics are added in step 4.
    std::vector<Token> tokenize();

private:
    bool at_end() const;
    char peek(std::size_t lookahead = 0) const;
    bool match(char expected);
    void advance();
    SourceLocation current_location() const;
    Token scan_token();
    TokenKind scan_quoted(char quote);
    TokenKind scan_comment();
    TokenKind scan_symbol();

    const SourceFile& source_;
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

} // namespace bminor

#endif
