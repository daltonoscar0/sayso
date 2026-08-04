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

std::string normalize(const std::string& input, Direction dir);

// Same pipeline, but hands back the tagging as well — used by the CLI's
// --explain mode and by the evaluation harness's per-class scoring.
std::vector<Segment> analyse(const std::string& input, Direction dir);

}  // namespace sayso
