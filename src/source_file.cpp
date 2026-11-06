#include "bminor/source_file.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace bminor {

SourceFile SourceFile::load(const std::string& path) {
    // Text-mode reads can translate line endings on Windows. Binary mode keeps
    // the bytes unchanged, so a token's offset always refers to the original
    // file and future highlighting can preserve its exact formatting.
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open source file: " + path);
    }

    // The two iterators describe the whole file, including embedded nulls.
    // Reaching EOF is normal; bad() checks for an actual stream read failure.
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
            // A CRLF pair is one line ending: its CR was already counted.
            ++lines;
        }
    }

    // The loop counts line endings. If text remains after the last ending,
    // count that final line too. An empty file never enters this branch.
    if (!text.empty() && text.back() != '\n' && text.back() != '\r') {
        ++lines;
    }
    return lines;
}

} // namespace bminor
