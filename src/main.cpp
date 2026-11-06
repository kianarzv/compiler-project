#include "bminor/ansi_renderer.hpp"
#include "bminor/console_output.hpp"
#include "bminor/html_renderer.hpp"
#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"
#include "bminor/source_file.hpp"
#include "bminor/syntax_classifier.hpp"
#include "bminor/text_output.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

enum class OutputMode { Source, Tokens, Ast, Classify, Ansi, Html };

struct Options {
    std::string path;
    OutputMode mode = OutputMode::Source;
    bool show_whitespace = false;
    std::string output_path;
};

void print_usage(std::ostream& out) {
    out << "B-Minor Front End - Step 11\n"
        << "Usage: bminor [--tokens | --ast | --classify | --ansi | --html] [options] [--] <source-file>\n"
        << "       bminor --help\n\n"
        << "Default: display the source file and its size.\n"
        << "--tokens: show token kinds, lexemes and source locations.\n"
        << "--ast: parse declarations and show their syntax tree.\n"
        << "--classify: show source spans with AST-based syntax roles.\n"
        << "--ansi: highlight the complete source for an ANSI-capable terminal.\n"
        << "--html: emit a standalone HTML/CSS document on stdout.\n"
        << "--output <file>: save HTML directly to a file (requires --html).\n"
        << "--show-whitespace: include whitespace in token/classification tables.\n\n"
        << "Recognizes base B-Minor literals, operators and comments.\n"
        << "Unknown bytes or malformed tokens appear as INVALID.\n"
        << "Analysis errors are reported on stderr; exit code 1.\n"
        << "AST mode accepts declarations, expressions, function bodies and statements.\n"
        << "Syntax errors use panic-mode recovery; recovered ASTs include error nodes.\n";
}

void select_mode(Options& options, OutputMode mode) {
    if (options.mode != OutputMode::Source && options.mode != mode) {
        throw std::invalid_argument("Choose one output mode.");
    }
    options.mode = mode;
}

Options parse_options(int argc, char* argv[]) {
    Options options;
    bool read_options = true;
    bool has_path = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (read_options && argument == "--") {
            // Everything after -- is a path, even if it begins with a dash.
            read_options = false;
        } else if (read_options && argument == "--tokens") {
            select_mode(options, OutputMode::Tokens);
        } else if (read_options && argument == "--ast") {
            select_mode(options, OutputMode::Ast);
        } else if (read_options && argument == "--classify") {
            select_mode(options, OutputMode::Classify);
        } else if (read_options && argument == "--ansi") {
            select_mode(options, OutputMode::Ansi);
        } else if (read_options && argument == "--html") {
            select_mode(options, OutputMode::Html);
        } else if (read_options && argument == "--output") {
            if (!options.output_path.empty()) {
                throw std::invalid_argument("Specify --output only once.");
            }
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() || argv[i + 1][0] == '-') {
                throw std::invalid_argument("--output requires a file path (use ./ for a leading dash).");
            }
            options.output_path = argv[++i];
        } else if (read_options && argument == "--show-whitespace") {
            options.show_whitespace = true;
        } else if (read_options && !argument.empty() && argument[0] == '-') {
            throw std::invalid_argument("Unknown option: " + argument);
        } else if (has_path) {
            throw std::invalid_argument("Expected exactly one source file.");
        } else {
            options.path = argument;
            has_path = true;
        }
    }

    if (options.path.empty()) {
        throw std::invalid_argument("Missing source file.");
    }
    if (options.show_whitespace && options.mode != OutputMode::Tokens
        && options.mode != OutputMode::Classify) {
        throw std::invalid_argument("--show-whitespace requires --tokens or --classify.");
    }
    if (!options.output_path.empty() && options.mode != OutputMode::Html) {
        throw std::invalid_argument("--output requires --html.");
    }
    return options;
}

bool print_tokens(const bminor::SourceFile& source, bool show_whitespace) {
    bminor::Lexer lexer(source);
    const auto result = lexer.tokenize();
    std::cout << "Location\tKind\tLexeme\n";
    for (const auto& token : result.tokens) {
        // Keep trivia in the lexer result for future renderers. Hiding it is
        // only a display choice and never changes source positions or offsets.
        if (!show_whitespace && token.kind == bminor::TokenKind::Whitespace) {
            continue;
        }
        std::cout << token.location.filename << ':' << token.location.line
                  << ':' << token.location.column << '\t'
                  << bminor::token_kind_name(token.kind) << '\t';
        bminor::write_escaped(std::cout, token.lexeme);
        std::cout << '\n';
    }
    // Keep the recovered token table on stdout and messages on stderr. Errors
    // affect the final exit status, but do not prevent later tokens or EOF output.
    std::cout.flush();
    for (const auto& diagnostic : result.diagnostics) {
        bminor::write_diagnostic(std::cerr, diagnostic);
    }
    return !result.has_errors();
}

bool print_ast(const bminor::SourceFile& source) {
    bminor::Lexer lexer(source);
    const auto lexed = lexer.tokenize();
    if (lexed.has_errors()) {
        // Lexical recovery still collects every error. Avoid turning INVALID
        // tokens into a second set of misleading parser errors in this mode.
        for (const auto& error : lexed.diagnostics) {
            bminor::write_diagnostic(std::cerr, error);
        }
        return false;
    }

    bminor::Parser parser(lexed.tokens);
    const auto parsed = parser.parse();
    bminor::dump_ast(std::cout, *parsed.program);
    std::cout.flush();
    for (const auto& error : parsed.diagnostics) {
        bminor::write_diagnostic(std::cerr, error);
    }
    return !parsed.has_errors();
}

bool print_classification(const bminor::SourceFile& source, bool show_whitespace) {
    const auto result = bminor::classify_source(source);
    std::cout << "Location\tOffset\tLength\tRole\tError\tLexeme\n";
    for (const auto& span : result.spans) {
        if (!show_whitespace && span.role == bminor::SyntaxRole::Whitespace) { continue; }
        std::cout << span.location.filename << ':' << span.location.line << ':' << span.location.column
                  << '\t' << span.location.offset << '\t' << span.length << '\t'
                  << bminor::syntax_role_name(span.role) << '\t'
                  << (span.has_error ? "yes" : "no") << '\t';
        // The original source supplies text. Renderers can use the same offsets
        // without rebuilding code from AST nodes or normalizing whitespace.
        bminor::write_escaped(std::cout, source.text.substr(span.location.offset, span.length));
        std::cout << '\n';
    }
    std::cout.flush();
    for (const auto& error : result.diagnostics) { bminor::write_diagnostic(std::cerr, error); }
    return !result.has_errors();
}

bool print_ansi(const bminor::SourceFile& source) {
    const auto result = bminor::classify_source(source);
    bminor::prepare_binary_stdout();
    bminor::render_ansi(std::cout, source, result.spans);
    std::cout.flush();
    if (!std::cout) { throw std::runtime_error("Could not flush ANSI output."); }
    // Even a recovered or lexically invalid program has full source coverage.
    // Keep that preview on stdout and report analysis failures separately.
    for (const auto& error : result.diagnostics) { bminor::write_diagnostic(std::cerr, error); }
    return !result.has_errors();
}

void check_html_destination(const bminor::SourceFile& source, const std::string& path) {
    // Comparing spellings alone misses ./ aliases, case differences and hard
    // links. Conservatively refuse any existing file with the source's exact
    // contents, using only standard streams and at most one extra byte at a
    // time. A previous generated HTML document can still be replaced normally.
    std::ifstream existing(path, std::ios::binary);
    if (!existing) { return; }
    for (char expected : source.text) {
        char actual = 0;
        if (!existing.get(actual) || actual != expected) { return; }
    }
    if (existing.peek() == std::char_traits<char>::eof() && !existing.bad()) {
        throw std::runtime_error("Refusing to overwrite a file containing the source: " + path);
    }
}

bool print_html(const bminor::SourceFile& source, const std::string& output_path) {
    const auto result = bminor::classify_source(source);
    if (output_path.empty()) {
        bminor::prepare_binary_stdout();
        bminor::render_html(std::cout, source, result.spans);
        std::cout.flush();
        if (!std::cout) { throw std::runtime_error("Could not flush HTML output."); }
    } else {
        // Finish rendering before opening/truncating an output file. Analysis
        // errors are allowed: a recovered document is useful for inspection.
        std::ostringstream document;
        bminor::render_html(document, source, result.spans);
        check_html_destination(source, output_path);
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) { throw std::runtime_error("Cannot open HTML output: " + output_path); }
        const std::string text = document.str();
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.close();
        if (!output) { throw std::runtime_error("Cannot write HTML output: " + output_path); }
        std::cout << "Saved HTML: " << output_path << '\n';
    }
    for (const auto& error : result.diagnostics) { bminor::write_diagnostic(std::cerr, error); }
    return !result.has_errors();
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }

    try {
        const Options options = parse_options(argc, argv);
        const bminor::SourceFile source = bminor::SourceFile::load(options.path);
        if (options.mode == OutputMode::Tokens) {
            return print_tokens(source, options.show_whitespace) ? 0 : 1;
        } else if (options.mode == OutputMode::Ast) {
            return print_ast(source) ? 0 : 1;
        } else if (options.mode == OutputMode::Classify) {
            return print_classification(source, options.show_whitespace) ? 0 : 1;
        } else if (options.mode == OutputMode::Ansi) {
            return print_ansi(source) ? 0 : 1;
        } else if (options.mode == OutputMode::Html) {
            return print_html(source, options.output_path) ? 0 : 1;
        } else {
            std::cout << "Loaded: " << source.path << '\n'
                      << "Bytes: " << source.text.size() << '\n'
                      << "Lines: " << source.line_count() << "\n\n"
                      << source.text;
        }
        return 0;
    } catch (const std::invalid_argument& error) {
        // Usage mistakes are separate from failures while reading a file.
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(std::cerr);
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
