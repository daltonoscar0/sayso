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

// Speech pairs a year's digits, "nineteen ninety-nine", "nineteen hundred",
// "nineteen oh five", except in the 2000s, where the cardinal reading wins.
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

// The abbreviations a date is written with, each optionally followed by a full
// stop. "may" is already three letters and has none of its own.
const char* kMonthAbbreviations[13] = {"",    "jan", "feb", "mar", "apr", "",
                                       "jun", "jul", "aug", "sep", "oct", "nov",
                                       "dec"};

Wfst month_name_to_number() {
    std::vector<Wfst> months;
    for (int m = 1; m <= 12; ++m) {
        std::string number = std::to_string(m);
        months.push_back(transducer(chars_of(kMonths[m]), chars_of(number)));

        // An abbreviation is read but never written: ITN emits M/D/YYYY, so
        // no month name of any length reaches the written tape from here.
        std::vector<std::string> short_forms;
        if (kMonthAbbreviations[m][0]) short_forms.push_back(kMonthAbbreviations[m]);
        if (m == 9) short_forms.push_back("sept");
        for (const std::string& form : short_forms)
            for (const std::string& stop : {std::string(), std::string(".")})
                months.push_back(reweight(
                    transducer(chars_of(form + stop), chars_of(number)),
                    kAbbreviatedMonthPenalty));
    }
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
// spoken after it, so the machine holds the day in its state, 31 values,
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

// A one-symbol flag on the intermediate tape, saying "this date was written
// day first". Only the format stage writes it and only the stage after reads
// it, so it never reaches either end. Interned whole rather than through
// chars_of(), which would split it into characters that could occur in text.
Label order_marker() {
    static const Label marker = ch("\x01" "day-first");
    return marker;
}

Wfst mark_day_first(const Wfst& format) {
    return concat_of({transducer({}, {order_marker()}), format});
}

// The two formats that write the day before the month. Kestrel speaks both of
// them day first, an ISO date included, which is not obvious until you read
// the data.
Wfst build_formats() {
    return connect(union_of({
        canonical_format(),
        reweight(month_name_format(), kMonthNameFormatPenalty),
        reweight(mark_day_first(day_first_format()), kDayFirstFormatPenalty),
        reweight(mark_day_first(iso_format()), kIsoFormatPenalty),
    }));
}

// ---- orientation ------------------------------------------------------------
//
// The stage between format and speech, and the only one the runtime option
// changes. Under DateSpeech::Canonical it drops the marker, so every written
// format reaches the one spoken form. Under PreserveWrittenOrder it keeps the
// marker and swaps the first two fields, so a day-first date stays day first.

Wfst drop_marker() {
    return concat_of({
        union_of({empty_string_fst(), transducer({order_marker()}, {})}),
        canonical_format(),
    });
}

// "3/14/2015" -> "14/3/2015", holding the month in the machine's state. Twelve
// values of a one- or two-character field, which is the cheap side of the
// reordering, the same reasoning that put the day in day_first_format().
Wfst swap_month_and_day() {
    std::vector<Wfst> months;
    for (int m = 1; m <= 12; ++m) {
        std::string number = std::to_string(m);
        for (const std::string& written : {number, two_digits(m)}) {
            months.push_back(concat_of({
                chars_in(written), chars_in("/"),
                copy_digit(), optional_of(copy_digit()),
                chars_out("/" + number + "/"),
                chars_in("/"),
                copy_digit(), copy_digit(), copy_digit(), copy_digit(),
            }));
            if (m >= 10) break;  // no zero-padded form to add
        }
    }
    return union_of(months);
}

Wfst reorient() {
    return connect(union_of({
        canonical_format(),
        concat_of({transducer({order_marker()}, {order_marker()}),
                   swap_month_and_day()}),
    }));
}

// ---- the day-first spoken form ----------------------------------------------

// "the fourteenth of march twenty fifteen", read off "14/3/2015" behind the
// marker. One copy each of the day, month and year fields, because the swap
// already happened upstream.
Wfst day_first_spoken() {
    return concat_of({
        transducer({order_marker()}, {}),
        words_out({"the"}), day_field(), words_out({"of"}),
        chars_in("/"), month_field(),
        chars_in("/"), year_field(),
    });
}

}  // namespace

const Cascade& date_grammar(DateSpeech speech) {
    // One format stage and one speaking stage, shared. Only the orientation
    // between them differs, which is what keeps the option from doubling the
    // largest grammar in the library.
    static const Wfst formats = build_formats();
    static const Wfst canonical_orientation = connect(drop_marker());
    static const Wfst preserved_orientation = reorient();

    // Two speaking machines, not one shared. The day-first branch is
    // unreachable under Canonical, the orientation stage has already dropped
    // its marker, but a machine does not know that, and composition would
    // explore it on every span. Each mode carries only what it can reach.
    static const Wfst month_first_only = connect(canonical_date());
    static const Wfst either_order =
        connect(union_of({canonical_date(), day_first_spoken()}));

    static const Cascade canonical_grammar =
        Cascade().then(formats).then(canonical_orientation).then(month_first_only);
    static const Cascade preserving_grammar =
        Cascade().then(formats).then(preserved_orientation).then(either_order);

    return speech == DateSpeech::Canonical ? canonical_grammar : preserving_grammar;
}

}  // namespace sayso
