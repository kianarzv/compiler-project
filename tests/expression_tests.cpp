#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using bminor::DiagnosticCode;
using bminor::Expr;
using bminor::NodeKind;

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

bminor::ParseResult parse_source(const std::string& text) {
    const bminor::SourceFile source{"expr.bminor", text};
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    require(!lexed.has_errors(), "Expression fixture has a lexical error.");
    bminor::Parser parser(lexed.tokens);
    return parser.parse();
}

std::unique_ptr<Expr> expression(const std::string& text) {
    auto parsed = parse_source("result: integer = " + text + ";");
    if (parsed.has_errors()) {
        throw std::runtime_error("Rejected " + text + ": " + parsed.diagnostics[0].message);
    }
    auto& declaration = static_cast<bminor::VarDecl&>(*parsed.program->declarations[0]);
    return std::move(declaration.initializer);
}

// Render only the expression's structure. Expected shapes below are independent
// of indentation, source paths and the production AST printer's formatting.
std::string shape(const Expr& node) {
    require(!node.semantic_type && node.location.filename == "expr.bminor",
            "Expression metadata was lost or assigned a semantic type.");
    switch (node.kind) {
    case NodeKind::IdentifierExpr:
        return static_cast<const bminor::IdentifierExpr&>(node).name;
    case NodeKind::LiteralExpr:
        return static_cast<const bminor::LiteralExpr&>(node).literal.lexeme;
    case NodeKind::UnaryExpr: {
        const auto& unary = static_cast<const bminor::UnaryExpr&>(node);
        return "(" + unary.operation.lexeme + " " + shape(*unary.operand) + ")";
    }
    case NodeKind::BinaryExpr: {
        const auto& binary = static_cast<const bminor::BinaryExpr&>(node);
        return "(" + binary.operation.lexeme + " " + shape(*binary.left)
            + " " + shape(*binary.right) + ")";
    }
    case NodeKind::GroupExpr:
        return "(group " + shape(*static_cast<const bminor::GroupExpr&>(node).expression) + ")";
    case NodeKind::PostfixExpr: {
        const auto& postfix = static_cast<const bminor::PostfixExpr&>(node);
        return "(post" + postfix.operation.lexeme + " " + shape(*postfix.operand) + ")";
    }
    case NodeKind::IndexExpr: {
        const auto& index = static_cast<const bminor::IndexExpr&>(node);
        return "(index " + shape(*index.base) + " " + shape(*index.index) + ")";
    }
    case NodeKind::CallExpr: {
        const auto& call = static_cast<const bminor::CallExpr&>(node);
        std::string result = "(call " + shape(*call.callee);
        for (const auto& argument : call.arguments) { result += " " + shape(*argument); }
        return result + ")";
    }
    case NodeKind::ArrayInitExpr: {
        std::string result = "(array";
        for (const auto& item : static_cast<const bminor::ArrayInitExpr&>(node).elements) {
            result += " " + shape(*item);
        }
        return result + ")";
    }
    default: throw std::runtime_error("Unexpected non-expression node.");
    }
}

void expect_shape(const std::string& text, const std::string& expected) {
    const auto parsed = expression(text);
    const std::string actual = shape(*parsed);
    require(actual == expected, text + " grouped as " + actual + ", expected " + expected);
}

void arithmetic_precedence() {
    expect_shape("1+2*3", "(+ 1 (* 2 3))");
    expect_shape("(1+2)*3", "(* (group (+ 1 2)) 3)");
    expect_shape("a*b+c/d", "(+ (* a b) (/ c d))");
    expect_shape("a-b-c", "(- (- a b) c)");
    expect_shape("20/5%3", "(% (/ 20 5) 3)");
    expect_shape("a*b^c", "(* a (^ b c))");
}

void comparisons_and_logic() {
    expect_shape("a+b<c*d", "(< (+ a b) (* c d))");
    expect_shape("a==b<c", "(== a (< b c))");
    expect_shape("a!=b>=c", "(!= a (>= b c))");
    expect_shape("a&&b==c", "(&& a (== b c))");
    expect_shape("a||b&&c", "(|| a (&& b c))");
    expect_shape("a<=b>c", "(> (<= a b) c)");
    expect_shape("a==b!=c", "(!= (== a b) c)");
    expect_shape("true||false||flag", "(|| (|| true false) flag)");
}

void right_associative_operations() {
    expect_shape("a=b=c", "(= a (= b c))");
    expect_shape("a=b||c&&d", "(= a (|| b (&& c d)))");
    expect_shape("2^3^2", "(^ 2 (^ 3 2))");
    expect_shape("-2^2", "(^ (- 2) 2)");
    expect_shape("2^-2", "(^ 2 (- 2))");
    // Assignment-target validity is intentionally left to semantic analysis.
    expect_shape("1=2", "(= 1 2)");
}

void prefix_and_postfix() {
    expect_shape("!-a++", "(! (- (post++ a)))");
    expect_shape("a+++b", "(+ (post++ a) b)");
    expect_shape("a---b", "(- (post-- a) b)");
    expect_shape("-(a+b)", "(- (group (+ a b)))");
    expect_shape("a++--", "(post-- (post++ a))");
}

void calls_and_indices() {
    expect_shape("f()", "(call f)");
    expect_shape("f(a=b,c)", "(call f (= a b) c)");
    expect_shape("f((a+b),g())", "(call f (group (+ a b)) (call g))");
    expect_shape("a[f(1)][i]", "(index (index a (call f 1)) i)");
    expect_shape("factory()(a)[i+1]++", "(post++ (index (call (call factory) a) (+ i 1)))");
    expect_shape("(f)(x)", "(call (group f) x)");
}

void bounds_and_initializers() {
    const auto parsed = parse_source(
        "a: array [n+1] integer = {n,n*2,f(3)}; "
        "m: array [2] array [k] integer = {{1+1,2},{a[0],-3}}; "
        "show: function void (items: array [] array [n+1] integer);");
    require(!parsed.has_errors(), "Expression-valued bounds/initializers were rejected.");
    const auto& a = static_cast<const bminor::VarDecl&>(*parsed.program->declarations[0]);
    require(shape(*a.declared_type->array_size) == "(+ n 1)", "Array bound was not an expression.");
    require(shape(*a.initializer) == "(array n (* n 2) (call f 3))", "Array elements lost expressions.");
    const auto& m = static_cast<const bminor::VarDecl&>(*parsed.program->declarations[1]);
    require(shape(*m.initializer) == "(array (array (+ 1 1) 2) (array (index a 0) (- 3)))",
            "Nested array initializers lost expression structure.");
    const auto& function = static_cast<const bminor::FuncDecl&>(*parsed.program->declarations[2]);
    const auto& type = *function.parameters[0]->declared_type;
    require(!type.array_size && shape(*type.element_type->array_size) == "(+ n 1)",
            "Sized/unsized parameter array dimensions were confused.");
}

void expression_locations() {
    const std::string text = "value: integer = (items[index + 1]++) * -f(2);";
    const auto parsed = parse_source(text);
    require(!parsed.has_errors(), "Location sample was rejected.");
    const auto& variable = static_cast<const bminor::VarDecl&>(*parsed.program->declarations[0]);
    const auto& binary = static_cast<const bminor::BinaryExpr&>(*variable.initializer);
    require(binary.location.offset == text.find('(')
                && binary.operation.location.offset == text.find('*'),
            "Binary node/operator locations were conflated.");
    const auto& group = static_cast<const bminor::GroupExpr&>(*binary.left);
    const auto& postfix = static_cast<const bminor::PostfixExpr&>(*group.expression);
    const auto& index = static_cast<const bminor::IndexExpr&>(*postfix.operand);
    require(group.location.offset == text.find('(')
                && postfix.location.offset == text.find("items")
                && postfix.operation.location.offset == text.find("++")
                && index.opening.location.offset == text.find('[')
                && index.base->location.offset == text.find("items"),
            "Grouping or suffix parsing overwrote a leaf's first token.");
    const auto& unary = static_cast<const bminor::UnaryExpr&>(*binary.right);
    const auto& call = static_cast<const bminor::CallExpr&>(*unary.operand);
    require(unary.location.offset == text.find("-f")
                && call.location.offset == text.find("f(")
                && call.opening.location.offset == text.find("(2")
                && call.arguments[0]->location.column == text.find('2') + 1,
            "Prefix/call source positions were lost.");
    shape(*variable.initializer); // Also visits all children and checks metadata.
}

void malformed_expressions() {
    for (const std::string text : {"", "1+", "f(", "f(1,)", "a[]", "(1+2", "a[1",
            "1 2", "1,2", "++a", "--a", "+1", "if", "1<", "f(1 2)",
            "a[1,2]", "f({1})", "a**b", "a="}) {
        const auto parsed = parse_source("result: integer = " + text + ";");
        require(parsed.has_errors() && parsed.diagnostics.size() == 1
                    && parsed.diagnostics[0].code == DiagnosticCode::UnexpectedToken
                    && parsed.program->declarations.size() == 1
                    && parsed.program->declarations[0]->kind == NodeKind::ErrorDecl,
                "Malformed expression was accepted: " + text);
    }
    const std::string eof_text = "result: integer = 1+";
    const auto eof = parse_source(eof_text);
    require(eof.has_errors() && eof.diagnostics[0].length == 0
                && eof.diagnostics[0].location.offset == eof_text.size(),
            "Missing right operand at EOF has the wrong diagnostic span.");
    const std::string comment_text = "result: integer = 1 + /* comment */ ;";
    const auto comment = parse_source(comment_text);
    require(comment.has_errors() && comment.diagnostics[0].location.offset == comment_text.find(';'),
            "Missing operand diagnostic must skip trivia and point to the current token.");
}

void bounded_expression_depth() {
    const std::string unary = std::string(160, '!') + "true";
    const std::string groups = std::string(160, '(') + "1" + std::string(160, ')');
    std::string left = "1", assignment = "a", power = "1", postfix = "f", calls = "0";
    for (int i = 0; i < 160; ++i) {
        left += "+1";
        assignment += "=a";
        power += "^1";
        postfix += "()";
        calls = "f(" + calls + ")";
    }
    for (const auto& text : {unary, groups, left, assignment, power, postfix, calls}) {
        const auto parsed = parse_source("result: integer = " + text + ";");
        require(parsed.has_errors() && parsed.diagnostics.size() == 1
                    && parsed.diagnostics[0].code == DiagnosticCode::ParserNestingLimit,
                "Deep expression did not produce a controlled resource diagnostic.");
    }
}

void wide_calls_and_depth_boundary() {
    std::string call = "f(1";
    for (int i = 1; i < 300; ++i) { call += ",1"; }
    call += ")";
    const auto wide = expression(call);
    require(wide->kind == NodeKind::CallExpr && wide->depth == 2
                && static_cast<const bminor::CallExpr&>(*wide).arguments.size() == 300,
            "A wide shallow call was incorrectly treated as deeply nested.");
    std::string left = "1";
    for (int i = 1; i < 128; ++i) { left += "+1"; }
    const auto boundary = expression(left);
    require(boundary->depth == 128, "Valid expression at the depth boundary was rejected.");
    const auto too_deep = parse_source("result: integer = " + left + "+1;");
    require(too_deep.has_errors()
                && too_deep.diagnostics[0].code == DiagnosticCode::ParserNestingLimit,
            "Expression depth 129 must be rejected safely.");
}

} // namespace

int main() {
    struct TestCase { const char* name; void (*run)(); };
    const TestCase cases[] = {
        {"arithmetic precedence", arithmetic_precedence},
        {"comparison and logical precedence", comparisons_and_logic},
        {"right-associated assignment and power", right_associative_operations},
        {"prefix/postfix grouping", prefix_and_postfix},
        {"calls and indices", calls_and_indices},
        {"bounds and initializers", bounds_and_initializers},
        {"expression source locations", expression_locations},
        {"malformed expressions", malformed_expressions},
        {"bounded expression depth", bounded_expression_depth},
        {"wide calls and depth boundary", wide_calls_and_depth_boundary}
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
