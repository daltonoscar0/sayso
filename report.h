#pragma once
// The pipeline contract's StageResult, rendered as JSON.
//
// The document is built here, in C++, rather than assembled by the Python
// wrapper, and that is deliberate. `decisions` are readings off the
// classifier's lattice and their `score`s are the tropical-semiring costs that
// chose between them; `alternatives` are the paths that lost. All of it is
// weights.h seen from outside. Rebuilding any of it on the other side of a pipe
// would be a second copy of the weighting scheme, free to drift from the one
// that actually decided.
//
// So the binary prints the whole document and the Python package parses it. The
// CLI and the library API cannot disagree, because there is only one of them.
#include <string>

#include "classify.h"
#include "normalizer.h"

namespace sayso {

// One JSON document, on one line, for one line of input.
std::string stage_result_json(const std::string& input, Direction dir,
                              DateSpeech speech, Style style);

}  // namespace sayso
