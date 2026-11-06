#ifndef BMINOR_ANSI_RENDERER_HPP
#define BMINOR_ANSI_RENDERER_HPP

#include "bminor/source_file.hpp"
#include "bminor/syntax_classifier.hpp"

#include <iosfwd>
#include <vector>

namespace bminor {

// Copy the original source to out, inserting SGR styles around classified
// spans. There are no headings, escaped lexemes or additional line endings.
// Spans must partition this source completely, in order, without EOF entries.
// Invalid spans throw invalid_argument before output; write failures throw
// runtime_error. File streams should be binary to preserve source bytes.
void render_ansi(std::ostream& out, const SourceFile& source,
                 const std::vector<ClassifiedSpan>& spans);

} // namespace bminor

#endif
