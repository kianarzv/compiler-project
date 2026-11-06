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

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

ParseResult parse(const std::string& text) {
    const bminor::SourceFile source{"recovery.bminor", text};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    require(!lexed.has_errors(), "Recovery fixture has lexical errors.");
    bminor::Parser parser(lexed.tokens);
    return parser.parse();
}

const bminor::FuncDecl& function(const ParseResult& result) {
    for (const auto& declaration : result.program->declarations) {
        if (declaration->kind == NodeKind::FuncDecl) {
            return static_cast<const bminor::FuncDecl&>(*declaration);
        }
    }
    throw std::runtime_error("Recovered function is missing.");
}

const std::vector<std::unique_ptr<bminor::Stmt>>& statements(const ParseResult& result) {
    const auto& fn = function(result);
    require(static_cast<bool>(fn.body), "Expected a definition, not a prototype.");
    return fn.body->statements;
}

void multiple_global_errors() {
    const auto result = parse("bad integer; good: integer; broken: boolean = ; after: integer;");
    require(result.diagnostics.size() == 2 && result.program->declarations.size() == 4,
            "Global recovery lost errors or surrounding declarations.");
    const auto& declarations = result.program->declarations;
    require(declarations[0]->kind == NodeKind::ErrorDecl
                && declarations[1]->kind == NodeKind::VarDecl
                && declarations[2]->kind == NodeKind::ErrorDecl
                && declarations[3]->kind == NodeKind::VarDecl,
            "Global declaration order was not retained.");
    require(static_cast<const bminor::VarDecl&>(*declarations[3]).name->name == "after",
            "The final global declaration was not parsed.");
}

void missing_semicolon_boundaries() {
    const auto global = parse("bad: integer = 1 good: integer = 2;");
    require(global.diagnostics.size() == 1 && global.program->declarations.size() == 2
                && global.program->declarations[1]->kind == NodeKind::VarDecl,
            "Missing global semicolon swallowed the following name/colon.");
    const auto local = parse("main: function void () = { print 1 return; "
                              "bad: integer = 1 good: integer = 2; }");
    const auto& nodes = statements(local);
    require(local.diagnostics.size() == 2 && nodes.size() == 4
                && nodes[0]->kind == NodeKind::ErrorStmt
                && nodes[1]->kind == NodeKind::ReturnStmt
                && nodes[2]->kind == NodeKind::ErrorStmt
                && nodes[3]->kind == NodeKind::DeclStmt,
            "Keyword/name-colon synchronization failed after missing semicolons.");
}

void multiple_local_errors() {
    const auto result = parse("main: function integer () = { x: integer = ; print 1; "
                              "print f(1,); return 2; } after: integer;");
    const auto& nodes = statements(result);
    require(result.diagnostics.size() == 2 && nodes.size() == 4
                && nodes[0]->kind == NodeKind::ErrorStmt
                && nodes[1]->kind == NodeKind::PrintStmt
                && nodes[2]->kind == NodeKind::ErrorStmt
                && nodes[3]->kind == NodeKind::ReturnStmt
                && result.program->declarations.size() == 2,
            "Local recovery stopped early or consumed the enclosing brace.");
}

void malformed_control_headers() {
    const auto result = parse("main: function integer () = { "
        "for(i=0 i<3;i++) { hidden: integer; print hidden; } "
        "if(+) print 1; else print 2; return 3; } after: integer;");
    const auto& nodes = statements(result);
    require(result.diagnostics.size() == 2 && nodes.size() == 3
                && nodes[0]->kind == NodeKind::ErrorStmt
                && nodes[1]->kind == NodeKind::ErrorStmt
                && nodes[2]->kind == NodeKind::ReturnStmt,
            "Broken control headers leaked header/body fragments into later statements.");
    std::ostringstream output;
    bminor::dump_ast(output, *result.program);
    require(output.str().find("hidden") == std::string::npos
                && result.program->declarations.size() == 2,
            "Discarded loop locals were promoted outside their scope.");
}

void else_and_owner_boundaries() {
    const auto result = parse("main: function integer () = { "
        "if(a) if(b) print 1 else print 2; else print 3; return 0; }");
    const auto& nodes = statements(result);
    require(result.diagnostics.size() == 1 && nodes[0]->kind == NodeKind::IfStmt,
            "An error in a branch discarded the surrounding if.");
    const auto& outer = static_cast<const bminor::IfStmt&>(*nodes[0]);
    const auto& inner = static_cast<const bminor::IfStmt&>(*outer.then_branch);
    require(outer.else_branch && inner.else_branch
                && inner.then_branch->kind == NodeKind::ErrorStmt,
            "Recovery stole an else or changed nearest-if ownership.");

    const auto missing = parse("main: function void () = { if(a) else print 2; for(;;) }");
    const auto& missing_nodes = statements(missing);
    require(missing.diagnostics.size() == 2 && missing_nodes.size() == 2,
            "Missing branch/loop body prevented completion of the block.");
    const auto& branch = static_cast<const bminor::IfStmt&>(*missing_nodes[0]);
    const auto& loop = static_cast<const bminor::ForStmt&>(*missing_nodes[1]);
    require(branch.then_branch->kind == NodeKind::ErrorStmt
                && static_cast<const bminor::ErrorStmt&>(*branch.then_branch).length == 0
                && branch.else_branch && loop.body->kind == NodeKind::ErrorStmt
                && static_cast<const bminor::ErrorStmt&>(*loop.body).length == 0,
            "A missing child must be explicit without consuming its owner's delimiter.");
}

void malformed_function_regions() {
    const auto top = parse("bad: function integer (first integer, second: integer) = { "
                           "hidden: integer; return 0; } after: integer;");
    require(top.diagnostics.size() == 1 && top.program->declarations.size() == 2
                && top.program->declarations[0]->kind == NodeKind::ErrorDecl
                && top.program->declarations[1]->kind == NodeKind::VarDecl,
            "A parameter/local was mistaken for a new global declaration.");
    const auto nested = parse("main: function integer () = { "
        "inner: function void () = { hidden: integer; print hidden; } return 0; }");
    require(nested.diagnostics.size() == 1 && statements(nested).size() == 2
                && statements(nested)[0]->kind == NodeKind::ErrorStmt
                && statements(nested)[1]->kind == NodeKind::ReturnStmt,
            "Forbidden nested function leaked its body into the outer function.");
}

void broken_delimiters_in_simple_statements() {
    const auto global = parse("bad: array [2] integer = {1,2; after: integer;");
    require(global.diagnostics.size() == 1 && global.program->declarations.size() == 2
                && global.program->declarations[1]->kind == NodeKind::VarDecl,
            "Unclosed global initializer swallowed the next declaration.");
    const auto local = parse("main: function integer () = { "
        "a: array [2] integer = {1,2; print 3; "
        "print f(1; return 0; }");
    require(local.diagnostics.size() == 2 && statements(local).size() == 4
                && statements(local)[1]->kind == NodeKind::PrintStmt
                && statements(local)[3]->kind == NodeKind::ReturnStmt,
            "Simple-statement semicolons failed to terminate unmatched delimiters.");
}

void unclosed_blocks_at_eof() {
    const std::string text = "main: function void () = {{ print 1;";
    const auto result = parse(text);
    const auto& block = *function(result).body;
    require(result.diagnostics.size() == 1 && result.diagnostics[0].length == 0
                && result.diagnostics[0].location.offset == text.size()
                && block.incomplete && block.statements.size() == 1,
            "EOF recovery lost an unfinished block or duplicated closing-brace errors.");
    const auto& nested = static_cast<const bminor::BlockStmt&>(*block.statements[0]);
    require(nested.incomplete && nested.statements[0]->kind == NodeKind::PrintStmt,
            "Known contents of nested unfinished blocks were lost.");
}

void error_spans_and_crlf() {
    const std::string text = "bad integer;\r\ngood: integer;\r\nmain: function void () = {\r\n"
        " print ;\r\n return;\r\n}\r\n";
    const auto result = parse(text);
    require(result.diagnostics.size() == 2, "Expected both global and local errors.");
    const auto& global = static_cast<const bminor::ErrorDecl&>(*result.program->declarations[0]);
    const auto& local = static_cast<const bminor::ErrorStmt&>(*statements(result)[0]);
    require(global.location.offset == 0 && global.length == text.find("good"),
            "ErrorDecl span must include the skipped terminator/trivia.");
    require(local.location.offset == text.find("print")
                && local.length == text.find("return") - text.find("print")
                && result.diagnostics[1].location.line == 4
                && result.diagnostics[1].location.column == 8
                && !global.semantic_type && !local.semantic_type,
            "Recovered spans/diagnostic columns changed under CRLF.");
}

void resource_failure_recovery() {
    std::string expression = "1";
    for (int i = 0; i < 160; ++i) { expression += "+1"; }
    const auto result = parse("main: function integer () = { x: integer = " + expression
                              + "; print 1; return 0; } after: integer;");
    require(result.diagnostics.size() == 1
                && result.diagnostics[0].code == DiagnosticCode::ParserNestingLimit
                && statements(result).size() == 3
                && statements(result)[1]->kind == NodeKind::PrintStmt
                && statements(result)[2]->kind == NodeKind::ReturnStmt
                && result.program->declarations.size() == 2,
            "Resource failure prevented subsequent ordinary parsing.");
}

void reset_and_stray_tokens() {
    const bminor::SourceFile source{"recovery.bminor", "} } bad integer; good: integer;"};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    bminor::Parser parser(lexed.tokens);
    const auto first = parser.parse();
    const auto second = parser.parse();
    require(first.diagnostics.size() == 3 && second.diagnostics.size() == 3
                && first.program->declarations.size() == 4
                && second.program->declarations.back()->kind == NodeKind::VarDecl,
            "Stray braces prevented progress or repeated parsing accumulated errors.");
    const bminor::SourceFile unfinished{"recovery.bminor", "main: function void () = {{"};
    bminor::Lexer eof_lexer(unfinished);
    const auto eof_tokens = eof_lexer.tokenize();
    bminor::Parser eof_parser(eof_tokens.tokens);
    const auto eof_first = eof_parser.parse();
    const auto eof_second = eof_parser.parse();
    require(eof_first.diagnostics.size() == 1 && eof_second.diagnostics.size() == 1,
            "Missing-brace reporting did not reset between parses.");
}

void deterministic_progress_inputs() {
    // Exercise recovery invariants without depending on a guessed repair of
    // arbitrary invalid syntax. Every generated token is lexically well formed.
    const char* vocabulary[] = {";", "}", "{", "if", "for", "else", "return", "print",
        "x", ":", "integer", "function", "(", ")", "[", "]", "=", "1", ",", "+", "true"};
    for (unsigned int seed = 0; seed < 64; ++seed) {
        unsigned int state = seed + 1;
        std::string text;
        for (unsigned int i = 0; i < 80; ++i) {
            state = state * 1664525u + 1013904223u;
            text += vocabulary[state % 21];
            text += ' ';
        }
        const auto first = parse(text);
        const auto second = parse(text);
        require(first.program && first.diagnostics.size() <= 162
                    && first.diagnostics.size() == second.diagnostics.size(),
                "Generated input did not make bounded, repeatable progress.");
        std::size_t previous = 0;
        for (const auto& diagnostic : first.diagnostics) {
            require(diagnostic.location.offset >= previous
                        && diagnostic.location.offset <= text.size()
                        && diagnostic.length <= text.size() - diagnostic.location.offset,
                    "Generated input produced an invalid/nonmonotonic diagnostic span.");
            previous = diagnostic.location.offset;
        }
        std::ostringstream a, b;
        bminor::dump_ast(a, *first.program);
        bminor::dump_ast(b, *second.program);
        require(a.str() == b.str(), "Recovery changed between equivalent scans.");
    }
}

} // namespace

int main() {
    struct TestCase { const char* name; void (*run)(); };
    const TestCase cases[] = {
        {"multiple global errors", multiple_global_errors},
        {"missing-semicolon boundaries", missing_semicolon_boundaries},
        {"multiple local errors", multiple_local_errors},
        {"malformed control headers", malformed_control_headers},
        {"else and owner boundaries", else_and_owner_boundaries},
        {"malformed function regions", malformed_function_regions},
        {"broken simple-statement delimiters", broken_delimiters_in_simple_statements},
        {"unfinished blocks at EOF", unclosed_blocks_at_eof},
        {"error spans and CRLF", error_spans_and_crlf},
        {"resource-failure recovery", resource_failure_recovery},
        {"reset and stray tokens", reset_and_stray_tokens},
        {"deterministic progress inputs", deterministic_progress_inputs}
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
