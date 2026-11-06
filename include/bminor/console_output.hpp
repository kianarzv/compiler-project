#ifndef BMINOR_CONSOLE_OUTPUT_HPP
#define BMINOR_CONSOLE_OUTPUT_HPP

namespace bminor {

// Call before writing a rendered document to stdout. On Windows this disables CRT
// newline translation; elsewhere stdout already preserves the original bytes.
// This CLI adapter is separate from the portable ostream-based renderer.
void prepare_binary_stdout();

} // namespace bminor

#endif
