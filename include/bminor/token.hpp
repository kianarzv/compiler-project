#ifndef BMINOR_TOKEN_HPP
#define BMINOR_TOKEN_HPP

#include "bminor/source_location.hpp"

#include <string>

namespace bminor {

// Keywords have separate kinds so a later parser can match them directly.
// true and false share a literal kind; their lexeme retains the actual value.
// Additional literals and punctuation will be introduced in step 3.
enum class TokenKind {
    Identifier,
    KwArray,
    KwBoolean,
    KwChar,
    KwElse,
    KwFor,
    KwFunction,
    KwIf,
    KwInteger,
    KwPrint,
    KwReturn,
    KwString,
    KwVoid,
    KwWhile,
    BooleanLiteral,
    Whitespace,
    Invalid,
    EndOfFile
};

struct Token {
    TokenKind kind;
    // Store the exact source slice, including whitespace and escape bytes.
    // Token length is lexeme.size(); the first byte is at location.offset.
    std::string lexeme;
    SourceLocation location;
};

// Stable labels for CLI output. This keeps display names out of lexer logic.
const char* token_kind_name(TokenKind kind);

} // namespace bminor

#endif
