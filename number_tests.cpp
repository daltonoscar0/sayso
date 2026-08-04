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

static std::string tts(const Wfst& f, const std::string& written) {
    Path p = best_transduction(f, chars_of(written));
    return p.found ? render(p.olabels) : "<none>";
}

static std::string itn(const Wfst& f, const std::string& spoken) {
    std::vector<std::string> words;
    std::string current;
    for (char c : spoken + " ") {
        if (c == ' ') { if (!current.empty()) words.push_back(current); current.clear(); }
        else current += c;
    }
    Path p = best_transduction(invert(f), words_of(words));
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
        {"1234", "one thousand two hundred thirty-four"},
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
        if (tts(cardinal_fst(), c.written) != c.spoken) {
            ok = false;
            std::cout << "  " << c.written << " -> " << tts(cardinal_fst(), c.written)
                      << " (want " << c.spoken << ")\n";
        }
    check(ok, "cardinal verbalisation 0 .. 999,999,999");

    ok = true;
    for (const Case& c : cases)
        if (itn(cardinal_fst(), c.spoken) != c.written) {
            ok = false;
            std::cout << "  " << c.spoken << " -> " << itn(cardinal_fst(), c.spoken)
                      << " (want " << c.written << ")\n";
        }
    check(ok, "cardinal inverse text normalisation");

    check(tts(cardinal_fst(), "-42") == "minus forty-two" &&
          itn(cardinal_fst(), "minus forty-two") == "-42" &&
          itn(cardinal_fst(), "negative forty-two") == "-42",
          "negatives, with 'negative' accepted but 'minus' generated");

    check(tts(cardinal_fst(), "1,234") == "one thousand two hundred thirty-four" &&
          tts(cardinal_fst(), "12,345,678") ==
              "twelve million three hundred forty-five thousand six hundred seventy-eight",
          "thousands commas are read");

    check(itn(cardinal_fst(), "one thousand two hundred thirty-four") == "1234",
          "thousands commas are not written back");

    check(itn(cardinal_fst(), "twenty five") == "25" &&
          itn(cardinal_fst(), "twenty-five") == "25",
          "spaced and hyphenated compounds both parse");

    check(tts(cardinal_fst(), "007") == "zero zero seven" &&
          tts(cardinal_fst(), "12345678901234") ==
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
        if (tts(ordinal_fst(), c.written) != c.spoken) {
            ok = false;
            std::cout << "  " << c.written << " -> " << tts(ordinal_fst(), c.written)
                      << " (want " << c.spoken << ")\n";
        }
    check(ok, "ordinal verbalisation");

    ok = true;
    for (const Case& c : cases)
        if (itn(ordinal_fst(), c.spoken) != c.written) {
            ok = false;
            std::cout << "  " << c.spoken << " -> " << itn(ordinal_fst(), c.spoken)
                      << " (want " << c.written << ")\n";
        }
    check(ok, "ordinal inverse text normalisation picks the agreeing suffix");

    check(tts(ordinal_fst(), "21th") == "<none>" && tts(ordinal_fst(), "11st") == "<none>",
          "suffix agreement is enforced, not guessed");
}

static void hyphenation_tests() {
    check(tts(cardinal_fst(), "45") == "forty-five" &&
          tts(cardinal_fst(), "40") == "forty" &&
          tts(cardinal_fst(), "40000") == "forty thousand",
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
        std::string spoken = tts(cardinal_fst(), written);
        return spoken != "<none>" && itn(cardinal_fst(), spoken) == written;
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
        std::string spoken = tts(ordinal_fst(), written);
        return spoken != "<none>" && itn(ordinal_fst(), spoken) == written;
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
        std::string canonical = tts(cardinal_fst(), std::to_string(n));
        if (canonical == "<none>") return false;
        std::string variant;
        for (char c : canonical) variant += (c == '-' ? ' ' : c);
        if (n < 0) variant = "negative" + variant.substr(5);
        return tts(cardinal_fst(), itn(cardinal_fst(), variant)) == canonical;
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
