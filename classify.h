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
#include "wfst.h"

namespace sayso {

enum class SemioticClass { PLAIN, CARDINAL, ORDINAL, MONEY, DATE };
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
struct Grammar {
    SemioticClass type;
    const Cascade* grammar;
    float entry_cost;
};
const std::vector<Grammar>& grammars();

std::vector<Segment> classify(const std::string& input,
                              const std::vector<Token>& tokens, Direction dir);

// Longest span, in tokens, a class may claim. The two directions differ by a
// lot. The longest written span is "$1,234,567.89" at eight tokens. The
// longest spoken one is a spelled-out cardinal, whose length depends on
// whether its compounds are hyphenated — and speech recognisers do not
// hyphenate, so ITN sees the long form. "nine hundred ninety nine million
// nine hundred ninety nine thousand nine hundred ninety nine" is fourteen
// tokens, fifteen with a sign word; sixteen covers every unhyphenated
// cardinal below a billion.
//
// Cost grows sharply with this number — raising it from twelve to twenty cost
// fivefold throughput — so the two directions get separate caps and the
// spoken one is set to what unhyphenated speech actually needs, not to the
// grammar's twelve-digit ceiling. Spelled-out *billions* are consequently
// classified in pieces; they remain correct written-to-spoken.
constexpr size_t kMaxWrittenSpanTokens = 8;
constexpr size_t kMaxSpokenSpanTokens = 16;
inline size_t max_span_tokens(Direction dir) {
    return dir == Direction::TTS ? kMaxWrittenSpanTokens : kMaxSpokenSpanTokens;
}

}  // namespace sayso
