#pragma once
// Weighted finite-state transducers over the tropical semiring.
//
//   ⊕ = min      ⊗ = +      0̄ = +inf      1̄ = 0
//
// Tropical means "cost": a path's weight is the sum of its arc weights plus
// the final weight of the state it ends in, and the best analysis of an input
// is the cheapest path. Every ambiguity in Sayso, "twenty five" vs
// "twenty-five", "$1 dollar" vs "$1 dollars", MONEY vs CARDINAL over the same
// span, is settled by that single rule.
//
// The operations below are the whole algebra the grammars are written in.
// Nothing in the grammar files pokes at states or arcs directly; they call
// transducer(), union_of(), concat_of(), closure(), compose() and invert().
#include <limits>
#include <unordered_set>
#include <vector>

#include "symbols.h"

namespace sayso {

constexpr float kInfinity = std::numeric_limits<float>::infinity();

struct Arc {
    Label ilabel = kEpsilon;
    Label olabel = kEpsilon;
    float weight = 0.0f;
    int next = -1;
};

class Wfst {
  public:
    int add_state(float final_weight = kInfinity);
    void add_arc(int from, const Arc& a) { arcs_[from].push_back(a); }
    void set_start(int s) { start_ = s; }
    void set_final(int s, float w) { final_[s] = w; }

    int start() const { return start_; }
    int num_states() const { return static_cast<int>(final_.size()); }
    size_t num_arcs() const;
    float final_weight(int s) const { return final_[s]; }
    bool is_final(int s) const { return final_[s] != kInfinity; }
    const std::vector<Arc>& arcs(int s) const { return arcs_[s]; }

  private:
    std::vector<std::vector<Arc>> arcs_;
    std::vector<float> final_;
    int start_ = -1;
};

// ---- primitive machines -----------------------------------------------------

// Accepts the empty string with weight w.
Wfst empty_string_fst(float w = 0.0f);
// Accepts nothing (the semiring zero of union).
Wfst empty_fst();
// One linear path reading `in` and writing `out`; the shorter side is padded
// with epsilon. Total path weight is w.
Wfst transducer(const std::vector<Label>& in, const std::vector<Label>& out,
                float w = 0.0f);

// ---- rational operations ----------------------------------------------------

Wfst union_of(const std::vector<Wfst>& parts);
Wfst concat_of(const std::vector<Wfst>& parts);
Wfst closure(const Wfst& a);           // Kleene star (accepts ε)
Wfst optional_of(const Wfst& a);       // a ∪ ε
Wfst reweight(const Wfst& a, float w); // add w to every path of a

// ---- relational operations --------------------------------------------------

// Swap the tapes. Weights are preserved, which is the whole point: one
// grammar, read forwards for TTS and backwards for ITN, with the same
// costs deciding both.
Wfst invert(const Wfst& a);
Wfst project_input(const Wfst& a);
Wfst project_output(const Wfst& a);

// Composition with Mohri's three-state epsilon filter, so that a pair of
// epsilon-bearing machines yields each output path exactly once.
//
// `left_of`, when given, receives the left-hand component of every state
// created: left_of[s] is the state of `a` that output state s was paired
// from. It is what lets a caller carry a property of `a`'s states, an input
// position, say, through a chain of compositions.
Wfst compose(const Wfst& a, const Wfst& b, std::vector<int>* left_of = nullptr);

// Drop states that are unreachable or from which no final state is reachable.
// `survivor_of`, when given, receives the old state id of each surviving one.
Wfst connect(const Wfst& a, std::vector<int>* survivor_of = nullptr);

// ---- search -----------------------------------------------------------------

struct Path {
    std::vector<Label> ilabels;
    std::vector<Label> olabels;
    float weight = kInfinity;
    bool found = false;
};

// Single-source shortest path (Dijkstra; all Sayso weights are >= 0).
Path shortest_path(const Wfst& a);

// The distance pass on its own, so one Dijkstra can be backtraced from many
// destinations, which is what reading every prefix off a lattice needs.
struct ShortestDistance {
    std::vector<float> dist;
    std::vector<int> from;
    std::vector<Arc> via;
    int start = -1;
};
ShortestDistance shortest_distance(const Wfst& a);
Path backtrace(const ShortestDistance& distance, int state, float cost);

// Cheapest output for a given input: compose an input chain, then decode.
Path best_transduction(const Wfst& a, const std::vector<Label>& input);

// A grammar is a cascade of transducers applied left to right.
//
// Composing the stages eagerly would be the obvious thing and is the wrong
// thing: the date grammar's four written formats composed against its
// canonical machine come to over a million states, none of which any single
// input visits. Composing each stage against the small lattice the previous
// stage produced keeps every intermediate in the hundreds of states. That is
// the standard lazy-composition trick, and it is also the natural home for
// inversion, inverting a cascade reverses it and inverts each stage, which
// is just invert(A ∘ B) = invert(B) ∘ invert(A) applied down the list.
class Cascade {
  public:
    // A cascade holds its stages by reference, so every stage must outlive it.
    // In practice each one is a function-local static built once; passing a
    // temporary compiles cleanly and then dangles.
    Cascade& then(const Wfst& stage);

    Path apply(const std::vector<Label>& input) const;
    Path apply_inverse(const std::vector<Label>& input) const;

    // Best path for *every* prefix of the input at once: result[k] transduces
    // input[0..k), and result[0] is the empty prefix.
    //
    // The classifier needs exactly this. Asking a grammar about each span of a
    // sentence separately re-walks the same prefixes over and over, a
    // sentence with a long spelled-out numeral pays for its prefixes
    // quadratically. Composing once against an all-final input chain and
    // reading the answer off each input position collapses that to one
    // composition per (span start, grammar), and one Dijkstra for all of them.
    std::vector<Path> apply_prefixes(const std::vector<Label>& input) const;
    std::vector<Path> apply_prefixes_inverse(const std::vector<Label>& input) const;

  private:
    // The labels that can begin and end an accepted input. The classifier asks
    // every grammar about every span of every sentence, and the overwhelming
    // majority of those questions are settled by the first and last symbols
    // alone: no currency grammar has anything to say about a span starting
    // with a digit, and no number grammar has anything to say about one
    // ending in "summary". Two set lookups replace two compositions.
    struct Boundary {
        std::unordered_set<Label> first, last;
        bool computed = false;
    };
    const Boundary& boundary(bool inverse) const;

    // The stages read backwards, built once on first inverse use.
    const std::vector<const Wfst*>& inverse_stages() const;

    std::vector<const Wfst*> stages_;
    mutable std::vector<Wfst> inverted_;
    mutable std::vector<const Wfst*> inverted_pointers_;
    mutable Boundary forward_, backward_;
};

}  // namespace sayso
