#include "fractions.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

// A denominator's written value and the words it is said with. Several forms
// may share a value: the first is what Sayso writes, the rest are what it will
// read. Costs are absolute, not relative to the canonical form, because the
// canonical form is the one at zero.
struct DenominatorForm {
    int value;
    std::vector<std::string> singular;
    std::vector<std::string> plural;
    float cost;
};

std::vector<DenominatorForm> denominator_forms() {
    std::vector<DenominatorForm> forms = {
        {2, {"half"}, {"halves"}, 0.0f},
        {3, {"third"}, {"thirds"}, 0.0f},
        // "quarter" is the ordinary word; "fourth" is read but not written.
        {4, {"quarter"}, {"quarters"}, 0.0f},
        {4, {"fourth"}, {"fourths"}, kAltDenominatorPenalty},
    };

    // 5 through 20, then the round tens: every one of these has a single-word
    // ordinal, which is what keeps them out of the hyphenator's way.
    static const char* kSimple[][2] = {
        {"fifth", "fifths"},         {"sixth", "sixths"},
        {"seventh", "sevenths"},     {"eighth", "eighths"},
        {"ninth", "ninths"},         {"tenth", "tenths"},
        {"eleventh", "elevenths"},   {"twelfth", "twelfths"},
        {"thirteenth", "thirteenths"}, {"fourteenth", "fourteenths"},
        {"fifteenth", "fifteenths"}, {"sixteenth", "sixteenths"},
        {"seventeenth", "seventeenths"}, {"eighteenth", "eighteenths"},
        {"nineteenth", "nineteenths"}, {"twentieth", "twentieths"},
    };
    for (int v = 5; v <= 20; ++v) {
        const char** words = kSimple[v - 5];
        forms.push_back({v, {words[0]}, {words[1]}, 0.0f});
    }
    static const std::pair<int, const char*> kTens[] = {
        {30, "thirtieth"}, {40, "fortieth"},   {50, "fiftieth"},
        {60, "sixtieth"},  {70, "seventieth"}, {80, "eightieth"},
        {90, "ninetieth"},
    };
    for (const auto& [value, ordinal] : kTens)
        forms.push_back({value, {ordinal}, {std::string(ordinal) + "s"}, 0.0f});
    forms.push_back({100, {"hundredth"}, {"hundredths"}, 0.0f});
    forms.push_back({1000, {"thousandth"}, {"thousandths"}, 0.0f});

    // The two compound denominators worth having. Both spellings are listed
    // outright rather than left to the hyphenator: it joins a tens word to a
    // unit *ordinal* ("thirty" + "second"), but "seconds" is not in its
    // joinable set, so the plural would never come back together.
    forms.push_back({32, {"thirty-second"}, {"thirty-seconds"}, 0.0f});
    forms.push_back({32, {"thirty", "second"}, {"thirty", "seconds"},
                     kAltDenominatorPenalty});
    forms.push_back({64, {"sixty-fourth"}, {"sixty-fourths"}, 0.0f});
    forms.push_back({64, {"sixty", "fourth"}, {"sixty", "fourths"},
                     kAltDenominatorPenalty});
    return forms;
}

Wfst denominator(bool plural, float extra) {
    std::vector<Wfst> parts;
    for (const DenominatorForm& d : denominator_forms())
        parts.push_back(rule(std::to_string(d.value), plural ? d.plural : d.singular,
                             d.cost + extra));
    return union_of(parts);
}

// A numerator of exactly one. "a" is only offered inside a mixed number,
// where "and" precedes it and there is nothing else it could be.
//
// Standing alone it was a menace, because it runs backwards: "a" plus an
// ordinal is an ordinary English phrase, and the fraction span covering both
// is cheaper than ORDINAL plus the per-character cost of leaving the article
// uncovered. "a nineteenth century painter" became "1/19 century painter" —
// the article not misread but absorbed into the numerator and gone from the
// output. "a third and final season" went the same way.
Wfst numerator_one(bool allow_article, float one_cost) {
    std::vector<Wfst> parts{rule("1", {"one"}, one_cost)};
    if (allow_article) parts.push_back(rule("1", {"a"}));
    return union_of(parts);
}

// Numerator, slash, agreeing denominator. The singular branch only fits a
// numerator of exactly one; the margin on the plural branch is what makes the
// singular win wherever both do.
Wfst slashed_body(bool allow_article, float one_cost) {
    return union_of({
        concat_of({numerator_one(allow_article, one_cost), chars_in("/"),
                   denominator(false, 0.0f)}),
        concat_of({cardinal_positive_fst(), chars_in("/"),
                   denominator(true, kPluralPreferenceMargin)}),
    });
}

// Unicode's precomposed fractions. Each is a single character, so there is no
// numerator to read separately and the whole thing is one rule — which is also
// what makes the space before it optional in a mixed number without "21/2"
// becoming ambiguous.
struct VulgarFraction {
    const char* written;
    int numerator;              // 1 takes the article, the rest are plural
    const char* denominator;    // already in the agreeing number
};
const VulgarFraction kVulgarFractions[] = {
    {"½", 1, "half"},      {"⅓", 1, "third"},    {"¼", 1, "quarter"},
    {"⅕", 1, "fifth"},     {"⅙", 1, "sixth"},    {"⅐", 1, "seventh"},
    {"⅛", 1, "eighth"},    {"⅑", 1, "ninth"},    {"⅒", 1, "tenth"},
    {"⅔", 2, "thirds"},    {"¾", 3, "quarters"},
    {"⅖", 2, "fifths"},    {"⅗", 3, "fifths"},   {"⅘", 4, "fifths"},
    {"⅚", 5, "sixths"},
    {"⅜", 3, "eighths"},   {"⅝", 5, "eighths"},  {"⅞", 7, "eighths"},
};

Wfst vulgar_body(bool allow_article, float one_cost) {
    static const char* kNumerators[8] = {"", "one", "two",  "three",
                                         "four", "five", "six", "seven"};
    std::vector<Wfst> parts;
    for (const VulgarFraction& v : kVulgarFractions) {
        if (v.numerator == 1) {
            if (allow_article) parts.push_back(rule(v.written, {"a", v.denominator}));
            parts.push_back(rule(v.written, {"one", v.denominator}, one_cost));
        } else {
            parts.push_back(
                rule(v.written, {kNumerators[v.numerator], v.denominator}));
        }
    }
    return reweight(union_of(parts), kVulgarFractionPenalty);
}

Wfst build_fraction() {
    // Standing alone the numerator is "one" and only "one"; after a whole
    // number, where "and" precedes it, "a" is free and "one" costs a hair.
    Wfst simple = union_of({
        slashed_body(false, 0.0f),
        vulgar_body(false, 0.0f),
    });
    // "2 1/2" needs its space — without one, "21/2" would read as a mixed
    // number rather than as twenty-one halves. A precomposed character cannot
    // be misread that way, so there the space is optional.
    Wfst mixed = union_of({
        concat_of({cardinal_positive_fst(), chars_in(" "), words_out({"and"}),
                   slashed_body(true, kArticleNumeratorPenalty)}),
        concat_of({cardinal_positive_fst(),
                   union_of({chars_in(" "), empty_string_fst(kMixedNoSpacePenalty)}),
                   words_out({"and"}),
                   vulgar_body(true, kArticleNumeratorPenalty)}),
    });

    return connect(hyphenated(
        concat_of({optional_sign_fst(), union_of({simple, mixed})})));
}

}  // namespace

const Cascade& fraction_grammar() {
    static const Wfst core = build_fraction();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
