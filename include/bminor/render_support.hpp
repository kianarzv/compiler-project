#ifndef BMINOR_RENDER_SUPPORT_HPP
#define BMINOR_RENDER_SUPPORT_HPP

#include "bminor/source_file.hpp"
#include "bminor/syntax_classifier.hpp"

#include <vector>

namespace bminor {

// Both renderers require a complete, ordered partition of the same source.
// Validate before emitting a document so a bad span cannot produce partial
// output or an out-of-bounds source read. Throws std::invalid_argument.
void validate_render_spans(const SourceFile& source,
                           const std::vector<ClassifiedSpan>& spans);

} // namespace bminor

#endif
