#include "numbers.h"

#include <set>
#include <string>

#include "weights.h"

namespace sayso {

namespace {

const char* kUnits[10] = {"zero", "one", "two",   "three", "four",
                          "five", "six", "seven", "eight", "nine"};
const char* kTeens[10] = {"ten",      "eleven",  "twelve",    "thirteen", "fourteen",
                          "fifteen",  "sixteen", "seventeen", "eighteen", "nineteen"};
const char* kTens[10] = {"", "", "twenty", "thirty", "forty",
                         "fifty", "sixty", "seventy", "eighty", "ninety"};
const char* kScales[3] = {"thousand", "million", "billion"};

// Cardinal word -> ordinal word. The ordinal grammar is the cardinal grammar
// composed with this table applied to the final word, which is exactly the
// linguistic generalisation: only the last element of a numeral inflects.
struct OrdinalPair { const char* cardinal; const char* ordinal; };
const OrdinalPair kOrdinals[] = {
    {"zero", "zeroth"},        {"one", "first"},         {"two", "second"},
    {"three", "third"},        {"four", "fourth"},       {"five", "fifth"},
    {"six", "sixth"},          {"seven", "seventh"},     {"eight", "eighth"},
    {"nine", "ninth"},         {"ten", "tenth"},         {"eleven", "eleventh"},
    {"twelve", "twelfth"},     {"thirteen", "thirteenth"},
    {"fourteen", "fourteenth"},{"fifteen", "fifteenth"}, {"sixteen", "sixteenth"},
    {"seventeen", "seventeenth"}, {"eighteen", "eighteenth"},
    {"nineteen", "nineteenth"},{"twenty", "twentieth"},  {"thirty", "thirtieth"},
    {"forty", "fortieth"},     {"fifty", "fiftieth"},    {"sixty", "sixtieth"},
    {"seventy", "seventieth"}, {"eighty", "eightieth"},  {"ninety", "ninetieth"},
    {"hundred", "hundredth"},  {"thousand", "thousandth"},
    {"million", "millionth"},  {"billion", "billionth"},
};

std::string digit(int d) { return std::string(1, static_cast<char>('0' + d)); }
std::string two_digits(int n) { return digit(n / 10) + digit(n % 10); }

// Left unhyphenated; the hyphenator stage joins "eighty" to "four".
std::vector<std::string> cardinal_words(int n) {
    if (n < 10) return {kUnits[n]};
    if (n < 20) return {kTeens[n - 10]};
    if (n % 10 == 0) return {kTens[n / 10]};
    return {kTens[n / 10], kUnits[n % 10]};
}

// An optional thousands comma. Reading "1,234" costs a little; writing one
// costs the same, which is why ITN settles on the unseparated "1234".
Wfst optional_comma() {
    return union_of({empty_string_fst(), chars_in(",", kCommaCost)});
}

NumberParts build_parts() {
    NumberParts p;
    p.zero = rule("0", {kUnits[0]});

    std::vector<Wfst> units;
    for (int d = 1; d <= 9; ++d) units.push_back(rule(digit(d), {kUnits[d]}));
    p.unit_nz = union_of(units);

    std::vector<Wfst> two;
    for (int n = 0; n <= 9; ++n) two.push_back(rule("1" + digit(n), {kTeens[n]}));
    for (int t = 2; t <= 9; ++t) two.push_back(rule(digit(t) + "0", {kTens[t]}));
    for (int t = 2; t <= 9; ++t)
        two.push_back(concat_of({rule(digit(t), {kTens[t]}), p.unit_nz}));
    p.two_digit_nz = union_of(two);

    // H then a two-digit remainder, which may be empty ("00"), a bare unit
    // ("0X") or a full pair.
    Wfst remainder = union_of({chars_in("00"),
                               concat_of({chars_in("0"), p.unit_nz}),
                               p.two_digit_nz});
    p.three_nz = concat_of({p.unit_nz, words_out({"hundred"}), remainder});

    p.lead3 = union_of({p.unit_nz, p.two_digit_nz, p.three_nz});
    p.padded3_nz = union_of({concat_of({chars_in("00"), p.unit_nz}),
                             concat_of({chars_in("0"), p.two_digit_nz}),
                             p.three_nz});
    p.padded3 = union_of({chars_in("000"), p.padded3_nz});
    return p;
}

// A thousands/millions/billions group: three digits plus its scale word, or
// nothing at all when the group is "000".
Wfst scale_group(const NumberParts& p, const char* scale) {
    return union_of({chars_in("000"),
                     concat_of({p.padded3_nz, words_out({scale})})});
}

Wfst build_cardinal_core(bool include_zero) {
    const NumberParts& p = number_parts();
    std::vector<Wfst> alternatives{p.lead3};
    if (include_zero) alternatives.push_back(p.zero);

    // 1,000 .. 999,999,999,999: a leading group, its scale word, then one
    // group per descending scale.
    for (int top = 0; top < 3; ++top) {
        std::vector<Wfst> parts{p.lead3, words_out({kScales[top]})};
        for (int lower = top - 1; lower >= 0; --lower) {
            parts.push_back(optional_comma());
            parts.push_back(scale_group(p, kScales[lower]));
        }
        parts.push_back(optional_comma());
        parts.push_back(p.padded3);
        alternatives.push_back(concat_of(parts));
    }

    // Digit-by-digit fallback for anything the grammar above cannot parse —
    // strings longer than twelve digits, and leading-zero strings like "007".
    // Priced per digit so it never outbids a real reading of a short number.
    std::vector<Wfst> any_digit, first_digit;
    for (int d = 0; d <= 9; ++d) {
        any_digit.push_back(rule(digit(d), {kUnits[d]}, kDigitByDigitPerDigit));
        if (d > 0 || include_zero) first_digit.push_back(any_digit.back());
    }
    alternatives.push_back(
        concat_of({union_of(first_digit), closure(union_of(any_digit))}));

    return connect(union_of(alternatives));
}

// Reads the whole word sequence and rewrites only the last word. The
// requirement that the machine end in the "rewritten" state is what pins
// "last": no other alignment consumes the input and reaches a final state.
Wfst build_ordinaliser() {
    Wfst f;
    int copy = f.add_state();
    int done = f.add_state(0.0f);
    f.set_start(copy);
    for (const std::string& w : cardinal_vocabulary()) {
        Arc keep;
        keep.ilabel = keep.olabel = word(w);
        keep.next = copy;
        f.add_arc(copy, keep);
    }
    for (const OrdinalPair& o : kOrdinals) {
        Arc infl;
        infl.ilabel = word(o.cardinal);
        infl.olabel = word(o.ordinal);
        infl.next = done;
        f.add_arc(copy, infl);
    }
    return f;
}

// Copies digits while remembering the last one (and whether the one before
// it was a '1', which is what makes 11th/12th/13th behave), then consumes
// the ordinal suffix that agrees with it. Inverted, the same machine *emits*
// the agreeing suffix, so ITN never produces "21th".
Wfst build_suffix_stripper() {
    Wfst f;
    int start = f.add_state();
    f.set_start(start);
    int last[10], teen[10];
    for (int d = 0; d <= 9; ++d) {
        last[d] = f.add_state();
        teen[d] = f.add_state();
    }
    auto successor = [&](int previous_digit, int d) {
        return previous_digit == 1 ? teen[d] : last[d];
    };
    for (int d = 0; d <= 9; ++d) {
        Arc a;
        a.ilabel = a.olabel = ch(digit(d));
        a.next = last[d];
        f.add_arc(start, a);
    }
    for (int prev = 0; prev <= 9; ++prev)
        for (int d = 0; d <= 9; ++d) {
            Arc a;
            a.ilabel = a.olabel = ch(digit(d));
            a.next = successor(prev, d);
            f.add_arc(last[prev], a);
            f.add_arc(teen[prev], a);
        }
    // A thousands comma leaves the digit memory alone.
    for (int d = 0; d <= 9; ++d) {
        Arc comma;
        comma.ilabel = comma.olabel = ch(",");
        comma.next = last[d];
        f.add_arc(last[d], comma);
        comma.next = teen[d];
        f.add_arc(teen[d], comma);
    }

    int accept = f.add_state(0.0f);
    auto suffix_from = [&](int state, const std::string& suffix) {
        int cur = state;
        for (size_t i = 0; i < suffix.size(); ++i) {
            int next = (i + 1 == suffix.size()) ? accept : f.add_state();
            Arc a;
            a.ilabel = ch(std::string(1, suffix[i]));
            a.next = next;
            f.add_arc(cur, a);
            cur = next;
        }
    };
    for (int d = 0; d <= 9; ++d) {
        const char* suffix = d == 1 ? "st" : d == 2 ? "nd" : d == 3 ? "rd" : "th";
        suffix_from(last[d], suffix);
        suffix_from(teen[d], "th");  // 11th, 12th, 13th ... 19th
    }
    return f;
}

// The compound-joining machine over an explicit vocabulary. States: `home`,
// where any word passes through, and one `held` state per tens word, entered
// without emitting anything so the following word can decide whether to join.
Wfst build_hyphenator(const std::vector<std::string>& vocabulary) {
    Wfst f;
    int home = f.add_state(0.0f);
    f.set_start(home);

    std::vector<std::string> joinable;
    for (const std::string& w : vocabulary) {
        for (int d = 1; d <= 9; ++d)
            if (w == kUnits[d]) joinable.push_back(w);
        for (const OrdinalPair& o : kOrdinals)
            if (w == o.ordinal)
                for (int d = 1; d <= 9; ++d)
                    if (std::string(o.cardinal) == kUnits[d]) joinable.push_back(w);
    }

    std::vector<std::string> tens;
    for (const std::string& w : vocabulary)
        for (int t = 2; t <= 9; ++t)
            if (w == kTens[t]) tens.push_back(w);

    for (const std::string& w : vocabulary) {
        bool is_tens = false;
        for (const std::string& t : tens) if (w == t) is_tens = true;
        if (is_tens) continue;
        Arc keep;
        keep.ilabel = keep.olabel = word(w);
        keep.next = home;
        f.add_arc(home, keep);
    }

    for (const std::string& t : tens) {
        int held = f.add_state();
        Arc hold;
        hold.ilabel = word(t);
        hold.next = held;
        f.add_arc(home, hold);
        for (const std::string& u : joinable) {
            Arc join;
            join.ilabel = word(u);
            join.olabel = word(t + "-" + u);
            join.next = home;
            f.add_arc(held, join);
        }
        Arc release;                       // nothing joinable followed
        release.olabel = word(t);
        release.weight = kSpacedCompoundPenalty;
        release.next = home;
        f.add_arc(held, release);
    }
    return f;
}

// Roman numerals, as a table and never as a parser.
//
// A parser over M{0,4}(CM|CD|D?C{0,3})... accepts "MIX", "DID", "CIVIC" and
// every other word made of those seven letters, and spans arrive case-folded,
// so there is no capital left to tell a numeral from a word. A table chooses
// exactly which strings are numerals, and this one omits every single letter:
// I, V, X, L, C, D and M are 16,855 occurrences in the shard of which 5.7% are
// numerals, and "I" alone is almost all of the rest.
//
// The reading is cardinal because that is what the reference does three times
// in four. "Henry VIII" is "the eighth" and "World War II" is "two", and the
// only thing that separates them is the preceding word, which CLASSIFY does
// not see.
const char* kRomanNumerals[] = {
    "ii", "iii", "iv", "vi", "vii", "viii", "ix", "xi", "xii", "xiii",
    "xiv", "xv", "xvi", "xvii", "xviii", "xix", "xx", "xxi", "xxii", "xxiii",
    "xxiv", "xxv", "xxvi", "xxvii", "xxviii", "xxix", "xxx",
};
const int kRomanValues[] = {2,  3,  4,  6,  7,  8,  9,  11, 12, 13, 14, 15, 16, 17,
                            18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};

Wfst roman_numerals() {
    std::vector<Wfst> parts;
    size_t count = sizeof(kRomanNumerals) / sizeof(kRomanNumerals[0]);
    for (size_t i = 0; i < count; ++i)
        parts.push_back(rule(kRomanNumerals[i], cardinal_words(kRomanValues[i])));
    return union_of(parts);
}

// Every distinct word `f` can write.
std::vector<std::string> output_vocabulary(const Wfst& f) {
    std::vector<std::string> words;
    std::set<Label> seen;
    for (int s = 0; s < f.num_states(); ++s)
        for (const Arc& a : f.arcs(s)) {
            if (a.olabel == kEpsilon || symbols().kind(a.olabel) != Kind::WORD) continue;
            if (seen.insert(a.olabel).second) words.push_back(symbols().text(a.olabel));
        }
    return words;
}

}  // namespace

// ---- shorthands -------------------------------------------------------------

Wfst rule(const std::string& written, const std::vector<std::string>& spoken, float w) {
    return transducer(chars_of(written), words_of(spoken), w);
}

Wfst chars_in(const std::string& written, float w) {
    return transducer(chars_of(written), {}, w);
}

Wfst chars_out(const std::string& written, float w) {
    return transducer({}, chars_of(written), w);
}

Wfst words_out(const std::vector<std::string>& spoken, float w) {
    return transducer({}, words_of(spoken), w);
}

// ---- vocabularies -----------------------------------------------------------

const char* digit_word(int d) { return kUnits[d]; }

const std::vector<std::string>& cardinal_vocabulary() {
    static const std::vector<std::string> vocabulary = [] {
        std::vector<std::string> v;
        for (int d = 0; d <= 9; ++d) v.push_back(kUnits[d]);
        for (int n = 0; n <= 9; ++n) v.push_back(kTeens[n]);
        for (int t = 2; t <= 9; ++t) v.push_back(kTens[t]);
        v.push_back("hundred");
        for (const char* s : kScales) v.push_back(s);
        v.push_back("minus");
        v.push_back("negative");
        return v;
    }();
    return vocabulary;
}

const std::vector<std::string>& ordinal_vocabulary() {
    static const std::vector<std::string> vocabulary = [] {
        std::vector<std::string> v;
        for (const OrdinalPair& o : kOrdinals) v.push_back(o.ordinal);
        return v;
    }();
    return vocabulary;
}

// ---- grammars ---------------------------------------------------------------

const NumberParts& number_parts() {
    static const NumberParts parts = build_parts();
    return parts;
}

Wfst year_reading(int first_century, int last_century, bool with_cardinal_fallback) {
    const NumberParts& p = number_parts();
    std::vector<Wfst> years;
    for (int century = first_century; century <= last_century; ++century) {
        Wfst lead = rule(two_digits(century), cardinal_words(century));
        // 2000 and 2005 are "two thousand" and "two thousand five", not
        // "twenty hundred" and "twenty oh five"; every other century pairs.
        // The bare two-digit tail is never penalised, which is what leaves
        // 2015 as "twenty fifteen".
        float paired_cost = century == 20 ? kTwoThousandsPenalty : 0.0f;
        Wfst tail = union_of({
            rule("00", {"hundred"}, paired_cost),
            reweight(concat_of({rule("0", {"oh"}), p.unit_nz}), paired_cost),
            p.two_digit_nz,
        });
        years.push_back(concat_of({lead, tail}));
    }
    if (with_cardinal_fallback)
        years.push_back(reweight(
            concat_of({p.unit_nz, words_out({"thousand"}), p.padded3}),
            kCardinalYearPenalty));
    return union_of(years);
}

const Wfst& cardinal_core_fst() {
    static const Wfst f = build_cardinal_core(true);
    return f;
}

const Wfst& cardinal_positive_fst() {
    static const Wfst f = build_cardinal_core(false);
    return f;
}

Wfst hyphenated(const Wfst& f) {
    return connect(compose(f, build_hyphenator(output_vocabulary(f))));
}

const Wfst& bare_number_fst() {
    // The sign is concatenated onto the number rather than distributed over
    // three copies of it: A·B ∪ A·C is A·(B ∪ C), and B here is the whole
    // cardinal machine.
    static const Wfst sign = union_of({
        empty_string_fst(),
        rule("-", {"minus"}),
        rule("-", {"negative"}, kAltSignPenalty),
    });
    // A bare four-digit number in the year range gets the paired reading, by
    // the margin on the general branch rather than by excluding anything. A
    // signed number is never a year, so the year branch stands outside the
    // sign.
    //
    // A Roman numeral is a written-side spelling of a small integer, so it
    // belongs here rather than in a class of its own — and being here is what
    // makes it safe in both directions. ITN's spoken "two" faces "2" at the
    // margin and "II" at this penalty, so the digits win; a separate class
    // would have had to beat PLAIN instead, and could not.
    static const Wfst body = union_of({
        reweight(concat_of({sign, cardinal_core_fst()}), kBareYearMargin),
        year_reading(kBareYearFirstCentury, kBareYearLastCentury, false),
        reweight(roman_numerals(), kRomanNumeralPenalty),
    });
    return body;
}

const Cascade& cardinal_grammar() {
    static const Wfst joined = hyphenated(bare_number_fst());
    static const Cascade grammar = Cascade().then(joined);
    return grammar;
}

const Cascade& ordinal_grammar() {
    static const Wfst stripper = build_suffix_stripper();
    static const Wfst ordinaliser = hyphenated(build_ordinaliser());
    static const Cascade grammar =
        Cascade().then(stripper).then(cardinal_core_fst()).then(ordinaliser);
    return grammar;
}

}  // namespace sayso
