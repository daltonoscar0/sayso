#pragma once
// Fraction grammar: "3/4" <-> "three quarters", "2 1/2" <-> "two and a half".
//
// A fraction is a cardinal numerator, a slash, and a denominator word that
// agrees with the numerator in grammatical number, the same priority-union-
// by-margin idiom the currency grammar uses for "dollar" against "dollars".
//
// The denominator is a table rather than a derivation. English ordinals do
// most of the work ("fifth" -> "fifths"), but the two commonest denominators
// are suppletive: 2 is "half", not "second", and 4 is "quarter" in preference
// to "fourth". A table states that directly, and a table is also where the
// decision to stop lives, see the note on compound denominators in
// docs/DECISIONS.md.
//
// The mixed form is where the "and" of "two and a half" comes from, and the
// article: standing alone, "1/2" is "one half", but after a whole number it is
// "a half". One weight expresses both preferences, applied in opposite
// directions to the two branches.
#include "style.h"
#include "wfst.h"

namespace sayso {

const Cascade& fraction_grammar(Style style = Style::Plain);

}  // namespace sayso
