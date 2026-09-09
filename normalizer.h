#pragma once
// The top-level pipeline.
//
//   text -> Scalpel tokeniser -> CLASSIFY -> per-class grammar -> serialise
//
// Serialisation is not a detokeniser: every segment knows the byte range it
// came from, so the text between segments is copied through verbatim and only
// the classified spans are rewritten. Whitespace, quoting and punctuation
// outside a semiotic span therefore survive untouched.
#include <string>
#include <vector>

#include "classify.h"

namespace sayso {

std::string normalize(const std::string& input, Direction dir,
                      DateSpeech speech = DateSpeech::Canonical,
                      Style style = Style::Plain);

// Same pipeline, but hands back the tagging as well, used by the CLI's
// --explain mode and by the evaluation harness's per-class scoring.
std::vector<Segment> analyse(const std::string& input, Direction dir,
                             DateSpeech speech = DateSpeech::Canonical,
                             Style style = Style::Plain);

// Where a segment's output landed in the finished string.
//
// The serialiser copies the text between segments through verbatim, so a
// segment's offsets in the output are not its offsets in the input and cannot
// be derived from them. A caller that has to point at a span of the *spoken*
// side, to say which words are a proper name, say, needs the pipeline to
// record it on the way past.
struct Placement {
    size_t segment = 0;              // index into Analysis::segments
    size_t out_begin = 0, out_end = 0;
};

// Everything one pass produces: the text, the tagging, the readings that lost,
// and where each segment ended up. This is what the pipeline contract's
// StageResult is built from; see report.h.
struct Analysis {
    std::string output;
    std::vector<Segment> segments;
    std::vector<Reading> considered;
    std::vector<Placement> placements;  // one per non-PLAIN segment
};

Analysis analyse_fully(const std::string& input, Direction dir,
                       DateSpeech speech = DateSpeech::Canonical,
                       Style style = Style::Plain);

}  // namespace sayso
