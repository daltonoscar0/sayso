#include "times.h"

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

std::string digit(int d) { return std::string(1, static_cast<char>('0' + d)); }
std::string two_digits(int n) { return digit(n / 10) + digit(n % 10); }

// Left unhyphenated; the hyphenator stage joins "twenty" to "one".
std::vector<std::string> cardinal_words(int n) {
    if (n < 10) return {kUnits[n]};
    if (n < 20) return {kTeens[n - 10]};
    if (n % 10 == 0) return {kTens[n / 10]};
    return {kTens[n / 10], kUnits[n % 10]};
}

// Zero-padded or not. Padding costs, so ITN writes "3:30".
//
// The range is a parameter because the two clocks say a round hour
// differently: 1-12 take "o'clock", 0 and 13-23 take "hundred".
Wfst hour_field(int first, int last) {
    std::vector<Wfst> hours;
    for (int h = first; h <= last; ++h) {
        hours.push_back(rule(std::to_string(h), cardinal_words(h)));
        if (h < 10) hours.push_back(rule(two_digits(h), cardinal_words(h), kZeroPadPenalty));
    }
    return union_of(hours);
}

Wfst civil_hours() { return hour_field(1, 12); }
Wfst military_hours() {
    return union_of({hour_field(0, 0), hour_field(13, kLastHour)});
}

// Always two written digits. A round hour may be spoken as "o'clock", as
// "hundred", or not spoken at all; the caller says which of the three it would
// rather produce, which is what makes "10:00" an o'clock and "18:00" a
// hundred without either reading being refused to the other.
Wfst minute_field(float oclock_cost, float hundred_cost, float bare_cost) {
    std::vector<Wfst> minutes{
        rule("00", {"o'clock"}, oclock_cost),
        rule("00", {"hundred"}, hundred_cost),
        chars_in("00", bare_cost),
    };
    for (int m = 1; m <= 9; ++m)
        minutes.push_back(rule(two_digits(m), {"oh", kUnits[m]}));
    for (int m = 10; m <= 59; ++m)
        minutes.push_back(rule(two_digits(m), cardinal_words(m)));
    return union_of(minutes);
}

struct Meridiem {
    const char* written;
    float cost;
    std::vector<std::string> spoken;
};

const std::vector<Meridiem>& meridiems() {
    static const std::vector<Meridiem> table = {
        {"am", 0.0f, {"a", "m"}},
        {"pm", 0.0f, {"p", "m"}},
        {"a.m.", kDottedMeridiemPenalty, {"a", "m"}},
        {"p.m.", kDottedMeridiemPenalty, {"p", "m"}},
    };
    return table;
}

Wfst build_time() {
    // Standing alone, a round hour is "o'clock" on a twelve-hour clock and
    // "hundred" on a twenty-four-hour one; before a meridiem it is not said at
    // all. Three weights, leaning differently in each branch.
    Wfst civil = concat_of({civil_hours(), chars_in(":"),
                            minute_field(0.0f, kWrongClockStylePenalty,
                                         kBareHourPenalty)});
    Wfst military = concat_of({military_hours(), chars_in(":"),
                               minute_field(kWrongClockStylePenalty, 0.0f,
                                            kBareHourPenalty)});
    Wfst before_meridiem = concat_of({civil_hours(), chars_in(":"),
                                      minute_field(kBareHourPenalty,
                                                   kWrongClockStylePenalty, 0.0f)});
    // The British full stop, and only where a meridiem follows it. Without
    // one, "10.30" is a decimal far more often than it is a time, and the
    // classifier has no context to tell them apart.
    Wfst dotted = concat_of({civil_hours(), chars_in(".", kDotTimeSeparatorPenalty),
                             minute_field(kBareHourPenalty, kWrongClockStylePenalty,
                                          0.0f)});
    Wfst separator =
        union_of({chars_in(" "), empty_string_fst(kMeridiemSpacingPenalty)});

    // "5pm", with no minute field at all. Read, but ITN writes "5:00 pm".
    Wfst bare_hour = reweight(civil_hours(), kOmittedMinutePenalty);

    std::vector<Wfst> branches{civil, military};
    for (const Meridiem& m : meridiems()) {
        Wfst tail = concat_of({separator, chars_in(m.written, m.cost),
                               words_out(m.spoken)});
        branches.push_back(concat_of({before_meridiem, tail}));
        branches.push_back(concat_of({dotted, tail}));
        branches.push_back(concat_of({bare_hour, tail}));
    }
    return connect(hyphenated(union_of(branches)));
}

}  // namespace

const Cascade& time_grammar() {
    static const Wfst core = build_time();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
