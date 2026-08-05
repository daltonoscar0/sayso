#pragma once
// Telephone grammar: "978-0-131-40221-8" -> "nine seven eight o one three one
// four o two two one eight".
//
// Serial numbers, ISBNs and phone numbers: three or more groups of digits
// joined by hyphens, read one digit at a time, with zero read as "o" rather
// than "zero". That last is the corpus's convention and a real one — nobody
// reads a phone number's zeroes as "zero".
//
// The class exists less for its own sake than to stop RANGE claiming the first
// two fields of a hyphen chain. A range cannot see past its own span, so
// "978-0-131-40221-8" looked to it exactly like a range followed by some
// cardinals; 2,064 of the shard's telephone tokens got a RANGE claim.
//
// Its entry cost is what keeps an ISO date safe. Three hyphen-joined groups is
// also the shape of "2015-03-14", and DATE reads that for 0.35, so this class
// enters above that and wins only where DATE cannot go.
//
// One-directional, like RANGE and for a similar reason: spoken digit sequences
// carry no group boundaries, so nothing says where the hyphens went, and
// counting aloud — "one two three" — is not a serial number.
#include "wfst.h"

namespace sayso {

const Cascade& telephone_grammar();

}  // namespace sayso
