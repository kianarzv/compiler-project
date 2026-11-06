#ifndef BMINOR_SOURCE_LOCATION_HPP
#define BMINOR_SOURCE_LOCATION_HPP

#include <cstddef>
#include <string>

namespace bminor {

// A token starts at a one-based line and column, and a zero-based byte offset.
// Columns count bytes, so a tab advances one column rather than a display tab
// stop. The filename makes locations usable in diagnostics from multiple files.
struct SourceLocation {
    std::string filename;
    std::size_t line;
    std::size_t column;
    std::size_t offset;
};

} // namespace bminor

#endif
