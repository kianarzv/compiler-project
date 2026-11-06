#include "bminor/lexer.hpp"
#include "bminor/source_file.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
    std::string path;
    bool show_tokens = false;
    bool show_whitespace = false;
};

void print_usage(std::ostream& out) {
    out << "B-Minor Front End - Step 2\n"
        << "Usage: bminor [--tokens [--show-whitespace]] [--] <source-file>\n"
        << "       bminor --help\n\n"
        << "Default: display the source file and its size.\n"
        << "--tokens: show token kinds, lexemes and source locations.\n"
        << "--show-whitespace: include whitespace rows in the token display.\n\n"
        << "Step 2 recognizes words, boolean literals and whitespace.\n"
        << "Other bytes are shown as INVALID until the lexer is extended.\n";
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
    return options;
}

void write_escaped(std::ostream& out, const std::string& text) {
    // Each lexeme occupies one table row. Escape tabs, newlines, quotes and
    // non-printable bytes for display only; the token's stored text is intact.
    const char hex[] = "0123456789ABCDEF";
    out << '"';
    for (unsigned char c : text) {
        switch (c) {
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        case '\\': out << "\\\\"; break;
        case '"':  out << "\\\""; break;
        default:
            if (c < 32 || c >= 127) {
                out << "\\x" << hex[c >> 4] << hex[c & 15];
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    out << '"';
}

void print_tokens(const bminor::SourceFile& source, bool show_whitespace) {
    bminor::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    std::cout << "Location\tKind\tLexeme\n";
    for (const auto& token : tokens) {
        // Keep trivia in the lexer result for future renderers. Hiding it is
        // only a display choice and never changes source positions or offsets.
        if (!show_whitespace && token.kind == bminor::TokenKind::Whitespace) {
            continue;
        }
        std::cout << token.location.filename << ':' << token.location.line
                  << ':' << token.location.column << '\t'
                  << bminor::token_kind_name(token.kind) << '\t';
        write_escaped(std::cout, token.lexeme);
        std::cout << '\n';
    }
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
            print_tokens(source, options.show_whitespace);
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
