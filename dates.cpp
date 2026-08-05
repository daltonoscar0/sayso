#include "dates.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

const char* kMonths[13] = {"",        "january",  "february", "march",   "april",
                           "may",     "june",     "july",     "august",  "september",
                           "october", "november", "december"};

const char* kUnits[10] = {"zero", "one", "two",   "three", "four",
                          "five", "six", "seven", "eight", "nine"};
const char* kTeens[10] = {"ten",      "eleven",  "twelve",    "thirteen", "fourteen",
                          "fifteen",  "sixteen", "seventeen", "eighteen", "nineteen"};
const char* kTens[10] = {"", "", "twenty", "thirty", "forty",
                         "fifty", "sixty", "seventy", "eighty", "ninety"};
const char* kOrdinalUnits[10] = {"",      "first", "second",  "third",  "fourth",
                                 "fifth", "sixth", "seventh", "eighth", "ninth"};
const char* kOrdinalTeens[10] = {"tenth",       "eleventh",    "twelfth",
                                 "thirteenth",  "fourteenth",  "fifteenth",
                                 "sixteenth",   "seventeenth", "eighteenth",
                                 "nineteenth"};
const char* kOrdinalTens[10] = {"", "", "twentieth", "thirtieth", "fortieth",
                                "fiftieth", "sixtieth", "seventieth", "eightieth",
                                "ninetieth"};

std::string digit(int d) { return std::string(1, static_cast<char>('0' + d)); }
std::string two_digits(int n) { return digit(n / 10) + digit(n % 10); }

std::vector<std::string> cardinal_words(int n) {
    if (n < 10) return {kUnits[n]};
    if (n < 20) return {kTeens[n - 10]};
    if (n % 10 == 0) return {kTens[n / 10]};
    return {kTens[n / 10], kUnits[n % 10]};
}

// Left unhyphenated: the hyphenator stage joins "twenty" to "first".
std::vector<std::string> ordinal_words(int n) {
    if (n < 10) return {kOrdinalUnits[n]};
    if (n < 20) return {kOrdinalTeens[n - 10]};
    if (n % 10 == 0) return {kOrdinalTens[n / 10]};
    return {kTens[n / 10], kOrdinalUnits[n % 10]};
}

// A written field that may or may not be zero-padded. Padding costs, so ITN
// writes "3/14/2015" rather than "03/14/2015".
Wfst padded_variants(int value, const std::vector<std::string>& spoken, float w) {
    std::vector<Wfst> variants{rule(std::to_string(value), spoken, w)};
    if (value < 10)
        variants.push_back(rule(two_digits(value), spoken, w + kZeroPadPenalty));
    return union_of(variants);
}

Wfst copy_digit() {
    std::vector<Wfst> digits;
    for (int d = 0; d <= 9; ++d)
        digits.push_back(transducer(chars_of(digit(d)), chars_of(digit(d))));
    return union_of(digits);
}

// ---- the canonical grammar --------------------------------------------------

Wfst month_field() {
    std::vector<Wfst> months;
    for (int m = 1; m <= 12; ++m) months.push_back(padded_variants(m, {kMonths[m]}, 0.0f));
    return union_of(months);
}

Wfst day_field() {
    std::vector<Wfst> days;
    for (int d = 1; d <= 31; ++d) {
        days.push_back(padded_variants(d, ordinal_words(d), 0.0f));
        days.push_back(padded_variants(d, cardinal_words(d), kCardinalDayPenalty));
    }
    return hyphenated(union_of(days));
}

// Speech pairs a year's digits — "nineteen ninety-nine", "nineteen hundred",
// "nineteen oh five" — except in the 2000s, where the cardinal reading wins.
// The machine itself lives in numbers.cpp, because the cardinal grammar offers
// the same reading for a bare four-digit span and a second copy would drift.
// A date's year field takes the whole range and keeps the cardinal fallback,
// so "two thousand five" is readable here.
Wfst year_field() { return hyphenated(year_reading(10, 99, true)); }

// M/D/YYYY -> "march fourteenth twenty fifteen". Every other written format
// reaches speech through this one machine.
Wfst canonical_date() {
    return concat_of({month_field(), chars_in("/"), day_field(), chars_in("/"),
                      year_field()});
}

// ---- written-format normalisers ---------------------------------------------
//
// Character-to-character machines that rewrite a surface format into the
// canonical one, then compose with it. Composition is what keeps the date
// grammar a single statement of how a date is *said*, with the formats as a
// separate, tiny statement of how one may be *written*.

Wfst month_name_to_number() {
    std::vector<Wfst> months;
    for (int m = 1; m <= 12; ++m)
        months.push_back(transducer(chars_of(kMonths[m]), chars_of(std::to_string(m))));
    return union_of(months);
}

// "march 14, 2015" -> "3/14/2015". Written and spoken order agree here, so
// nothing needs buffering.
Wfst month_name_format() {
    return concat_of({
        month_name_to_number(), chars_in(" "), chars_out("/"),
        copy_digit(), optional_of(copy_digit()),
        union_of({chars_in(", "), chars_in(" ", kMissingDateCommaPenalty)}),
        chars_out("/"),
        copy_digit(), copy_digit(), copy_digit(), copy_digit(),
    });
}

// "14 march 2015" -> "3/14/2015". The day is written before the month and
// spoken after it, so the machine holds the day in its state — 31 values,
// one copy of the month-name machine each.
Wfst day_first_format() {
    std::vector<Wfst> days;
    for (int d = 1; d <= 31; ++d) {
        std::vector<Wfst> written{chars_in(std::to_string(d))};
        if (d < 10) written.push_back(chars_in(two_digits(d), kZeroPadPenalty));
        days.push_back(concat_of({
            union_of(written), chars_in(" "), month_name_to_number(), chars_out("/"),
            chars_out(std::to_string(d)), chars_out("/"), chars_in(" "),
            copy_digit(), copy_digit(), copy_digit(), copy_digit(),
        }));
    }
    return union_of(days);
}

// "2015-03-14" -> "03/14/2015". Same trick with a four-digit field, which is
// why the year range is bounded: each year is one path through the machine.
Wfst iso_format() {
    std::vector<Wfst> years;
    for (int y = kIsoYearFrom; y <= kIsoYearTo; ++y) {
        std::string written = std::to_string(y);
        years.push_back(concat_of({
            chars_in(written), chars_in("-"),
            copy_digit(), copy_digit(), chars_out("/"), chars_in("-"),
            copy_digit(), copy_digit(), chars_out("/"), chars_out(written),
        }));
    }
    return union_of(years);
}

// Reading a date already in the canonical format: copy it through unchanged.
Wfst canonical_format() {
    std::vector<Wfst> any;
    for (int d = 0; d <= 9; ++d)
        any.push_back(transducer(chars_of(digit(d)), chars_of(digit(d))));
    any.push_back(transducer(chars_of("/"), chars_of("/")));
    return closure(union_of(any));
}

Wfst build_formats() {
    return connect(union_of({
        canonical_format(),
        reweight(month_name_format(), kMonthNameFormatPenalty),
        reweight(day_first_format(), kDayFirstFormatPenalty),
        reweight(iso_format(), kIsoFormatPenalty),
    }));
}

}  // namespace

const Cascade& date_grammar() {
    static const Wfst formats = build_formats();
    static const Wfst canonical = connect(canonical_date());
    static const Cascade grammar = Cascade().then(formats).then(canonical);
    return grammar;
}

}  // namespace sayso
