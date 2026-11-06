#ifndef BMINOR_PARSER_HPP
#define BMINOR_PARSER_HPP

#include "bminor/ast.hpp"
#include "bminor/diagnostic.hpp"

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
    // Step 5 stops at the first syntax error and retains completed declarations.
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

    std::unique_ptr<Decl> parse_declaration();
    std::unique_ptr<ParamDecl> parse_parameter();
    std::unique_ptr<TypeSyntax> parse_atomic_type(bool allow_void);
    std::unique_ptr<TypeSyntax> parse_value_type(bool allow_unsized);
    std::unique_ptr<Expr> parse_initializer();

    const std::vector<Token>& tokens_;
    std::size_t position_ = 0;
    std::size_t nesting_ = 0;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace bminor

#endif
