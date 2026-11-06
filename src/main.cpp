#include "bminor/source_file.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {

void print_usage(std::ostream& out) {
    out << "B-Minor Front End - Step 1\n"
        << "Usage: bminor <source-file>\n"
        << "       bminor --help\n\n"
        << "Load a source file and display its contents.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }

    if (argc != 2) {
        print_usage(std::cerr);
        return 2;
    }

    const std::string path = argv[1];
    if (!path.empty() && path[0] == '-') {
        std::cerr << "Unknown option: " << path << '\n';
        print_usage(std::cerr);
        return 2;
    }

    try {
        const bminor::SourceFile source = bminor::SourceFile::load(path);
        std::cout << "Loaded: " << source.path << '\n'
                  << "Bytes: " << source.text.size() << '\n'
                  << "Lines: " << source.line_count() << "\n\n"
                  << source.text;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
