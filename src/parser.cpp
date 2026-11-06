#include "bminor/parser.hpp"

#include <stdexcept>
#include <utility>

namespace bminor {
namespace {

// A private control-flow signal unwinds a failed declaration. It is always
// caught by parse(); malformed source is returned as diagnostics to the caller.
struct ParseFailure {};

class NestingGuard {
public:
    explicit NestingGuard(std::size_t& depth) : depth_(depth) { ++depth_; }
    ~NestingGuard() { --depth_; }
private:
    std::size_t& depth_;
};

bool is_literal(TokenKind kind) {
    return kind == TokenKind::IntegerLiteral || kind == TokenKind::BooleanLiteral
        || kind == TokenKind::StringLiteral || kind == TokenKind::CharacterLiteral;
}

bool starts_expression(TokenKind kind) {
    return is_literal(kind) || kind == TokenKind::Identifier
        || kind == TokenKind::LeftParen || kind == TokenKind::Minus
        || kind == TokenKind::Bang;
}

bool continues_expression(TokenKind kind) {
    switch (kind) {
    case TokenKind::Plus: case TokenKind::Minus: case TokenKind::Star:
    case TokenKind::Slash: case TokenKind::Percent: case TokenKind::Caret:
    case TokenKind::PlusPlus: case TokenKind::MinusMinus: case TokenKind::Less:
    case TokenKind::LessEqual: case TokenKind::Greater: case TokenKind::GreaterEqual:
    case TokenKind::EqualEqual: case TokenKind::BangEqual: case TokenKind::AndAnd:
    case TokenKind::OrOr: case TokenKind::Assign: case TokenKind::LeftParen:
    case TokenKind::LeftBracket:
        return true;
    default:
        return false;
    }
}

} // namespace

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {
    if (tokens.empty() || tokens.back().kind != TokenKind::EndOfFile) {
        throw std::invalid_argument("Parser requires an EOF-terminated token vector.");
    }
}

ParseResult Parser::parse() {
    position_ = 0;
    nesting_ = 0;
    diagnostics_.clear();
    skip_trivia();
    std::unique_ptr<Program> program(new Program(current().location));
    try {
        while (!check(TokenKind::EndOfFile)) {
            program->declarations.push_back(parse_declaration());
        }
    } catch (const ParseFailure&) {
        // The incomplete declaration was locally owned and is destroyed here.
        // Completed declarations remain available without pretending the whole
        // input was accepted. Recovery to later declarations is added in step 8.
    }
    return ParseResult{std::move(program), std::move(diagnostics_)};
}

const Token& Parser::current() const { return tokens_[position_]; }
bool Parser::check(TokenKind kind) const { return current().kind == kind; }

void Parser::skip_trivia() {
    while (check(TokenKind::Whitespace) || check(TokenKind::LineComment)
           || check(TokenKind::BlockComment)) {
        ++position_;
    }
}

Token Parser::advance() {
    const Token token = current();
    if (!check(TokenKind::EndOfFile)) {
        ++position_;
        skip_trivia();
    }
    return token;
}

bool Parser::match(TokenKind kind) {
    if (!check(kind)) { return false; }
    advance();
    return true;
}

Token Parser::expect(TokenKind kind, const std::string& message) {
    if (!check(kind)) { fail(DiagnosticCode::UnexpectedToken, message); }
    return advance();
}

void Parser::fail(DiagnosticCode code, const std::string& message) {
    const Token& token = current();
    diagnostics_.push_back(Diagnostic{code, token.location, token.lexeme.size(), message});
    throw ParseFailure{};
}

void Parser::check_nesting() {
    // Bound recursive array types/initializers before they exhaust the process
    // stack. The guard restores the counter even while a parse failure unwinds.
    if (nesting_ >= 128) {
        fail(DiagnosticCode::ParserNestingLimit, "Parser nesting exceeds the limit of 128.");
    }
}

std::unique_ptr<TypeSyntax> Parser::parse_atomic_type(bool allow_void) {
    const Token token = current();
    TypeKind kind;
    switch (token.kind) {
    case TokenKind::KwInteger: kind = TypeKind::Integer; break;
    case TokenKind::KwBoolean: kind = TypeKind::Boolean; break;
    case TokenKind::KwChar:    kind = TypeKind::Character; break;
    case TokenKind::KwString:  kind = TypeKind::String; break;
    case TokenKind::KwVoid:
        if (!allow_void) {
            fail(DiagnosticCode::UnexpectedToken, "'void' is only allowed as a return type.");
        }
        kind = TypeKind::Void;
        break;
    default:
        fail(DiagnosticCode::UnexpectedToken, "Expected an atomic type.");
    }
    advance();
    return std::unique_ptr<TypeSyntax>(new TypeSyntax(kind, token.location));
}

std::unique_ptr<TypeSyntax> Parser::parse_value_type(bool allow_unsized) {
    check_nesting();
    NestingGuard guard(nesting_);
    if (!check(TokenKind::KwArray)) { return parse_atomic_type(false); }

    const Token array = advance();
    std::unique_ptr<TypeSyntax> type(new TypeSyntax(TypeKind::Array, array.location));
    expect(TokenKind::LeftBracket, "Expected '[' after 'array'.");
    if (check(TokenKind::RightBracket)) {
        if (!allow_unsized) {
            fail(DiagnosticCode::UnexpectedToken, "An array variable requires a size.");
        }
    } else {
        if (!check(TokenKind::IntegerLiteral)) {
            if (starts_expression(current().kind)) {
                fail(DiagnosticCode::UnsupportedSyntax,
                     "Array-bound expressions arrive in step 6; use an integer literal for now.");
            }
            fail(DiagnosticCode::UnexpectedToken, "Expected an integer literal as the array size.");
        }
        type->array_size.reset(new LiteralExpr(advance()));
        if (continues_expression(current().kind)) {
            fail(DiagnosticCode::UnsupportedSyntax,
                 "Array-bound expressions arrive in step 6; use an integer literal for now.");
        }
    }
    expect(TokenKind::RightBracket, "Expected ']' after the array size.");
    type->element_type = parse_value_type(allow_unsized);
    return type;
}

std::unique_ptr<ParamDecl> Parser::parse_parameter() {
    const Token name = expect(TokenKind::Identifier, "Expected a parameter name.");
    std::unique_ptr<ParamDecl> parameter(new ParamDecl(name.location));
    parameter->name.reset(new Identifier(name));
    expect(TokenKind::Colon, "Expected ':' after the parameter name.");
    parameter->declared_type = parse_value_type(true);
    return parameter;
}

std::unique_ptr<Expr> Parser::parse_initializer() {
    check_nesting();
    NestingGuard guard(nesting_);
    if (is_literal(current().kind)) {
        std::unique_ptr<Expr> literal(new LiteralExpr(advance()));
        if (continues_expression(current().kind)) {
            fail(DiagnosticCode::UnsupportedSyntax,
                 "Expression parsing arrives in step 6; only literal initializers are supported now.");
        }
        return literal;
    }
    if (check(TokenKind::LeftBrace)) {
        const Token brace = advance();
        std::unique_ptr<ArrayInitExpr> array(new ArrayInitExpr(brace.location));
        if (!check(TokenKind::RightBrace)) {
            do {
                array->elements.push_back(parse_initializer());
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::RightBrace, "Expected '}' after the array initializer.");
        return std::unique_ptr<Expr>(array.release());
    }
    if (starts_expression(current().kind)) {
        fail(DiagnosticCode::UnsupportedSyntax,
             "Expression parsing arrives in step 6; only literal initializers are supported now.");
    }
    fail(DiagnosticCode::UnexpectedToken, "Expected a literal or array initializer.");
}

std::unique_ptr<Decl> Parser::parse_declaration() {
    const Token name = expect(TokenKind::Identifier, "Expected a declaration name.");
    expect(TokenKind::Colon, "Expected ':' after the declaration name.");
    if (match(TokenKind::KwFunction)) {
        std::unique_ptr<FuncDecl> function(new FuncDecl(name.location));
        function->name.reset(new Identifier(name));
        function->return_type = parse_atomic_type(true);
        expect(TokenKind::LeftParen, "Expected '(' before the parameter list.");
        if (!check(TokenKind::RightParen)) {
            do {
                function->parameters.push_back(parse_parameter());
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::RightParen, "Expected ')' after the parameter list.");
        if (check(TokenKind::Assign)) {
            fail(DiagnosticCode::UnsupportedSyntax, "Function bodies arrive in step 7; use a prototype for now.");
        }
        expect(TokenKind::Semicolon, "Expected ';' after the function prototype.");
        return std::unique_ptr<Decl>(function.release());
    }

    std::unique_ptr<VarDecl> variable(new VarDecl(name.location));
    variable->name.reset(new Identifier(name));
    variable->declared_type = parse_value_type(false);
    if (match(TokenKind::Assign)) { variable->initializer = parse_initializer(); }
    expect(TokenKind::Semicolon, "Expected ';' after the variable declaration.");
    return std::unique_ptr<Decl>(variable.release());
}

} // namespace bminor
