#pragma once
// Telephone numbers, and the other digit strings a voice reads one digit at a
// time: "(555) 121-2121" -> "five five five, one two one two".
//
// TELEPHONE already exists and is not this. That class reads hyphen-joined
// serials -- ISBNs, mostly -- and is guarded by demanding four groups, because
// three is the shape of an ISO date. A telephone number has three groups and
// would be refused by that guard, which is why it needs its own class rather
// than another branch of that one.
//
// What separates a phone number from a range or a cardinal is its shape:
// exactly three then three then four digits, or seven digits split three and
// four, optionally behind a country code and optionally in parentheses. Those
// are structural facts, so they are structural guards here, in the same spirit
// as telephone.cpp's. Nothing about "555-1212" is settled by a weight.
//
// Two branches are not structural, and both are gated on Style::Phone instead:
//
//   - A *bare* run of seven or more digits. On a page, 1234567 is a cardinal
//     and Sayso has always read it as one; over a telephone it is an order
//     number and reading it as "one million two hundred thirty-four thousand"
//     is useless. The corpus says the first, the product says the second, and
//     the style option is exactly the thing that knows which is being asked
//     for.
//   - A local seven-digit number written "121-2121", whose shape RANGE also
//     reads and reads plausibly.
//
// A ZIP code is not here. Five digits are a cardinal far more often than a
// postcode, so ZIP lives inside the STATE grammar, where a state abbreviation
// stands in front of it and settles the matter.
#include "style.h"
#include "wfst.h"

namespace sayso {

const Cascade& phone_grammar(Style style = Style::Plain);

}  // namespace sayso
