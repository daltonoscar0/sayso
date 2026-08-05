#include "decimals.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

std::string digit(int d) { return std::string(1, static_cast<char>('0' + d)); }

// One or more digits, each spoken on its own. Free, unlike the cardinal
// grammar's per-digit fallback: after a decimal point this is not a fallback,
// it is the reading.
Wfst fractional_digits() {
    std::vector<Wfst> digits;
    for (int d = 0; d <= 9; ++d) digits.push_back(rule(digit(d), {digit_word(d)}));
    return concat_of({union_of(digits), closure(union_of(digits))});
}

// The point and everything after it — what turns a cardinal into a decimal.
Wfst decimal_tail() {
    return concat_of({rule(".", {"point"}), fractional_digits()});
}

// The two ways a decimal below one loses its leading zero. Neither is
// canonical; their relative cost is what decides which written form ITN
// restores — "0.5" rather than ".5".
std::vector<Wfst> elided_zero_branches() {
    return {
        concat_of({chars_in("0", kUnspokenLeadingZeroPenalty), decimal_tail()}),
        reweight(decimal_tail(), kUnwrittenLeadingZeroPenalty),
    };
}

Wfst build_decimal_core() {
    std::vector<Wfst> branches{concat_of({cardinal_core_fst(), decimal_tail()})};
    for (Wfst& elided : elided_zero_branches()) branches.push_back(elided);
    return connect(union_of(branches));
}

// As above, but the point is optional, so a bare cardinal is accepted too and
// the cardinal machine appears once rather than in both halves of a union.
//
// No elided-zero branch here, unlike decimal_core_fst(). An amount in front of
// a unit is always written with its integer part, and admitting ".4" as one
// let MEASURE claim the cents of a currency amount: "$5.4 M" came apart into
// MONEY("$5") and MEASURE(".4 M"), whose outputs meet as the non-word
// "dollarspoint".
Wfst build_number_or_decimal() {
    return connect(concat_of(
        {cardinal_core_fst(), union_of({empty_string_fst(), decimal_tail()})}));
}

// The magnitude a decimal may carry. This grammar was written without one, on
// the reasoning that "3.5 million" comes out right anyway with DECIMAL taking
// "3.5" and PLAIN copying "million" through. That is true going forwards and
// false coming back: PLAIN is priced per character, so "million" costs 7.0,
// and CARDINAL reading "five million" as 5000000 for 0.21 undercuts the whole
// thing. "three point five million" came back as "3 point 5000000" — a round
// trip that is not merely unidiomatic but wrong by six orders of magnitude.
Wfst optional_magnitude() {
    static const char* kMagnitudes[] = {"thousand", "million", "billion", "trillion"};
    std::vector<Wfst> parts{empty_string_fst()};
    for (const char* m : kMagnitudes)
        parts.push_back(
            concat_of({chars_in(" "), transducer(chars_of(m), words_of({m}))}));
    return union_of(parts);
}

}  // namespace

const Wfst& decimal_core_fst() {
    static const Wfst f = build_decimal_core();
    return f;
}

const Wfst& number_or_decimal_fst() {
    static const Wfst f = build_number_or_decimal();
    return f;
}

const Cascade& decimal_grammar() {
    // The magnitude is on this cascade only, not on decimal_core_fst(): the
    // measure grammar embeds the core, and "2.5 million km" is not worth
    // another copy of these branches inside every unit.
    static const Wfst signed_decimal =
        concat_of({optional_sign_fst(), decimal_core_fst(), optional_magnitude()});
    static const Wfst joined = hyphenated(signed_decimal);
    static const Cascade grammar = Cascade().then(joined);
    return grammar;
}

}  // namespace sayso
