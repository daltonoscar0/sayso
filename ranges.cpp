#include "ranges.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

const char* kUnits[10] = {"zero", "one", "two",   "three", "four",
                          "five", "six", "seven", "eight", "nine"};
const char* kTeens[10] = {"ten",     "eleven",  "twelve",    "thirteen", "fourteen",
                          "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
const char* kTens[10] = {"", "", "twenty", "thirty", "forty",
                         "fifty", "sixty", "seventy", "eighty", "ninety"};

std::vector<std::string> cardinal_words(int n) {
    if (n < 10) return {kUnits[n]};
    if (n < 20) return {kTeens[n - 10]};
    if (n % 10 == 0) return {kTens[n / 10]};
    return {kTens[n / 10], kUnits[n % 10]};
}

// One digit, 0-9. Zero is here because "5 - 0" is a score.
Wfst one_digit() {
    std::vector<Wfst> parts;
    for (int n = 0; n <= 9; ++n) parts.push_back(rule(std::to_string(n), cardinal_words(n)));
    return union_of(parts);
}

// Exactly two digits, from `lo` upwards. Leading zeros are excluded
// throughout: "2003-04" is a season the reference reads with the separator
// dropped, so a range has no business claiming it.
Wfst two_digits_from(int lo) {
    std::vector<Wfst> parts;
    for (int n = lo; n <= 99; ++n) parts.push_back(rule(std::to_string(n), cardinal_words(n)));
    return union_of(parts);
}

// Four digits, read as a year where one is available and as a cardinal
// otherwise — the same margin cardinal_grammar() uses, so "1624-1713" is
// "sixteen twenty four to seventeen thirteen".
Wfst four_digits() {
    const NumberParts& p = number_parts();
    return union_of({
        reweight(concat_of({p.unit_nz, words_out({"thousand"}), p.padded3}),
                 kBareYearMargin),
        year_reading(kBareYearFirstCentury, kBareYearLastCentury, false),
    });
}

Wfst build_range() {
    const NumberParts& p = number_parts();

    Wfst short_operand = union_of({one_digit(), two_digits_from(10), p.three_nz});
    Wfst any_operand = union_of({short_operand, four_digits()});

    // The one structural guard, and the reason it is structural rather than a
    // weight: a four-digit number followed by a two-digit one in 01-12 is the
    // year-month prefix of an ISO date. Refusing that shape outright is what
    // keeps "2011-11-11" a date; leaving it to the entry cost worked only
    // while zero-padding happened to make the alternative expensive.
    Wfst not_a_month = union_of({one_digit(), two_digits_from(13), p.three_nz,
                                 four_digits()});

    // Hyphen only. The en dash does not occur in the corpus at all and every
    // one of the 11,063 em dashes is silent; the colon is 95.7% bibliographic
    // citation ("Astrophysical Journal 94: 501"), which is not a reading
    // anyone speaks, and it would put this class against TIME on every clock.
    Wfst space = union_of({empty_string_fst(), chars_in(" ")});
    Wfst separator = concat_of({space, rule("-", {"to"}), space});

    return connect(hyphenated(union_of({
        concat_of({short_operand, separator, any_operand}),
        concat_of({four_digits(), separator, not_a_month}),
    })));
}

}  // namespace

const Cascade& range_grammar() {
    static const Wfst core = build_range();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
