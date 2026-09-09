#include "names.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "proper.h"
#include "weights.h"

namespace sayso {

namespace {

struct Title {
    const char* written;
    const char* spoken;
};

// Spoken capitalised, because a title is part of the name it introduces:
// "Doctor Lee" is the name, "doctor lee" is a description of one.
//
// "Ms" is "miz", which is the pronunciation every TTS front end settles on and
// the only one that is not either "miss" or "missus", both of which are
// different words with their own abbreviations in this table.
const std::vector<Title>& titles() {
    static const std::vector<Title> table = {
        {"Mr", "Mister"},     {"Mrs", "Missus"},   {"Ms", "Miz"},
        {"Dr", "Doctor"},     {"Prof", "Professor"}, {"Rev", "Reverend"},
        {"St", "Saint"},      {"Mt", "Mount"},     {"Fr", "Father"},
        {"Sgt", "Sergeant"},  {"Capt", "Captain"}, {"Lt", "Lieutenant"},
        {"Gen", "General"},   {"Gov", "Governor"}, {"Sen", "Senator"},
        {"Rep", "Representative"}, {"Hon", "Honourable"},
        // Spelled in full, and here only so that a regnal numeral has something
        // to hang on. See regnal_suffixes().
        {"Queen", "Queen"},   {"King", "King"},    {"Pope", "Pope"},
        {"Emperor", "Emperor"}, {"Prince", "Prince"}, {"Princess", "Princess"},
    };
    return table;
}

struct NameSuffix {
    const char* written;
    std::vector<std::string> spoken;
    float cost = 0.0f;
};

// What may follow a bare name. Each of these is unambiguous: no common noun is
// ever followed by "Jr", so the suffix is evidence enough on its own that what
// stands in front of it is a person.
const std::vector<NameSuffix>& generational_suffixes() {
    static const std::vector<NameSuffix> table = {
        {"Jr", {"Junior"}},  {"Sr", {"Senior"}},
        // The degrees pay a hair, because they are also postal codes and the
        // postal reading is the commoner one after a comma: "Baltimore, MD" is
        // a city far more often than a physician. STATE enters at 0.1 against
        // NAME's 0.04, so without this NAME took every "City, MD" in the
        // country and said "Baltimore M D".
        {"PhD", {"P", "h", "D"}, kDegreeSuffixPenalty},
        {"MD", {"M", "D"}, kDegreeSuffixPenalty},
    };
    return table;
}

// A regnal numeral is an ordinal with an article rather than a cardinal:
// "Elizabeth II" is "Elizabeth the second", never "Elizabeth two".
//
// It is offered only behind a title, and that is not fussiness. A Roman numeral
// after a capitalised word is "World War II" far more often than it is a
// monarch, and the two are the same shape: two capitalised words and a numeral.
// Nothing in the span distinguishes them, and a weight cannot either, because
// the regnal reading's advantage is that it absorbs the name in front of it and
// that advantage grows with the length of the name -- so any penalty that saves
// "World War II" also loses "Elizabeth II", and any penalty that keeps
// "Elizabeth II" reads a world war as a monarch.
//
// A title is the evidence that settles it, so a title is what this branch
// requires. The cost is that a bare "Elizabeth II" is read "Elizabeth two";
// the README's failure-case section lists it.
const std::vector<NameSuffix>& regnal_suffixes() {
    static const std::vector<NameSuffix> table = {
        {"II", {"the", "second"}}, {"III", {"the", "third"}},
        {"IV", {"the", "fourth"}}, {"V", {"the", "fifth"}},
        {"VI", {"the", "sixth"}},  {"VIII", {"the", "eighth"}},
    };
    return table;
}

// "J.R.R.", "T.S.", "F. Scott". Each initial is spoken as the letter itself,
// capitalised, and the stops are read but never said.
//
// Two or more, because a single dotted capital is not enough to go on: "A."
// opens a list as often as it opens a name, and LETTERS already reads the
// dotted shapes that are acronyms.
Wfst initials(int fewest) {
    Wfst one = concat_of({spelled_letter(), chars_in("."), optional_of(gap())});
    std::vector<Wfst> parts;
    for (int i = 0; i < fewest; ++i) parts.push_back(one);
    parts.push_back(closure(one));
    return concat_of(parts);
}

Wfst build_name() {
    std::vector<Wfst> branches;

    // A title, its optional stop, and the name it introduces.
    //
    // The stop is consumed here rather than left to PLAIN, which is the one
    // place this grammar has to differ from ABBREV. abbrev.cpp refuses a
    // trailing-stop branch because "Acme Ltd. The deal closed" would swallow
    // the sentence's punctuation into the abbreviation. A title cannot do that
    // damage, because it is only ever claimed when a capitalised name follows
    // it inside the same span, and a sentence does not end in its own subject.
    // Titles unioned before the name is concatenated onto them, so that this
    // grammar holds one copy of the capitalised-word machine rather than one
    // per title. See the note in addresses.cpp: written the other way round,
    // these two classes between them made a corpus pass twenty times slower.
    Wfst stop = optional_of(chars_in("."));
    std::vector<Wfst> title_rules;
    for (const Title& t : titles())
        title_rules.push_back(rule(t.written, {t.spoken}));
    Wfst one_title = concat_of({union_of(title_rules), stop, gap()});
    // Titles stack: "Rev Dr King", "Sgt Maj Baker". Two is as deep as English
    // goes in practice, and each extra one is another way for the class to eat
    // a word it should have left alone.
    Wfst title_head = concat_of({one_title, optional_of(one_title)});

    // Initials, with or without a surname behind them.
    //
    // Two at least, either way. pipeline_tests.cpp already records why a
    // one-letter rule is dangerous -- LETTERS refuses it so that the [s] [.] of
    // "the 1980s." is not read as an initial -- and a single initial in front of
    // a capitalised name is worse, because the name is absorbed for free. It
    // cost "$5.4 M. Central Park" its magnitude: NAME read "M. Central Park" and
    // left MONEY four fifty. Two initials is the same structural guard LETTERS
    // uses, for the same reason.
    //
    // Three standing alone, because two dotted capitals with nothing behind
    // them are an acronym far more often than a name, and LETTERS reads those
    // from a table.
    branches.push_back(initials(3));
    branches.push_back(concat_of({initials(2), proper_name(2)}));

    // A name and the generational suffix that follows it. The suffix is the
    // anchor, so the name in front of it needs no other justification.
    std::vector<Wfst> generational;
    for (const NameSuffix& s : generational_suffixes())
        generational.push_back(rule(s.written, s.spoken, s.cost));
    Wfst generational_tail = concat_of(
        {optional_of(chars_in(",")), gap(), union_of(generational)});

    // A regnal numeral, behind a title and only behind a title.
    std::vector<Wfst> regnal;
    for (const NameSuffix& s : regnal_suffixes())
        regnal.push_back(rule(s.written, s.spoken, s.cost));
    Wfst regnal_tail = concat_of({gap(), union_of(regnal)});

    branches.push_back(
        concat_of({title_head, proper_name(3), optional_of(generational_tail)}));
    branches.push_back(concat_of({title_head, proper_name(3), regnal_tail}));
    branches.push_back(concat_of({proper_name(3), generational_tail}));

    return connect(union_of(branches));
}

}  // namespace

const Cascade& name_grammar() {
    static const Wfst core = build_name();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
