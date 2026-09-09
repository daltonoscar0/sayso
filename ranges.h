#pragma once
// Range grammar: "1-10" -> "one to ten", "1729-1811" -> "seventeen twenty nine
// to eighteen eleven".
//
// A four-digit operand is read as a year, because it is a bare number and that
// is what a bare number gets.
//
// Whether a hyphen between two numbers means "to" is not decidable from the
// hyphen, only from what flanks it: in the Google TN data a hyphen with digits
// on both sides is "to" 84% of the time and silent otherwise, while a hyphen
// with a word on either side is silent essentially always. That is a
// span-local fact, which is the only kind CLASSIFY can act on.
//
// 84% is not the honest figure, though, and it is worth writing down why. It
// is measured only over hyphens that Kestrel had already decided were
// separators and split into their own token. Sayso's tokeniser makes no such
// decision: it also splits the ISBNs, phone numbers and model numbers Kestrel
// kept whole, and this shape fires inside thousands of them. Counting those,
// precision is nearer 70%. Two structural guards claw back most of the
// difference, operands are at most four digits with no leading zero, and the
// year-month prefix of an ISO date is refused outright, but a range remains
// the least certain thing Sayso claims, which is what its entry cost says.
//
// Hyphen only. The en dash does not appear in the corpus; every em dash is
// silent; and the colon, which looks like the same construction, turns out to
// be 95.7% bibliographic citation, "Astrophysical Journal 94: 501" read as
// "ninety four to five hundred one" is a reference-list artefact rather than
// anything a person says, and admitting it would put this class up against
// TIME on every clock in the corpus.
//
// One-directional, and for a different reason from the other two classes that
// are. Written "5-10" spoken as "five to ten" is normalisation; spoken "five
// to ten" written as "5-10" is a change of syntax rather than of spelling,
// "increased from five to ten percent" would become "increased from 5-10
// percent", which does not mean the same thing.
#include "wfst.h"

namespace sayso {

const Cascade& range_grammar();

}  // namespace sayso
