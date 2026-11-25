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
//
// One thing the verbatim copy must not do is run a verbalisation into the
// character beside it. A classified span becomes *words*, and a word with a
// stray letter or digit welded to its edge is never a reading of anything:
// `x22` came out `xtwenty-two` and `221B` came out `two hundred twenty-oneB`,
// because `x` and `B` are outside the span the grammar claimed and get copied
// through hard against it. So a space goes in when a rewritten span meets an
// alphanumeric on either side.
//
// Only alphanumeric, and only beside a rewritten span. Punctuation keeps
// touching, because punctuation touching a word is correct and load-bearing: a
// sentence-final full stop stays on its word, quotes and brackets stay around
// their span, and both are pinned by tests. That also means `-$40` is still
// `-forty dollars` rather than `minus forty dollars`; a sign wants a *reading*,
// which is a grammar's job, not the serialiser's, and spacing it to `- forty`
// would only make the gap harder to see.
std::string serialise(const std::string& input,
                      const std::vector<Segment>& segments,
                      std::vector<Placement>* placements) {
    auto alphanumeric = [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z');
    };
    // A lone trailing `s` is the one thing that really is a suffix on the
    // number word rather than a token beside it: "Under-14s" is "Under
    // fourteens" and wants no space, where "221B" is "two twenty-one B" and
    // does. Both arrive here identically, as a rewritten span followed by a
    // one-character PLAIN segment, so the plural has to be named.
    //
    // This is the whole measured cost of the rule. Without the exception the
    // held-out Google TN slice drops 94.0 to 93.9: fourteen sentences change,
    // twelve of them from things like "u sthirty-six million" and
    // "two oh sevenpoint four" to the spaced readings, and the only two
    // regressions are this plural.
    auto plural_suffix = [&](const std::string& right) {
        if (right.empty() || (right[0] != 's' && right[0] != 'S')) return false;
        return right.size() == 1 ||
               !alphanumeric(static_cast<unsigned char>(right[1]));
    };
    auto joins = [&](const std::string& left, const std::string& right) {
        return !left.empty() && !right.empty() &&
               alphanumeric(static_cast<unsigned char>(left.back())) &&
               alphanumeric(static_cast<unsigned char>(right.front())) &&
               !plural_suffix(right);
    };

    std::string out;
    size_t cursor = 0;
    // Whether the last thing appended was a rewritten span, so the rule applies
    // on the way out of one as well as on the way in. "221B" is two segments,
    // CARDINAL "221" and PLAIN "B", not a segment and a gap, so a rule that
    // only looked at the gaps missed exactly half the cases.
    bool after_rewrite = false;
    auto append = [&](const std::string& text, bool rewritten) -> size_t {
        if ((after_rewrite || rewritten) && joins(out, text)) out += ' ';
        size_t begin = out.size();
        out += text;
        after_rewrite = rewritten;
        return begin;
    };

    for (size_t i = 0; i < segments.size(); ++i) {
        const Segment& s = segments[i];
        if (s.start_byte > cursor)
            append(input.substr(cursor, s.start_byte - cursor), false);
        const bool rewritten =
            s.type != SemioticClass::PLAIN && s.output != s.source;
        size_t begin = append(s.output, rewritten);
        if (placements != nullptr && s.type != SemioticClass::PLAIN)
            placements->push_back({i, begin, out.size()});
        cursor = s.end_byte;
    }
    if (cursor < input.size()) append(input.substr(cursor), false);
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
