// Generates a synthetic evaluation slice when the real Google TN slice is not
// present (see eval/README.md for how to place the real one).
//
// The spoken side is produced by the small reference verbaliser below, not by
// Sayso's transducers. That matters: a slice generated from the grammars
// under test would score 100% and measure nothing. This one is written the
// other way — as ordinary arithmetic and string concatenation — so a
// disagreement is a real disagreement between two implementations of the same
// conventions. It still cannot tell you whether those conventions match the
// Google TN dataset's; only the real slice can.
//
// Build:
//   g++ -std=c++17 -O2 -o generate_slice generate_slice.cpp -I..
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../property.h"

namespace {

const char* kUnits[10] = {"zero", "one", "two",   "three", "four",
                          "five", "six", "seven", "eight", "nine"};
const char* kTeens[10] = {"ten",      "eleven",  "twelve",    "thirteen", "fourteen",
                          "fifteen",  "sixteen", "seventeen", "eighteen", "nineteen"};
const char* kTens[10] = {"", "", "twenty", "thirty", "forty",
                         "fifty", "sixty", "seventy", "eighty", "ninety"};
const char* kOrdinalUnits[10] = {"zeroth", "first", "second",  "third",  "fourth",
                                 "fifth",  "sixth", "seventh", "eighth", "ninth"};
const char* kOrdinalTeens[10] = {"tenth",       "eleventh",    "twelfth",
                                 "thirteenth",  "fourteenth",  "fifteenth",
                                 "sixteenth",   "seventeenth", "eighteenth",
                                 "nineteenth"};
const char* kOrdinalTens[10] = {"", "", "twentieth", "thirtieth", "fortieth",
                                "fiftieth", "sixtieth", "seventieth", "eightieth",
                                "ninetieth"};
const char* kMonths[13] = {"",        "january",  "february", "march",   "april",
                           "may",     "june",     "july",     "august",  "september",
                           "october", "november", "december"};
const int kMonthLengths[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return a + " " + b;
}

// 1-999, no leading "and".
std::string under_thousand(long long n) {
    std::string out;
    if (n >= 100) {
        out = std::string(kUnits[n / 100]) + " hundred";
        n %= 100;
    }
    if (n >= 20) {
        std::string tens = kTens[n / 10];
        if (n % 10) tens += std::string("-") + kUnits[n % 10];
        out = join(out, tens);
    } else if (n >= 10) {
        out = join(out, kTeens[n - 10]);
    } else if (n > 0) {
        out = join(out, kUnits[n]);
    }
    return out;
}

std::string cardinal(long long n) {
    if (n < 0) return "minus " + cardinal(-n);
    if (n == 0) return "zero";
    static const char* scales[] = {"", " thousand", " million", " billion"};
    std::vector<long long> groups;
    for (long long v = n; v > 0; v /= 1000) groups.push_back(v % 1000);
    std::string out;
    for (size_t i = groups.size(); i-- > 0;) {
        if (groups[i] == 0) continue;
        out = join(out, under_thousand(groups[i]) + scales[i]);
    }
    return out;
}

// Inflect the last word of a cardinal reading.
std::string ordinal(long long n) {
    std::string words = cardinal(n);
    size_t cut = words.find_last_of(" -");
    std::string head = cut == std::string::npos ? "" : words.substr(0, cut + 1);
    std::string last = cut == std::string::npos ? words : words.substr(cut + 1);
    static const std::pair<const char*, const char*> irregular[] = {
        {"hundred", "hundredth"}, {"thousand", "thousandth"},
        {"million", "millionth"}, {"billion", "billionth"},
    };
    for (const auto& [from, to] : irregular)
        if (last == from) return head + to;
    for (int d = 0; d <= 9; ++d) {
        if (last == kUnits[d]) return head + kOrdinalUnits[d];
        if (last == kTeens[d]) return head + kOrdinalTeens[d];
        if (d >= 2 && last == kTens[d]) return head + kOrdinalTens[d];
    }
    return head + last + "th";
}

const char* ordinal_suffix(long long n) {
    long long tail = n % 100;
    if (tail >= 11 && tail <= 13) return "th";
    switch (n % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

std::string year_words(int y) {
    int lead = y / 100, tail = y % 100;
    if (y >= 2000 && y <= 2009) return cardinal(y);
    if (tail == 0) return under_thousand(lead) + " hundred";
    if (tail < 10) return under_thousand(lead) + " oh " + kUnits[tail];
    return under_thousand(lead) + " " + under_thousand(tail);
}

std::string two_digits(int n) {
    return std::string(1, static_cast<char>('0' + n / 10)) +
           std::string(1, static_cast<char>('0' + n % 10));
}

struct Example {
    std::string semiotic_class, written, spoken;
    // The written form ITN is expected to produce. It differs from `written`
    // only where a class has several written forms for one spoken form —
    // dates, in practice. See the note in eval/eval_main.cpp.
    std::string canonical;
};

const char* kCarriers[] = {
    "the report lists %s in the summary",
    "she wrote %s on the form",
    "%s was the figure they agreed",
    "he checked %s twice",
    "the record shows %s",
    "according to the notes %s",
    "they settled on %s after the meeting",
    "the total came to %s",
};

// The written and spoken sides of an example have to sit in the *same*
// carrier sentence, so the pattern is chosen once and applied to both.
std::string carry(size_t pattern_index, const std::string& body) {
    std::string pattern = kCarriers[pattern_index];
    size_t at = pattern.find("%s");
    return pattern.substr(0, at) + body + pattern.substr(at + 2);
}

Example make_cardinal(prop::Rng& rng) {
    int digits = static_cast<int>(rng.in_range(1, 9));
    long long hi = 1;
    for (int i = 0; i < digits; ++i) hi *= 10;
    long long n = rng.in_range(0, hi - 1);
    if (rng.in_range(0, 9) == 0) n = -n;
    // A bare four-digit number in the year range is read the way a year is
    // read, so the reference has to say so too. A signed one is not: nothing
    // writes a negative year.
    std::string spoken = (n >= 1100 && n <= 2099) ? year_words(static_cast<int>(n))
                                                  : cardinal(n);
    return {"CARDINAL", std::to_string(n), spoken, std::to_string(n)};
}

Example make_ordinal(prop::Rng& rng) {
    int digits = static_cast<int>(rng.in_range(1, 6));
    long long hi = 1;
    for (int i = 0; i < digits; ++i) hi *= 10;
    long long n = rng.in_range(1, hi);
    std::string written = std::to_string(n) + ordinal_suffix(n);
    return {"ORDINAL", written, ordinal(n), written};
}

Example make_money(prop::Rng& rng) {
    struct Unit { const char* symbol; const char* one; const char* many;
                  const char* minor_one; const char* minor_many; };
    static const Unit units[] = {
        {"$", "dollar", "dollars", "cent", "cents"},
        {"€", "euro", "euros", "cent", "cents"},
        {"£", "pound", "pounds", "penny", "pence"},
    };
    const Unit& u = units[rng.pick(3)];
    long long dollars = rng.in_range(0, 99999);
    long long cents = rng.in_range(0, 4) == 0 ? rng.in_range(1, 99) : 0;
    std::string written = std::string(u.symbol) + std::to_string(dollars);
    std::string spoken;
    if (dollars == 0 && cents == 0) { dollars = 1; }
    if (dollars > 0) spoken = cardinal(dollars) + " " + (dollars == 1 ? u.one : u.many);
    if (cents > 0) {
        written += "." + two_digits(static_cast<int>(cents));
        std::string minor = cardinal(cents) + " " + (cents == 1 ? u.minor_one : u.minor_many);
        spoken = spoken.empty() ? minor : spoken + " and " + minor;
    }
    if (dollars == 0) written = std::string(u.symbol) + "0." + two_digits(static_cast<int>(cents));
    return {"MONEY", written, spoken, written};
}

Example make_date(prop::Rng& rng) {
    int year = static_cast<int>(rng.in_range(1900, 2099));
    int month = static_cast<int>(rng.in_range(1, 12));
    int day = static_cast<int>(rng.in_range(1, kMonthLengths[month]));
    std::string spoken = std::string(kMonths[month]) + " " + ordinal(day) + " " +
                         year_words(year);
    std::string written;
    switch (rng.pick(4)) {
        case 0: written = std::to_string(month) + "/" + std::to_string(day) + "/" +
                          std::to_string(year); break;
        case 1: written = std::to_string(year) + "-" + two_digits(month) + "-" +
                          two_digits(day); break;
        case 2: written = std::string(kMonths[month]) + " " + std::to_string(day) + ", " +
                          std::to_string(year); break;
        default: written = std::to_string(day) + " " + kMonths[month] + " " +
                           std::to_string(year); break;
    }
    std::string canonical = std::to_string(month) + "/" + std::to_string(day) + "/" +
                            std::to_string(year);
    return {"DATE", written, spoken, canonical};
}

// The tail of a decimal is spoken digit by digit, so the reference builds it
// that way rather than reaching for cardinal().
Example make_decimal(prop::Rng& rng) {
    long long whole = rng.in_range(0, 99999);
    int places = static_cast<int>(rng.in_range(1, 3));
    std::string tail, spoken_tail;
    for (int i = 0; i < places; ++i) {
        int d = static_cast<int>(rng.in_range(0, 9));
        tail += static_cast<char>('0' + d);
        spoken_tail += std::string(" ") + kUnits[d];
    }
    std::string written = std::to_string(whole) + "." + tail;
    return {"DECIMAL", written, cardinal(whole) + " point" + spoken_tail, written};
}

Example make_fraction(prop::Rng& rng) {
    struct Denominator { int value; const char* singular; const char* plural; };
    static const Denominator kDenominators[] = {
        {2, "half", "halves"},          {3, "third", "thirds"},
        {4, "quarter", "quarters"},     {5, "fifth", "fifths"},
        {8, "eighth", "eighths"},       {10, "tenth", "tenths"},
        {16, "sixteenth", "sixteenths"},{100, "hundredth", "hundredths"},
    };
    const Denominator& d = kDenominators[rng.pick(8)];
    long long numerator = rng.in_range(1, 99);
    bool mixed = rng.in_range(0, 3) == 0;

    // Standing alone the numerator is "one"; after a whole number it is "a".
    std::string part = numerator == 1
                           ? std::string(mixed ? "a " : "one ") + d.singular
                           : cardinal(numerator) + " " + d.plural;
    std::string body = std::to_string(numerator) + "/" + std::to_string(d.value);
    if (!mixed) return {"FRACTION", body, part, body};

    long long whole = rng.in_range(1, 99);
    std::string written = std::to_string(whole) + " " + body;
    return {"FRACTION", written, cardinal(whole) + " and " + part, written};
}

Example make_time(prop::Rng& rng) {
    int hour = static_cast<int>(rng.in_range(1, 12));
    int minute = static_cast<int>(rng.in_range(0, 59));
    bool meridiem = rng.in_range(0, 1) == 1;

    std::string written = std::to_string(hour) + ":" + two_digits(minute);
    std::string spoken = cardinal(hour);
    if (minute == 0) {
        // A round hour is "o'clock" alone, and simply unsaid before a meridiem.
        if (!meridiem) spoken += " o'clock";
    } else if (minute < 10) {
        spoken += std::string(" oh ") + kUnits[minute];
    } else {
        spoken += " " + under_thousand(minute);
    }
    if (meridiem) {
        written += " pm";
        spoken += " p m";
    }
    return {"TIME", written, spoken, written};
}

Example make_measure(prop::Rng& rng) {
    struct Unit { const char* written; const char* singular; const char* plural;
                  bool spaced; };
    static const Unit kUnitTable[] = {
        {"km", "kilometer", "kilometers", true},
        {"kg", "kilogram", "kilograms", true},
        {"cm", "centimeter", "centimeters", true},
        {"mi", "mile", "miles", true},
        {"ft", "foot", "feet", true},
        {"MB", "megabyte", "megabytes", true},
        {"%", "percent", "percent", false},
        {"\xc2\xb0" "C", "degree celsius", "degrees celsius", false},
    };
    const Unit& u = kUnitTable[rng.pick(8)];
    long long amount = rng.in_range(1, 9999);
    std::string written =
        std::to_string(amount) + (u.spaced ? " " : "") + u.written;
    std::string spoken =
        cardinal(amount) + " " + (amount == 1 ? u.singular : u.plural);
    return {"MEASURE", written, spoken, written};
}

}  // namespace

int main(int argc, char** argv) {
    std::string path = "data/google_tn_slice.tsv";
    int count = 500;
    bool force = false;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--force") force = true;
        else positional.push_back(arg);
    }
    if (!positional.empty()) path = positional[0];
    if (positional.size() > 1) count = std::atoi(positional[1].c_str());

    // Refuse to clobber. The default path is where the *real* slice goes, and
    // that one costs a multi-gigabyte download to replace; overwriting it with
    // synthetic data by running this with no arguments would be a quiet and
    // expensive mistake.
    {
        std::ifstream existing(path);
        if (existing && !force) {
            std::cerr << path << " already exists.\n"
                      << "Pass --force to overwrite it, or give another path:\n"
                      << "  ./generate_slice data/synthetic_slice.tsv\n";
            return 1;
        }
    }

    std::ofstream out(path);
    if (!out) {
        std::cerr << "cannot write " << path << "\n";
        return 1;
    }
    out << "# Synthetic slice generated by eval/generate_slice.cpp.\n"
        << "# Columns: semiotic class, written, spoken, canonical written (for ITN).\n"
        << "# Replace with the real Google TN slice; see eval/README.md.\n";

    prop::Rng rng(20260804);
    for (int i = 0; i < count; ++i) {
        Example e;
        switch (i % 8) {
            case 0: e = make_cardinal(rng); break;
            case 1: e = make_ordinal(rng); break;
            case 2: e = make_money(rng); break;
            case 3: e = make_date(rng); break;
            case 4: e = make_decimal(rng); break;
            case 5: e = make_fraction(rng); break;
            case 6: e = make_time(rng); break;
            default: e = make_measure(rng); break;
        }
        size_t pattern = rng.pick(sizeof(kCarriers) / sizeof(kCarriers[0]));
        out << e.semiotic_class << "\t" << carry(pattern, e.written) << "\t"
            << carry(pattern, e.spoken) << "\t" << carry(pattern, e.canonical) << "\n";
    }
    std::cout << "wrote " << count << " synthetic examples to " << path << "\n";
    return 0;
}
