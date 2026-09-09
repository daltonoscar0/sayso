// Decimal, fraction and measure grammar tests, including round-trip
// properties over random quantities.
//
// These are grammar-level tests: each cascade is exercised on its own, so a
// case here says what the machine can read and write, not what CLASSIFY would
// choose. Where two classes compete for the same span, "three pounds" is
// sterling as well as weight, the arbitration is the classifier's business
// and is pinned in pipeline_tests.cpp instead.
//
// Build:
//   g++ -std=c++17 -O2 -o quantity_tests quantity_tests.cpp decimals.cpp \
//       fractions.cpp measures.cpp numbers.cpp money.cpp dates.cpp times.cpp \
//       wfst.cpp symbols.cpp -I.
#include <iostream>
#include <string>
#include <vector>

#include "decimals.h"
#include "fractions.h"
#include "measures.h"
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

static std::string two_digits(long long n) {
    return std::string(1, static_cast<char>('0' + n / 10)) +
           std::string(1, static_cast<char>('0' + n % 10));
}

// A restatement of part of the unit table rather than a reference to it: if
// the two ever disagree about spelling or spacing, that is worth a failure.
static const char* kUnits[] = {"km", "m",  "cm",  "kg", "g",  "mi",
                               "ft", "oz", "min", "hr", "MB", "GB",
                               "%",  "°C", "Hz"};
static const size_t kUnitCount = sizeof(kUnits) / sizeof(kUnits[0]);

// Symbol units are written flush against the amount, letter units are not.
// The degree sign is multi-byte, so its lead byte has the high bit set.
static std::string measure_written(const long long& key) {
    const char* unit = kUnits[key % 100];
    bool spaced = unit[0] != '%' && static_cast<unsigned char>(unit[0]) < 0x80;
    return std::to_string(key / 100) + (spaced ? " " : "") + unit;
}

// ---- decimals ---------------------------------------------------------------

static void decimal_tests() {
    check(table(decimal_grammar(),
                {
                    {"3.14", "three point one four"},
                    {"0.5", "zero point five"},
                    {"10.5", "ten point five"},
                    {"25.05", "twenty-five point zero five"},
                    {"-2.75", "minus two point seven five"},
                    {"1000.1", "one thousand point one"},
                    {"0.001", "zero point zero zero one"},
                },
                true),
          "decimals round trip in both directions");

    // The tail is read digit by digit, never as a cardinal: the whole point of
    // the class is that "3.14" is not "three point fourteen".
    check(tts(decimal_grammar(), "3.25") == "three point two five" &&
              tts(decimal_grammar(), "1.10") == "one point one zero",
          "the fractional tail is spoken digit by digit");

    // A leading zero may go missing on either tape. Both are read; the one
    // Sayso writes is the one with the zero restored.
    check(tts(decimal_grammar(), ".5") == "point five" &&
              itn(decimal_grammar(), "point five") == "0.5" &&
              itn(decimal_grammar(), "zero point five") == "0.5",
          "a missing leading zero is read, and written back");

    check(itn(decimal_grammar(), "negative two point five") == "-2.5" &&
              tts(decimal_grammar(), "-2.5") == "minus two point five",
          "'negative' is read, 'minus' is written");

    // A decimal needs a point and something after it; a bare integer is a
    // cardinal and this grammar must not claim it.
    check(tts(decimal_grammar(), "314") == "<none>" &&
              tts(decimal_grammar(), "3.") == "<none>",
          "a decimal needs a point and a tail");
}

// ---- fractions --------------------------------------------------------------

static void fraction_tests() {
    check(table(fraction_grammar(),
                {
                    {"1/2", "one half"},
                    {"3/4", "three quarters"},
                    {"2/3", "two thirds"},
                    {"1/3", "one third"},
                    {"7/8", "seven eighths"},
                    {"15/16", "fifteen sixteenths"},
                    {"3/64", "three sixty-fourths"},
                    {"1/100", "one hundredth"},
                    {"9/1000", "nine thousandths"},
                    {"-1/2", "minus one half"},
                },
                true),
          "fractions round trip in both directions");

    check(table(fraction_grammar(),
                {
                    {"2 1/2", "two and a half"},
                    {"5 3/4", "five and three quarters"},
                    {"1 1/3", "one and a third"},
                },
                true),
          "mixed numbers take 'and', and an article for a numerator of one");

    // Standing alone the numerator is "one" and only "one". "a" is offered
    // after a whole number, where "and" precedes it, and nowhere else,
    // because it runs backwards: a fraction span covering the article of
    // "a nineteenth century painter" is cheaper than ORDINAL plus the
    // per-character cost of leaving "a" alone, and the article is absorbed
    // into the numerator rather than merely misread.
    check(tts(fraction_grammar(), "1/2") == "one half" &&
              tts(fraction_grammar(), "2 1/2") == "two and a half" &&
              itn(fraction_grammar(), "two and one half") == "2 1/2" &&
              itn(fraction_grammar(), "a half") == "<none>",
          "'one half' alone, 'and a half' only after a whole number");

    check(itn(fraction_grammar(), "three fourths") == "3/4" &&
              tts(fraction_grammar(), "3/4") == "three quarters",
          "'fourths' is read, 'quarters' is written");

    check(itn(fraction_grammar(), "three sixty fourths") == "3/64",
          "an unhyphenated compound denominator is read");

    // The singular of the same thing, which is what a speech recogniser hands
    // an inverse normaliser. The alternate-denominator weight has to stay
    // small enough to beat CARDINAL("one") plus ORDINAL("thirty second").
    check(itn(fraction_grammar(), "one thirty second") == "1/32" &&
              itn(fraction_grammar(), "one sixty fourth") == "1/64",
          "a de-hyphenated compound denominator beats a cardinal plus an ordinal");

    // Unicode's precomposed fractions, which real text is full of. Read, never
    // written: "3/4" is the form that survives every encoding.
    check(table(fraction_grammar(),
                {
                    {"½", "one half"},
                    {"¾", "three quarters"},
                    {"⅞", "seven eighths"},
                    {"⅔", "two thirds"},
                    {"2½", "two and a half"},
                    {"4¾", "four and three quarters"},
                },
                false),
          "precomposed fractions are read");

    check(itn(fraction_grammar(), "seven eighths") == "7/8" &&
              itn(fraction_grammar(), "two and a half") == "2 1/2",
          "a precomposed fraction is read, a slashed one is written");

    // A precomposed character cannot be misread, so the space before it is
    // optional. The slashed form's space is not: without it, "21/2" would be
    // a mixed number rather than twenty-one halves.
    check(tts(fraction_grammar(), "3 ½") == "three and a half" &&
              tts(fraction_grammar(), "3½") == "three and a half" &&
              tts(fraction_grammar(), "21/2") == "twenty-one halves",
          "only a precomposed fraction may sit flush against its whole number");

    // Singular against plural, by margin rather than by exclusion: the plural
    // branch fits "1/2" too, it is just a hair dearer.
    check(tts(fraction_grammar(), "1/4") == "one quarter" &&
              itn(fraction_grammar(), "one quarters") == "1/4",
          "the singular branch wins '1/4' by a margin, not by exclusion");

    // Denominators run 2-20, the round tens, 32, 64, 100 and 1000, every one
    // of which has a single-word ordinal or is listed with both spellings.
    // "1/25" would need "twenty-fifths", a compound plural the hyphenator
    // cannot rebuild, so the table stops rather than guess.
    check(tts(fraction_grammar(), "3/13") == "three thirteenths" &&
              tts(fraction_grammar(), "7/90") == "seven ninetieths" &&
              tts(fraction_grammar(), "1/25") == "<none>",
          "the denominator is a table, and says where it stops");
}

// ---- measures ---------------------------------------------------------------

static void measure_tests() {
    check(table(measure_grammar(),
                {
                    {"5 km", "five kilometers"},
                    {"1 km", "one kilometer"},
                    {"12 cm", "twelve centimeters"},
                    {"3.5 kg", "three point five kilograms"},
                    {"100 mph", "one hundred miles per hour"},
                    {"1 ft", "one foot"},
                    {"2 ft", "two feet"},
                    {"50%", "fifty percent"},
                    {"25°C", "twenty-five degrees celsius"},
                    {"500 MB", "five hundred megabytes"},
                    {"2 L", "two liters"},
                    {"1 min", "one minute"},
                },
                true),
          "measures round trip in both directions");

    // The unit table is stored in the case the unit is spelled with, but
    // CLASSIFY folds a span's case before a grammar sees it. Both spellings
    // are therefore read; only the canonical one is written.
    check(tts(measure_grammar(), "25°c") == "twenty-five degrees celsius" &&
              tts(measure_grammar(), "2.4 ghz") == "two point four gigahertz" &&
              itn(measure_grammar(), "two point four gigahertz") == "2.4 GHz",
          "folded case is read, canonical case is written");

    // Letter units take a space and symbol units do not; the other spacing is
    // still read.
    check(tts(measure_grammar(), "5km") == "five kilometers" &&
              tts(measure_grammar(), "50 %") == "fifty percent" &&
              itn(measure_grammar(), "five kilometers") == "5 km" &&
              itn(measure_grammar(), "fifty percent") == "50%",
          "spacing is canonicalised per unit");

    // A measurement may be negative. This was the one numeric grammar with no
    // sign branch, so "-56 kg" left its hyphen to PLAIN, which copied it
    // through welded to the first spoken word.
    check(table(measure_grammar(),
                {
                    {"-56 kg", "minus fifty-six kilograms"},
                    {"-2.75 km", "minus two point seven five kilometers"},
                },
                true),
          "a measurement may be signed");

    check(tts(measure_grammar(), "5 lbs") == "five pounds" &&
              itn(measure_grammar(), "five pounds") == "5 lb",
          "'lbs' is read, 'lb' is written");

    // A unit whose written form is an ordinary English word is deliberately
    // absent, because the classifier has no context to tell them apart.
    check(tts(measure_grammar(), "3 in") == "<none>" &&
              tts(measure_grammar(), "5 s") == "<none>",
          "homograph units are not in the table");

    check(table(measure_grammar(),
                {
                    {"69 ha", "sixty-nine hectares"},
                    {"129 kW", "one hundred twenty-nine kilowatts"},
                    {"250 MW", "two hundred fifty megawatts"},
                    {"173 hp", "one hundred seventy-three horsepower"},
                    {"294 km²", "two hundred ninety-four square kilometers"},
                    {"700 m²", "seven hundred square meters"},
                    {"1046 sq mi", "one thousand forty-six square miles"},
                },
                true),
          "area, power and the units a corpus actually uses");

    // The squared units have an ASCII spelling ending in a digit, which is
    // where the alternate-spelling weight has to stay small: at a third of a
    // unit, MEASURE("40.285 km") plus CARDINAL("2") was cheaper than
    // MEASURE("40.285 km2") and the span split in two.
    check(tts(measure_grammar(), "40.285 km2") ==
              "forty point two eight five square kilometers" &&
              itn(measure_grammar(), "forty point two eight five square kilometers") ==
                  "40.285 km²",
          "an ASCII squared unit is read, the typographic one is written");

    // Density, which is written flush against its amount.
    check(tts(measure_grammar(), "725.1/km²") ==
              "seven hundred twenty-five point one per square kilometers",
          "a density unit needs no space");

    check(tts(measure_grammar(), "50 percent") == "fifty percent" &&
              itn(measure_grammar(), "fifty percent") == "50%",
          "'percent' spelled out is read, '%' is written");
}

// ---- properties -------------------------------------------------------------

static void property_tests() {
    std::string witness;

    // Two written fraction digits, so the round trip is exact: the tail is
    // read digit by digit, which preserves a trailing zero that a cardinal
    // reading would swallow.
    prop::Property<long long> decimal_round_trip;
    decimal_round_trip.generate = [](prop::Rng& r) {
        return r.in_range(0, 9999) * 100 + r.in_range(0, 99);
    };
    decimal_round_trip.shrink = [](const long long& n) { return prop::shrink_integer(n); };
    decimal_round_trip.show = [](const long long& n) {
        return std::to_string(n / 100) + "." + two_digits(n % 100);
    };
    decimal_round_trip.holds = [](const long long& n) {
        std::string written = std::to_string(n / 100) + "." + two_digits(n % 100);
        std::string spoken = tts(decimal_grammar(), written);
        return spoken != "<none>" && itn(decimal_grammar(), spoken) == written;
    };
    check(prop::check_property(decimal_round_trip, 20260805, 400, witness),
          "property: itn(tts(x)) == x for random decimals" +
              (witness.empty() ? "" : " [" + witness + "]"));

    // Numerator in the high half of the key, denominator in the low half, so
    // the shrinker can simplify the numerator without inventing a denominator
    // that is not in the table.
    static const int kDenominators[] = {2, 3, 4, 5, 8, 10, 16, 32, 64, 100, 1000};
    prop::Property<long long> fraction_round_trip;
    fraction_round_trip.generate = [](prop::Rng& r) {
        long long numerator = r.in_range(1, 99);
        long long denominator = kDenominators[r.pick(sizeof(kDenominators) /
                                                     sizeof(kDenominators[0]))];
        return numerator * 10000 + denominator;
    };
    fraction_round_trip.shrink = [](const long long& n) {
        std::vector<long long> out;
        for (long long smaller : prop::shrink_integer(n / 10000))
            if (smaller >= 1) out.push_back(smaller * 10000 + n % 10000);
        return out;
    };
    fraction_round_trip.show = [](const long long& n) {
        return std::to_string(n / 10000) + "/" + std::to_string(n % 10000);
    };
    fraction_round_trip.holds = [](const long long& n) {
        std::string written =
            std::to_string(n / 10000) + "/" + std::to_string(n % 10000);
        std::string spoken = tts(fraction_grammar(), written);
        return spoken != "<none>" && itn(fraction_grammar(), spoken) == written;
    };
    witness.clear();
    check(prop::check_property(fraction_round_trip, 20260806, 400, witness),
          "property: itn(tts(n/d)) == n/d for random fractions" +
              (witness.empty() ? "" : " [" + witness + "]"));

    prop::Property<long long> measure_round_trip;
    measure_round_trip.generate = [](prop::Rng& r) {
        return r.in_range(1, 99999) * 100 +
               static_cast<long long>(r.pick(kUnitCount));
    };
    measure_round_trip.shrink = [](const long long& n) {
        std::vector<long long> out;
        for (long long smaller : prop::shrink_integer(n / 100))
            if (smaller >= 1) out.push_back(smaller * 100 + n % 100);
        return out;
    };
    measure_round_trip.show = measure_written;
    measure_round_trip.holds = [](const long long& n) {
        std::string written = measure_written(n);
        std::string spoken = tts(measure_grammar(), written);
        return spoken != "<none>" && itn(measure_grammar(), spoken) == written;
    };
    witness.clear();
    check(prop::check_property(measure_round_trip, 20260807, 400, witness),
          "property: itn(tts(n unit)) == n unit for random measures" +
              (witness.empty() ? "" : " [" + witness + "]"));
}

int main() {
    decimal_tests();
    fraction_tests();
    measure_tests();
    property_tests();
    std::cout << "\nTests run:" << run << "\nTests failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
