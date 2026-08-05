#pragma once
// Abbreviation grammar: "St" <-> "saint", "vol" <-> "volume", "Dr" <-> "doctor".
//
// A lexical table, and the first class in Sayso whose written side is drawn
// from ordinary English rather than from digits and symbols. That is what
// makes it one-directional: expanding "St" to "saint" is normalisation, and
// contracting every "saint" back to "St" is not. CLASSIFY prices PLAIN per
// character, so a five-letter word costs 5.0 against this class's entry cost
// of 0.05 — there is no weight that lets a lexical class lose to leaving the
// word alone. The registry marks it `speaking_only` instead; see classify.h.
//
// The table is short on purpose. Every entry earned its place by being an
// expansion the Google TN reference makes and almost never declines to make:
// "no" is excluded because it is the word "no" 8,805 times against 288
// expansions to "number", and "min", "col" and "co" are excluded for the same
// reason. What is left is the set where the abbreviation reading is at least
// twenty times commoner than the ordinary word.
#include "wfst.h"

namespace sayso {

const Cascade& abbreviation_grammar();

}  // namespace sayso
