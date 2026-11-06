#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"
#include "bminor/syntax_classifier.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using bminor::ClassificationResult;
using bminor::SyntaxRole;

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

ClassificationResult classify(const std::string& text) {
    return bminor::classify_source(bminor::SourceFile{"classify.bminor", text});
}

const bminor::ClassifiedSpan& at(const ClassificationResult& result, std::size_t offset) {
    for (const auto& span : result.spans) {
        if (span.location.offset == offset) { return span; }
    }
    throw std::runtime_error("Missing classified span at " + std::to_string(offset));
}

std::vector<SyntaxRole> roles(const ClassificationResult& result,
                             const std::string& source, const std::string& spelling) {
    std::vector<SyntaxRole> found;
    for (const auto& span : result.spans) {
        if (source.substr(span.location.offset, span.length) == spelling) {
            found.push_back(span.role);
        }
    }
    return found;
}

void check_coverage(const ClassificationResult& result, const std::string& text) {
    std::size_t offset = 0;
    std::string reconstructed;
    for (const auto& span : result.spans) {
        require(span.location.offset == offset && span.length > 0
                    && span.length <= text.size() - offset,
                "Spans overlap, leave a gap, extend beyond input or include EOF.");
        require(span.location.filename == "classify.bminor"
                    && std::string(bminor::syntax_role_name(span.role)) != "UNKNOWN",
                "Span is missing source identity or a stable role label.");
        if (span.role == SyntaxRole::Whitespace) {
            require(!span.has_error, "Whitespace must not gain an error decoration.");
        }
        reconstructed += text.substr(offset, span.length);
        offset += span.length;
    }
    require(offset == text.size() && reconstructed == text, "Classification changed source bytes.");
}

void lexical_categories() {
    const bminor::SourceFile source{"classify.bminor",
        "if return function while array integer boolean char string void true false "
        "42 \"work()\" 'x' + != && (){}[],:; // comment\n/* block */ name"};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    require(!lexed.has_errors(), "Lexical role fixture is invalid.");
    ClassificationResult result{bminor::classify_tokens(lexed.tokens), {}};
    check_coverage(result, source.text);
    const std::pair<const char*, SyntaxRole> expected[] = {
        {"if", SyntaxRole::Keyword}, {"while", SyntaxRole::Keyword},
        {"function", SyntaxRole::Keyword}, {"array", SyntaxRole::TypeName},
        {"integer", SyntaxRole::TypeName}, {"void", SyntaxRole::TypeName},
        {"true", SyntaxRole::BooleanLiteral}, {"42", SyntaxRole::IntegerLiteral},
        {"\"work()\"", SyntaxRole::StringLiteral}, {"'x'", SyntaxRole::CharacterLiteral},
        {"!=", SyntaxRole::Operator}, {"(", SyntaxRole::Delimiter},
        {"// comment", SyntaxRole::Comment}, {"/* block */", SyntaxRole::Comment},
        {"name", SyntaxRole::Identifier}
    };
    for (const auto& item : expected) {
        // A spelling such as '(' may also occur inside the string token. Match
        // complete classified lexemes rather than searching raw characters.
        const auto found = roles(result, source.text, item.first);
        require(!found.empty(), std::string("Missing lexical category for ") + item.first);
        for (SyntaxRole role : found) {
            require(role == item.second, std::string("Wrong lexical role for ") + item.first);
        }
    }
}

void declarations_parameters_and_calls() {
    const std::string text = "work: function integer (p: integer); "
        "work: function integer (p: integer) = { return p; } "
        "main: function integer () = { x: integer = work(1); return x; }";
    const auto result = classify(text);
    require(!result.has_errors(), "Valid classification fixture was rejected.");
    require(roles(result, text, "work") == std::vector<SyntaxRole>{
                SyntaxRole::FunctionDeclaration, SyntaxRole::FunctionDeclaration, SyntaxRole::FunctionCall},
            "Prototype/definition/call names were not classified from their nodes.");
    require(roles(result, text, "p") == std::vector<SyntaxRole>{
                SyntaxRole::ParameterDeclaration, SyntaxRole::ParameterDeclaration, SyntaxRole::VariableReference},
            "Parameter declaration and expression reference were conflated.");
    require(roles(result, text, "x") == std::vector<SyntaxRole>{
                SyntaxRole::VariableDeclaration, SyntaxRole::VariableReference},
            "Local declaration/reference roles were lost.");
}

void repeated_spellings() {
    const std::string text = "work: function integer (p: integer); main: function integer () = { "
        "copy: integer = work; work(1); { work: integer = 7; print work; } return 0; }";
    const auto result = classify(text);
    require(!result.has_errors(), "Syntax-only fixture unexpectedly failed semantic checks.");
    require(roles(result, text, "work") == std::vector<SyntaxRole>{
        SyntaxRole::FunctionDeclaration, SyntaxRole::VariableReference, SyntaxRole::FunctionCall,
        SyntaxRole::VariableDeclaration, SyntaxRole::VariableReference},
        "A global spelling map replaced classification by exact source occurrence.");
}

void ast_is_required_and_unchanged() {
    const bminor::SourceFile source{"classify.bminor",
        "work: function integer (); main: function integer () = { return (work)(); }"};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    const auto original = lexed.tokens;
    bminor::Parser parser(lexed.tokens);
    const auto parsed = parser.parse();
    std::ostringstream before, after;
    bminor::dump_ast(before, *parsed.program);
    const ClassificationResult fallback{bminor::classify_tokens(lexed.tokens), {}};
    const ClassificationResult refined{bminor::classify_tokens(lexed.tokens, parsed.program.get()), {}};
    require(roles(fallback, source.text, "work") == std::vector<SyntaxRole>{
                SyntaxRole::Identifier, SyntaxRole::Identifier}, "Token fallback guessed function roles.");
    require(roles(refined, source.text, "work") == std::vector<SyntaxRole>{
                SyntaxRole::FunctionDeclaration, SyntaxRole::FunctionCall}, "AST did not refine identifier roles.");
    bminor::dump_ast(after, *parsed.program);
    require(before.str() == after.str(), "Classifier mutated the AST or semantic annotations.");
    for (std::size_t i = 0; i < original.size(); ++i) {
        require(original[i].kind == lexed.tokens[i].kind && original[i].lexeme == lexed.tokens[i].lexeme
                    && original[i].location.offset == lexed.tokens[i].location.offset,
                "Classifier changed the lexer token stream.");
    }
}

void callee_forms() {
    const std::string text = "main: function integer () = { (work)(x); factory()(x); "
        "handlers[i](x); (-work)(x); return 0; }";
    const auto result = classify(text);
    require(!result.has_errors(), "Composite callee fixture failed.");
    require(roles(result, text, "work") == std::vector<SyntaxRole>{
                SyntaxRole::FunctionCall, SyntaxRole::VariableReference},
            "Groups/unary expressions were mistaken for the same kind of callee.");
    require(roles(result, text, "factory")[0] == SyntaxRole::FunctionCall
                && roles(result, text, "handlers")[0] == SyntaxRole::VariableReference
                && roles(result, text, "i")[0] == SyntaxRole::VariableReference,
            "Calling an indexed/call result incorrectly relabeled its base/index.");
}

void complete_ast_traversal() {
    const std::string text = "f: function void (p: array [bound()] integer) = { "
        "a: array [size()] integer = {init(), {nested()}}; "
        "for(start(); check(); advance()) if(test() && !other()) print show(); else return done(); "
        "items[index()]++; a[0] = -value(); ; }";
    const auto result = classify(text);
    require(!result.has_errors(), "Traversal fixture was not parsed.");
    for (const std::string name : {"bound", "size", "init", "nested", "start", "check", "advance",
                                   "test", "other", "show", "done", "index", "value"}) {
        const auto occurrences = roles(result, text, name);
        require(occurrences.size() == 1 && occurrences[0] == SyntaxRole::FunctionCall,
                "Traversal missed a call below an AST field: " + name);
    }
    check_coverage(result, text);
}

void crlf_and_empty_coverage() {
    const std::string text = "// header\r\n\tmain: function integer () = {\r\n return work(1);\r\n}\r\n";
    const auto result = classify(text);
    check_coverage(result, text);
    const auto& name = at(result, text.find("main"));
    const auto& call = at(result, text.find("work"));
    require(name.location.line == 2 && name.location.column == 2
                && call.location.line == 3 && call.location.column == 9,
            "Classification changed byte-based columns or CRLF line positions.");
    require(classify("").spans.empty() && !classify("").has_errors(), "EOF became a span.");
    const auto white = classify(" \t\r\n");
    require(!white.has_errors() && white.spans.size() == 1
                && white.spans[0].role == SyntaxRole::Whitespace, "Whitespace-only input was lost.");
}

void recovered_syntax_roles() {
    const std::string text = "bad integer; good: function integer () = { "
        "print 1 return work(); print broken(1,); return later(); }";
    const auto result = classify(text);
    require(result.diagnostics.size() == 3, "Expected all independent syntax errors.");
    require(at(result, text.find("bad")).role == SyntaxRole::Identifier
                && at(result, text.find("bad")).has_error
                && at(result, text.find("good")).role == SyntaxRole::FunctionDeclaration
                && !at(result, text.find("good")).has_error,
            "A discarded declaration gained a false AST role or tainted a later function.");
    require(at(result, text.find("work")).role == SyntaxRole::FunctionCall
                && !at(result, text.find("return")).has_error,
            "Missing-semicolon diagnostic incorrectly decorated the preserved return.");
    require(at(result, text.find("broken")).role == SyntaxRole::Identifier
                && at(result, text.find("broken")).has_error
                && at(result, text.find("later")).role == SyntaxRole::FunctionCall,
            "Recovered valid calls and discarded call fragments were conflated.");
    require(at(result, text.find("print")).role == SyntaxRole::Keyword
                && at(result, text.find("print")).has_error,
            "Error decoration erased the base category.");
    check_coverage(result, text);
}

void lexical_fallback() {
    const std::string text = "bad@; work(1); return true;";
    const auto result = classify(text);
    require(result.diagnostics.size() == 1
                && at(result, text.find('@')).role == SyntaxRole::Invalid
                && at(result, text.find('@')).has_error,
            "Unknown byte lost its lexical error marker.");
    require(at(result, text.find("work")).role == SyntaxRole::Identifier
                && at(result, text.find("return")).role == SyntaxRole::Keyword
                && at(result, text.find("true")).role == SyntaxRole::BooleanLiteral,
            "Lexical fallback guessed AST roles or dropped recognized categories.");
    check_coverage(result, text);
}

void arbitrary_byte_coverage() {
    for (unsigned int seed = 0; seed < 16; ++seed) {
        std::string text;
        for (unsigned int i = 0; i < 256; ++i) {
            text.push_back(static_cast<char>((i * 73 + seed * 19) & 255));
        }
        check_coverage(classify(text), text);
    }
}

void owned_results_and_repeatability() {
    const std::string text = "main: function integer () = { return f(); }";
    const auto first = classify(text);
    const auto second = classify(text);
    require(!first.has_errors() && !second.has_errors()
                && first.spans.size() == second.spans.size(), "Repeated classification changed size/errors.");
    for (std::size_t i = 0; i < first.spans.size(); ++i) {
        const auto& a = first.spans[i];
        const auto& b = second.spans[i];
        require(a.location.offset == b.location.offset && a.length == b.length
                    && a.role == b.role && a.has_error == b.has_error,
                "Classification inherited state or borrowed destroyed token data.");
    }
}

void missing_nodes_have_no_source_bytes() {
    const std::string missing = "main: function void () = { if(true) else return; }";
    const auto branch = classify(missing);
    require(branch.has_errors() && !at(branch, missing.find("else")).has_error,
            "A zero-byte missing branch decorated its owning else.");
    check_coverage(branch, missing);
    const std::string unfinished = "main: function void () = { return;";
    const auto block = classify(unfinished);
    require(block.has_errors(), "Missing brace diagnostic was lost.");
    for (const auto& span : block.spans) {
        require(!span.has_error, "An absent closing brace caused synthetic source decoration.");
    }
    check_coverage(block, unfinished);
}

} // namespace

int main() {
    struct TestCase { const char* name; void (*run)(); };
    const TestCase cases[] = {
        {"lexical categories", lexical_categories},
        {"declarations parameters and calls", declarations_parameters_and_calls},
        {"same spelling at different positions", repeated_spellings},
        {"AST dependency and immutability", ast_is_required_and_unchanged},
        {"direct grouped and composite callees", callee_forms},
        {"complete AST traversal", complete_ast_traversal},
        {"CRLF whitespace and empty input", crlf_and_empty_coverage},
        {"recovered syntax roles and error decoration", recovered_syntax_roles},
        {"lexical fallback", lexical_fallback},
        {"arbitrary byte coverage", arbitrary_byte_coverage},
        {"owned results and repeatability", owned_results_and_repeatability},
        {"missing nodes have no source bytes", missing_nodes_have_no_source_bytes}
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
