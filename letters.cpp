#include "letters.h"

#include <string>
#include <vector>

#include "numbers.h"
#include "weights.h"

namespace sayso {

namespace {

Wfst build_letters() {
    // A letter carrying its full stop. Every path must contain at least one of
    // these, which is what keeps a bare "a" or "I" out of the grammar.
    std::vector<Wfst> dotted, bare;
    for (char c = 'a'; c <= 'z'; ++c) {
        std::string letter(1, c);
        dotted.push_back(rule(letter + ".", {letter}));
        bare.push_back(rule(letter, {letter}));
    }
    Wfst with_stop = union_of(dotted);

    // Two letters minimum, and it is the sentence-final full stop that forces
    // it. Scalpel gives "the 1980s." the tokens [1980] [s] [.], and a
    // one-letter rule reads the last two of those as a dotted initial — which
    // both spells out the plural of a decade and eats the sentence's full
    // stop, since a claimed span is replaced whole. "vitamin C." loses its
    // stop the same way. Requiring two letters costs the bare initial of
    // "J. Smith" and keeps every acronym.
    //
    // "U.S." and "e.g." are the ordinary shape; "U.S" merely drops the last
    // stop, so the final letter may go bare — but only after one that did not.
    return connect(union_of({
        concat_of({with_stop, with_stop, closure(with_stop)}),
        concat_of({with_stop, closure(with_stop),
                   reweight(union_of(bare), kUndottedFinalLetterPenalty)}),
    }));
}

}  // namespace

const Cascade& letters_grammar() {
    static const Wfst core = build_letters();
    static const Cascade grammar = Cascade().then(core);
    return grammar;
}

}  // namespace sayso
