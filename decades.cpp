#include "decades.h"

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

// The tens words in the plural, indexed by the tens digit. Zero is "hundreds",
// which is what "1900s" is; one is "tens", which is what "1910s" is.
const char* kTensPlural[10] = {"hundreds",  "tens",     "twenties", "thirties",
                               "forties",   "fifties",  "sixties",  "seventies",
                               "eighties",  "nineties"};

std::string digit(int d) { return std::string(1, static_cast<char>('0' + d)); }
std::string two_digits(int n) { return digit(n / 10) + digit(n % 10); }

std::vector<std::string> cardinal_words(int n) {
    if (n < 10) return {kUnits[n]};
    if (n < 20) return {kTeens[n - 10]};
    if (n % 10 == 0) return {kTens[n / 10]};
    return {kTens[n / 10], kUnits[n % 10]};
}

Wfst build_decade() {
    std::vector<Wfst> branches;

    // The full form: a century pair, then a pluralised decade.
    for (int century = kBareYearFirstCentury; century <= kBareYearLastCentury;
         ++century) {
        std::vector<Wfst> tails;
        for (int tens = 0; tens <= 9; ++tens) {
            // 2000s is "two thousands", handled below; the paired reading of
            // that one decade pays the same cost that keeps 2005 out of
            // "twenty oh five".
            float cost = (century == 20 && tens == 0) ? kTwoThousandsPenalty : 0.0f;
            tails.push_back(rule(digit(tens) + "0s", {kTensPlural[tens]}, cost));
        }
        branches.push_back(concat_of({rule(two_digits(century), cardinal_words(century)),
                                      union_of(tails)}));
    }
    branches.push_back(rule("2000s", {"two", "thousands"}));

    // The short form, with or without the apostrophe that marks the elision.
    // No weight separates them from the full form: "seventies" alone is only
    // ever "70s", and "nineteen seventies" only ever "1970s".
    //
    // It starts at twenty because "tens" is an ordinary English quantifier and
    // this grammar runs backwards: with a "10s" branch, "tens of thousands of
    // casualties" becomes "10s of thousands of casualties". The other tens
    // words are not quantifiers, so they carry no such risk. "hundreds" and
    // "thousands" are safe for a different reason, they appear only in the
    // full form, which needs a century word in front of them.
    for (int tens = 2; tens <= 9; ++tens) {
        Wfst body = rule(digit(tens) + "0s", {kTensPlural[tens]});
        branches.push_back(body);
        branches.push_back(concat_of({chars_in("'", kElidedCenturyPenalty), body}));
    }
    return connect(hyphenated(union_of(branches)));
}

}  // namespace

const Cascade& decade_grammar() {
    static const Wfst core = build_decade();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
