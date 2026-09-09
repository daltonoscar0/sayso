#include "proper.h"

#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

Wfst identical(const std::string& c) {
    return transducer(chars_of(c), chars_of(c));
}

Wfst any_of(const std::string& alphabet) {
    std::vector<Wfst> parts;
    for (char c : alphabet) parts.push_back(identical(std::string(1, c)));
    return union_of(parts);
}

Wfst upper_letter() { return any_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"); }

// The apostrophe and the hyphen are letters as far as a name is concerned:
// "O'Brien" and "Stratford-upon-Avon" are each one word.
Wfst lower_letter() { return any_of("abcdefghijklmnopqrstuvwxyz'-"); }

Wfst capitalised_word() {
    return reweight(
        concat_of({upper_letter(), lower_letter(), closure(lower_letter())}),
        kUnexplainedNameWord);
}

Wfst proper_name(int most) {
    Wfst another = optional_of(concat_of({identical(" "), capitalised_word()}));
    std::vector<Wfst> parts{capitalised_word()};
    for (int i = 1; i < most; ++i) parts.push_back(another);
    return concat_of(parts);
}

Wfst gap() { return chars_in(" "); }

Wfst spelled_letter() {
    std::vector<Wfst> parts;
    for (char c = 'A'; c <= 'Z'; ++c) {
        std::string s(1, c);
        parts.push_back(rule(s, {s}));
    }
    return union_of(parts);
}

}  // namespace sayso
