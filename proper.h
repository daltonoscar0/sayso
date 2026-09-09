#pragma once
// Shared machinery for the classes that read a proper name.
//
// ADDRESS, NAME and STATE all have the same shape: a word Sayso must expand
// sitting next to one it must leave completely alone. "Reading" in "Reading
// Rd", "Lee" in "Dr. Lee" and "Little Rock" in "Little Rock, AR" are not
// verbalised at all, and the grammar's job is to recognise them, not to say
// them.
//
// A name therefore travels the machine as itself, character by character, on
// both tapes. That works because render() concatenates adjacent characters and
// puts a space on either side of a spoken *word*: "Reading" arrives as seven
// characters, "Road" as one word, and the space between them is supplied by the
// renderer rather than by either grammar.
//
// The capital is what makes any of this safe. A pass-through costing nothing
// would otherwise let these classes eat leftwards through a sentence, since
// anything they swallow is a character PLAIN no longer has to pay for. Insisting
// the name be capitalised is a structural bound rather than a priced one, and it
// costs nothing real: street names, surnames and city names are proper nouns.
#include <string>

#include "wfst.h"

namespace sayso {

// One arc reading a character and writing the same character back.
Wfst identical(const std::string& c);

// The union of identity arcs over every character in `alphabet`.
Wfst any_of(const std::string& alphabet);

Wfst upper_letter();
Wfst lower_letter();

// One capital, then at least one lowercase letter.
//
// The second letter is required so that a lone capital stays available to the
// classes that mean something by it: "E" is a compass direction in an address
// and an initial in a name, never a one-letter word.
Wfst capitalised_word();

// One to `most` capitalised words, space separated: "Baker", "Little Rock",
// "Martin Luther King". The spaces between them are emitted, because there are
// characters on both sides and the renderer supplies nothing.
Wfst proper_name(int most = 3);

// A field separator that is read and not written, for the boundaries where the
// renderer supplies the space itself.
Wfst gap();

// A capital that is *said* rather than passed through: the "J" of "J.R.R.", the
// "F" of "1-800-FLOWERS". It leaves as a word rather than as the character it
// arrived as, which is the whole difference between "J R R" and "JRR" -- the
// renderer concatenates adjacent characters and spaces adjacent words.
Wfst spelled_letter();

}  // namespace sayso
