#include "normalizer.h"

#include "text.h"

namespace sayso {

std::vector<Segment> analyse(const std::string& input, Direction dir,
                             DateSpeech speech) {
    return classify(input, tokenize(input), dir, speech);
}

std::string normalize(const std::string& input, Direction dir,
                      DateSpeech speech) {
    std::vector<Segment> segments = analyse(input, dir, speech);
    if (segments.empty()) return input;

    std::string out;
    size_t cursor = 0;
    for (const Segment& s : segments) {
        if (s.start_byte > cursor) out += input.substr(cursor, s.start_byte - cursor);
        out += s.output;
        cursor = s.end_byte;
    }
    if (cursor < input.size()) out += input.substr(cursor);
    return out;
}

}  // namespace sayso
