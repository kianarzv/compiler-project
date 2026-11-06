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
    case TokenKind::IntegerLiteral: return "INTEGER_LITERAL";
    case TokenKind::StringLiteral:  return "STRING_LITERAL";
    case TokenKind::CharacterLiteral: return "CHARACTER_LITERAL";
    case TokenKind::Plus:           return "PLUS";
    case TokenKind::Minus:          return "MINUS";
    case TokenKind::Star:           return "STAR";
    case TokenKind::Slash:          return "SLASH";
    case TokenKind::Percent:        return "PERCENT";
    case TokenKind::Caret:          return "CARET";
    case TokenKind::PlusPlus:       return "PLUS_PLUS";
    case TokenKind::MinusMinus:     return "MINUS_MINUS";
    case TokenKind::Bang:           return "BANG";
    case TokenKind::Less:           return "LESS";
    case TokenKind::LessEqual:      return "LESS_EQUAL";
    case TokenKind::Greater:        return "GREATER";
    case TokenKind::GreaterEqual:   return "GREATER_EQUAL";
    case TokenKind::EqualEqual:     return "EQUAL_EQUAL";
    case TokenKind::BangEqual:      return "BANG_EQUAL";
    case TokenKind::AndAnd:         return "AND_AND";
    case TokenKind::OrOr:           return "OR_OR";
    case TokenKind::Assign:         return "ASSIGN";
    case TokenKind::LeftParen:      return "LEFT_PAREN";
    case TokenKind::RightParen:     return "RIGHT_PAREN";
    case TokenKind::LeftBrace:      return "LEFT_BRACE";
    case TokenKind::RightBrace:     return "RIGHT_BRACE";
    case TokenKind::LeftBracket:    return "LEFT_BRACKET";
    case TokenKind::RightBracket:   return "RIGHT_BRACKET";
    case TokenKind::Comma:          return "COMMA";
    case TokenKind::Semicolon:      return "SEMICOLON";
    case TokenKind::Colon:          return "COLON";
    case TokenKind::LineComment:    return "LINE_COMMENT";
    case TokenKind::BlockComment:   return "BLOCK_COMMENT";
    case TokenKind::Whitespace:     return "WHITESPACE";
    case TokenKind::Invalid:        return "INVALID";
    case TokenKind::EndOfFile:      return "EOF";
    }
    return "UNKNOWN";
}

} // namespace bminor
