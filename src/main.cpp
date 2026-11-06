#include "bminor/lexer.hpp"
#include "bminor/parser.hpp"
#include "bminor/source_file.hpp"
#include "bminor/text_output.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
    std::string path;
    bool show_tokens = false;
    bool show_ast = false;
    bool show_whitespace = false;
};

void print_usage(std::ostream& out) {
    out << "B-Minor Front End - Step 7\n"
        << "Usage: bminor [--tokens | --ast] [--show-whitespace] [--] <source-file>\n"
        << "       bminor --help\n\n"
        << "Default: display the source file and its size.\n"
        << "--tokens: show token kinds, lexemes and source locations.\n"
        << "--ast: parse declarations and show their syntax tree.\n"
        << "--show-whitespace: include whitespace rows in the token display.\n\n"
        << "Recognizes base B-Minor literals, operators and comments.\n"
        << "Unknown bytes or malformed tokens appear as INVALID.\n"
        << "In token/AST mode, errors are reported on stderr; exit code 1.\n"
        << "AST mode accepts declarations, expressions, function bodies and statements.\n"
        << "Syntax parsing stops at its first error; recovery arrives in step 8.\n";
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
            options.show_tokens = true;
        } else if (read_options && argument == "--ast") {
            options.show_ast = true;
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
    if (options.show_whitespace && !options.show_tokens) {
        throw std::invalid_argument("--show-whitespace requires --tokens.");
    }
    if (options.show_tokens && options.show_ast) {
        throw std::invalid_argument("Choose either --tokens or --ast.");
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

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }

    try {
        const Options options = parse_options(argc, argv);
        const bminor::SourceFile source = bminor::SourceFile::load(options.path);
        if (options.show_tokens) {
            return print_tokens(source, options.show_whitespace) ? 0 : 1;
        } else if (options.show_ast) {
            return print_ast(source) ? 0 : 1;
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
