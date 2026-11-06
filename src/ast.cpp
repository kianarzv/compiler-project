#include "bminor/ast.hpp"
#include "bminor/text_output.hpp"

#include <algorithm>
#include <ostream>
#include <utility>

namespace bminor {

AstNode::AstNode(NodeKind kind, const SourceLocation& location)
    : kind(kind), location(location) {}
Identifier::Identifier(const Token& token)
    : AstNode(NodeKind::Identifier, token.location), name(token.lexeme) {}
Expr::Expr(NodeKind kind, const SourceLocation& location) : AstNode(kind, location) {}
IdentifierExpr::IdentifierExpr(const Token& token)
    : Expr(NodeKind::IdentifierExpr, token.location), name(token.lexeme) {}
UnaryExpr::UnaryExpr(const Token& operation, std::unique_ptr<Expr> operand)
    : Expr(NodeKind::UnaryExpr, operation.location), operation(operation),
      operand(std::move(operand)) {
    depth = 1 + this->operand->depth;
}
BinaryExpr::BinaryExpr(const Token& operation, std::unique_ptr<Expr> left,
                       std::unique_ptr<Expr> right)
    : Expr(NodeKind::BinaryExpr, left->location), operation(operation),
      left(std::move(left)), right(std::move(right)) {
    depth = 1 + std::max(this->left->depth, this->right->depth);
}
GroupExpr::GroupExpr(const Token& opening, std::unique_ptr<Expr> expression)
    : Expr(NodeKind::GroupExpr, opening.location), opening(opening),
      expression(std::move(expression)) {
    depth = 1 + this->expression->depth;
}
CallExpr::CallExpr(const Token& opening, std::unique_ptr<Expr> callee)
    : Expr(NodeKind::CallExpr, callee->location), opening(opening),
      callee(std::move(callee)) {
    depth = 1 + this->callee->depth;
}
IndexExpr::IndexExpr(const Token& opening, std::unique_ptr<Expr> base,
                     std::unique_ptr<Expr> index)
    : Expr(NodeKind::IndexExpr, base->location), opening(opening),
      base(std::move(base)), index(std::move(index)) {
    depth = 1 + std::max(this->base->depth, this->index->depth);
}
PostfixExpr::PostfixExpr(const Token& operation, std::unique_ptr<Expr> operand)
    : Expr(NodeKind::PostfixExpr, operand->location), operation(operation),
      operand(std::move(operand)) {
    depth = 1 + this->operand->depth;
}
LiteralExpr::LiteralExpr(const Token& token)
    : Expr(NodeKind::LiteralExpr, token.location), literal(token) {}
ArrayInitExpr::ArrayInitExpr(const SourceLocation& location)
    : Expr(NodeKind::ArrayInitExpr, location) {}
TypeSyntax::TypeSyntax(TypeKind type_kind, const SourceLocation& location)
    : AstNode(NodeKind::TypeSyntax, location), type_kind(type_kind) {}
Decl::Decl(NodeKind kind, const SourceLocation& location) : AstNode(kind, location) {}
VarDecl::VarDecl(const SourceLocation& location) : Decl(NodeKind::VarDecl, location) {}
ParamDecl::ParamDecl(const SourceLocation& location) : AstNode(NodeKind::ParamDecl, location) {}
FuncDecl::FuncDecl(const SourceLocation& location) : Decl(NodeKind::FuncDecl, location) {}
Program::Program(const SourceLocation& location) : AstNode(NodeKind::Program, location) {}

namespace {

const char* type_name(TypeKind kind) {
    switch (kind) {
    case TypeKind::Integer:   return "integer";
    case TypeKind::Boolean:   return "boolean";
    case TypeKind::Character: return "char";
    case TypeKind::String:    return "string";
    case TypeKind::Void:      return "void";
    case TypeKind::Array:     return "array";
    }
    return "unknown";
}

void dump_node(std::ostream& out, const AstNode& node, std::size_t depth) {
    out << std::string(depth * 2, ' ');
    // Labels are followed by the same metadata for every kind of node. The
    // semantic annotation is initially null; a declared Type node is syntax.
    switch (node.kind) {
    case NodeKind::Program:       out << "Program"; break;
    case NodeKind::VarDecl:       out << "VarDecl"; break;
    case NodeKind::FuncDecl:      out << "FuncDecl prototype"; break;
    case NodeKind::ParamDecl:     out << "ParamDecl"; break;
    case NodeKind::Identifier:
        out << "Identifier ";
        write_escaped(out, static_cast<const Identifier&>(node).name);
        break;
    case NodeKind::TypeSyntax: {
        const auto& type = static_cast<const TypeSyntax&>(node);
        out << "Type " << type_name(type.type_kind);
        if (type.type_kind == TypeKind::Array && !type.array_size) {
            out << " unsized";
        }
        break;
    }
    case NodeKind::LiteralExpr: {
        const auto& literal = static_cast<const LiteralExpr&>(node).literal;
        out << "Literal " << token_kind_name(literal.kind) << ' ';
        write_escaped(out, literal.lexeme);
        break;
    }
    case NodeKind::ArrayInitExpr: out << "ArrayInit"; break;
    case NodeKind::IdentifierExpr:
        out << "IdentifierExpr ";
        write_escaped(out, static_cast<const IdentifierExpr&>(node).name);
        break;
    case NodeKind::UnaryExpr:
        out << "UnaryExpr ";
        write_escaped(out, static_cast<const UnaryExpr&>(node).operation.lexeme);
        break;
    case NodeKind::BinaryExpr:
        out << "BinaryExpr ";
        write_escaped(out, static_cast<const BinaryExpr&>(node).operation.lexeme);
        break;
    case NodeKind::PostfixExpr:
        out << "PostfixExpr ";
        write_escaped(out, static_cast<const PostfixExpr&>(node).operation.lexeme);
        break;
    case NodeKind::GroupExpr: out << "GroupExpr"; break;
    case NodeKind::CallExpr:  out << "CallExpr"; break;
    case NodeKind::IndexExpr: out << "IndexExpr"; break;
    }
    out << " @" << node.location.filename << ':' << node.location.line
        << ':' << node.location.column
        << " type=" << (node.semantic_type ? "resolved" : "null") << '\n';

    // Each child has exactly one owner. Traversal follows structural children,
    // never the semantic annotation, which may later refer to shared type data.
    switch (node.kind) {
    case NodeKind::Program:
        for (const auto& declaration : static_cast<const Program&>(node).declarations) {
            dump_node(out, *declaration, depth + 1);
        }
        break;
    case NodeKind::VarDecl: {
        const auto& declaration = static_cast<const VarDecl&>(node);
        dump_node(out, *declaration.name, depth + 1);
        dump_node(out, *declaration.declared_type, depth + 1);
        if (declaration.initializer) {
            dump_node(out, *declaration.initializer, depth + 1);
        }
        break;
    }
    case NodeKind::FuncDecl: {
        const auto& function = static_cast<const FuncDecl&>(node);
        dump_node(out, *function.name, depth + 1);
        dump_node(out, *function.return_type, depth + 1);
        for (const auto& parameter : function.parameters) {
            dump_node(out, *parameter, depth + 1);
        }
        break;
    }
    case NodeKind::ParamDecl: {
        const auto& parameter = static_cast<const ParamDecl&>(node);
        dump_node(out, *parameter.name, depth + 1);
        dump_node(out, *parameter.declared_type, depth + 1);
        break;
    }
    case NodeKind::TypeSyntax: {
        const auto& type = static_cast<const TypeSyntax&>(node);
        if (type.array_size) {
            dump_node(out, *type.array_size, depth + 1);
        }
        if (type.element_type) {
            dump_node(out, *type.element_type, depth + 1);
        }
        break;
    }
    case NodeKind::ArrayInitExpr:
        for (const auto& element : static_cast<const ArrayInitExpr&>(node).elements) {
            dump_node(out, *element, depth + 1);
        }
        break;
    case NodeKind::Identifier:
    case NodeKind::LiteralExpr:
    case NodeKind::IdentifierExpr:
        break;
    case NodeKind::UnaryExpr:
        dump_node(out, *static_cast<const UnaryExpr&>(node).operand, depth + 1);
        break;
    case NodeKind::BinaryExpr: {
        const auto& binary = static_cast<const BinaryExpr&>(node);
        dump_node(out, *binary.left, depth + 1);
        dump_node(out, *binary.right, depth + 1);
        break;
    }
    case NodeKind::PostfixExpr:
        dump_node(out, *static_cast<const PostfixExpr&>(node).operand, depth + 1);
        break;
    case NodeKind::GroupExpr:
        dump_node(out, *static_cast<const GroupExpr&>(node).expression, depth + 1);
        break;
    case NodeKind::CallExpr: {
        const auto& call = static_cast<const CallExpr&>(node);
        dump_node(out, *call.callee, depth + 1);
        for (const auto& argument : call.arguments) {
            dump_node(out, *argument, depth + 1);
        }
        break;
    }
    case NodeKind::IndexExpr: {
        const auto& index = static_cast<const IndexExpr&>(node);
        dump_node(out, *index.base, depth + 1);
        dump_node(out, *index.index, depth + 1);
        break;
    }
    }
}

} // namespace

void dump_ast(std::ostream& out, const Program& program) {
    dump_node(out, program, 0);
}

} // namespace bminor
