#ifndef BMINOR_AST_HPP
#define BMINOR_AST_HPP

#include "bminor/token.hpp"

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace bminor {

// Semantic analysis will define this type in a later phase. Syntax nodes keep
// a nullable annotation separately from the type spelling written in the source.
struct TypeInfo;

enum class NodeKind {
    Program, VarDecl, FuncDecl, ParamDecl, Identifier, TypeSyntax,
    LiteralExpr, ArrayInitExpr, IdentifierExpr, UnaryExpr, BinaryExpr,
    GroupExpr, CallExpr, IndexExpr, PostfixExpr, BlockStmt, DeclStmt,
    ExprStmt, IfStmt, ForStmt, ReturnStmt, PrintStmt, ErrorDecl, ErrorStmt
};

struct AstNode {
    AstNode(NodeKind kind, const SourceLocation& location);
    virtual ~AstNode() = default;

    const NodeKind kind;
    SourceLocation location;
    std::shared_ptr<const TypeInfo> semantic_type;
};

struct Identifier final : AstNode {
    explicit Identifier(const Token& token);
    std::string name;
};

struct Expr : AstNode {
    Expr(NodeKind kind, const SourceLocation& location);
    // Iterative parsing can still build deep left-associated or postfix trees.
    // Track height so the parser can bound traversal/destruction depth as well
    // as its own recursive call depth. A leaf has depth 1.
    std::size_t depth = 1;
};

struct IdentifierExpr final : Expr {
    explicit IdentifierExpr(const Token& token);
    std::string name;
};

struct UnaryExpr final : Expr {
    UnaryExpr(const Token& operation, std::unique_ptr<Expr> operand);
    Token operation;
    std::unique_ptr<Expr> operand;
};

struct BinaryExpr final : Expr {
    BinaryExpr(const Token& operation, std::unique_ptr<Expr> left,
               std::unique_ptr<Expr> right);
    // Assignment uses this node too. The exact operator token retains its own
    // location; the expression's first token is the start of its left operand.
    Token operation;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct GroupExpr final : Expr {
    GroupExpr(const Token& opening, std::unique_ptr<Expr> expression);
    // Keep an explicit group so its opening-parenthesis location does not
    // overwrite the original location of a name or literal inside it.
    Token opening;
    std::unique_ptr<Expr> expression;
};

struct CallExpr final : Expr {
    CallExpr(const Token& opening, std::unique_ptr<Expr> callee);
    Token opening;
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;
};

struct IndexExpr final : Expr {
    IndexExpr(const Token& opening, std::unique_ptr<Expr> base,
              std::unique_ptr<Expr> index);
    Token opening;
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> index;
};

struct PostfixExpr final : Expr {
    PostfixExpr(const Token& operation, std::unique_ptr<Expr> operand);
    Token operation;
    std::unique_ptr<Expr> operand;
};

struct LiteralExpr final : Expr {
    explicit LiteralExpr(const Token& token);
    // Keep the original spelling, including quotes, leading zeros and escapes.
    // Parsing an expression does not perform value conversion or type checking.
    Token literal;
};

struct ArrayInitExpr final : Expr {
    explicit ArrayInitExpr(const SourceLocation& location);
    std::vector<std::unique_ptr<Expr>> elements;
};

enum class TypeKind { Integer, Boolean, Character, String, Void, Array };

struct TypeSyntax final : AstNode {
    TypeSyntax(TypeKind type_kind, const SourceLocation& location);
    TypeKind type_kind;
    // Array types own their element type and optional bound expression. Only
    // parameter arrays may omit a bound. Atomic types leave both fields null.
    std::unique_ptr<TypeSyntax> element_type;
    std::unique_ptr<Expr> array_size;
};

struct Decl : AstNode {
    Decl(NodeKind kind, const SourceLocation& location);
};

struct VarDecl final : Decl {
    explicit VarDecl(const SourceLocation& location);
    std::unique_ptr<Identifier> name;
    std::unique_ptr<TypeSyntax> declared_type;
    std::unique_ptr<Expr> initializer;
};

// Recovery keeps an explicit source span instead of pretending discarded input
// was a successfully parsed declaration. Its diagnostic is in ParseResult.
struct ErrorDecl final : Decl {
    ErrorDecl(const SourceLocation& location, std::size_t length);
    std::size_t length;
};

struct ParamDecl final : AstNode {
    explicit ParamDecl(const SourceLocation& location);
    std::unique_ptr<Identifier> name;
    std::unique_ptr<TypeSyntax> declared_type;
};

struct Stmt : AstNode {
    Stmt(NodeKind kind, const SourceLocation& location);
};

struct BlockStmt final : Stmt {
    explicit BlockStmt(const SourceLocation& location);
    std::vector<std::unique_ptr<Stmt>> statements;
    // EOF can end a recovered block without a real closing brace. Keep that
    // distinction visible; no closing token is fabricated during recovery.
    bool incomplete = false;
};

struct ErrorStmt final : Stmt {
    ErrorStmt(const SourceLocation& location, std::size_t length);
    // Zero is possible for a missing branch/body at its owner's else or '}'.
    std::size_t length;
};

// A declaration remains a declaration node; this wrapper places it in a
// statement list without giving VarDecl two different AST base classes.
struct DeclStmt final : Stmt {
    explicit DeclStmt(const SourceLocation& location);
    std::unique_ptr<VarDecl> declaration;
};

struct ExprStmt final : Stmt {
    explicit ExprStmt(const SourceLocation& location);
    // Null represents a source semicolon with no expression, not a missing node.
    std::unique_ptr<Expr> expression;
};

struct IfStmt final : Stmt {
    explicit IfStmt(const SourceLocation& location);
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> then_branch;
    std::unique_ptr<Stmt> else_branch;
};

struct ForStmt final : Stmt {
    explicit ForStmt(const SourceLocation& location);
    // Preserve omitted header expressions as null. Later analysis can interpret
    // an omitted condition without pretending the source contained "true".
    std::unique_ptr<Expr> initializer;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> update;
    std::unique_ptr<Stmt> body;
};

struct ReturnStmt final : Stmt {
    explicit ReturnStmt(const SourceLocation& location);
    std::unique_ptr<Expr> value;
};

struct PrintStmt final : Stmt {
    explicit PrintStmt(const SourceLocation& location);
    std::vector<std::unique_ptr<Expr>> values;
};

struct FuncDecl final : Decl {
    explicit FuncDecl(const SourceLocation& location);
    std::unique_ptr<Identifier> name;
    std::unique_ptr<TypeSyntax> return_type;
    std::vector<std::unique_ptr<ParamDecl>> parameters;
    // A null body is a prototype; a non-null body is a parsed definition.
    std::unique_ptr<BlockStmt> body;
};

struct Program final : AstNode {
    explicit Program(const SourceLocation& location);
    std::vector<std::unique_ptr<Decl>> declarations;
};

// AST output is a separate traversal, not a responsibility of the parser.
void dump_ast(std::ostream& out, const Program& program);

} // namespace bminor

#endif
