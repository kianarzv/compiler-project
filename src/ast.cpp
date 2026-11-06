#include "bminor/ast.hpp"
#include "bminor/text_output.hpp"

#include <ostream>

namespace bminor {

AstNode::AstNode(NodeKind kind, const SourceLocation& location)
    : kind(kind), location(location) {}
Identifier::Identifier(const Token& token)
    : AstNode(NodeKind::Identifier, token.location), name(token.lexeme) {}
Expr::Expr(NodeKind kind, const SourceLocation& location) : AstNode(kind, location) {}
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
        break;
    }
}

} // namespace

void dump_ast(std::ostream& out, const Program& program) {
    dump_node(out, program, 0);
}

} // namespace bminor
