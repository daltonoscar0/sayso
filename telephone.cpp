#include "telephone.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

// Zero is "o", not "zero". Every other digit keeps its name.
Wfst spoken_digit() {
    std::vector<Wfst> digits{rule("0", {"o"})};
    for (int d = 1; d <= 9; ++d)
        digits.push_back(rule(std::string(1, static_cast<char>('0' + d)),
                              {digit_word(d)}));
    return union_of(digits);
}

Wfst build_telephone() {
    Wfst d = spoken_digit();
    Wfst group = concat_of({d, closure(d)});
    // The separator is silent: the reference marks it "sil" throughout.
    Wfst joined = concat_of({chars_in("-"), group});

    // Four groups minimum, and the bound is structural rather than a weight
    // because a weight cannot do the job. Three hyphen-joined groups is
    // exactly the shape of an ISO date, and the two cannot be separated by
    // cost: a zero-padded "2015-03-04" costs DATE 0.95, so this class would
    // have to enter above that, and above that it also loses to RANGE plus a
    // run of negative cardinals at 0.88. No value lies in both intervals.
    //
    // No date has four hyphen-joined fields, so the guard is exact. It gives
    // up the three-group serial numbers, 751 of the shard's 4,120 telephone
    // tokens, and keeps the 3,369 that were doing the damage.
    return connect(concat_of({group, joined, joined, joined, closure(joined)}));
}

}  // namespace

const Cascade& telephone_grammar() {
    static const Wfst core = build_telephone();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
