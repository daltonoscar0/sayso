// Tokeniser bridge, CLASSIFY and end-to-end normalisation.
//
// Build:
//   g++ -std=c++17 -O2 -o pipeline_tests pipeline_tests.cpp normalizer.cpp \
//       classify.cpp text.cpp numbers.cpp money.cpp dates.cpp wfst.cpp \
//       symbols.cpp ../Scalpel/tokenizer.cpp ../Scalpel/char_classes.cpp -I.
#include <iostream>
#include <string>
#include <vector>

#include "normalizer.h"
#include "text.h"

using namespace sayso;

static int run = 0, failed = 0;

static void check(bool ok, const std::string& name) {
    ++run;
    if (!ok) { ++failed; std::cout << "[FAIL]" << name << "\n"; }
    else     {           std::cout << "[PASS]" << name << "\n"; }
}

static std::string tagging(const std::string& text, Direction dir) {
    std::string out;
    for (const Segment& s : analyse(text, dir)) {
        if (!out.empty()) out += " ";
        out += std::string(class_name(s.type)) + "(" + s.source + ")";
    }
    return out;
}

struct Case { const char* written; const char* spoken; };

static bool table(const std::vector<Case>& cases, bool both_ways) {
    bool ok = true;
    for (const Case& c : cases) {
        if (normalize(c.written, Direction::TTS) != c.spoken) {
            ok = false;
            std::cout << "  tts [" << c.written << "] -> ["
                      << normalize(c.written, Direction::TTS) << "] (want [" << c.spoken
                      << "])\n";
        }
        if (both_ways && normalize(c.spoken, Direction::ITN) != c.written) {
            ok = false;
            std::cout << "  itn [" << c.spoken << "] -> ["
                      << normalize(c.spoken, Direction::ITN) << "] (want [" << c.written
                      << "])\n";
        }
    }
    return ok;
}

// ---- the Scalpel bridge -----------------------------------------------------

static void tokenizer_tests() {
    auto texts_of = [](const std::string& s) {
        std::string out;
        for (const Token& t : tokenize(s)) {
            if (!out.empty()) out += "|";
            out += t.text;
        }
        return out;
    };

    check(texts_of("The quick brown fox") == "The|quick|brown|fox",
          "ordinary words pass through Scalpel unchanged");

    // Scalpel's START state ignores HYPHEN and OTHER, so these characters are
    // exactly the ones the adapter has to put back.
    check(texts_of("2015-03-14") == "2015|-|03|-|14",
          "a leading hyphen is recovered rather than dropped");
    check(texts_of("€40 and £3") == "€|40|and|£|3",
          "currency symbols outside Latin-1 letters are recovered");
    check(texts_of("-42") == "-|42", "a sign at the start of input survives");

    // Offsets have to bracket their own text for the serialiser to work.
    bool offsets_ok = true;
    std::string sample = "He paid €40 on 2015-03-14, twice.";
    for (const Token& t : tokenize(sample))
        if (sample.substr(t.start_index, t.end_index - t.start_index) != t.text)
            offsets_ok = false;
    check(offsets_ok, "every token's byte range holds exactly its own text");
}

// ---- CLASSIFY ---------------------------------------------------------------

static void classify_tests() {
    check(tagging("It costs $25.10 on 3/14/2015.", Direction::TTS) ==
              "PLAIN(It) PLAIN(costs) MONEY($25.10) PLAIN(on) DATE(3/14/2015) PLAIN(.)",
          "money and date spans beat their parts");

    check(tagging("the 21st of 1,234", Direction::TTS) ==
              "PLAIN(the) ORDINAL(21st) PLAIN(of) CARDINAL(1,234)",
          "ordinal and cardinal spans, comma included");

    check(tagging("twenty-five dollars and ten cents", Direction::ITN) ==
              "MONEY(twenty-five dollars and ten cents)",
          "ITN classifies over the same lattice, reading the grammars backwards");

    // Longest match falls out of pricing PLAIN per character: the five-token
    // date span is cheaper than any decomposition of it.
    check(tagging("on 2015-03-14", Direction::TTS) == "PLAIN(on) DATE(2015-03-14)",
          "a long span beats a cheap decomposition without a longest-match rule");

    check(tagging("abc123def", Direction::TTS) == "PLAIN(abc) CARDINAL(123) PLAIN(def)",
          "spans follow Scalpel's token boundaries");
}

// ---- serialisation ----------------------------------------------------------

static void serialisation_tests() {
    check(normalize("  spaced   out  25  ", Direction::TTS) ==
              "  spaced   out  twenty-five  ",
          "text outside a span is copied through byte for byte");
    check(normalize("\"quoted 25\", he said.", Direction::TTS) ==
              "\"quoted twenty-five\", he said.",
          "quoting and punctuation survive normalisation");
    check(normalize("", Direction::TTS) == "" &&
          normalize("nothing to do here", Direction::TTS) == "nothing to do here",
          "empty and class-free input are returned unchanged");
}

// ---- end to end -------------------------------------------------------------

static void normalisation_tests() {
    check(table(
              {
                  {"25", "twenty-five"},
                  {"the 3rd of 25", "the third of twenty-five"},
                  {"$25.10", "twenty-five dollars and ten cents"},
                  {"3/14/2015", "march fourteenth twenty fifteen"},
                  {"$2.5 million", "two point five million dollars"},
                  {"He was 21 on 12/25/2020.", "He was twenty-one on december twenty-fifth twenty twenty."},
              },
              true),
          "sentences round trip through the whole pipeline");

    check(normalize("Born 1999-12-25, she paid £3 for 1,234 stamps.", Direction::TTS) ==
              "Born december twenty-fifth nineteen ninety-nine, she paid three pounds "
              "for one thousand two hundred thirty-four stamps.",
          "all four classes in one sentence");

    check(normalize("born december twenty-fifth nineteen ninety-nine, she paid three "
                    "pounds for one thousand two hundred thirty-four stamps.",
                    Direction::ITN) ==
              "born 12/25/1999, she paid £3 for 1234 stamps.",
          "and back again, in canonical written form");
}

// ---- ambiguity, resolved by weight ------------------------------------------

static void ambiguity_tests() {
    // "may second" is a date only with a year: the grammar has no two-field
    // path, so DATE offers no candidate and ORDINAL takes the second word.
    check(tagging("may second", Direction::ITN) == "PLAIN(may) ORDINAL(second)" &&
              tagging("may second twenty twenty", Direction::ITN) ==
                  "DATE(may second twenty twenty)",
          "'may second' is a date only once a year is present");

    // $25 as MONEY (entry 0.05) against PLAIN '$' (1.0) plus CARDINAL 25
    // (0.2): the composite span is cheaper, so the currency wins the '$'.
    check(tagging("$25", Direction::TTS) == "MONEY($25)",
          "MONEY claims the symbol rather than leaving it to PLAIN");

    // A bare four-digit number in 1100-2099 is read as a year, in or out of a
    // date. The Google TN corpus is emphatic about this: when the reading was
    // licensed only inside the date grammar, it was the single largest source
    // of disagreement, and "in 1987" came out as "in one thousand nine hundred
    // eighty seven".
    check(normalize("1905", Direction::TTS) == "nineteen oh five" &&
              normalize("1/1/1905", Direction::TTS) == "january first nineteen oh five" &&
              normalize("Since 1987 the island", Direction::TTS) ==
                  "Since nineteen eighty-seven the island",
          "a bare four-digit number is read as a year");

    // But only a bare one. A number inside a larger semiotic object is a
    // quantity, and those classes embed the cardinal machine directly, so the
    // year branch is not reachable from them.
    check(normalize("$1984", Direction::TTS) ==
              "one thousand nine hundred eighty-four dollars" &&
              normalize("1984 m", Direction::TTS) ==
                  "one thousand nine hundred eighty-four meters" &&
              normalize("1984.5", Direction::TTS) ==
                  "one thousand nine hundred eighty-four point five",
          "a number inside money, a measure or a decimal is never a year");

    // The cost of the above, pinned like the other homographs: a four-digit
    // quantity standing alone is read as a year, because nothing in the span
    // says otherwise.
    check(normalize("1984 people attended", Direction::TTS) ==
              "nineteen eighty-four people attended",
          "known limitation: a bare four-digit quantity is read as a year");

    // Speech recognisers do not hyphenate, so ITN has to hold a whole
    // unhyphenated numeral in one span. This is what the spoken span cap is
    // sized for, and it regressed once when the cap was too small.
    check(normalize("one billion two hundred thirty four million five hundred sixty seven "
                    "thousand eight hundred ninety",
                    Direction::ITN) == "1234567890" &&
              normalize("eight hundred forty six million one hundred sixty eight thousand "
                        "nine hundred forty nine dollars and ninety nine cents",
                        Direction::ITN) == "$846168949.99",
          "unhyphenated numerals stay one span all the way to a billion");

    // Sayso converts number words wherever it finds them; "second" as a verb
    // is a genuine homograph the classifier has no context to resolve. Pinned
    // here so the behaviour is a decision rather than a surprise.
    check(normalize("I second the motion", Direction::ITN) == "I 2nd the motion",
          "known limitation: homographs like 'second' always convert");
}

// ---- the eight classes against each other -----------------------------------
//
// Every class added after the first four overlaps something already there.
// These are the overlaps, and which way each was made to fall.

static void class_competition_tests() {
    // A slash between two numbers is a fraction; three fields make it a date.
    // DATE is cheaper than FRACTION plus the leftovers, so the longer span
    // wins without a rule saying dates come first.
    check(tagging("3/4", Direction::TTS) == "FRACTION(3/4)" &&
              tagging("3/4/2015", Direction::TTS) == "DATE(3/4/2015)",
          "a two-field slash is a fraction, a three-field one is a date");

    // A point between digits is a decimal; a currency symbol in front of it
    // makes the whole thing money, symbol included.
    check(tagging("25.10", Direction::TTS) == "DECIMAL(25.10)" &&
              tagging("$25.10", Direction::TTS) == "MONEY($25.10)",
          "MONEY claims a decimal amount rather than leaving it to DECIMAL");

    // MEASURE has to beat CARDINAL plus a PLAIN unit, which is what its entry
    // cost being below kPlainPerChar buys.
    check(tagging("5 km", Direction::TTS) == "MEASURE(5 km)" &&
              tagging("3.5 kg", Direction::TTS) == "MEASURE(3.5 kg)",
          "MEASURE claims its unit rather than leaving it to PLAIN");

    // "three pounds" is sterling as often as it is weight and the classifier
    // has no context to tell them apart, so the measure reading is priced to
    // lose the tie — while "5 lb", which no currency writes, still wins it.
    check(tagging("three pounds", Direction::ITN) == "MONEY(three pounds)" &&
              tagging("5 lb", Direction::TTS) == "MEASURE(5 lb)",
          "a spoken 'pounds' is money, a written 'lb' is weight");

    // A colon between an hour and a two-digit minute is a time; the minute
    // field is what stops "2:1" from being one.
    check(tagging("3:30", Direction::TTS) == "TIME(3:30)" &&
              tagging("3:30 pm", Direction::TTS) == "TIME(3:30 pm)" &&
              tagging("2:1", Direction::TTS) ==
                  "CARDINAL(2) PLAIN(:) CARDINAL(1)",
          "TIME needs a two-digit minute, so a ratio is not one");

    check(normalize("The 2.5 km run starts at 6:45 am on 3/14/2015.",
                    Direction::TTS) ==
              "The two point five kilometers run starts at six forty-five a m on "
              "march fourteenth twenty fifteen.",
          "four of the new classes in one sentence");

    check(normalize("we used 2 1/2 cups and 0.5 L at 3:05 pm", Direction::TTS) ==
              "we used two and a half cups and zero point five liters at three oh "
              "five p m",
          "fractions, decimals and times share a sentence");

    check(normalize("we used two and a half cups and zero point five liters at "
                    "three oh five p m",
                    Direction::ITN) == "we used 2 1/2 cups and 0.5 L at 3:05 pm",
          "and those three back again, in canonical written form");

    // The counterpart of "I second the motion": a bare "3:16" has no context
    // to say it is a verse rather than a time. Pinned as a decision.
    check(normalize("John 3:16", Direction::TTS) == "John three sixteen",
          "known limitation: a colon between numbers is always a time");
}

// ---- the classes that only speak ---------------------------------------------
//
// Three classes are registered `speaking_only`, and this is what that buys.
// Each of them would, run backwards, rewrite ordinary English: every "saint"
// would contract to "st", every pair of spoken letters would glue into an
// acronym, and every "five to ten" would become "5-10". No weight can prevent
// it, because CLASSIFY prices PLAIN per character and a five-letter word costs
// 5.0 against a class entry cost of 0.05.

static void one_directional_tests() {
    check(normalize("St Paul", Direction::TTS) == "saint Paul" &&
              normalize("vol 3", Direction::TTS) == "volume three" &&
              normalize("Smith vs Jones", Direction::TTS) == "Smith versus Jones",
          "abbreviations expand when speaking");

    check(normalize("saint paul", Direction::ITN) == "saint paul" &&
              normalize("volume three", Direction::ITN) == "volume 3",
          "and do not contract when writing");

    // A lone initial is left alone: LETTERS needs two letters, because a
    // one-letter rule would read the [s] [.] of "the 1980s." as an initial.
    check(normalize("U.S. policy", Direction::TTS) == "u s policy" &&
              normalize("e.g. this", Direction::TTS) == "e g this" &&
              normalize("J. Smith", Direction::TTS) == "J. Smith",
          "dotted acronyms are spelled out when speaking, lone initials are not");

    // The trailing full stop Scalpel attaches to a short token, split back off
    // by the adapter. Without it a sentence-final decade or measure is
    // unclaimable, and whatever did claim it would eat the full stop.
    check(normalize("the 1980s.", Direction::TTS) == "the nineteen eighties." &&
              normalize("5 km.", Direction::TTS) == "five kilometers." &&
              normalize("at 3:30 p.m.", Direction::TTS) == "at three thirty p m",
          "a sentence-final full stop survives, and its span still classifies");

    check(normalize("u s policy", Direction::ITN) == "u s policy" &&
              normalize("a and I went", Direction::ITN) == "a and I went",
          "and single letters are not glued back into acronyms");

    check(normalize("pp 1-10", Direction::TTS) == "pp one to ten" &&
              normalize("five to ten people", Direction::ITN) == "5 to 10 people",
          "a range speaks as 'to', and 'to' is not written back as a hyphen");
}

// ---- the newer classes against the older ones ---------------------------------

static void later_class_tests() {
    // A range's shape is the prefix of an ISO date's. The guard is structural
    // — ranges.cpp refuses a four-digit operand followed by a month-shaped one
    // — because relying on the entry cost worked only while zero-padding
    // happened to make the alternative expensive.
    check(tagging("2011-11-11", Direction::TTS) == "DATE(2011-11-11)" &&
              tagging("2015-03-14", Direction::TTS) == "DATE(2015-03-14)" &&
              tagging("1729-1811", Direction::TTS) == "RANGE(1729-1811)",
          "a date outranks a range over the same characters");

    // And a colon is a time, never a ratio: the colon branch was cut from
    // RANGE because it was 96% bibliographic citation in the corpus.
    check(tagging("9:30", Direction::TTS) == "TIME(9:30)",
          "a colon between numbers stays a time");

    // TELEPHONE exists mostly to stop RANGE claiming the first two fields of a
    // hyphen chain and leaving the rest to negative cardinals.
    check(tagging("978-0-131-40221-8", Direction::TTS) ==
              "TELEPHONE(978-0-131-40221-8)" &&
              tagging("2015-03-14", Direction::TTS) == "DATE(2015-03-14)" &&
              tagging("1729-1811", Direction::TTS) == "RANGE(1729-1811)",
          "a hyphen chain is a serial, three fields are a date, two are a range");

    check(tagging("1970s", Direction::TTS) == "DECADE(1970s)" &&
              tagging("1970", Direction::TTS) == "CARDINAL(1970)",
          "a decade is its own class, a bare year is a cardinal");

    // Roman numerals live inside CARDINAL, which is what makes them safe: the
    // digits are always the cheaper written form.
    check(tagging("World War II", Direction::TTS) ==
              "PLAIN(World) PLAIN(War) CARDINAL(II)" &&
              normalize("world war two", Direction::ITN) == "world war 2",
          "a Roman numeral is a cardinal, and never what ITN writes");

    check(normalize("Retrieved Jun 29, 2011 in the 1980s.", Direction::TTS) ==
              "Retrieved june twenty-ninth twenty eleven in the nineteen eighties.",
          "an abbreviated month and a decade in one sentence");

    check(normalize("St Paul's, vol 3, pp 1-10, e.g. 1729-1811.",
                    Direction::TTS) ==
              "saint Paul's, volume three, pp one to ten, e g seventeen "
              "twenty-nine to eighteen eleven.",
          "five of the later classes in one sentence");
}

// ---- what an adversarial review found ---------------------------------------
//
// Every check here is a defect a reviewer reproduced against a built binary,
// counted in the corpus, and which no existing test caught.

static void review_regression_tests() {
    // A year in 2000-2009 costs the *cardinal* reading nothing, because there
    // that reading is the ordinary one. Charging it kCardinalYearPenalty made
    // every date carrying such a year 0.6 dearer, and 9.8% of the shard's ISO
    // dates lost to three cardinals and two minus signs.
    check(normalize("2007-10-19", Direction::TTS) == "october nineteenth two thousand seven" &&
              normalize("2015-10-19", Direction::TTS) == "october nineteenth twenty fifteen" &&
              normalize("2000-01-01", Direction::TTS) == "january first two thousand",
          "an ISO date with a 2000s year is still a date");

    // Read backwards a clock and a year are the same words. TIME entered at
    // 0.05 and won every one: 588 of the 1000 years in 1100-2099 failed to
    // round-trip, "nineteen forty five" writing as 19:45.
    check(normalize("nineteen forty five", Direction::ITN) == "1945" &&
              normalize("twenty eleven", Direction::ITN) == "2011" &&
              normalize("three thirty p m", Direction::ITN) == "3:30 pm" &&
              normalize("ten a m", Direction::ITN) == "10:00 am",
          "a spoken year is a year, and a clock still needs its meridiem");

    // The adapter splits a sentence-final stop off, so an abbreviation branch
    // that also read the stop claimed both tokens and deleted the punctuation.
    check(normalize("She works for Acme Ltd. The deal closed.", Direction::TTS) ==
              "She works for Acme limited. The deal closed." &&
              normalize("He bought apples, oranges, etc.", Direction::TTS) ==
                  "He bought apples, oranges, etcetera.",
          "an abbreviation at a sentence end keeps the full stop");

    // "a" plus an ordinal is ordinary English, and a fraction span covering
    // both was cheaper than ORDINAL plus the cost of leaving the article
    // alone — so the article was absorbed into the numerator and vanished.
    check(normalize("a nineteenth century painter", Direction::ITN) ==
              "a 19th century painter" &&
              normalize("a third and final season", Direction::ITN) ==
                  "a 3rd and final season",
          "an indefinite article before an ordinal is not a numerator");

    // PLAIN is priced per character, so "million" at 7.0 lost to CARDINAL
    // reading "five million" as 5000000 for 0.21: the round trip was wrong by
    // six orders of magnitude.
    check(normalize("three point five million", Direction::ITN) == "3.5 million" &&
              normalize("one point eight million", Direction::ITN) == "1.8 million",
          "a decimal keeps its magnitude word");

    // A currency amount keeps its cents when a magnitude follows. MEASURE used
    // to claim ".4 M" as metres, because its amount admitted an elided leading
    // zero — which no measurement is ever written with — and the two outputs
    // met as the non-word "dollarspoint".
    check(normalize("Sells $5.4 M. Central Park", Direction::TTS) ==
              "Sells five point four million dollars. Central Park" &&
              normalize("5.4 M of cable", Direction::TTS) ==
                  "five point four meters of cable",
          "a magnitude after an amount is money, without one it is metres");

    // A thousands comma and a one-digit cents field are each affordable alone;
    // only their sum lost, and the amount came apart with a literal comma
    // reaching the spoken tape.
    check(normalize("$1,234.5", Direction::TTS) ==
              "one thousand two hundred thirty-four dollars and fifty cents",
          "a comma and short cents together still make one amount");

    // Folded, "Xi" is a surname and "XI" a cricket team as often as either is
    // eleven.
    check(normalize("Xi Jinping met Zhu Xi scholars.", Direction::TTS) ==
              "Xi Jinping met Zhu Xi scholars." &&
              normalize("World War II", Direction::TTS) == "World War two",
          "the numerals that are also names are left alone");
}

int main() {
    tokenizer_tests();
    classify_tests();
    serialisation_tests();
    normalisation_tests();
    ambiguity_tests();
    class_competition_tests();
    one_directional_tests();
    later_class_tests();
    review_regression_tests();
    std::cout << "\nTests run:" << run << "\nTests failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
