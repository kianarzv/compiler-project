#include "bminor/render_support.hpp"

#include <stdexcept>

namespace bminor {

void validate_render_spans(const SourceFile& source,
                           const std::vector<ClassifiedSpan>& spans) {
    std::size_t next_offset = 0;
    for (const auto& span : spans) {
        // Each successful iteration keeps next_offset within the source.
        // Subtraction avoids overflow from an untrusted offset + length.
        if (span.location.filename != source.path || span.location.offset != next_offset
            || span.length == 0 || span.length > source.text.size() - next_offset) {
            throw std::invalid_argument("Rendering spans must partition the matching source file.");
        }
        next_offset += span.length;
    }
    if (next_offset != source.text.size()) {
        throw std::invalid_argument("Rendering spans must cover the complete source file.");
    }
}

} // namespace bminor
