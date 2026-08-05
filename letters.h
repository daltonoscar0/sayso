#pragma once
// Letter grammar: "U.S." <-> "u s", "J." <-> "j", "e.g." <-> "e g".
//
// Dotted shapes only, and that restriction is forced rather than chosen.
// CLASSIFY folds a span's case before any grammar sees it, so nothing here can
// tell "PDF" from "pdf" — and a rule for bare letter runs would therefore
// spell out every lower-case word it met. The dots are the only signal that
// survives folding, and they are an unambiguous one: no English word contains
// a full stop between two of its letters.
//
// That costs coverage. In the Google TN data the bare all-caps run is the
// largest shape by some way, and Sayso reads none of it: "PDF" stays "PDF".
// The alternative was a grammar that turned "the" into "t h e".
//
// One-directional, for the reason abbrev.h sets out at more length: run
// backwards this machine turns any two spoken single letters into an acronym,
// and "a" and "i" are ordinary English words.
#include "wfst.h"

namespace sayso {

const Cascade& letters_grammar();

}  // namespace sayso
