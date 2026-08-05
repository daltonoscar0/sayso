// Decades, abbreviations, dotted acronyms, Roman numerals and ranges.
//
// These are the classes whose written side is drawn from ordinary English
// rather than from digits, and most of them are one-directional. A grammar
// test here says only what the machine reads; whether CLASSIFY offers it at
// all in the ITN direction is a property of the registry, and is pinned in
// pipeline_tests.cpp.
//
// Build:
//   g++ -std=c++17 -O2 -o lexical_tests lexical_tests.cpp decades.cpp \
//       abbrev.cpp letters.cpp ranges.cpp numbers.cpp money.cpp dates.cpp \
//       decimals.cpp fractions.cpp times.cpp measures.cpp wfst.cpp symbols.cpp -I.
#include <iostream>
#include <string>
#include <vector>

#include "abbrev.h"
#include "decades.h"
#include "letters.h"
#include "numbers.h"
#include "property.h"
#include "ranges.h"
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

// ---- decades ----------------------------------------------------------------

static void decade_tests() {
    check(table(decade_grammar(),
                {
                    {"1970s", "nineteen seventies"},
                    {"1980s", "nineteen eighties"},
                    {"1890s", "eighteen nineties"},
                    {"1590s", "fifteen nineties"},
                },
                true),
          "a decade is a year pair with a pluralised tail");

    // The irregular ones, all of which the reference is explicit about.
    check(table(decade_grammar(),
                {
                    {"1900s", "nineteen hundreds"},
                    {"1800s", "eighteen hundreds"},
                    {"1910s", "nineteen tens"},
                    {"2010s", "twenty tens"},
                    {"2000s", "two thousands"},
                },
                true),
          "hundreds, tens, and the thousands across the millennium");

    // The same weight that keeps 2005 out of "twenty oh five".
    check(tts(decade_grammar(), "2000s") == "two thousands" &&
              itn(decade_grammar(), "twenty hundreds") == "2000s",
          "'twenty hundreds' is read, 'two thousands' is written");

    check(table(decade_grammar(),
                {
                    {"70s", "seventies"},
                    {"20s", "twenties"},
                },
                true),
          "the short form drops the century");

    check(tts(decade_grammar(), "'80s") == "eighties" &&
              itn(decade_grammar(), "eighties") == "80s",
          "the apostrophe is read, the plain form is written");

    // "tens" is an ordinary English quantifier and this grammar runs
    // backwards, so the short form starts at twenty: with a "10s" branch,
    // "tens of thousands" would come back as "10s of thousands".
    check(tts(decade_grammar(), "10s") == "<none>" &&
              itn(decade_grammar(), "tens") == "<none>",
          "the short form starts at twenty, so 'tens' is left alone");
}

// ---- abbreviations ----------------------------------------------------------

static void abbreviation_tests() {
    check(table(abbreviation_grammar(),
                {
                    {"st", "saint"},
                    {"vol", "volume"},
                    {"dr", "doctor"},
                    {"ltd", "limited"},
                    {"vs", "versus"},
                    {"jr", "junior"},
                    {"mr", "mister"},
                    {"etc", "etcetera"},
                    {"mt", "mount"},
                    {"prof", "professor"},
                },
                false),
          "abbreviations expand");

    check(tts(abbreviation_grammar(), "vol.") == "volume",
          "a trailing full stop is read");

    // Excluded because the ordinary word is far commoner than the
    // abbreviation: "no" is the word "no" 8,805 times against 288 expansions.
    check(tts(abbreviation_grammar(), "no") == "<none>" &&
              tts(abbreviation_grammar(), "co") == "<none>" &&
              tts(abbreviation_grammar(), "min") == "<none>" &&
              tts(abbreviation_grammar(), "col") == "<none>",
          "abbreviations that are commoner as words are not in the table");
}

// ---- dotted acronyms --------------------------------------------------------

static void letters_tests() {
    check(table(letters_grammar(),
                {
                    {"u.s.", "u s"},
                    {"d.c.", "d c"},
                    {"e.g.", "e g"},
                    {"a.k.a.", "a k a"},
                },
                false),
          "a dotted acronym is spelled out");

    check(tts(letters_grammar(), "u.s") == "u s",
          "the last full stop may be missing");

    // Two letters minimum. A one-letter rule reads the [s] [.] of "the 1980s."
    // as a dotted initial, which both spells out the decade's plural and eats
    // the sentence's full stop, because a claimed span is replaced whole.
    check(tts(letters_grammar(), "s.") == "<none>" &&
              tts(letters_grammar(), "c.") == "<none>",
          "a single dotted letter is refused, so a final full stop survives");

    // The whole restriction in one test. Spans arrive case-folded, so nothing
    // here can tell "PDF" from "pdf"; a bare-letter-run branch would spell out
    // every lower-case word it met, and a bare single letter would catch the
    // English words "a" and "i".
    check(tts(letters_grammar(), "pdf") == "<none>" &&
              tts(letters_grammar(), "the") == "<none>" &&
              tts(letters_grammar(), "a") == "<none>" &&
              tts(letters_grammar(), "i") == "<none>",
          "an undotted run is refused, because case folding leaves no signal");
}

// ---- Roman numerals, which live in the cardinal grammar ---------------------

static void roman_tests() {
    check(table(cardinal_grammar(),
                {
                    {"ii", "two"},
                    {"iii", "three"},
                    {"iv", "four"},
                    {"viii", "eight"},
                    {"xii", "twelve"},
                    {"xx", "twenty"},
                },
                false),
          "Roman numerals read as cardinals");

    // Read, never written: ITN's "two" meets "2" at a hair and "II" at a real
    // penalty. That is the whole reason they live inside the cardinal grammar
    // rather than in a class of their own.
    check(itn(cardinal_grammar(), "two") == "2" &&
              itn(cardinal_grammar(), "twelve") == "12",
          "a spoken number is written in digits, never as a numeral");

    // Every single letter is excluded: I, V, X, L, C, D and M are overwhelmingly
    // not numerals, and a span arrives with its capital folded away.
    check(tts(cardinal_grammar(), "i") == "<none>" &&
              tts(cardinal_grammar(), "v") == "<none>" &&
              tts(cardinal_grammar(), "x") == "<none>" &&
              tts(cardinal_grammar(), "m") == "<none>",
          "single-letter numerals are not in the table");

    // And it is a table, not a parser: a parser over those seven letters
    // accepts a great deal of English.
    check(tts(cardinal_grammar(), "mix") == "<none>" &&
              tts(cardinal_grammar(), "did") == "<none>" &&
              tts(cardinal_grammar(), "civic") == "<none>",
          "words made of numeral letters are not numerals");
}

// ---- ranges -----------------------------------------------------------------

static void range_tests() {
    check(table(range_grammar(),
                {
                    {"1-10", "one to ten"},
                    {"5 - 0", "five to zero"},
                    {"25 - 26", "twenty-five to twenty-six"},
                    {"1729 - 1811", "seventeen twenty-nine to eighteen eleven"},
                    {"1893-94", "eighteen ninety-three to ninety-four"},
                },
                false),
          "a hyphen between two numbers is 'to'");

    check(tts(range_grammar(), "1624-1713") ==
              "sixteen twenty-four to seventeen thirteen",
          "a four-digit operand is read as a year");

    // The right operand may carry the unit for both halves. Without this,
    // RANGE claimed "5-10" and left the "%" to PLAIN, giving "five to ten%".
    check(tts(range_grammar(), "5-10%") == "five to ten percent" &&
              tts(range_grammar(), "21-26mm") ==
                  "twenty-one to twenty-six millimeters",
          "the right operand may carry a unit");

    // The structural guard: a four-digit number followed by a two-digit one in
    // 01-12 is the year-month prefix of an ISO date, and is refused outright
    // rather than left to the entry cost.
    check(tts(range_grammar(), "2011-11") == "<none>" &&
              tts(range_grammar(), "2015-03") == "<none>" &&
              tts(range_grammar(), "2011-13") == "twenty eleven to thirteen",
          "the year-month prefix of an ISO date is refused");

    // Leading zeros are refused too: "2003-04" is a season the reference reads
    // with the separator dropped.
    check(tts(range_grammar(), "2003-04") == "<none>" &&
              tts(range_grammar(), "01-02") == "<none>",
          "a zero-padded operand is refused");

    // Operands stop at four digits, which keeps most ISBNs and serial numbers
    // out. The corpus has no en dash at all and every em dash is silent.
    check(tts(range_grammar(), "43365-12345") == "<none>" &&
              tts(range_grammar(), "1—10") == "<none>" &&
              tts(range_grammar(), "1:10") == "<none>",
          "long operands, em dashes and colons are all refused");
}

// ---- properties -------------------------------------------------------------

static void property_tests() {
    std::string witness;

    // Every decade of every century in range, both directions.
    prop::Property<long long> decade_round_trip;
    decade_round_trip.generate = [](prop::Rng& r) {
        return r.in_range(kBareYearFirstCentury, kBareYearLastCentury) * 10 +
               r.in_range(0, 9);
    };
    decade_round_trip.shrink = [](const long long& n) {
        std::vector<long long> out;
        for (long long c : prop::shrink_integer(n / 10))
            if (c >= kBareYearFirstCentury) out.push_back(c * 10 + n % 10);
        return out;
    };
    decade_round_trip.show = [](const long long& n) {
        return std::to_string(n / 10) + std::to_string(n % 10) + "0s";
    };
    decade_round_trip.holds = [](const long long& n) {
        std::string written =
            std::to_string(n / 10) + std::to_string(n % 10) + "0s";
        std::string spoken = tts(decade_grammar(), written);
        return spoken != "<none>" && itn(decade_grammar(), spoken) == written;
    };
    check(prop::check_property(decade_round_trip, 20260809, 300, witness),
          "property: itn(tts(decade)) == decade" +
              (witness.empty() ? "" : " [" + witness + "]"));

    // A range reads as its two operands with "to" between them, which is the
    // property that says the operands are ordinary bare numbers.
    prop::Property<long long> range_operands;
    range_operands.generate = [](prop::Rng& r) {
        return r.in_range(1, 999) * 1000 + r.in_range(1, 999);
    };
    range_operands.shrink = [](const long long& n) {
        std::vector<long long> out;
        for (long long a : prop::shrink_integer(n / 1000))
            if (a >= 1) out.push_back(a * 1000 + n % 1000);
        return out;
    };
    range_operands.show = [](const long long& n) {
        return std::to_string(n / 1000) + "-" + std::to_string(n % 1000);
    };
    range_operands.holds = [](const long long& n) {
        long long left = n / 1000, right = n % 1000;
        std::string spoken = tts(range_grammar(), std::to_string(left) + "-" +
                                                      std::to_string(right));
        std::string want = tts(cardinal_grammar(), std::to_string(left)) + " to " +
                           tts(cardinal_grammar(), std::to_string(right));
        return spoken == want;
    };
    witness.clear();
    check(prop::check_property(range_operands, 20260810, 300, witness),
          "property: a range reads as its operands with 'to' between" +
              (witness.empty() ? "" : " [" + witness + "]"));
}

int main() {
    decade_tests();
    abbreviation_tests();
    letters_tests();
    roman_tests();
    range_tests();
    property_tests();
    std::cout << "\nTests run:" << run << "\nTests failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
