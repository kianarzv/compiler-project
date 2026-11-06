#include "bminor/syntax_classifier.hpp"
#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"

#include <algorithm>
#include <utility>

namespace bminor {

const char* syntax_role_name(SyntaxRole role) {
    switch (role) {
    case SyntaxRole::Identifier:           return "IDENTIFIER";
    case SyntaxRole::VariableDeclaration:  return "VARIABLE_DECLARATION";
    case SyntaxRole::ParameterDeclaration: return "PARAMETER_DECLARATION";
    case SyntaxRole::VariableReference:    return "VARIABLE_REFERENCE";
    case SyntaxRole::FunctionDeclaration: return "FUNCTION_DECLARATION";
    case SyntaxRole::FunctionCall:         return "FUNCTION_CALL";
    case SyntaxRole::Keyword:              return "KEYWORD";
    case SyntaxRole::TypeName:             return "TYPE_NAME";
    case SyntaxRole::IntegerLiteral:       return "INTEGER_LITERAL";
    case SyntaxRole::BooleanLiteral:       return "BOOLEAN_LITERAL";
    case SyntaxRole::StringLiteral:        return "STRING_LITERAL";
    case SyntaxRole::CharacterLiteral:     return "CHARACTER_LITERAL";
    case SyntaxRole::Operator:             return "OPERATOR";
    case SyntaxRole::Delimiter:            return "DELIMITER";
    case SyntaxRole::Comment:              return "COMMENT";
    case SyntaxRole::Whitespace:           return "WHITESPACE";
    case SyntaxRole::Invalid:              return "INVALID";
    }
    return "UNKNOWN";
}

namespace {

SyntaxRole lexical_role(TokenKind kind) {
    switch (kind) {
    case TokenKind::Identifier: return SyntaxRole::Identifier;
    case TokenKind::KwArray: case TokenKind::KwBoolean: case TokenKind::KwChar:
    case TokenKind::KwInteger: case TokenKind::KwString: case TokenKind::KwVoid:
        return SyntaxRole::TypeName;
    case TokenKind::KwElse: case TokenKind::KwFor: case TokenKind::KwFunction:
    case TokenKind::KwIf: case TokenKind::KwPrint: case TokenKind::KwReturn:
    case TokenKind::KwWhile:
        return SyntaxRole::Keyword;
    case TokenKind::IntegerLiteral:   return SyntaxRole::IntegerLiteral;
    case TokenKind::BooleanLiteral:   return SyntaxRole::BooleanLiteral;
    case TokenKind::StringLiteral:    return SyntaxRole::StringLiteral;
    case TokenKind::CharacterLiteral: return SyntaxRole::CharacterLiteral;
    case TokenKind::LineComment: case TokenKind::BlockComment: return SyntaxRole::Comment;
    case TokenKind::Whitespace: return SyntaxRole::Whitespace;
    case TokenKind::Invalid:    return SyntaxRole::Invalid;
    case TokenKind::LeftParen: case TokenKind::RightParen: case TokenKind::LeftBrace:
    case TokenKind::RightBrace: case TokenKind::LeftBracket: case TokenKind::RightBracket:
    case TokenKind::Comma: case TokenKind::Semicolon: case TokenKind::Colon:
        return SyntaxRole::Delimiter;
    case TokenKind::Plus: case TokenKind::Minus: case TokenKind::Star:
    case TokenKind::Slash: case TokenKind::Percent: case TokenKind::Caret:
    case TokenKind::PlusPlus: case TokenKind::MinusMinus: case TokenKind::Bang:
    case TokenKind::Less: case TokenKind::LessEqual: case TokenKind::Greater:
    case TokenKind::GreaterEqual: case TokenKind::EqualEqual: case TokenKind::BangEqual:
    case TokenKind::AndAnd: case TokenKind::OrOr: case TokenKind::Assign:
        return SyntaxRole::Operator;
    case TokenKind::EndOfFile: return SyntaxRole::Whitespace; // EOF produces no span.
    }
    return SyntaxRole::Invalid;
}

bool is_identifier_role(SyntaxRole role) {
    return role == SyntaxRole::Identifier || role == SyntaxRole::VariableDeclaration
        || role == SyntaxRole::ParameterDeclaration || role == SyntaxRole::VariableReference
        || role == SyntaxRole::FunctionDeclaration || role == SyntaxRole::FunctionCall;
}

class AstClassifier {
public:
    explicit AstClassifier(std::vector<ClassifiedSpan>& spans) : spans_(spans) {}
    void visit(const AstNode& node);

private:
    void mark_name(const SourceLocation& location, std::size_t length, SyntaxRole role);
    void mark_error(const SourceLocation& location, std::size_t length);
    void mark_callee(const Expr& expression);
    std::vector<ClassifiedSpan>& spans_;
};

void AstClassifier::mark_name(const SourceLocation& location, std::size_t length, SyntaxRole role) {
    const auto found = std::lower_bound(spans_.begin(), spans_.end(), location.offset,
        [](const ClassifiedSpan& span, std::size_t offset) { return span.location.offset < offset; });
    // Only replace the exact identifier token named by the AST. Matching source
    // offsets rather than spellings handles repeated names and local shadowing.
    if (found != spans_.end() && found->location.offset == location.offset
        && found->location.filename == location.filename
        && found->length == length && is_identifier_role(found->role)) {
        found->role = role;
    }
}

void AstClassifier::mark_error(const SourceLocation& location, std::size_t length) {
    if (length == 0) { return; } // A missing child/brace has no source bytes to decorate.
    const std::size_t end = location.offset + length;
    auto found = std::lower_bound(spans_.begin(), spans_.end(), location.offset,
        [](const ClassifiedSpan& span, std::size_t offset) {
            return span.location.offset + span.length <= offset;
        });
    for (; found != spans_.end() && found->location.offset < end; ++found) {
        if (found->role != SyntaxRole::Whitespace && found->location.filename == location.filename) {
            found->has_error = true;
        }
    }
}

void AstClassifier::mark_callee(const Expr& expression) {
    // Parentheses do not change the named callee: (work)(x) still calls work.
    // For an index or call result, do not falsely label the array/factory result
    // as a directly named function. Its children keep their own visited roles.
    const Expr* callee = &expression;
    while (callee->kind == NodeKind::GroupExpr) {
        callee = static_cast<const GroupExpr*>(callee)->expression.get();
    }
    if (callee->kind == NodeKind::IdentifierExpr) {
        const auto& name = static_cast<const IdentifierExpr&>(*callee);
        mark_name(name.location, name.name.size(), SyntaxRole::FunctionCall);
    }
}

void AstClassifier::visit(const AstNode& node) {
    switch (node.kind) {
    case NodeKind::Program:
        for (const auto& declaration : static_cast<const Program&>(node).declarations) {
            visit(*declaration);
        }
        break;
    case NodeKind::VarDecl: {
        const auto& declaration = static_cast<const VarDecl&>(node);
        mark_name(declaration.name->location, declaration.name->name.size(), SyntaxRole::VariableDeclaration);
        visit(*declaration.declared_type);
        if (declaration.initializer) { visit(*declaration.initializer); }
        break;
    }
    case NodeKind::FuncDecl: {
        const auto& function = static_cast<const FuncDecl&>(node);
        mark_name(function.name->location, function.name->name.size(), SyntaxRole::FunctionDeclaration);
        visit(*function.return_type);
        for (const auto& parameter : function.parameters) { visit(*parameter); }
        if (function.body) { visit(*function.body); }
        break;
    }
    case NodeKind::ParamDecl: {
        const auto& parameter = static_cast<const ParamDecl&>(node);
        mark_name(parameter.name->location, parameter.name->name.size(), SyntaxRole::ParameterDeclaration);
        visit(*parameter.declared_type);
        break;
    }
    case NodeKind::TypeSyntax: {
        const auto& type = static_cast<const TypeSyntax&>(node);
        if (type.array_size) { visit(*type.array_size); }
        if (type.element_type) { visit(*type.element_type); }
        break;
    }
    case NodeKind::IdentifierExpr: {
        const auto& identifier = static_cast<const IdentifierExpr&>(node);
        mark_name(identifier.location, identifier.name.size(), SyntaxRole::VariableReference);
        break;
    }
    case NodeKind::UnaryExpr: visit(*static_cast<const UnaryExpr&>(node).operand); break;
    case NodeKind::BinaryExpr: {
        const auto& binary = static_cast<const BinaryExpr&>(node);
        visit(*binary.left);
        visit(*binary.right);
        break;
    }
    case NodeKind::GroupExpr: visit(*static_cast<const GroupExpr&>(node).expression); break;
    case NodeKind::CallExpr: {
        const auto& call = static_cast<const CallExpr&>(node);
        visit(*call.callee);
        for (const auto& argument : call.arguments) { visit(*argument); }
        // Apply this after visiting the callee's expression so a direct call
        // overrides the generic variable-reference role at that exact token.
        mark_callee(*call.callee);
        break;
    }
    case NodeKind::IndexExpr: {
        const auto& index = static_cast<const IndexExpr&>(node);
        visit(*index.base);
        visit(*index.index);
        break;
    }
    case NodeKind::PostfixExpr: visit(*static_cast<const PostfixExpr&>(node).operand); break;
    case NodeKind::ArrayInitExpr:
        for (const auto& element : static_cast<const ArrayInitExpr&>(node).elements) { visit(*element); }
        break;
    case NodeKind::BlockStmt:
        for (const auto& statement : static_cast<const BlockStmt&>(node).statements) { visit(*statement); }
        break;
    case NodeKind::DeclStmt: visit(*static_cast<const DeclStmt&>(node).declaration); break;
    case NodeKind::ExprStmt:
        if (static_cast<const ExprStmt&>(node).expression) {
            visit(*static_cast<const ExprStmt&>(node).expression);
        }
        break;
    case NodeKind::IfStmt: {
        const auto& branch = static_cast<const IfStmt&>(node);
        visit(*branch.condition);
        visit(*branch.then_branch);
        if (branch.else_branch) { visit(*branch.else_branch); }
        break;
    }
    case NodeKind::ForStmt: {
        const auto& loop = static_cast<const ForStmt&>(node);
        if (loop.initializer) { visit(*loop.initializer); }
        if (loop.condition) { visit(*loop.condition); }
        if (loop.update) { visit(*loop.update); }
        visit(*loop.body);
        break;
    }
    case NodeKind::ReturnStmt:
        if (static_cast<const ReturnStmt&>(node).value) { visit(*static_cast<const ReturnStmt&>(node).value); }
        break;
    case NodeKind::PrintStmt:
        for (const auto& value : static_cast<const PrintStmt&>(node).values) { visit(*value); }
        break;
    case NodeKind::ErrorDecl: {
        const auto& error = static_cast<const ErrorDecl&>(node);
        mark_error(error.location, error.length);
        break;
    }
    case NodeKind::ErrorStmt: {
        const auto& error = static_cast<const ErrorStmt&>(node);
        mark_error(error.location, error.length);
        break;
    }
    case NodeKind::Identifier:
    case NodeKind::LiteralExpr:
        break; // Parent declarations classify names; literals already have precise token roles.
    }
}

} // namespace

std::vector<ClassifiedSpan> classify_tokens(const std::vector<Token>& tokens, const Program* program) {
    std::vector<ClassifiedSpan> spans;
    spans.reserve(tokens.size());
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) { continue; }
        spans.push_back(ClassifiedSpan{token.location, token.lexeme.size(),
            lexical_role(token.kind), token.kind == TokenKind::Invalid});
    }
    if (program) { AstClassifier(spans).visit(*program); }
    return spans;
}

ClassificationResult classify_source(const SourceFile& source) {
    Lexer lexer(source);
    auto lexed = lexer.tokenize();
    if (lexed.has_errors()) {
        return ClassificationResult{classify_tokens(lexed.tokens), std::move(lexed.diagnostics)};
    }
    Parser parser(lexed.tokens);
    auto parsed = parser.parse();
    return ClassificationResult{classify_tokens(lexed.tokens, parsed.program.get()),
                                std::move(parsed.diagnostics)};
}

} // namespace bminor
