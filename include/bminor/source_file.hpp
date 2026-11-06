#ifndef BMINOR_SOURCE_FILE_HPP
#define BMINOR_SOURCE_FILE_HPP

#include <cstddef>
#include <string>

namespace bminor {

// Owns the original file contents. The lexer reads this text without modifying
// it, and later renderers can use token offsets to copy the same source back.
struct SourceFile {
    std::string path;
    std::string text;

    // Read in binary mode so Windows does not turn CRLF into LF. Throws a
    // runtime_error with the path if opening or reading the file fails.
    static SourceFile load(const std::string& path);

    // Count physical lines, accepting LF, CRLF, or CR as a line ending.
    // An empty file has zero lines; a final newline does not add another line.
    std::size_t line_count() const;
};

} // namespace bminor

#endif
