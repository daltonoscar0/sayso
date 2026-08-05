#include "measures.h"

#include <string>
#include <vector>

#include "decimals.h"
#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

// `written` is stored in the case the unit is conventionally spelled with, so
// that is the case ITN writes. `spaced` says which side of
// kMeasureSpacingPenalty a unit sits on: letter units take a space ("5 km"),
// symbol units do not ("50%", "25°C"). The other spelling of each is still
// read, just not written.
struct Unit {
    const char* written;
    std::vector<std::string> singular;
    std::vector<std::string> plural;
    bool spaced;
    // Spellings read but never written, so ITN settles on `written`.
    std::vector<std::string> alternates = {};
    // Extra cost for a unit whose spoken form another class also claims.
    float extra = 0.0f;
};

// ASCII-only, which is all that is needed and all that is safe to do byte at a
// time: every byte of a multi-byte UTF-8 sequence has its high bit set, so the
// degree sign passes through untouched.
std::string fold_ascii(const std::string& s) {
    std::string out;
    for (char c : s)
        out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    return out;
}

const std::vector<Unit>& units() {
    static const std::vector<Unit> table = {
        // length
        {"km", {"kilometer"}, {"kilometers"}, true},
        {"m", {"meter"}, {"meters"}, true},
        {"cm", {"centimeter"}, {"centimeters"}, true},
        {"mm", {"millimeter"}, {"millimeters"}, true},
        {"nm", {"nanometer"}, {"nanometers"}, true},
        {"mi", {"mile"}, {"miles"}, true},
        {"ft", {"foot"}, {"feet"}, true},
        {"yd", {"yard"}, {"yards"}, true},
        // mass
        {"kg", {"kilogram"}, {"kilograms"}, true},
        {"g", {"gram"}, {"grams"}, true},
        {"mg", {"milligram"}, {"milligrams"}, true},
        // "pounds" is also sterling; see kCurrencyHomographPenalty.
        {"lb", {"pound"}, {"pounds"}, true, {"lbs"}, kCurrencyHomographPenalty},
        {"oz", {"ounce"}, {"ounces"}, true},
        // speed
        {"mph", {"mile", "per", "hour"}, {"miles", "per", "hour"}, true},
        {"kph", {"kilometer", "per", "hour"}, {"kilometers", "per", "hour"}, true},
        {"km/h", {"kilometer", "per", "hour"}, {"kilometers", "per", "hour"}, true},
        // volume
        {"ml", {"milliliter"}, {"milliliters"}, true},
        {"cl", {"centiliter"}, {"centiliters"}, true},
        {"L", {"liter"}, {"liters"}, true},
        // time spans, where the abbreviation is not also a word
        {"min", {"minute"}, {"minutes"}, true},
        {"hr", {"hour"}, {"hours"}, true},
        {"ms", {"millisecond"}, {"milliseconds"}, true},
        // frequency
        {"Hz", {"hertz"}, {"hertz"}, true},
        {"kHz", {"kilohertz"}, {"kilohertz"}, true},
        {"MHz", {"megahertz"}, {"megahertz"}, true},
        {"GHz", {"gigahertz"}, {"gigahertz"}, true},
        // data
        {"KB", {"kilobyte"}, {"kilobytes"}, true},
        {"MB", {"megabyte"}, {"megabytes"}, true},
        {"GB", {"gigabyte"}, {"gigabytes"}, true},
        {"TB", {"terabyte"}, {"terabytes"}, true},
        // symbols, written flush against the amount
        {"%", {"percent"}, {"percent"}, false},
        {"°", {"degree"}, {"degrees"}, false},
        {"°C", {"degree", "celsius"}, {"degrees", "celsius"}, false},
        {"°F", {"degree", "fahrenheit"}, {"degrees", "fahrenheit"}, false},
    };
    return table;
}

Wfst build_measure() {
    // Cardinals and decimals both count as amounts; a decimal is never
    // singular, so it only ever reaches the plural branch.
    const Wfst& amount = number_or_decimal_fst();

    // Concatenation distributes over union, so the amount is concatenated onto
    // the *union* of the unit tails rather than onto each of them. Written the
    // obvious way round, thirty-odd units mean thirty-odd copies of the
    // cardinal and decimal machines, and that one difference is most of what
    // this grammar costs to compose against a span.
    std::vector<Wfst> singular_tails, plural_tails;
    for (const Unit& u : units()) {
        Wfst separator =
            u.spaced ? union_of({chars_in(" "), empty_string_fst(kMeasureSpacingPenalty)})
                     : union_of({empty_string_fst(), chars_in(" ", kMeasureSpacingPenalty)});
        // Where the two spellings differ, only one of them is reachable in a
        // given direction: TTS is handed a folded span and can only match the
        // folded branch, ITN writes whichever is cheaper and so writes the
        // canonical one.
        std::vector<Wfst> spellings{chars_in(u.written)};
        std::string folded = fold_ascii(u.written);
        if (folded != u.written)
            spellings.push_back(chars_in(folded, kFoldedCasePenalty));
        for (const std::string& alternate : u.alternates) {
            spellings.push_back(chars_in(alternate, kAltUnitSpellingPenalty));
            std::string alternate_folded = fold_ascii(alternate);
            if (alternate_folded != alternate)
                spellings.push_back(chars_in(alternate_folded,
                                             kAltUnitSpellingPenalty + kFoldedCasePenalty));
        }
        Wfst symbol = reweight(union_of(spellings), u.extra);

        singular_tails.push_back(concat_of({separator, symbol, words_out(u.singular)}));
        plural_tails.push_back(concat_of({separator, symbol,
                                          words_out(u.plural, kPluralPreferenceMargin)}));
    }
    return connect(hyphenated(union_of({
        concat_of({rule("1", {"one"}), union_of(singular_tails)}),
        concat_of({amount, union_of(plural_tails)}),
    })));
}

}  // namespace

const Cascade& measure_grammar() {
    static const Wfst core = build_measure();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
