#include "addresses.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "proper.h"
#include "weights.h"

namespace sayso {

namespace {

const char* kUnits[10] = {"zero", "one", "two",   "three", "four",
                          "five", "six", "seven", "eight", "nine"};
const char* kTeens[10] = {"tenth",      "eleventh",  "twelfth",
                          "thirteenth", "fourteenth", "fifteenth",
                          "sixteenth",  "seventeenth", "eighteenth",
                          "nineteenth"};
const char* kTens[10] = {"", "", "twenty", "thirty", "forty",
                         "fifty", "sixty", "seventy", "eighty", "ninety"};
const char* kTensOrdinal[10] = {"", "", "twentieth", "thirtieth", "fortieth",
                                "fiftieth", "sixtieth", "seventieth",
                                "eightieth", "ninetieth"};
const char* kUnitOrdinal[10] = {"",      "first", "second", "third", "fourth",
                                "fifth", "sixth", "seventh", "eighth", "ninth"};

// The written ordinal suffix English gives a number: 1st, 2nd, 3rd, 4th, and
// the teens all "th" whatever their last digit says.
std::string ordinal_suffix(int n) {
    if (n % 100 >= 11 && n % 100 <= 13) return "th";
    switch (n % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

// The spoken ordinal, already hyphenated. Compounds are emitted as one word
// rather than two because this machine also passes characters through, and
// hyphenated() reads its vocabulary off the output labels and interns every one
// of them as a word: run over a machine that emits characters, it would build
// word arcs for "B" and "a" and the character path would die. Emitting
// "forty-second" whole keeps the hyphenator out of this file entirely.
std::string ordinal_word(int n) {
    if (n >= 100) {
        std::string head = std::string(kUnits[n / 100]) + " hundred";
        int rest = n % 100;
        if (rest == 0) return head + "th";
        return head + " " + ordinal_word(rest);
    }
    if (n < 10) return kUnitOrdinal[n];
    if (n < 20) return kTeens[n - 10];
    if (n % 10 == 0) return kTensOrdinal[n / 10];
    return std::string(kTens[n / 10]) + "-" + kUnitOrdinal[n % 10];
}

// One to three capitalised words: "Baker", "Martin Luther King". Three is where
// real street names stop often enough to matter, and each extra word is a place
// a sentence could be misparsed, so the bound is deliberate.
Wfst street_name() { return proper_name(3); }

// A numbered street, spoken as an ordinal: "5th" -> "fifth", "42nd" ->
// "forty-second".
Wfst numbered_street() {
    std::vector<Wfst> parts;
    for (int n = 1; n <= kHighestNumberedStreet; ++n)
        parts.push_back(
            rule(std::to_string(n) + ordinal_suffix(n), {ordinal_word(n)}));
    return union_of(parts);
}

struct Suffix {
    const char* written;
    const char* spoken;   // lowercase, for a numbered street
    const char* titled;   // capitalised, for a named one
    bool ambiguous;       // also an ordinary abbreviation ABBREV already reads
};

// Cited in the README: USPS Publication 28, Appendix C1, "Street Suffix
// Abbreviations". Only the suffixes a conversational voice actually meets are
// here; C1 lists some five hundred.
const std::vector<Suffix>& suffixes() {
    static const std::vector<Suffix> table = {
        {"Rd",   "road",      "Road",      false},
        {"St",   "street",    "Street",    true},
        {"Ave",  "avenue",    "Avenue",    false},
        {"Blvd", "boulevard", "Boulevard", false},
        {"Dr",   "drive",     "Drive",     true},
        {"Ln",   "lane",      "Lane",      false},
        {"Ct",   "court",     "Court",     false},
        {"Pl",   "place",     "Place",     false},
        {"Pkwy", "parkway",   "Parkway",   false},
        {"Hwy",  "highway",   "Highway",   false},
    };
    return table;
}

struct Directional {
    const char* written;
    const char* spoken;
};

const std::vector<Directional>& directionals() {
    static const std::vector<Directional> table = {
        {"N", "north"},     {"S", "south"},     {"E", "east"},
        {"W", "west"},      {"NE", "northeast"}, {"NW", "northwest"},
        {"SE", "southeast"}, {"SW", "southwest"},
    };
    return table;
}

struct UnitMarker {
    const char* written;
    const char* spoken;
};

const std::vector<UnitMarker>& unit_markers() {
    static const std::vector<UnitMarker> table = {
        {"Apt", "apartment"}, {"Ste", "suite"},  {"Unit", "unit"},
        {"Rm", "room"},       {"Fl", "floor"},   {"#", "number"},
    };
    return table;
}

const Wfst& house_number() {
    static const Wfst n = hyphenated(cardinal_positive_fst());
    return n;
}

// "4B", "200", "12C". The number is a cardinal and a trailing letter is spelled
// as itself, which is what a voice does with an apartment letter.
Wfst unit_designator() {
    return concat_of({house_number(), optional_of(upper_letter())});
}

Wfst unit_part() {
    std::vector<Wfst> parts;
    for (const UnitMarker& u : unit_markers()) {
        // "#" is written flush against its number as often as not.
        Wfst space = std::string(u.written) == "#" ? optional_of(gap()) : gap();
        parts.push_back(concat_of(
            {rule(u.written, {u.spoken}), space, unit_designator()}));
    }
    return union_of(parts);
}

Wfst build_address() {
    Wfst number_part = concat_of({house_number(), gap()});

    std::vector<Wfst> directional_parts;
    for (const Directional& d : directionals())
        directional_parts.push_back(
            concat_of({rule(d.written, {d.spoken}), gap()}));
    Wfst directional_part = optional_of(union_of(directional_parts));

    // The suffixes are unioned *before* anything is concatenated onto them,
    // which is the same trick money.cpp uses on its currency tails and for the
    // same reason. Written the other way round -- one branch per suffix, each
    // beginning with the house number and the street name -- this grammar
    // carries ten copies of the cardinal machine and of the capitalised-word
    // machine instead of one apiece, and composing it against every span start
    // is what the classifier spends its time on. The first version of this file
    // did that, and the corpus eval went from three minutes to over an hour.
    // Path weights are unchanged: the tropical semiring sums along a path, so a
    // reweight() moved inside a concatenation lands on the same total.
    std::vector<Wfst> titled_free, titled_unanchored, spoken_plain;
    for (const Suffix& s : suffixes()) {
        // A named street keeps the suffix as part of the name, so it is spoken
        // capitalised: "Reading Road". A numbered one is an ordinary noun
        // phrase and is not: "fifth avenue".
        titled_free.push_back(rule(s.written, {s.titled}));
        titled_unanchored.push_back(
            rule(s.written, {s.titled},
                 s.ambiguous ? kUnanchoredAmbiguousSuffix : 0.0f));
        spoken_plain.push_back(rule(s.written, {s.spoken}));
    }

    // A number in front settles the span, whether it is the house number or the
    // street's own. "221 Baker St" and "E 42nd St" each have exactly one
    // reading, so neither pays anything.
    //
    // A named street with no number in front is the one shape that genuinely
    // competes with a title: "Wall St" is a street and "St Peter" is a saint,
    // and both are a capital beside an ambiguous abbreviation. NAME claims the
    // title reading, which puts the abbreviation *before* the name rather than
    // after, so the two rarely reach the same characters at all. Where they do,
    // the weight above settles it in the title's favour.
    Wfst named_anchored = concat_of({number_part, directional_part,
                                     street_name(), gap(),
                                     union_of(titled_free)});
    Wfst named_bare = concat_of({directional_part, street_name(), gap(),
                                 union_of(titled_unanchored)});
    Wfst numbered = concat_of({optional_of(number_part), directional_part,
                               numbered_street(), gap(),
                               union_of(spoken_plain)});

    // A trailing unit, and a unit standing on its own. The comma before it is
    // read and not spoken.
    Wfst whole = union_of({named_anchored, named_bare, numbered});
    Wfst unit_tail = optional_of(
        concat_of({optional_of(chars_in(",")), gap(), unit_part()}));

    return connect(union_of({concat_of({whole, unit_tail}), unit_part()}));
}

}  // namespace

const Cascade& address_grammar() {
    static const Wfst core = build_address();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
