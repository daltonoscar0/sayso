#include "money.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

struct Currency {
    const char* symbol;
    const char* major_singular;
    const char* major_plural;
    const char* minor_singular;
    const char* minor_plural;
};

const Currency kCurrencies[] = {
    {"$", "dollar", "dollars", "cent", "cents"},
    {"€", "euro", "euros", "cent", "cents"},   // €
    {"£", "pound", "pounds", "penny", "pence"},  // £
};

// Written " million" and friends: the magnitude survives as a word on both
// tapes, so "$2.5 million" speaks as "two point five million dollars".
const char* kMagnitudes[] = {"thousand", "million", "billion", "trillion"};

// The same magnitudes written as a letter suffix, which is how a newspaper
// writes them: "£20m", "$26.6bn", "$50k". Spans are case-folded before a
// grammar sees them, so only the lowercase spellings belong here — "$3B"
// arrives as "$3b".
struct MagnitudeSuffix {
    const char* written;
    const char* spoken;
};
const MagnitudeSuffix kMagnitudeSuffixes[] = {
    {"k", "thousand"}, {"m", "million"}, {"bn", "billion"}, {"b", "billion"},
    {"tn", "trillion"},
};

// One-digit cents: "$2.5" is two dollars fifty, not two dollars five.
const char* kTensWords[10] = {"", "ten", "twenty", "thirty", "forty",
                              "fifty", "sixty", "seventy", "eighty", "ninety"};

std::string digit(int d) { return std::string(1, static_cast<char>('0' + d)); }

// Exactly two digits, 01-99.
Wfst cents_two_digit() {
    const NumberParts& p = number_parts();
    return hyphenated(union_of({concat_of({chars_in("0"), p.unit_nz}), p.two_digit_nz}));
}

// The amount plus its currency word, in either grammatical number. The
// singular branch only fits an amount of exactly one; the margin on the
// plural branch is what makes the singular win where both fit.
Wfst amount_with_unit(const char* singular, const char* plural, const Wfst& amount) {
    return union_of({
        concat_of({rule("1", {"one"}), words_out({singular})}),
        concat_of({amount, words_out({plural}, kPluralPreferenceMargin)}),
    });
}

Wfst optional_and() {
    return union_of({words_out({"and"}), empty_string_fst(kNoAndPenalty)});
}

Wfst build_money() {
    Wfst amount = hyphenated(cardinal_positive_fst());
    Wfst cents = cents_two_digit();

    // The fractional tail of a magnitude amount, spoken digit by digit:
    // "2.5 million" -> "two point five million".
    std::vector<Wfst> fraction_digits;
    for (int d = 0; d <= 9; ++d) fraction_digits.push_back(rule(digit(d), {digit_word(d)}));

    std::vector<Wfst> branches;
    for (size_t index = 0; index < sizeof(kCurrencies) / sizeof(kCurrencies[0]); ++index) {
        const Currency& c = kCurrencies[index];
        float currency_cost = index == 0 ? 0.0f : kNonDefaultCurrencyPenalty;
        // "$ 1" is written often enough to read and never worth producing.
        Wfst symbol = concat_of({
            chars_in(c.symbol, currency_cost),
            union_of({empty_string_fst(), chars_in(" ", kCurrencySpacePenalty)}),
        });
        Wfst major = amount_with_unit(c.major_singular, c.major_plural, amount);
        Wfst minor = union_of({
            concat_of({chars_in("0"), rule("1", {"one"}), words_out({c.minor_singular})}),
            concat_of({cents, words_out({c.minor_plural}, kPluralPreferenceMargin)}),
        });

        // $2.5 -> two dollars and fifty cents, written back as $2.50
        std::vector<Wfst> short_cents;
        for (int d = 1; d <= 9; ++d)
            short_cents.push_back(rule(digit(d), {kTensWords[d]}));

        // Everything that may follow a major amount, unioned *before* the
        // amount is concatenated onto it. Written the other way round — one
        // branch per tail, each beginning with the amount — every currency
        // carries four copies of the cardinal machine instead of one, and
        // composing this grammar against a span is the single largest cost in
        // the classifier. Path weights are unchanged: the tropical semiring
        // sums along a path, so a reweight() moved inside a concatenation
        // lands on the same total.
        branches.push_back(concat_of({
            symbol, major,
            union_of({
                empty_string_fst(),                            // $25
                chars_in(".00", kExplicitZeroCentsPenalty),    // $25.00
                concat_of({chars_in("."), optional_and(), minor}),  // $25.10
                reweight(concat_of({chars_in("."), optional_and(),
                                    union_of(short_cents),
                                    words_out({c.minor_plural})}),
                         kShortCentsPenalty),                  // $2.5
            }),
        }));

        // $0.99 -> ninety-nine cents, with the spelled-out zero accepted too
        branches.push_back(concat_of({symbol, chars_in("0."), minor}));
        branches.push_back(reweight(
            concat_of({symbol, rule("0", {"zero"}), words_out({c.major_plural}),
                       chars_in("."), optional_and(), minor}),
            kZeroDollarsPenalty));

        // $2.5 million, with the fractional tail factored out the same way
        Wfst decimal = concat_of({
            amount,
            union_of({empty_string_fst(),
                      concat_of({rule(".", {"point"}), union_of(fraction_digits),
                                 closure(union_of(fraction_digits))})}),
        });
        // A magnitude is written either spelled out after a space or as a
        // letter suffix flush against the amount. Both are read; the spelled
        // one costs nothing, so that is what ITN writes.
        std::vector<Wfst> magnitudes;
        for (const char* m : kMagnitudes)
            magnitudes.push_back(
                concat_of({chars_in(" "), transducer(chars_of(m), words_of({m}))}));
        for (const MagnitudeSuffix& s : kMagnitudeSuffixes)
            magnitudes.push_back(rule(s.written, {s.spoken}, kLetterMagnitudePenalty));
        branches.push_back(reweight(concat_of({symbol, decimal, union_of(magnitudes),
                                               words_out({c.major_plural})}),
                                    kSpelledMagnitudePenalty));
    }
    return connect(union_of(branches));
}

}  // namespace

const Cascade& money_grammar() {
    static const Wfst core = build_money();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
