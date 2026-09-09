#pragma once
// US states, and the proper nouns a letter-to-sound model gets wrong.
//
// Two related jobs, in one file because they are the same problem seen twice.
//
// The first is expanding a postal abbreviation, and the whole difficulty is
// knowing when not to. "AR" is Arkansas in "Little Rock, AR" and is the letters
// A R everywhere else -- an ticker symbol, an initialism, the start of a
// sentence in a language Sayso is not reading. Every two-letter code is a word
// or an abbreviation somewhere: IN, OR, HI, ME, OK, PA, MS and DE are all
// postal codes and all something else more often. No weight can carry that,
// because the evidence is not in the span. So the grammar demands the shape
// that only an address has: a capitalised place name, a comma, then the code.
// A bare "AR" has no parse here at all and falls through to LETTERS, which
// says "a r", which is right.
//
// The second job is a warning to the stage downstream. Expanding "AR" to
// "Arkansas" hands a grapheme-to-phoneme model a word it will very likely
// mispronounce, because "Arkansas" does not rhyme with "Kansas" and nothing in
// its spelling says so. Sayso would be replacing a token lede could have
// spelled out with one it will get wrong, which is a worse failure than not
// expanding at all.
//
// So the verbaliser says what it knows: any output token in the table below is
// reported in meta.pron_hints with its pronunciation, and the next stage can
// take the hint or ignore it. The list is short and curated rather than
// general, because a general one is a pronunciation dictionary and that is
// lede's job, not this one.
#include <string>
#include <vector>

#include "wfst.h"

namespace sayso {

const Cascade& state_grammar();

// A proper noun whose spelling misleads, and what it actually sounds like.
struct PronunciationHint {
    const char* word;      // as it appears in Sayso's output
    const char* arpabet;   // ARPAbet with stress digits, the form lede speaks
    const char* respell;   // a human-readable respelling, for the demo page
};

// Looked up case insensitively; returns nullptr when the word is not irregular.
const PronunciationHint* pronunciation_hint(const std::string& word);

const std::vector<PronunciationHint>& pronunciation_hints();

}  // namespace sayso
