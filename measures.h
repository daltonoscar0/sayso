#pragma once
// Measure grammar: "5 km" <-> "five kilometers", "50%" <-> "fifty percent".
//
// An amount followed by a unit, with the unit word agreeing in grammatical
// number — the currency grammar's structure exactly, which is unsurprising,
// since a currency *is* a measure whose unit happens to be written in front of
// the amount instead of after it.
//
// The amount may be a cardinal or a decimal, so "3.5 kg" works without the
// measure grammar knowing anything about decimals beyond embedding the
// machine. Fractions are deliberately left out; see docs/DECISIONS.md.
//
// The unit table is where the judgement lives. It is not a list of every SI
// abbreviation: any unit whose written form is also an ordinary English word
// is left out, because CLASSIFY sees no context and would happily read "3 in
// the morning" as three inches. "in", "s", "h", "t" and "a" are excluded for
// that reason and no other.
#include "wfst.h"

namespace sayso {

const Cascade& measure_grammar();

}  // namespace sayso
