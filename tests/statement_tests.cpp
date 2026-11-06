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

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

bminor::ParseResult parse(const std::string& text) {
    const bminor::SourceFile source{"stmt.bminor", text};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    require(!lexed.has_errors(), "Statement fixture contains a lexical error.");
    bminor::Parser parser(lexed.tokens);
    return parser.parse();
}

bminor::ParseResult body(const std::string& text) {
    return parse("main: function integer () = {" + text + "}");
}

const bminor::FuncDecl& function(const bminor::ParseResult& result, std::size_t index = 0) {
    require(!result.has_errors() && index < result.program->declarations.size(),
            "Missing or rejected function definition.");
    const auto& declaration = *result.program->declarations[index];
    require(declaration.kind == NodeKind::FuncDecl, "Expected a function node.");
    return static_cast<const bminor::FuncDecl&>(declaration);
}

const bminor::Stmt& statement(const bminor::ParseResult& result, std::size_t index = 0) {
    const auto& fn = function(result);
    require(fn.body && index < fn.body->statements.size(), "Missing statement node.");
    return *fn.body->statements[index];
}

void prototypes_and_bodies() {
    const auto result = parse("f: function void (); f: function void () = {} "
                              "g: function integer () = { return 0; }");
    require(!result.has_errors() && result.program->declarations.size() == 3,
            "Prototypes and definitions cannot coexist syntactically.");
    require(!function(result, 0).body && function(result, 1).body
                && function(result, 1).body->statements.empty(),
            "Empty function body was confused with a prototype.");
    const auto& returned = *function(result, 2).body->statements[0];
    require(returned.kind == NodeKind::ReturnStmt, "Definition lost its return statement.");
}

void declaration_lookahead() {
    const auto result = body("value /* comment */ : integer = 1; "
                              "value /* comment */ = value+1; "
                              "show /* comment */ (value); value;");
    require(!result.has_errors(), "Trivia broke identifier/colon lookahead.");
    require(statement(result, 0).kind == NodeKind::DeclStmt,
            "Local declaration was parsed as an expression.");
    const auto& declared = static_cast<const bminor::DeclStmt&>(statement(result, 0));
    require(declared.declaration->name->name == "value", "Local name was lost.");
    const auto& assigned = static_cast<const bminor::ExprStmt&>(statement(result, 1));
    const auto& called = static_cast<const bminor::ExprStmt&>(statement(result, 2));
    const auto& referenced = static_cast<const bminor::ExprStmt&>(statement(result, 3));
    require(assigned.expression->kind == NodeKind::BinaryExpr
                && called.expression->kind == NodeKind::CallExpr
                && referenced.expression->kind == NodeKind::IdentifierExpr,
            "A name-led expression was incorrectly treated as a declaration.");
}

void nested_blocks_and_empty_statements() {
    const auto result = body("; { x: integer = 1; ; { x: integer = 2; } } ;");
    require(!result.has_errors() && function(result).body->statements.size() == 3,
            "Block or empty statement was dropped.");
    require(statement(result, 0).kind == NodeKind::ExprStmt
                && !static_cast<const bminor::ExprStmt&>(statement(result, 0)).expression,
            "An empty statement should have a null expression.");
    const auto& block = static_cast<const bminor::BlockStmt&>(statement(result, 1));
    require(block.statements.size() == 3 && block.statements[0]->kind == NodeKind::DeclStmt
                && block.statements[2]->kind == NodeKind::BlockStmt,
            "Nested block structure was flattened.");
}

void nearest_else() {
    const auto result = body("if(a) if(b) return 1; else return 2;");
    const auto& outer = static_cast<const bminor::IfStmt&>(statement(result));
    require(!outer.else_branch && outer.then_branch->kind == NodeKind::IfStmt,
            "An else incorrectly attached to the outer if.");
    const auto& inner = static_cast<const bminor::IfStmt&>(*outer.then_branch);
    require(inner.else_branch && inner.else_branch->kind == NodeKind::ReturnStmt,
            "The nearest unmatched if lost its else.");

    const auto braced = body("if(a) { if(b) return 1; } else return 2;");
    const auto& braced_if = static_cast<const bminor::IfStmt&>(statement(braced));
    const auto& block = static_cast<const bminor::BlockStmt&>(*braced_if.then_branch);
    require(braced_if.else_branch
                && !static_cast<const bminor::IfStmt&>(*block.statements[0]).else_branch,
            "A closing block did not preserve the outer else boundary.");

    const auto both = body("if(a) if(b) return 1; else return 2; else return 3;");
    const auto& both_if = static_cast<const bminor::IfStmt&>(statement(both));
    require(both_if.else_branch
                && static_cast<const bminor::IfStmt&>(*both_if.then_branch).else_branch,
            "Two nested else branches were not assigned independently.");
}

void optional_for_headers() {
    // All eight combinations of the three optional expressions must retain
    // their actual presence; the parser must not invent a true condition.
    for (unsigned int mask = 0; mask < 8; ++mask) {
        const std::string header = "for(" + std::string(mask & 1 ? "i=0" : "")
            + ";" + (mask & 2 ? "i<3" : "") + ";" + (mask & 4 ? "i++" : "") + ") ;";
        const auto result = body(header);
        require(statement(result).kind == NodeKind::ForStmt, "Expected a for node.");
        const auto& loop = static_cast<const bminor::ForStmt&>(statement(result));
        require(static_cast<bool>(loop.initializer) == static_cast<bool>(mask & 1)
                    && static_cast<bool>(loop.condition) == static_cast<bool>(mask & 2)
                    && static_cast<bool>(loop.update) == static_cast<bool>(mask & 4),
                "Optional for expression presence changed.");
        require(loop.body->kind == NodeKind::ExprStmt
                    && !static_cast<const bminor::ExprStmt&>(*loop.body).expression,
                "An unbraced empty loop body was lost.");
    }
}

void return_and_print_values() {
    const auto result = body("return; return 1+2; print \"a\", x, f(2);");
    const auto& empty = static_cast<const bminor::ReturnStmt&>(statement(result, 0));
    const auto& value = static_cast<const bminor::ReturnStmt&>(statement(result, 1));
    const auto& print = static_cast<const bminor::PrintStmt&>(statement(result, 2));
    require(!empty.value && value.value && value.value->kind == NodeKind::BinaryExpr,
            "Return forms did not preserve their optional value.");
    require(print.values.size() == 3 && print.values[0]->kind == NodeKind::LiteralExpr
                && print.values[1]->kind == NodeKind::IdentifierExpr
                && print.values[2]->kind == NodeKind::CallExpr,
            "Print expressions lost their order or structure.");
}

void malformed_statement_forms() {
    for (const std::string text : {
            "if true) ;", "if() ;", "if(true ;", "if(true)", "if(true); else",
            "else ;", "for(;;", "for(;) ;", "for(i=0 i<3;i++) ;", "for(;;)",
            "for(i: integer=0;;) ;", "return 1", "print;", "print 1,;",
            "print 1", "f(1)", "x: integer = ;", "inner: function void ();",
            "inner: function void () = {}", "while(true);"}) {
        const auto result = body(text);
        require(result.has_errors() && result.diagnostics.size() == 1
                    && result.diagnostics[0].code == DiagnosticCode::UnexpectedToken,
                "Malformed/out-of-grammar statement was accepted: " + text);
    }
    const std::string unclosed = "main: function integer () = { print 1;";
    const auto eof = parse(unclosed);
    require(eof.has_errors() && eof.diagnostics[0].length == 0
                && eof.diagnostics[0].location.offset == unclosed.size(),
            "Unclosed block must report EOF with an empty span.");
}

void top_level_context() {
    for (const std::string text : {"return 0;", "print 1;", "if(true);", "{;}",
                                   "f: function void () {}", "f: function void () = {};"}) {
        const auto result = parse(text);
        require(result.has_errors() && result.diagnostics[0].code == DiagnosticCode::UnexpectedToken,
                "Program scope accepted syntax outside global declarations.");
    }
    const auto semantic = body("x: integer = true; if(1) return; print unknown; return false;");
    require(!semantic.has_errors(), "Parser unexpectedly performed name/type/return checks.");
}

void statement_locations_and_dump() {
    const std::string text = "// note\r\nmain: function integer () = {\r\n"
        "  if (true) return 1; else print \"x\";\r\n  for (;;) ;\r\n}\r\n";
    const auto result = parse(text);
    const auto& fn = function(result);
    const auto& branch = static_cast<const bminor::IfStmt&>(statement(result, 0));
    const auto& loop = static_cast<const bminor::ForStmt&>(statement(result, 1));
    require(fn.location.line == 2 && fn.location.offset == 9
                && fn.body->location.offset == text.find('{'), "Function/block location was lost.");
    require(branch.location.line == 3 && branch.location.column == 3
                && branch.then_branch->location.column == 13
                && branch.else_branch->location.column == 28,
            "Keyword-led statements did not retain their first-token locations.");
    require(loop.location.line == 4 && loop.location.column == 3
                && loop.body->location.column == 12, "Loop/empty-body positions are incorrect.");
    std::ostringstream out;
    bminor::dump_ast(out, *result.program);
    require(out.str().find("FuncDecl definition") != std::string::npos
                && out.str().find("ForStmt init=no condition=no update=no") != std::string::npos,
            "AST display lost definition or optional-header labels.");
    std::istringstream lines(out.str());
    std::string line;
    while (std::getline(lines, line)) {
        require(line.find("@stmt.bminor:") != std::string::npos
                    && line.find("type=null") != std::string::npos,
                "Statement AST output omitted source/type metadata.");
    }
}

void failed_body_and_repeat_parse() {
    const bminor::SourceFile source{"stmt.bminor",
        "version: integer = 1; main: function integer () = { return 0 } later: integer;"};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    bminor::Parser parser(lexed.tokens);
    const auto first = parser.parse();
    const auto second = parser.parse();
    for (const auto* result : {&first, &second}) {
        require(result->has_errors() && result->diagnostics.size() == 1
                    && result->program->declarations.size() == 3
                    && result->program->declarations[0]->kind == NodeKind::VarDecl
                    && result->program->declarations[1]->kind == NodeKind::FuncDecl
                    && result->program->declarations[2]->kind == NodeKind::VarDecl,
                "Recovered function/later declaration was lost or errors accumulated.");
        const auto& recovered = static_cast<const bminor::FuncDecl&>(*result->program->declarations[1]);
        require(recovered.body && recovered.body->statements[0]->kind == NodeKind::ErrorStmt,
                "Failed return statement must have an explicit error placeholder.");
    }
}

void depth_and_width_limits() {
    const std::string blocks = std::string(160, '{') + ";" + std::string(160, '}');
    std::string branches, loops;
    for (int i = 0; i < 160; ++i) { branches += "if(true) "; loops += "for(;;) "; }
    for (const auto& text : {blocks, branches + ";", loops + ";"}) {
        const auto result = body(text);
        require(result.has_errors() && result.diagnostics[0].code == DiagnosticCode::ParserNestingLimit,
                "Deep statements must fail gracefully at the nesting limit.");
    }
    const auto wide = body(std::string(400, ';'));
    require(!wide.has_errors() && function(wide).body->statements.size() == 400,
            "A shallow statement list was mistaken for deep nesting.");
}

void complete_recursive_program() {
    const auto result = parse(
        "fib: function integer (n: integer) = { "
        "if(n<2) return n; else return fib(n-1)+fib(n-2); } "
        "main: function integer () = { values: array [3] integer = {1,2,3}; "
        "i: integer; for(i=0;i<3;i++) { print fib(values[i]), \"\\n\"; } return 0; }");
    require(!result.has_errors() && result.program->declarations.size() == 2
                && function(result, 0).body && function(result, 1).body,
            "A complete program did not produce both function bodies.");
    require(function(result, 1).body->statements.size() == 4,
            "Complete program lost a local declaration, loop or return.");
}

} // namespace

int main() {
    struct TestCase { const char* name; void (*run)(); };
    const TestCase cases[] = {
        {"prototypes and definitions", prototypes_and_bodies},
        {"local declaration lookahead", declaration_lookahead},
        {"nested blocks and empty statements", nested_blocks_and_empty_statements},
        {"nearest unmatched else", nearest_else},
        {"optional for headers", optional_for_headers},
        {"return and print values", return_and_print_values},
        {"malformed statement forms", malformed_statement_forms},
        {"top-level and semantic boundaries", top_level_context},
        {"statement locations and output", statement_locations_and_dump},
        {"failed body and repeated parsing", failed_body_and_repeat_parse},
        {"statement depth and width", depth_and_width_limits},
        {"complete recursive program", complete_recursive_program}
    };
    for (const auto& test : cases) {
        try { test.run(); std::cout << "PASS: " << test.name << '\n'; }
        catch (const std::exception& error) {
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
