// Currency and date grammar tests, including round-trip properties over
// random amounts and random dates.
//
// Build:
//   g++ -std=c++17 -O2 -o money_date_tests money_date_tests.cpp money.cpp \
//       dates.cpp numbers.cpp wfst.cpp symbols.cpp -I.
#include <iostream>
#include <string>
#include <vector>

#include "dates.h"
#include "money.h"
#include "numbers.h"
#include "property.h"
#include "wfst.h"

using namespace sayso;

static int run = 0, failed = 0;

static void check(bool ok, const std::string& name) {
    ++run;
    if (!ok) { ++failed; std::cout << "[FAIL]" << name << "\n"; }
    else     {           std::cout << "[PASS]" << name << "\n"; }
}

static std::string tts(const Cascade& g, const std::string& written) {
    Path p = g.apply(chars_of(written));
    return p.found ? render(p.olabels) : "<none>";
}

static std::string itn(const Cascade& g, const std::string& spoken) {
    std::vector<std::string> words;
    std::string current;
    for (char c : spoken + " ") {
        if (c == ' ') { if (!current.empty()) words.push_back(current); current.clear(); }
        else current += c;
    }
    Path p = g.apply_inverse(words_of(words));
    return p.found ? render(p.olabels) : "<none>";
}

struct Case { const char* written; const char* spoken; };

static bool table(const Cascade& g, const std::vector<Case>& cases, bool both_ways) {
    bool ok = true;
    for (const Case& c : cases) {
        if (tts(g, c.written) != c.spoken) {
            ok = false;
            std::cout << "  tts " << c.written << " -> " << tts(g, c.written)
                      << " (want " << c.spoken << ")\n";
        }
        if (both_ways && itn(g, c.spoken) != c.written) {
            ok = false;
            std::cout << "  itn " << c.spoken << " -> " << itn(g, c.spoken)
                      << " (want " << c.written << ")\n";
        }
    }
    return ok;
}

// ---- money ------------------------------------------------------------------

static void money_tests() {
    check(table(money_grammar(),
                {
                    {"$25", "twenty-five dollars"},
                    {"$1", "one dollar"},
                    {"$100", "one hundred dollars"},
                    {"$1234", "one thousand two hundred thirty-four dollars"},
                    {"$25.10", "twenty-five dollars and ten cents"},
                    {"$1.01", "one dollar and one cent"},
                    {"$0.99", "ninety-nine cents"},
                    {"$0.01", "one cent"},
                    {"€40", "forty euros"},
                    {"£3", "three pounds"},
                    {"£1.50", "one pound and fifty pence"},
                    {"£0.01", "one penny"},
                },
                true),
          "currency amounts round trip in both directions");

    // A magnitude survives the round trip only where plain digits could not
    // say the same words: "two point five million" has no numeral spelling.
    check(table(money_grammar(),
                {
                    {"$2.5 million", "two point five million dollars"},
                    {"$1.75 billion", "one point seven five billion dollars"},
                },
                true),
          "magnitudes: $2.5 million");

    // Where digits do say the same words, digits are what ITN writes — the
    // same canonicalisation that drops the thousands comma. Both written forms
    // are still read.
    check(tts(money_grammar(), "$3 million") == "three million dollars" &&
              tts(money_grammar(), "€250 thousand") == "two hundred fifty thousand euros" &&
              itn(money_grammar(), "three million dollars") == "$3000000" &&
              itn(money_grammar(), "two hundred fifty thousand euros") == "€250000",
          "a magnitude word is read, but digits are written where they suffice");

    // Newspaper suffixes, which arrive case-folded: "$3B" is "$3b" by the time
    // the grammar sees it.
    check(table(money_grammar(),
                {
                    {"$6.5m", "six point five million dollars"},
                    {"£20m", "twenty million pounds"},
                    {"$26.6bn", "twenty-six point six billion dollars"},
                    {"$3b", "three billion dollars"},
                    {"$50k", "fifty thousand dollars"},
                },
                false),
          "letter magnitude suffixes are read");

    check(itn(money_grammar(), "six point five million dollars") == "$6.5 million",
          "a suffix is read, the spelled magnitude is written");

    check(tts(money_grammar(), "$ 1") == "one dollar" &&
              itn(money_grammar(), "one dollar") == "$1",
          "a space after the symbol is read, never written");

    check(tts(money_grammar(), "$1,234") ==
              "one thousand two hundred thirty-four dollars" &&
          tts(money_grammar(), "$25.00") == "twenty-five dollars",
          "thousands commas and .00 are read");

    check(itn(money_grammar(), "twenty-five dollars") == "$25" &&
          itn(money_grammar(), "one thousand two hundred thirty-four dollars") == "$1234",
          "ITN writes the canonical form: no comma, no trailing .00");

    // Ungrammatical but readable: accepted, never generated.
    check(itn(money_grammar(), "one dollars") == "$1" &&
          itn(money_grammar(), "twenty-five dollars ten cents") == "$25.10" &&
          itn(money_grammar(), "zero dollars and ninety-nine cents") == "$0.99",
          "variants Sayso accepts but will not produce");

    check(tts(money_grammar(), "$1.234") == "<none>" &&
          tts(money_grammar(), "25") == "<none>",
          "money needs a symbol and a well-formed amount");
}

// ---- dates ------------------------------------------------------------------

static void date_tests() {
    check(table(date_grammar(),
                {
                    {"3/14/2015", "march fourteenth twenty fifteen"},
                    {"12/25/1999", "december twenty-fifth nineteen ninety-nine"},
                    {"1/1/2000", "january first two thousand"},
                    {"7/4/1776", "july fourth seventeen seventy-six"},
                    {"5/2/2005", "may second two thousand five"},
                    {"11/9/1905", "november ninth nineteen oh five"},
                    {"6/30/1900", "june thirtieth nineteen hundred"},
                    {"10/31/2021", "october thirty-first twenty twenty-one"},
                },
                true),
          "numeric dates round trip in both directions");

    check(table(date_grammar(),
                {
                    {"03/14/2015", "march fourteenth twenty fifteen"},
                    {"2015-03-14", "march fourteenth twenty fifteen"},
                    {"march 14, 2015", "march fourteenth twenty fifteen"},
                    {"march 14 2015", "march fourteenth twenty fifteen"},
                    {"14 march 2015", "march fourteenth twenty fifteen"},
                    {"1999-12-25", "december twenty-fifth nineteen ninety-nine"},
                },
                false),
          "every written format reaches the same spoken form");

    check(itn(date_grammar(), "march fourteenth twenty fifteen") == "3/14/2015",
          "ITN writes M/D/YYYY whatever format it was told about");

    check(itn(date_grammar(), "march fourteen twenty fifteen") == "3/14/2015",
          "a cardinal day is read, an ordinal one is written");

    // Year readings: paired by default, cardinal in the 2000s.
    check(tts(date_grammar(), "1/1/2005") == "january first two thousand five" &&
          tts(date_grammar(), "1/1/1905") == "january first nineteen oh five" &&
          tts(date_grammar(), "1/1/2015") == "january first twenty fifteen",
          "2005 is 'two thousand five' but 1905 is 'nineteen oh five'");

    check(tts(date_grammar(), "13/1/2015") == "<none>" &&
          tts(date_grammar(), "1/32/2015") == "<none>",
          "month and day ranges are part of the grammar");
}

// ---- ambiguity --------------------------------------------------------------

static void ambiguity_tests() {
    // "may second" without a year is not a date: the date grammar requires all
    // three fields, so no path exists and the weights never get consulted.
    check(itn(date_grammar(), "may second") == "<none>",
          "a date needs a year, so 'may second' is not one");

    // With a year it is, and the date reading is cheaper than any
    // month-as-word alternative because there is no other date path at all.
    check(itn(date_grammar(), "may second twenty twenty") == "5/2/2020",
          "'may second twenty twenty' is a date");

    // "$1" has both a singular and a plural path; the margin picks singular.
    Path singular = money_grammar().apply(chars_of("$1"));
    check(singular.found && render(singular.olabels) == "one dollar" &&
              singular.weight < money_grammar().apply(chars_of("$2")).weight + 1.0f,
          "the singular branch wins '$1' by a margin, not by exclusion");

    // Two written forms, one spoken form: the format costs pick the canonical.
    check(itn(date_grammar(), "december twenty-fifth nineteen ninety-nine") == "12/25/1999",
          "ISO input, canonical output");
}

// ---- round-trip properties --------------------------------------------------

static void property_tests() {
    // Amounts in cents, so the generator covers whole dollars and part cents.
    prop::Property<long long> money_round_trip;
    money_round_trip.generate = [](prop::Rng& r) {
        int digits = static_cast<int>(r.in_range(1, 7));
        long long hi = 1;
        for (int i = 0; i < digits; ++i) hi *= 10;
        return r.in_range(1, hi);
    };
    money_round_trip.shrink = [](const long long& n) {
        std::vector<long long> out;
        for (long long c : prop::shrink_integer(n))
            if (c > 0) out.push_back(c);
        return out;
    };
    money_round_trip.show = [](const long long& n) {
        long long cents = n % 100;
        return "$" + std::to_string(n / 100) + "." +
               std::string(cents < 10 ? "0" : "") + std::to_string(cents);
    };
    money_round_trip.holds = [](const long long& n) {
        long long dollars = n / 100, cents = n % 100;
        std::string written = "$" + std::to_string(dollars);
        if (cents > 0)
            written += "." + std::string(cents < 10 ? "0" : "") + std::to_string(cents);
        std::string spoken = tts(money_grammar(), written);
        return spoken != "<none>" && itn(money_grammar(), spoken) == written;
    };
    std::string witness;
    check(prop::check_property(money_round_trip, 20260803, 300, witness),
          "property: itn(tts(amount)) == amount" +
              (witness.empty() ? "" : " [" + witness + "]"));

    // Dates as a packed integer so the shrinker has something to walk.
    static const int kMonthLengths[13] = {0,  31, 28, 31, 30, 31, 30,
                                          31, 31, 30, 31, 30, 31};
    prop::Property<long long> date_round_trip;
    date_round_trip.generate = [](prop::Rng& r) {
        long long year = r.in_range(1000, 9999);
        long long month = r.in_range(1, 12);
        long long day = r.in_range(1, kMonthLengths[month]);
        return (year * 100 + month) * 100 + day;
    };
    date_round_trip.shrink = [](const long long& n) {
        std::vector<long long> out;
        long long year = n / 10000, month = (n / 100) % 100, day = n % 100;
        if (year > 1000) out.push_back(((year - 1) * 100 + month) * 100 + day);
        if (month > 1) out.push_back((year * 100 + month - 1) * 100 + day);
        if (day > 1) out.push_back((year * 100 + month) * 100 + day - 1);
        return out;
    };
    date_round_trip.show = [](const long long& n) {
        return std::to_string((n / 100) % 100) + "/" + std::to_string(n % 100) + "/" +
               std::to_string(n / 10000);
    };
    date_round_trip.holds = [](const long long& n) {
        std::string written = std::to_string((n / 100) % 100) + "/" +
                              std::to_string(n % 100) + "/" + std::to_string(n / 10000);
        std::string spoken = tts(date_grammar(), written);
        return spoken != "<none>" && itn(date_grammar(), spoken) == written;
    };
    witness.clear();
    check(prop::check_property(date_round_trip, 20260803, 300, witness),
          "property: itn(tts(date)) == date" +
              (witness.empty() ? "" : " [" + witness + "]"));

    // Every written format must speak the same way as the canonical one.
    prop::Property<long long> format_agreement;
    format_agreement.generate = [](prop::Rng& r) {
        long long year = r.in_range(kIsoYearFrom, kIsoYearTo);
        long long month = r.in_range(1, 12);
        long long day = r.in_range(1, kMonthLengths[month]);
        return (year * 100 + month) * 100 + day;
    };
    format_agreement.shrink = date_round_trip.shrink;
    format_agreement.show = date_round_trip.show;
    format_agreement.holds = [](const long long& n) {
        int year = static_cast<int>(n / 10000);
        int month = static_cast<int>((n / 100) % 100);
        int day = static_cast<int>(n % 100);
        static const char* names[13] = {"",     "january", "february", "march",
                                        "april", "may",    "june",     "july",
                                        "august", "september", "october", "november",
                                        "december"};
        auto pad = [](int v) {
            return (v < 10 ? std::string("0") : std::string()) + std::to_string(v);
        };
        std::string canonical = std::to_string(month) + "/" + std::to_string(day) + "/" +
                                std::to_string(year);
        std::string spoken = tts(date_grammar(), canonical);
        if (spoken == "<none>") return false;
        const std::string others[] = {
            std::to_string(year) + "-" + pad(month) + "-" + pad(day),
            std::string(names[month]) + " " + std::to_string(day) + ", " +
                std::to_string(year),
            std::to_string(day) + " " + names[month] + " " + std::to_string(year),
        };
        for (const std::string& other : others)
            if (tts(date_grammar(), other) != spoken) return false;
        return itn(date_grammar(), spoken) == canonical;
    };
    witness.clear();
    check(prop::check_property(format_agreement, 20260804, 200, witness),
          "property: all four written formats speak alike, ITN writes one" +
              (witness.empty() ? "" : " [" + witness + "]"));
}

int main() {
    money_tests();
    date_tests();
    ambiguity_tests();
    property_tests();
    std::cout << "\nTests run:" << run << "\nTests failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
