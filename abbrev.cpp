#include "abbrev.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

// Counts are occurrences in the shard the table was built from, against the
// number of times the reference left the token alone. Spans reach a grammar
// case-folded, so every written form here is lowercase.
struct Abbreviation {
    const char* written;
    std::vector<std::string> spoken;
};

const std::vector<Abbreviation>& abbreviations() {
    static const std::vector<Abbreviation> table = {
        // "st" is "saint" 3,205 times and "street" 156. Twenty to one is
        // enough to pick, and not enough to be right about "Oxford St", the
        // same context-free bet as reading "second" as an ordinal.
        {"st", {"saint"}},
        {"vol", {"volume"}},      // 2,019
        {"dr", {"doctor"}},       // 1,302
        {"ltd", {"limited"}},     // 1,265
        {"vs", {"versus"}},       // 1,000
        {"jr", {"junior"}},       //   959
        {"mr", {"mister"}},       //   932
        {"etc", {"etcetera"}},    //   594
        {"sr", {"senior"}},       //   358
        {"mt", {"mount"}},        //   173
        {"bros", {"brothers"}},   //   148
        {"sq", {"square"}},       //   129
        {"prof", {"professor"}},  //   121
        {"dept", {"department"}}, //    81
        {"rd", {"road"}},         //    83
        {"ave", {"avenue"}},      //    39
        {"blvd", {"boulevard"}},  //    25
        {"mrs", {"missus"}},
    };
    return table;
}

Wfst build_abbreviation() {
    // No trailing-stop branch. The adapter in text.cpp splits a final full
    // stop off a token that has no other, so "Ltd." reaches CLASSIFY as two
    // tokens, and a branch reading "ltd." would then claim both and swallow
    // the sentence's punctuation: "Acme Ltd. The deal closed." came out as
    // "Acme limited The deal closed." 593 sentences in the shard end this way.
    std::vector<Wfst> parts;
    for (const Abbreviation& a : abbreviations())
        parts.push_back(rule(a.written, a.spoken));
    return connect(union_of(parts));
}

}  // namespace

const Cascade& abbreviation_grammar() {
    static const Wfst core = build_abbreviation();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
