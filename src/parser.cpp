#include "bminor/parser.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace bminor {
namespace {

// A private control-flow signal unwinds a failed declaration. It is always
// caught by parse(); malformed source is returned as diagnostics to the caller.
struct ParseFailure {};

const std::size_t max_parser_depth = 128;

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
    // Bound recursive types, initializers and expression edges before exhausting
    // stack. The guard restores the counter even while a parse failure unwinds.
    if (nesting_ >= max_parser_depth) {
        fail(DiagnosticCode::ParserNestingLimit, "Parser nesting exceeds the limit of 128.");
    }
}

void Parser::check_expression_depth(const Expr& expression) {
    // A long iterative chain does not recurse while parsing, but the resulting
    // tree would recurse during printing/destruction. Bound that height too.
    if (expression.depth > max_parser_depth) {
        fail(DiagnosticCode::ParserNestingLimit, "Expression tree depth exceeds the limit of 128.");
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
        type->array_size = parse_expression();
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
    if (check(TokenKind::LeftBrace)) {
        const Token brace = advance();
        std::unique_ptr<ArrayInitExpr> array(new ArrayInitExpr(brace.location));
        if (!check(TokenKind::RightBrace)) {
            do {
                auto element = parse_initializer();
                array->depth = std::max(array->depth, 1 + element->depth);
                array->elements.push_back(std::move(element));
                check_expression_depth(*array);
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::RightBrace, "Expected '}' after the array initializer.");
        return std::unique_ptr<Expr>(array.release());
    }
    return parse_expression();
}

std::unique_ptr<Expr> Parser::parse_expression() {
    return parse_assignment();
}

std::unique_ptr<Expr> Parser::parse_assignment() {
    auto left = parse_logical_or();
    if (!check(TokenKind::Assign)) { return left; }
    const Token operation = advance();
    check_nesting();
    NestingGuard guard(nesting_);
    // Recurse at the same level on the right: a=b=c means a=(b=c).
    auto right = parse_assignment();
    std::unique_ptr<Expr> expression(new BinaryExpr(operation, std::move(left), std::move(right)));
    check_expression_depth(*expression);
    return expression;
}

std::unique_ptr<Expr> Parser::parse_left_associative(
    OperandParser operand_parser, std::initializer_list<TokenKind> operators) {
    auto left = (this->*operand_parser)();
    while (std::find(operators.begin(), operators.end(), current().kind) != operators.end()) {
        const Token operation = advance();
        auto right = (this->*operand_parser)();
        // Reuse the accumulated tree as the LEFT child: a-b-c means (a-b)-c.
        left = std::unique_ptr<Expr>(new BinaryExpr(operation, std::move(left), std::move(right)));
        check_expression_depth(*left);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parse_logical_or() {
    return parse_left_associative(&Parser::parse_logical_and, {TokenKind::OrOr});
}

std::unique_ptr<Expr> Parser::parse_logical_and() {
    return parse_left_associative(&Parser::parse_equality, {TokenKind::AndAnd});
}

std::unique_ptr<Expr> Parser::parse_equality() {
    return parse_left_associative(&Parser::parse_relational,
                                 {TokenKind::EqualEqual, TokenKind::BangEqual});
}

std::unique_ptr<Expr> Parser::parse_relational() {
    return parse_left_associative(&Parser::parse_additive,
                                 {TokenKind::Less, TokenKind::LessEqual,
                                  TokenKind::Greater, TokenKind::GreaterEqual});
}

std::unique_ptr<Expr> Parser::parse_additive() {
    return parse_left_associative(&Parser::parse_multiplicative,
                                 {TokenKind::Plus, TokenKind::Minus});
}

std::unique_ptr<Expr> Parser::parse_multiplicative() {
    return parse_left_associative(&Parser::parse_power,
                                 {TokenKind::Star, TokenKind::Slash, TokenKind::Percent});
}

std::unique_ptr<Expr> Parser::parse_power() {
    auto left = parse_unary();
    if (!check(TokenKind::Caret)) { return left; }
    const Token operation = advance();
    check_nesting();
    NestingGuard guard(nesting_);
    // Power is right-associated, and unary binds more tightly in our grammar.
    auto right = parse_power();
    std::unique_ptr<Expr> expression(new BinaryExpr(operation, std::move(left), std::move(right)));
    check_expression_depth(*expression);
    return expression;
}

std::unique_ptr<Expr> Parser::parse_unary() {
    if (check(TokenKind::Minus) || check(TokenKind::Bang)) {
        const Token operation = advance();
        check_nesting();
        NestingGuard guard(nesting_);
        auto operand = parse_unary();
        std::unique_ptr<Expr> expression(new UnaryExpr(operation, std::move(operand)));
        check_expression_depth(*expression);
        return expression;
    }
    return parse_postfix();
}

std::unique_ptr<Expr> Parser::parse_postfix() {
    auto expression = parse_primary();
    while (true) {
        if (check(TokenKind::LeftParen)) {
            const Token opening = advance();
            check_nesting();
            NestingGuard guard(nesting_);
            std::unique_ptr<CallExpr> call(new CallExpr(opening, std::move(expression)));
            if (!check(TokenKind::RightParen)) {
                do {
                    auto argument = parse_expression();
                    call->depth = std::max(call->depth, 1 + argument->depth);
                    call->arguments.push_back(std::move(argument));
                    check_expression_depth(*call);
                } while (match(TokenKind::Comma));
            }
            expect(TokenKind::RightParen, "Expected ')' after the call arguments.");
            check_expression_depth(*call);
            expression = std::move(call);
        } else if (check(TokenKind::LeftBracket)) {
            const Token opening = advance();
            check_nesting();
            NestingGuard guard(nesting_);
            auto index = parse_expression();
            expect(TokenKind::RightBracket, "Expected ']' after the index expression.");
            expression = std::unique_ptr<Expr>(new IndexExpr(opening, std::move(expression), std::move(index)));
            check_expression_depth(*expression);
        } else if (check(TokenKind::PlusPlus) || check(TokenKind::MinusMinus)) {
            const Token operation = advance();
            expression = std::unique_ptr<Expr>(new PostfixExpr(operation, std::move(expression)));
            check_expression_depth(*expression);
        } else {
            break;
        }
    }
    return expression;
}

std::unique_ptr<Expr> Parser::parse_primary() {
    if (is_literal(current().kind)) {
        return std::unique_ptr<Expr>(new LiteralExpr(advance()));
    }
    if (check(TokenKind::Identifier)) {
        return std::unique_ptr<Expr>(new IdentifierExpr(advance()));
    }
    if (check(TokenKind::LeftParen)) {
        const Token opening = advance();
        check_nesting();
        NestingGuard guard(nesting_);
        auto inner = parse_expression();
        expect(TokenKind::RightParen, "Expected ')' after the grouped expression.");
        std::unique_ptr<Expr> expression(new GroupExpr(opening, std::move(inner)));
        check_expression_depth(*expression);
        return expression;
    }
    fail(DiagnosticCode::UnexpectedToken, "Expected an expression.");
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
