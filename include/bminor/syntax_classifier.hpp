#ifndef BMINOR_SYNTAX_CLASSIFIER_HPP
#define BMINOR_SYNTAX_CLASSIFIER_HPP

#include "bminor/ast.hpp"
#include "bminor/diagnostic.hpp"
#include "bminor/source_file.hpp"

#include <cstddef>
#include <vector>

namespace bminor {

// Roles describe source syntax, not resolved names/types. A call's identifier
// is a FunctionCall even if semantic analysis later finds it is not callable.
enum class SyntaxRole {
    Identifier, VariableDeclaration, ParameterDeclaration, VariableReference,
    FunctionDeclaration, FunctionCall, Keyword, TypeName, IntegerLiteral,
    BooleanLiteral, StringLiteral, CharacterLiteral, Operator, Delimiter,
    Comment, Whitespace, Invalid
};

struct ClassifiedSpan {
    SourceLocation location;
    std::size_t length;
    SyntaxRole role;
    // Errors are separate from the base category: a discarded string/comment
    // keeps its lexical identity while a renderer can add an error decoration.
    bool has_error;
};

struct ClassificationResult {
    std::vector<ClassifiedSpan> spans;
    std::vector<Diagnostic> diagnostics;
    bool has_errors() const { return !diagnostics.empty(); }
};

const char* syntax_role_name(SyntaxRole role);

// Produce one span per non-EOF token, including whitespace. With a matching
// AST, refine identifier roles and mark discarded regions. A null AST supplies
// lexical categories only, keeping uncertain names as Identifier.
std::vector<ClassifiedSpan> classify_tokens(const std::vector<Token>& tokens,
                                           const Program* program = nullptr);

// Run the existing front end. Lexical failures retain complete fallback spans;
// clean lexing permits the recovered AST to refine them even after syntax errors.
ClassificationResult classify_source(const SourceFile& source);

} // namespace bminor

#endif
