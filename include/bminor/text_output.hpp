#ifndef BMINOR_TEXT_OUTPUT_HPP
#define BMINOR_TEXT_OUTPUT_HPP

#include <iosfwd>
#include <string>

namespace bminor {

// Quote and escape a source spelling for a readable single-line display.
// This is shared by token/AST output and never modifies the stored source.
void write_escaped(std::ostream& out, const std::string& text);

} // namespace bminor

#endif
