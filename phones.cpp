#include "phones.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "proper.h"
#include "weights.h"

namespace sayso {

namespace {

// Zero is "oh", the way a voice says it inside a number, and the way times.cpp
// already says it in "ten oh five". telephone.cpp says "o" for the same sound
// in an ISBN; relaxed scoring folds the two together, and nothing else depends
// on which spelling reaches the tape.
const char* kDigit[10] = {"oh",   "one", "two",   "three", "four",
                          "five", "six", "seven", "eight", "nine"};

std::string digit_char(int d) { return std::string(1, static_cast<char>('0' + d)); }

Wfst spoken_digit() {
    std::vector<Wfst> parts;
    for (int d = 0; d <= 9; ++d) parts.push_back(rule(digit_char(d), {kDigit[d]}));
    return union_of(parts);
}

// The last digit of a group carries the pause, as a comma glued to its own
// word. render() puts a space on either side of a spoken word, so a comma
// interned separately would come out as "five , one"; there is nowhere else to
// put it that survives rendering.
Wfst spoken_digit_pause() {
    std::vector<Wfst> parts;
    for (int d = 0; d <= 9; ++d)
        parts.push_back(rule(digit_char(d), {std::string(kDigit[d]) + ","}));
    return union_of(parts);
}

Wfst run_of(const Wfst& d, int n) {
    std::vector<Wfst> parts;
    for (int i = 0; i < n; ++i) parts.push_back(d);
    return concat_of(parts);
}

// A group of `n` digits whose last one carries the pause.
Wfst group_pausing(int n) {
    Wfst d = spoken_digit();
    std::vector<Wfst> parts;
    for (int i = 0; i + 1 < n; ++i) parts.push_back(d);
    parts.push_back(spoken_digit_pause());
    return concat_of(parts);
}

// A vanity word: "FLOWERS", "CALL-NOW". Each letter is said on its own, which
// is what a voice does with a word it cannot dial: "F L O W E R S", not
// "flowers". The hyphen inside "CALL-NOW" is a separator like any other in a
// phone number, read and not spoken.
Wfst vanity_word() {
    Wfst letters =
        concat_of({spelled_letter(), spelled_letter(), closure(spelled_letter())});
    return concat_of(
        {letters, closure(concat_of({chars_in("-"), letters}))});
}

Wfst build_phone(Style style) {
    Wfst d = spoken_digit();

    // The separator between groups: a hyphen, a full stop, or a space. All
    // three are read and none is spoken.
    Wfst sep = union_of({chars_in("-"), chars_in("."), chars_in(" ")});

    // An area code, bare or in parentheses, followed by its pause.
    Wfst area_bare = group_pausing(3);
    Wfst area_paren = concat_of({chars_in("("), run_of(d, 2), spoken_digit_pause(),
                                 chars_in(")")});
    Wfst area = union_of({concat_of({area_bare, sep}),
                          concat_of({area_paren, optional_of(chars_in(" "))})});

    // The country code, spoken and then paused over.
    Wfst country = optional_of(concat_of(
        {optional_of(chars_in("+")), rule("1", {"one,"}), sep}));

    // Exchange and line: three digits, a pause, then four.
    Wfst local = concat_of({group_pausing(3), sep, run_of(d, 4)});
    // The same, with a vanity word standing in for the last seven digits.
    Wfst vanity = vanity_word();

    std::vector<Wfst> branches;
    branches.push_back(concat_of({country, area, local}));
    branches.push_back(concat_of({country, area, vanity}));
    // Ten digits run together, which is how a form stores them.
    branches.push_back(concat_of({country, group_pausing(3), group_pausing(3),
                                  run_of(d, 4)}));

    // The two readings that are a matter of style rather than of shape. Under
    // Style::Plain each is available and dear, so a page still reads its
    // numbers as numbers; under Style::Phone each is free.
    float elided = style == Style::Phone ? 0.0f : kWrittenNumberReading;
    branches.push_back(reweight(local, elided));
    // A bare run of seven or more digits, said one at a time and with no
    // grouping, because there is no separator to say where the groups are.
    //
    // It pays a hair even under Style::Phone so that it loses to the branches
    // above wherever they also fit. Ten digits run together are a phone number
    // with its punctuation lost, not an unknown string of ten, and without the
    // hair "5551212121" tied with the grouped reading and came out flat.
    Wfst bare = concat_of({run_of(d, 7), closure(d)});
    branches.push_back(reweight(bare, elided + kUngroupedDigitsPenalty));

    return connect(union_of(branches));
}

}  // namespace

const Cascade& phone_grammar(Style style) {
    static const Wfst plain = build_phone(Style::Plain);
    static const Wfst phone = build_phone(Style::Phone);
    static const Cascade plain_grammar = Cascade().then(plain);
    static const Cascade phone_grammar_ = Cascade().then(phone);
    return style == Style::Phone ? phone_grammar_ : plain_grammar;
}

}  // namespace sayso
