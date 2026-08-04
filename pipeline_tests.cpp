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

    // A four-digit number is a year inside a date and a cardinal outside one.
    check(normalize("1905", Direction::TTS) == "one thousand nine hundred five" &&
              normalize("1/1/1905", Direction::TTS) == "january first nineteen oh five",
          "year readings are licensed by the date grammar, not by the digits");

    // Sayso converts number words wherever it finds them; "second" as a verb
    // is a genuine homograph the classifier has no context to resolve. Pinned
    // here so the behaviour is a decision rather than a surprise.
    check(normalize("I second the motion", Direction::ITN) == "I 2nd the motion",
          "known limitation: homographs like 'second' always convert");
}

int main() {
    tokenizer_tests();
    classify_tests();
    serialisation_tests();
    normalisation_tests();
    ambiguity_tests();
    std::cout << "\nTests run:" << run << "\nTests failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
