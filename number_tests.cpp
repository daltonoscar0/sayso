// Cardinal and ordinal grammar tests, including the round-trip properties
// that hold the two directions together.
//
// Build:
//   g++ -std=c++17 -O2 -o number_tests number_tests.cpp numbers.cpp wfst.cpp \
//       symbols.cpp -I.
#include <iostream>
#include <string>
#include <vector>

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

static void cardinal_tests() {
    const Case cases[] = {
        {"0", "zero"},
        {"7", "seven"},
        {"13", "thirteen"},
        {"20", "twenty"},
        {"25", "twenty-five"},
        {"99", "ninety-nine"},
        {"100", "one hundred"},
        {"101", "one hundred one"},
        {"110", "one hundred ten"},
        {"999", "nine hundred ninety-nine"},
        {"1000", "one thousand"},
        // 1100-2099 is the year range, where a bare number pairs instead; see
        // the year test below. 3456 is a four-digit cardinal outside it.
        {"3456", "three thousand four hundred fifty-six"},
        {"10000", "ten thousand"},
        {"100000", "one hundred thousand"},
        {"1000000", "one million"},
        {"2500000", "two million five hundred thousand"},
        {"999999999", "nine hundred ninety-nine million nine hundred ninety-nine "
                      "thousand nine hundred ninety-nine"},
        {"1000000001", "one billion one"},
    };
    bool ok = true;
    for (const Case& c : cases)
        if (tts(cardinal_grammar(), c.written) != c.spoken) {
            ok = false;
            std::cout << "  " << c.written << " -> " << tts(cardinal_grammar(), c.written)
                      << " (want " << c.spoken << ")\n";
        }
    check(ok, "cardinal verbalisation 0 .. 999,999,999");

    // A bare four-digit number in 1100-2099 is read the way a year is read.
    // The corpus is emphatic about this, it was the single largest source of
    // disagreement when the reading was licensed only inside a date, and the
    // cost is that a four-digit quantity in that range is read as a year too.
    check(tts(cardinal_grammar(), "1984") == "nineteen eighty-four" &&
              tts(cardinal_grammar(), "1905") == "nineteen oh five" &&
              tts(cardinal_grammar(), "1113") == "eleven thirteen" &&
              tts(cardinal_grammar(), "2015") == "twenty fifteen",
          "a bare four-digit number in the year range is paired");

    // Not across 2000-2009, where the cardinal reading is the ordinary one,
    // and not outside the range at either end.
    check(tts(cardinal_grammar(), "2005") == "two thousand five" &&
              tts(cardinal_grammar(), "2000") == "two thousand" &&
              tts(cardinal_grammar(), "1050") == "one thousand fifty" &&
              tts(cardinal_grammar(), "2100") == "two thousand one hundred",
          "the 2000s and numbers outside the range stay cardinals");

    // Both wordings are read back to the same digits, which is what keeps the
    // margin a margin rather than an exclusion.
    check(itn(cardinal_grammar(), "nineteen eighty-four") == "1984" &&
              itn(cardinal_grammar(), "one thousand nine hundred eighty-four") == "1984",
          "either wording of a year is read back to the same digits");

    // A signed number is never a year: the year branch stands outside the sign.
    check(tts(cardinal_grammar(), "-1984") == "minus one thousand nine hundred eighty-four",
          "a signed four-digit number is a cardinal, not a year");

    ok = true;
    for (const Case& c : cases)
        if (itn(cardinal_grammar(), c.spoken) != c.written) {
            ok = false;
            std::cout << "  " << c.spoken << " -> " << itn(cardinal_grammar(), c.spoken)
                      << " (want " << c.written << ")\n";
        }
    check(ok, "cardinal inverse text normalisation");

    check(tts(cardinal_grammar(), "-42") == "minus forty-two" &&
          itn(cardinal_grammar(), "minus forty-two") == "-42" &&
          itn(cardinal_grammar(), "negative forty-two") == "-42",
          "negatives, with 'negative' accepted but 'minus' generated");

    check(tts(cardinal_grammar(), "1,234") == "one thousand two hundred thirty-four" &&
          tts(cardinal_grammar(), "12,345,678") ==
              "twelve million three hundred forty-five thousand six hundred seventy-eight",
          "thousands commas are read");

    check(itn(cardinal_grammar(), "one thousand two hundred thirty-four") == "1234",
          "thousands commas are not written back");

    check(itn(cardinal_grammar(), "twenty five") == "25" &&
          itn(cardinal_grammar(), "twenty-five") == "25",
          "spaced and hyphenated compounds both parse");

    check(tts(cardinal_grammar(), "007") == "zero zero seven" &&
          tts(cardinal_grammar(), "12345678901234") ==
              "one two three four five six seven eight nine zero one two three four",
          "digit-by-digit fallback for leading zeros and over-long strings");
}

static void ordinal_tests() {
    const Case cases[] = {
        {"1st", "first"},        {"2nd", "second"},       {"3rd", "third"},
        {"4th", "fourth"},       {"5th", "fifth"},        {"8th", "eighth"},
        {"9th", "ninth"},        {"11th", "eleventh"},    {"12th", "twelfth"},
        {"13th", "thirteenth"},  {"20th", "twentieth"},   {"21st", "twenty-first"},
        {"22nd", "twenty-second"},{"23rd", "twenty-third"},{"30th", "thirtieth"},
        {"42nd", "forty-second"},{"100th", "one hundredth"},
        {"101st", "one hundred first"}, {"111th", "one hundred eleventh"},
        {"1000th", "one thousandth"},
        {"1000000th", "one millionth"},
    };
    bool ok = true;
    for (const Case& c : cases)
        if (tts(ordinal_grammar(), c.written) != c.spoken) {
            ok = false;
            std::cout << "  " << c.written << " -> " << tts(ordinal_grammar(), c.written)
                      << " (want " << c.spoken << ")\n";
        }
    check(ok, "ordinal verbalisation");

    ok = true;
    for (const Case& c : cases)
        if (itn(ordinal_grammar(), c.spoken) != c.written) {
            ok = false;
            std::cout << "  " << c.spoken << " -> " << itn(ordinal_grammar(), c.spoken)
                      << " (want " << c.written << ")\n";
        }
    check(ok, "ordinal inverse text normalisation picks the agreeing suffix");

    check(tts(ordinal_grammar(), "21th") == "<none>" && tts(ordinal_grammar(), "11st") == "<none>",
          "suffix agreement is enforced, not guessed");
}

static void hyphenation_tests() {
    check(tts(cardinal_grammar(), "45") == "forty-five" &&
          tts(cardinal_grammar(), "40") == "forty" &&
          tts(cardinal_grammar(), "40000") == "forty thousand",
          "hyphenation joins tens to units and nothing else");
}

// ---- round-trip properties --------------------------------------------------

static void property_tests() {
    prop::Property<long long> cardinal_round_trip;
    cardinal_round_trip.generate = [](prop::Rng& r) {
        // Sample across magnitudes so short numbers are not swamped.
        int digits = static_cast<int>(r.in_range(1, 9));
        long long hi = 1;
        for (int i = 0; i < digits; ++i) hi *= 10;
        long long n = r.in_range(0, hi - 1);
        return r.in_range(0, 4) == 0 ? -n : n;
    };
    cardinal_round_trip.shrink = prop::shrink_integer;
    cardinal_round_trip.show = [](const long long& n) { return std::to_string(n); };
    cardinal_round_trip.holds = [](const long long& n) {
        std::string written = std::to_string(n);
        std::string spoken = tts(cardinal_grammar(), written);
        return spoken != "<none>" && itn(cardinal_grammar(), spoken) == written;
    };
    std::string witness;
    check(prop::check_property(cardinal_round_trip, 20260803, 400, witness),
          "property: itn(tts(n)) == n for random integers" +
              (witness.empty() ? "" : " [" + witness + "]"));

    prop::Property<long long> ordinal_round_trip;
    ordinal_round_trip.generate = [](prop::Rng& r) {
        int digits = static_cast<int>(r.in_range(1, 7));
        long long hi = 1;
        for (int i = 0; i < digits; ++i) hi *= 10;
        return r.in_range(0, hi - 1);
    };
    ordinal_round_trip.shrink = prop::shrink_integer;
    ordinal_round_trip.show = [](const long long& n) { return std::to_string(n); };
    ordinal_round_trip.holds = [](const long long& n) {
        long long tail = n % 100;
        const char* suffix = (tail >= 11 && tail <= 13) ? "th"
                           : n % 10 == 1 ? "st"
                           : n % 10 == 2 ? "nd"
                           : n % 10 == 3 ? "rd" : "th";
        std::string written = std::to_string(n) + suffix;
        std::string spoken = tts(ordinal_grammar(), written);
        return spoken != "<none>" && itn(ordinal_grammar(), spoken) == written;
    };
    witness.clear();
    check(prop::check_property(ordinal_round_trip, 20260803, 300, witness),
          "property: itn(tts(nth)) == nth for random ordinals" +
              (witness.empty() ? "" : " [" + witness + "]"));

    // The other direction: a well-formed spoken form normalises back to the
    // canonical spoken form (hyphens restored, "negative" folded to "minus").
    prop::Property<long long> spoken_canonical;
    spoken_canonical.generate = [](prop::Rng& r) { return r.in_range(-999999, 999999); };
    spoken_canonical.shrink = prop::shrink_integer;
    spoken_canonical.show = [](const long long& n) { return std::to_string(n); };
    spoken_canonical.holds = [](const long long& n) {
        std::string canonical = tts(cardinal_grammar(), std::to_string(n));
        if (canonical == "<none>") return false;
        std::string variant;
        for (char c : canonical) variant += (c == '-' ? ' ' : c);
        if (n < 0) variant = "negative" + variant.substr(5);
        return tts(cardinal_grammar(), itn(cardinal_grammar(), variant)) == canonical;
    };
    witness.clear();
    check(prop::check_property(spoken_canonical, 20260804, 300, witness),
          "property: tts(itn(s)) is the canonical spoken form" +
              (witness.empty() ? "" : " [" + witness + "]"));
}

int main() {
    cardinal_tests();
    ordinal_tests();
    hyphenation_tests();
    property_tests();
    std::cout << "\nTests run:" << run << "\nTests failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
