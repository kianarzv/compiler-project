#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using bminor::DiagnosticCode;
using bminor::NodeKind;
using bminor::ParseResult;
using bminor::TokenKind;
using bminor::TypeKind;

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

ParseResult parse(const std::string& text) {
    const bminor::SourceFile source{"parser.bminor", text};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    require(!lexed.has_errors(), "Parser test contains a lexical error.");
    bminor::Parser parser(lexed.tokens);
    // Returning after the source, lexer and tokens die also exercises AST ownership.
    return parser.parse();
}

const bminor::VarDecl& variable(const ParseResult& result, std::size_t index) {
    require(index < result.program->declarations.size(), "Missing variable declaration.");
    const auto& node = *result.program->declarations[index];
    require(node.kind == NodeKind::VarDecl, "Expected a variable declaration node.");
    return static_cast<const bminor::VarDecl&>(node);
}

const bminor::LiteralExpr& literal(const bminor::Expr& expression) {
    require(expression.kind == NodeKind::LiteralExpr, "Expected a literal node.");
    return static_cast<const bminor::LiteralExpr&>(expression);
}

void atomic_declarations() {
    const auto result = parse("i: integer; b: boolean; c: char; s: string;");
    require(!result.has_errors() && result.program->declarations.size() == 4,
            "Atomic declarations were rejected.");
    const TypeKind expected[] = {TypeKind::Integer, TypeKind::Boolean,
                                 TypeKind::Character, TypeKind::String};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto& declaration = variable(result, i);
        require(declaration.declared_type->type_kind == expected[i]
                    && !declaration.initializer, "Wrong declared type or default initializer.");
        require(!declaration.semantic_type && !declaration.name->semantic_type
                    && !declaration.declared_type->semantic_type,
                "Parsing assigned a semantic type prematurely.");
    }
}

void literal_initializers() {
    const auto result = parse(R"bminor(i: integer = 007; b: boolean = false;
c: char = '\n'; s: string = "say \"hi\"";)bminor");
    require(!result.has_errors(), "Literal initializers were rejected.");
    const TokenKind kinds[] = {TokenKind::IntegerLiteral, TokenKind::BooleanLiteral,
                               TokenKind::CharacterLiteral, TokenKind::StringLiteral};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto& expression = literal(*variable(result, i).initializer);
        require(expression.literal.kind == kinds[i] && !expression.semantic_type,
                "Literal kind or semantic annotation was lost.");
    }
    require(literal(*variable(result, 0).initializer).literal.lexeme == "007",
            "Literal spelling was converted instead of retained.");
    require(literal(*variable(result, 2).initializer).literal.lexeme == R"bminor('\n')bminor",
            "Character escape spelling was changed.");
}

void nested_arrays() {
    const auto result = parse("grid: array [2] array [2] integer = {{1,2},{3,4}}; "
                              "empty: array [0] integer = {};");
    require(!result.has_errors(), "Nested array declarations were rejected.");
    const auto& declaration = variable(result, 0);
    const auto& type = *declaration.declared_type;
    require(type.type_kind == TypeKind::Array && type.element_type
                && type.element_type->type_kind == TypeKind::Array
                && type.element_type->element_type->type_kind == TypeKind::Integer,
            "Array element types were flattened or lost.");
    require(literal(*type.array_size).literal.lexeme == "2", "Missing array bound node.");
    require(declaration.initializer->kind == NodeKind::ArrayInitExpr,
            "Missing array initializer node.");
    const auto& outer = static_cast<const bminor::ArrayInitExpr&>(*declaration.initializer);
    require(outer.elements.size() == 2 && outer.elements[0]->kind == NodeKind::ArrayInitExpr,
            "Nested initializer shape was lost.");
    const auto& inner = static_cast<const bminor::ArrayInitExpr&>(*outer.elements[1]);
    require(inner.elements.size() == 2 && literal(*inner.elements[1]).literal.lexeme == "4",
            "Nested initializer contents were changed.");
    require(static_cast<const bminor::ArrayInitExpr&>(*variable(result, 1).initializer)
                .elements.empty(), "Empty array initializer should remain empty.");
}

void function_prototypes() {
    const auto result = parse("show: function void (items: array [] integer, count: integer); "
                              "ready: function boolean ();");
    require(!result.has_errors() && result.program->declarations.size() == 2,
            "Function prototypes were rejected.");
    require(result.program->declarations[0]->kind == NodeKind::FuncDecl,
            "Prototype did not produce a function node.");
    const auto& function = static_cast<const bminor::FuncDecl&>(*result.program->declarations[0]);
    require(function.name->name == "show" && function.return_type->type_kind == TypeKind::Void
                && function.parameters.size() == 2, "Prototype signature was changed.");
    const auto& parameter = *function.parameters[0];
    require(parameter.name->name == "items" && !parameter.declared_type->array_size
                && parameter.declared_type->element_type->type_kind == TypeKind::Integer,
            "Unsized parameter array was not represented correctly.");
    const auto& empty = static_cast<const bminor::FuncDecl&>(*result.program->declarations[1]);
    require(empty.parameters.empty() && empty.return_type->type_kind == TypeKind::Boolean,
            "Empty parameter list or return type was lost.");
}

void trivia_and_locations() {
    const auto result = parse("// note\r\nx /* note */ :\tinteger = 007;");
    require(!result.has_errors(), "Trivia changed the declaration grammar.");
    const auto& declaration = variable(result, 0);
    require(declaration.location.filename == "parser.bminor"
                && declaration.location.line == 2 && declaration.location.column == 1
                && declaration.location.offset == 9,
            "Declaration did not start at its identifier.");
    require(declaration.declared_type->location.column == 16
                && declaration.declared_type->location.offset == 24,
            "Type node did not retain its first token location.");
    require(literal(*declaration.initializer).location.column == 26,
            "Literal location changed while skipping comments or tabs.");
}

void partial_tree_and_eof_errors() {
    const std::string text = "ok: integer; bad integer; later: boolean;";
    const auto result = parse(text);
    require(result.has_errors() && result.diagnostics.size() == 1
                && result.program->declarations.size() == 1,
            "Parser should retain completed declarations and stop at its first error.");
    require(variable(result, 0).name->name == "ok", "Completed declaration was lost.");
    require(result.diagnostics[0].code == DiagnosticCode::UnexpectedToken
                && result.diagnostics[0].location.offset == text.find("integer", 13),
            "Missing colon diagnostic points to the wrong token.");
    const std::string unfinished = "x: integer";
    const auto eof = parse(unfinished);
    require(eof.has_errors() && eof.diagnostics[0].length == 0
                && eof.diagnostics[0].location.offset == unfinished.size(),
            "A missing token at EOF must have an empty source span.");
}

void function_definitions() {
    const auto result = parse("f: function integer () = { return 0; } "
                              "empty: function void () = {}");
    require(!result.has_errors() && result.program->declarations.size() == 2,
            "Function definitions were not parsed.");
    const auto& function = static_cast<const bminor::FuncDecl&>(*result.program->declarations[0]);
    const auto& empty = static_cast<const bminor::FuncDecl&>(*result.program->declarations[1]);
    require(function.body && function.body->statements.size() == 1
                && function.body->statements[0]->kind == NodeKind::ReturnStmt,
            "Function body was skipped instead of parsed.");
    require(empty.body && empty.body->statements.empty(),
            "An empty definition must differ from a prototype's null body.");
}

void malformed_declarations() {
    for (const std::string source : {
            "x: void;", "x: array [] integer;", "f: function array [2] integer ();",
            "f: function integer (a: integer,);", "x: array [2] integer = {1,};",
            "x integer;", "x: integer = ;", "x: array [2 integer;", "x: integer; ;",
            "x: array ["}) {
        const auto result = parse(source);
        require(result.has_errors() && result.diagnostics.size() == 1
                    && result.diagnostics[0].code == DiagnosticCode::UnexpectedToken,
                "Malformed declaration lacks a syntax diagnostic.");
    }
}

void syntax_without_semantic_checks() {
    // Compatibility, bounds positivity, duplicate names and main requirements
    // belong to semantic analysis; this parser only checks the documented syntax.
    const auto result = parse("x: integer = true; x: array [0] integer = {1,2,3};");
    require(!result.has_errors() && result.program->declarations.size() == 2,
            "Parser accidentally performed semantic checks.");
}

void nesting_limit() {
    std::string type = "x: ";
    for (int i = 0; i < 160; ++i) { type += "array [1] "; }
    type += "integer;";
    const auto deep_type = parse(type);
    require(deep_type.has_errors()
                && deep_type.diagnostics[0].code == DiagnosticCode::ParserNestingLimit,
            "Deep array types should fail gracefully before exhausting the stack.");
    const auto deep_initializer = parse("x: integer = " + std::string(160, '{')
                                        + "1" + std::string(160, '}') + ";");
    require(deep_initializer.has_errors()
                && deep_initializer.diagnostics[0].code == DiagnosticCode::ParserNestingLimit,
            "Deep initializers need the same bounded-recursion diagnostic.");
}

void repeat_parse_and_dump() {
    const bminor::SourceFile source{"repeat.bminor", "x: integer = 1; f: function void ();"};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    bminor::Parser parser(lexed.tokens);
    const auto first = parser.parse();
    const auto second = parser.parse();
    require(!first.has_errors() && !second.has_errors()
                && first.program->declarations.size() == 2
                && second.program->declarations.size() == 2,
            "Repeated parsing inherited cursor state.");
    std::ostringstream out;
    bminor::dump_ast(out, *first.program);
    require(out.str().find("VarDecl") != std::string::npos
                && out.str().find("FuncDecl prototype") != std::string::npos,
            "AST output omitted structural node labels.");
    std::istringstream lines(out.str());
    std::string line;
    while (std::getline(lines, line)) {
        require(line.find("@repeat.bminor:") != std::string::npos
                    && line.find("type=null") != std::string::npos,
                "AST output omitted node location or nullable type annotation.");
    }
}

void empty_input_and_contract() {
    const auto empty = parse("");
    const auto comments = parse("/* empty */ // no declarations");
    require(!empty.has_errors() && empty.program->declarations.empty()
                && !comments.has_errors() && comments.program->declarations.empty(),
            "Empty/trivia-only source should produce an empty Program node.");
    bool rejected = false;
    try {
        const std::vector<bminor::Token> tokens;
        bminor::Parser parser(tokens);
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Parser API must reject a vector without EOF.");
}

} // namespace

int main() {
    struct TestCase { const char* name; void (*run)(); };
    const TestCase cases[] = {
        {"atomic declarations", atomic_declarations},
        {"literal initializers", literal_initializers},
        {"nested arrays", nested_arrays},
        {"function prototypes", function_prototypes},
        {"trivia and source locations", trivia_and_locations},
        {"partial AST and EOF errors", partial_tree_and_eof_errors},
        {"function definitions", function_definitions},
        {"malformed declarations", malformed_declarations},
        {"syntax without semantic checks", syntax_without_semantic_checks},
        {"bounded nesting", nesting_limit},
        {"repeated parsing and AST output", repeat_parse_and_dump},
        {"empty input and token contract", empty_input_and_contract}
    };
    for (const auto& test : cases) {
        try {
            test.run();
            std::cout << "PASS: " << test.name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
