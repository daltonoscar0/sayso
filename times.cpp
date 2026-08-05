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

// 0-23, zero-padded or not. Padding costs, so ITN writes "3:30".
Wfst hour_field() {
    std::vector<Wfst> hours;
    for (int h = 0; h <= kLastHour; ++h) {
        hours.push_back(rule(std::to_string(h), cardinal_words(h)));
        if (h < 10) hours.push_back(rule(two_digits(h), cardinal_words(h), kZeroPadPenalty));
    }
    return union_of(hours);
}

// Always two written digits. A round hour may be spoken as "o'clock" or not
// spoken at all, and the caller says which of those it would rather produce.
Wfst minute_field(float oclock_cost, float bare_cost) {
    std::vector<Wfst> minutes{
        rule("00", {"o'clock"}, oclock_cost),
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
    // Standing alone, a round hour is "o'clock"; before a meridiem it is not
    // said at all. The same weight, on opposite branches.
    Wfst plain = concat_of({hour_field(), chars_in(":"),
                            minute_field(0.0f, kBareHourPenalty)});
    Wfst before_meridiem = concat_of({hour_field(), chars_in(":"),
                                      minute_field(kBareHourPenalty, 0.0f)});
    Wfst separator =
        union_of({chars_in(" "), empty_string_fst(kMeridiemSpacingPenalty)});

    std::vector<Wfst> branches{plain};
    for (const Meridiem& m : meridiems())
        branches.push_back(concat_of({before_meridiem, separator,
                                      chars_in(m.written, m.cost),
                                      words_out(m.spoken)}));
    return connect(hyphenated(union_of(branches)));
}

}  // namespace

const Cascade& time_grammar() {
    static const Wfst core = build_time();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
