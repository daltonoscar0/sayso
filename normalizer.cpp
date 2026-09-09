#include "normalizer.h"

#include "text.h"

namespace sayso {

namespace {

// The serialiser, in one place. Every segment knows the byte range it came
// from, so the text between segments is copied through verbatim and only the
// classified spans are rewritten; whitespace, quoting and punctuation outside a
// semiotic span survive untouched.
//
// `placements`, when given, receives where each non-PLAIN segment's output
// landed in the string being built.
std::string serialise(const std::string& input,
                      const std::vector<Segment>& segments,
                      std::vector<Placement>* placements) {
    std::string out;
    size_t cursor = 0;
    for (size_t i = 0; i < segments.size(); ++i) {
        const Segment& s = segments[i];
        if (s.start_byte > cursor) out += input.substr(cursor, s.start_byte - cursor);
        size_t begin = out.size();
        out += s.output;
        if (placements != nullptr && s.type != SemioticClass::PLAIN)
            placements->push_back({i, begin, out.size()});
        cursor = s.end_byte;
    }
    if (cursor < input.size()) out += input.substr(cursor);
    return out;
}

}  // namespace

std::vector<Segment> analyse(const std::string& input, Direction dir,
                             DateSpeech speech, Style style) {
    return classify(input, tokenize(input), dir, speech, style);
}

std::string normalize(const std::string& input, Direction dir,
                      DateSpeech speech, Style style) {
    std::vector<Segment> segments = analyse(input, dir, speech, style);
    if (segments.empty()) return input;
    return serialise(input, segments, nullptr);
}

Analysis analyse_fully(const std::string& input, Direction dir,
                       DateSpeech speech, Style style) {
    Analysis analysis;
    analysis.segments =
        classify(input, tokenize(input), dir, speech, style, &analysis.considered);
    if (analysis.segments.empty()) {
        analysis.output = input;
        return analysis;
    }
    analysis.output = serialise(input, analysis.segments, &analysis.placements);
    return analysis;
}

}  // namespace sayso
