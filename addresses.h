#pragma once
// Street addresses: "221 Baker St" -> "two hundred twenty-one Baker Street".
//
// The class exists because the suffix abbreviations collide with words Sayso
// already reads. "St" is "saint" 3,205 times against "street" 156 in the
// corpus ABBREV was built from, and "Dr" is "doctor" 1,302 times against a
// drive it never saw. A table cannot tell those apart, and neither can a
// regex ordering. What tells them apart is the rest of the span.
//
// So ADDRESS claims *more than the abbreviation*: the street name in front of
// it, and the house number in front of that. That is the whole idea. A
// grammar in Sayso sees only its own span and has no context, so the way to
// condition on context is to put the context inside the span and let the
// classifier's lattice price the two readings against each other. "Lee Dr."
// parses here and "Dr. Lee" does not, because a suffix follows its street
// name and a title precedes its surname; where both parse, the weights below
// decide.
//
// Two structural constraints keep the class out of ordinary prose.
//
// The street name must be *capitalised*. Street names are proper nouns, so
// this costs nothing real, and without it a free pass-through would let
// ADDRESS eat its way leftwards through a sentence: "a live band on Reading
// Rd" would parse "band on Reading" as three name words, which is cheaper
// than leaving them to PLAIN. Requiring a capital is what makes "on" and
// "band" unparseable rather than merely expensive. This is the one grammar
// besides LETTERS that asks for its span with the case intact.
//
// And an ambiguous suffix wants a house number. "St" and "Dr" pay
// kUnanchoredAmbiguousSuffix when nothing numeric precedes the street name,
// which is enough to lose to ABBREV and NAME over the same characters. With
// a house number in front they pay nothing, because "221 Baker St" has only
// one reading. The unambiguous suffixes -- Rd, Blvd, Ave, Ln, Ct, Pkwy, Hwy --
// never pay it, which is why "Reading Rd" is a road with no number in sight.
#include "style.h"
#include "wfst.h"

namespace sayso {

const Cascade& address_grammar();

// Numbered streets are enumerated, one path each, the way times.cpp
// enumerates its hours: a street ordinal is a small integer with an
// irregular spoken form, and 199 paths is cheaper than composing the ordinal
// cascade into this one. Manhattan's numbered streets stop at 220th, so the
// bound clips a handful of real addresses and no common ones.
constexpr int kHighestNumberedStreet = 199;

}  // namespace sayso
