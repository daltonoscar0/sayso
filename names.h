#pragma once
// Personal names: "Dr. Lee" -> "Doctor Lee", "J.R.R. Tolkien" -> "J R R
// Tolkien", "Lee Jr" -> "Lee Junior".
//
// The class earns its place twice over.
//
// It is what resolves the abbreviation Sayso could never resolve before. ABBREV
// reads "Dr" as "doctor" and "St" as "saint" because that is what they usually
// are, and it is wrong every time the token is a street. Neither reading can be
// picked from the token alone. What picks it is *which side of the name the
// abbreviation sits on*: a title comes before ("Dr. Lee", "St. Helens") and a
// street suffix comes after ("Lee Dr", "Wall St"). One class reads each order,
// so most of the ambiguity never reaches a weight at all. See addresses.h for
// the other half of the same argument.
//
// And it is what tells the next stage where the names are. A proper name is
// exactly where a heteronym disambiguator should not be guessing: "Reading" is
// a place, "Sean Bean" does not rhyme with itself, and the surname "Lead" is
// not the verb. NAME hands its spans to the caller in meta.name_spans, so lede
// can leave them alone rather than get them wrong.
#include "wfst.h"

namespace sayso {

const Cascade& name_grammar();

}  // namespace sayso
