#ifndef BMINOR_SOURCE_FILE_HPP
#define BMINOR_SOURCE_FILE_HPP

#include <cstddef>
#include <string>

namespace bminor {

struct SourceFile {
    std::string path;
    std::string text;

    static SourceFile load(const std::string& path);
    std::size_t line_count() const;
};

} // namespace bminor

#endif
