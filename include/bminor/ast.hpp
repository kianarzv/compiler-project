#ifndef BMINOR_AST_HPP
#define BMINOR_AST_HPP

#include "bminor/token.hpp"

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
    LiteralExpr, ArrayInitExpr
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

struct ParamDecl final : AstNode {
    explicit ParamDecl(const SourceLocation& location);
    std::unique_ptr<Identifier> name;
    std::unique_ptr<TypeSyntax> declared_type;
};

struct FuncDecl final : Decl {
    explicit FuncDecl(const SourceLocation& location);
    std::unique_ptr<Identifier> name;
    std::unique_ptr<TypeSyntax> return_type;
    std::vector<std::unique_ptr<ParamDecl>> parameters;
    // Step 5 creates prototypes. A body field and statement nodes arrive in
    // step 7; definitions are explicitly rejected until they can be parsed.
};

struct Program final : AstNode {
    explicit Program(const SourceLocation& location);
    std::vector<std::unique_ptr<Decl>> declarations;
};

// AST output is a separate traversal, not a responsibility of the parser.
void dump_ast(std::ostream& out, const Program& program);

} // namespace bminor

#endif
