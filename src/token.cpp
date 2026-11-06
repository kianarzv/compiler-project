#include "bminor/token.hpp"

namespace bminor {

const char* token_kind_name(TokenKind kind) {
    switch (kind) {
    case TokenKind::Identifier:     return "IDENTIFIER";
    case TokenKind::KwArray:        return "KW_ARRAY";
    case TokenKind::KwBoolean:      return "KW_BOOLEAN";
    case TokenKind::KwChar:         return "KW_CHAR";
    case TokenKind::KwElse:         return "KW_ELSE";
    case TokenKind::KwFor:          return "KW_FOR";
    case TokenKind::KwFunction:     return "KW_FUNCTION";
    case TokenKind::KwIf:           return "KW_IF";
    case TokenKind::KwInteger:      return "KW_INTEGER";
    case TokenKind::KwPrint:        return "KW_PRINT";
    case TokenKind::KwReturn:       return "KW_RETURN";
    case TokenKind::KwString:       return "KW_STRING";
    case TokenKind::KwVoid:         return "KW_VOID";
    case TokenKind::KwWhile:        return "KW_WHILE";
    case TokenKind::BooleanLiteral: return "BOOLEAN_LITERAL";
    case TokenKind::Whitespace:     return "WHITESPACE";
    case TokenKind::Invalid:        return "INVALID";
    case TokenKind::EndOfFile:      return "EOF";
    }
    return "UNKNOWN";
}

} // namespace bminor
