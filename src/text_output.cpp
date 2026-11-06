#include "bminor/text_output.hpp"

#include <ostream>

namespace bminor {

void write_escaped(std::ostream& out, const std::string& text) {
    const char hex[] = "0123456789ABCDEF";
    out << '"';
    for (unsigned char c : text) {
        switch (c) {
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        case '\\': out << "\\\\"; break;
        case '"':  out << "\\\""; break;
        default:
            if (c < 32 || c >= 127) {
                out << "\\x" << hex[c >> 4] << hex[c & 15];
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    out << '"';
}

} // namespace bminor
