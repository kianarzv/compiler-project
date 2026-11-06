#ifndef BMINOR_PARSER_HPP
#define BMINOR_PARSER_HPP

#include "bminor/ast.hpp"
#include "bminor/diagnostic.hpp"

#include <initializer_list>
#include <memory>
#include <vector>

namespace bminor {

struct ParseResult {
    std::unique_ptr<Program> program;
    std::vector<Diagnostic> diagnostics;
    bool has_errors() const { return !diagnostics.empty(); }
};

// A recursive-descent parser over an EOF-terminated token vector. The vector
// must outlive the parser; returned nodes own their names, locations and children.
// Comments/whitespace are skipped here without changing the lexer's token stream.
class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    // Stops at the first syntax error and retains completed declarations.
    // Panic-mode recovery across syntax errors is scheduled for step 8.
    ParseResult parse();

private:
    const Token& current() const;
    bool check(TokenKind kind) const;
    Token advance();
    bool match(TokenKind kind);
    Token expect(TokenKind kind, const std::string& message);
    void skip_trivia();
    [[noreturn]] void fail(DiagnosticCode code, const std::string& message);
    void check_nesting();
    void check_expression_depth(const Expr& expression);

    std::unique_ptr<Decl> parse_declaration();
    std::unique_ptr<ParamDecl> parse_parameter();
    std::unique_ptr<TypeSyntax> parse_atomic_type(bool allow_void);
    std::unique_ptr<TypeSyntax> parse_value_type(bool allow_unsized);
    std::unique_ptr<Expr> parse_initializer();
    std::unique_ptr<Expr> parse_expression();
    std::unique_ptr<Expr> parse_assignment();
    std::unique_ptr<Expr> parse_logical_or();
    std::unique_ptr<Expr> parse_logical_and();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_relational();
    std::unique_ptr<Expr> parse_additive();
    std::unique_ptr<Expr> parse_multiplicative();
    std::unique_ptr<Expr> parse_power();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_postfix();
    std::unique_ptr<Expr> parse_primary();

    // Each left-associative level supplies its tighter operand routine and
    // operator set. The shared loop implements the same left fold for all levels.
    using OperandParser = std::unique_ptr<Expr> (Parser::*)();
    std::unique_ptr<Expr> parse_left_associative(
        OperandParser operand_parser, std::initializer_list<TokenKind> operators);

    const std::vector<Token>& tokens_;
    std::size_t position_ = 0;
    std::size_t nesting_ = 0;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace bminor

#endif
