#include "numbers.h"

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

Wfst build_cardinal_core() {
    const NumberParts& p = number_parts();
    std::vector<Wfst> alternatives{p.zero, p.lead3};

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
    std::vector<Wfst> any_digit;
    for (int d = 0; d <= 9; ++d)
        any_digit.push_back(rule(digit(d), {kUnits[d]}, kDigitByDigitPerDigit));
    Wfst one_digit = union_of(any_digit);
    alternatives.push_back(concat_of({one_digit, closure(one_digit)}));

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

Wfst build_hyphenator() {
    Wfst f;
    int home = f.add_state(0.0f);
    f.set_start(home);

    std::vector<std::string> joinable;
    for (int d = 1; d <= 9; ++d) joinable.push_back(kUnits[d]);
    for (const OrdinalPair& o : kOrdinals) {
        std::string c = o.cardinal;
        for (int d = 1; d <= 9; ++d)
            if (c == kUnits[d]) joinable.push_back(o.ordinal);
    }

    std::vector<std::string> vocabulary = cardinal_vocabulary();
    for (const std::string& w : ordinal_vocabulary()) vocabulary.push_back(w);

    for (const std::string& w : vocabulary) {
        bool is_tens = false;
        for (int t = 2; t <= 9; ++t) if (w == kTens[t]) is_tens = true;
        if (is_tens) continue;
        Arc keep;
        keep.ilabel = keep.olabel = word(w);
        keep.next = home;
        f.add_arc(home, keep);
    }

    for (int t = 2; t <= 9; ++t) {
        int held = f.add_state();
        Arc hold;
        hold.ilabel = word(kTens[t]);
        hold.next = held;
        f.add_arc(home, hold);
        for (const std::string& u : joinable) {
            Arc join;
            join.ilabel = word(u);
            join.olabel = word(std::string(kTens[t]) + "-" + u);
            join.next = home;
            f.add_arc(held, join);
        }
        Arc release;                       // nothing joinable followed
        release.olabel = word(kTens[t]);
        release.weight = kSpacedCompoundPenalty;
        release.next = home;
        f.add_arc(held, release);
    }
    return f;
}

}  // namespace

// ---- shorthands -------------------------------------------------------------

Wfst rule(const std::string& written, const std::vector<std::string>& spoken, float w) {
    return transducer(chars_of(written), words_of(spoken), w);
}

Wfst chars_in(const std::string& written, float w) {
    return transducer(chars_of(written), {}, w);
}

Wfst words_out(const std::vector<std::string>& spoken, float w) {
    return transducer({}, words_of(spoken), w);
}

// ---- vocabularies -----------------------------------------------------------

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

const Wfst& hyphenator_fst() {
    static const Wfst f = build_hyphenator();
    return f;
}

const Wfst& cardinal_core_fst() {
    static const Wfst f = build_cardinal_core();
    return f;
}

const Wfst& cardinal_fst() {
    static const Wfst f = [] {
        Wfst signed_number = union_of({
            cardinal_core_fst(),
            concat_of({rule("-", {"minus"}), cardinal_core_fst()}),
            concat_of({rule("-", {"negative"}, kAltSignPenalty), cardinal_core_fst()}),
        });
        return connect(compose(signed_number, hyphenator_fst()));
    }();
    return f;
}

const Wfst& ordinal_fst() {
    static const Wfst f = [] {
        Wfst stripped = compose(build_suffix_stripper(), cardinal_core_fst());
        Wfst inflected = compose(stripped, build_ordinaliser());
        return connect(compose(inflected, hyphenator_fst()));
    }();
    return f;
}

}  // namespace sayso
