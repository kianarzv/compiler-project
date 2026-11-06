#ifndef BMINOR_LEXER_HPP
#define BMINOR_LEXER_HPP

#include "bminor/source_file.hpp"
#include "bminor/token.hpp"

#include <cstddef>
#include <vector>

namespace bminor {

// Scans the source from left to right and retains whitespace in the result.
// The source is borrowed: it must outlive this lexer. Returned tokens own their
// text and locations, so they remain valid after the lexer is destroyed.
class Lexer {
public:
    explicit Lexer(const SourceFile& source);

    // Reset the cursor and scan the entire file, ending with exactly one EOF.
    // Unsupported bytes become INVALID tokens for now; scanning still advances.
    std::vector<Token> tokenize();

private:
    bool at_end() const;
    char peek() const;
    void advance();
    SourceLocation current_location() const;
    Token scan_token();

    const SourceFile& source_;
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

} // namespace bminor

#endif
