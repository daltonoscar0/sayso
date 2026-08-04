#pragma once
// Cardinal and ordinal grammars.
//
// Every grammar in Sayso is written in one direction only — written form on
// the lower tape, spoken form on the upper tape. The ITN direction is
// invert() of the same machine, so the two directions cannot drift apart.
#include <string>
#include <vector>

#include "wfst.h"

namespace sayso {

// "1,234" <-> "one thousand two hundred thirty four"
const Wfst& cardinal_fst();
// "21st" <-> "twenty-first"
const Wfst& ordinal_fst();

// Word-level pass that joins a tens word to a following unit:
// "twenty" "five" -> "twenty-five". The hyphenated path is free and the
// spaced one is not, so TTS produces "twenty-five" while ITN still reads
// both. Composed onto every number before it reaches a larger grammar.
const Wfst& hyphenator_fst();

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

// Unsigned cardinal without the hyphenation pass; money and date grammars
// embed this and hyphenate once at the end.
const Wfst& cardinal_core_fst();

// Every word the number grammars can emit, for building identity arcs.
const std::vector<std::string>& cardinal_vocabulary();
const std::vector<std::string>& ordinal_vocabulary();

// Grammar-writing shorthands, shared by numbers.cpp, money.cpp and dates.cpp.
Wfst rule(const std::string& written, const std::vector<std::string>& spoken,
          float w = 0.0f);
Wfst chars_in(const std::string& written, float w = 0.0f);
Wfst words_out(const std::vector<std::string>& spoken, float w = 0.0f);

}  // namespace sayso
