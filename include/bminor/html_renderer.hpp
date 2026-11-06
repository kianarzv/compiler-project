#ifndef BMINOR_HTML_RENDERER_HPP
#define BMINOR_HTML_RENDERER_HPP

#include "bminor/source_file.hpp"
#include "bminor/syntax_classifier.hpp"

#include <iosfwd>
#include <vector>

namespace bminor {

// Write a complete UTF-8 HTML document with embedded CSS and no scripts or
// external assets. Text comes from the source, not from AST pretty-printing.
// HTML metacharacters are escaped; CR uses a character reference to survive
// browser newline normalization. Unsupported control/invalid UTF-8 bytes are
// displayed as \xHH. Spans obey the same contract as render_ansi().
void render_html(std::ostream& out, const SourceFile& source,
                 const std::vector<ClassifiedSpan>& spans);

} // namespace bminor

#endif
