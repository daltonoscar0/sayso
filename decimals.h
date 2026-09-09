#pragma once
// Decimal grammar: "3.14" <-> "three point one four".
//
// The integer part is a cardinal and everything after the point is read digit
// by digit, which is what English does: "three point one four", never "three
// point fourteen". The cardinal grammar already has a digit-by-digit path, but
// it is priced as a last resort; here the same reading is free, because here
// it is the only correct one.
//
// A magnitude word is deliberately *not* part of this grammar. "3.5 million"
// comes out right without one, DECIMAL claims "3.5" and "million" is a word
// PLAIN copies through, in both directions, so putting the magnitudes in the
// machine would only add states to say what falls out anyway.
#include "wfst.h"

namespace sayso {

const Cascade& decimal_grammar();

// Unsigned and unhyphenated, for grammars that embed a decimal amount. A
// caller that wants one hyphenates the whole span itself, once, rather than
// receiving a machine that has already been through the compound-joining pass.
const Wfst& decimal_core_fst();

// "either a cardinal or a decimal", which is what an amount in front of a unit
// is. Not the same as the union of the two: a decimal *contains* a cardinal,
// so unioning them naively leaves two copies of the cardinal machine in the
// result, and a grammar that embeds this once per unit pays for both.
const Wfst& number_or_decimal_fst();

}  // namespace sayso
