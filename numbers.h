#pragma once
// Cardinal and ordinal grammars.
//
// Every grammar in Sayso is written in one direction only — written form on
// the lower tape, spoken form on the upper tape. The ITN direction is the
// inverse of the same cascade, so the two directions cannot drift apart.
#include <string>
#include <vector>

#include "wfst.h"

namespace sayso {

// "1,234" <-> "one thousand two hundred thirty-four"
const Cascade& cardinal_grammar();
// "21st" <-> "twenty-first"
const Cascade& ordinal_grammar();

// Compose `f` with a word-level pass that joins a tens word to a following
// unit: "twenty" "five" -> "twenty-five". The hyphenated path is free and the
// spaced one is not, so TTS produces "twenty-five" while ITN reads both.
//
// The pass needs an identity arc for every word it may see, and a word it has
// not been told about does not merely pass through — it has no arc, so the
// path dies. Rather than maintain that list by hand, the vocabulary is read
// off `f`'s own output labels. Applying this to a machine is therefore always
// safe, whatever words it emits.
Wfst hyphenated(const Wfst& f);

// Sub-machines shared with the money and date grammars. Each reads an exact
// number of written digits, which is what lets larger grammars concatenate
// them without a separator.
struct NumberParts {
    Wfst unit_nz;       // 1-9,     one digit
    Wfst two_digit_nz;  // 10-99,   two digits
    Wfst three_nz;      // 100-999, three digits
    Wfst lead3;         // 1-999,   no leading zeros
    Wfst padded3;       // 000-999, exactly three digits ("000" verbalises to nothing)
    Wfst padded3_nz;    // 001-999, exactly three digits
    Wfst zero;          // "0" -> "zero"
};
const NumberParts& number_parts();

// Unsigned cardinals, before hyphenation; money and date grammars embed these.
const Wfst& cardinal_core_fst();      // includes "0" -> "zero"
const Wfst& cardinal_positive_fst();  // excludes it, so "$0.99" is cents-only

// "zero" .. "nine", for grammars that read digits one at a time (the
// fractional part of "$2.5 million", say).
const char* digit_word(int d);

// Every word the number grammars can emit, for building identity arcs.
const std::vector<std::string>& cardinal_vocabulary();
const std::vector<std::string>& ordinal_vocabulary();

// Grammar-writing shorthands, shared by numbers.cpp, money.cpp and dates.cpp.
Wfst rule(const std::string& written, const std::vector<std::string>& spoken,
          float w = 0.0f);
Wfst chars_in(const std::string& written, float w = 0.0f);
Wfst chars_out(const std::string& written, float w = 0.0f);
Wfst words_out(const std::vector<std::string>& spoken, float w = 0.0f);

}  // namespace sayso
