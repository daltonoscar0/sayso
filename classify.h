#pragma once
// CLASSIFY: tag token spans with a semiotic class.
//
// The stage is a weighted FST over token positions. States are the n+1 token
// boundaries of a sentence; an arc from i to j either leaves token i alone
// (PLAIN, priced per character) or claims tokens [i, j) for a class, priced
// at that class's entry cost plus the cost the class grammar assigns to the
// span. Every path covers the whole sentence, so the costs are directly
// comparable and best-path decoding gives longest-match/lowest-cost tagging
// without a separate longest-match rule: a long span is expensive to skip and
// cheap to claim.
//
// Both directions use the same lattice. Only the orientation of the class
// grammars changes — written->spoken for TTS, invert() of the very same
// machines for ITN.
#include <string>
#include <vector>

#include "../Scalpel/token.h"
#include "dates.h"
#include "wfst.h"

namespace sayso {

enum class SemioticClass {
    PLAIN, CARDINAL, ORDINAL, DECIMAL, FRACTION, MONEY, MEASURE, DATE, TIME, DECADE, ABBREV, LETTERS, RANGE
};
const char* class_name(SemioticClass c);

enum class Direction { TTS, ITN };

struct Segment {
    SemioticClass type = SemioticClass::PLAIN;
    size_t begin = 0, end = 0;  // token indices, half-open
    size_t start_byte = 0, end_byte = 0;
    std::string source;
    std::string output;
    float cost = 0.0f;
};

// A class grammar, always oriented written->spoken; classify() applies the
// cascade in reverse when asked for ITN.
//
// `speaking_only` marks the classes that must not be read backwards. This is
// not a second grammar and not a weakening of the one-grammar rule — the
// machine is still written once and still inverted for everything else. It
// records that a *class* is one-directional: expanding "St" to "saint" is
// normalisation, contracting "saint" to "St" is not, and no weight can express
// that difference here. PLAIN is priced per character, so a five-letter word
// costs 5.0 against a class entry cost of 0.05; a lexical class can always
// outbid leaving the word alone, whichever way it is pointing.
//
// Only classes whose written side is drawn from ordinary English need it.
// Every numeric class stays bidirectional, because there a plain numeral is
// already the cheaper written form and the weights settle it.
struct Grammar {
    SemioticClass type;
    const Cascade* grammar;
    float entry_cost;
    bool speaking_only = false;
};
// The registry depends on the date option, which selects one of two date
// cascades. Everything else in it is shared.
const std::vector<Grammar>& grammars(
    DateSpeech speech = DateSpeech::Canonical);

std::vector<Segment> classify(const std::string& input,
                              const std::vector<Token>& tokens, Direction dir,
                              DateSpeech speech = DateSpeech::Canonical);

// Longest span, in tokens, a class may claim. The two directions differ by a
// lot. The longest written span is "$1,234,567.89" at eight tokens. The
// longest spoken one is a spelled-out amount, and its length depends on
// whether its compounds are hyphenated — speech recognisers do not hyphenate,
// so ITN sees the long form: "nine hundred ninety nine billion ... nine
// hundred ninety nine dollars and ninety nine cents" is twenty-four tokens.
//
// Both caps are affordable because CLASSIFY runs each grammar once per span
// *start* rather than once per span (see Cascade::apply_prefixes), so cost
// grows with the longest span a grammar can actually match, not with the cap.
constexpr size_t kMaxWrittenSpanTokens = 8;
constexpr size_t kMaxSpokenSpanTokens = 24;
inline size_t max_span_tokens(Direction dir) {
    return dir == Direction::TTS ? kMaxWrittenSpanTokens : kMaxSpokenSpanTokens;
}

}  // namespace sayso
