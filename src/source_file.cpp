#include "bminor/source_file.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace bminor {

SourceFile SourceFile::load(const std::string& path) {
    // Keep the original bytes so highlighting can preserve source formatting.
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open source file: " + path);
    }

    std::string text((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
    if (input.bad()) {
        throw std::runtime_error("Cannot read source file: " + path);
    }

    return SourceFile{path, text};
}

std::size_t SourceFile::line_count() const {
    std::size_t lines = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            ++lines;
        } else if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
            ++lines;
        }
    }

    // A trailing newline ends the last line; it does not add an empty line.
    if (!text.empty() && text.back() != '\n' && text.back() != '\r') {
        ++lines;
    }
    return lines;
}

} // namespace bminor
