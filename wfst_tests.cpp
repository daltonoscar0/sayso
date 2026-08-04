// Unit tests for the weighted FST engine, in Scalpel's house test style.
//
// Build:
//   g++ -std=c++17 -O2 -o wfst_tests wfst_tests.cpp wfst.cpp symbols.cpp -I.
#include <iostream>
#include <string>
#include <vector>

#include "property.h"
#include "wfst.h"

using namespace sayso;

static int run = 0, failed = 0;

static void check(bool ok, const std::string& name) {
    ++run;
    if (!ok) { ++failed; std::cout << "[FAIL]" << name << "\n"; }
    else     {           std::cout << "[PASS]" << name << "\n"; }
}

// Written string in, spoken words out.
static Wfst rule(const std::string& written, const std::vector<std::string>& spoken,
                 float w = 0.0f) {
    return transducer(chars_of(written), words_of(spoken), w);
}

static std::string transduce(const Wfst& f, const std::string& written) {
    Path p = best_transduction(f, chars_of(written));
    return p.found ? render(p.olabels) : "<none>";
}

static void primitive_tests() {
    Wfst f = rule("12", {"twelve"});
    check(transduce(f, "12") == "twelve", "linear transducer maps its one path");
    check(transduce(f, "13") == "<none>", "linear transducer rejects other input");
    check(transduce(empty_string_fst(), "") == "", "empty-string machine accepts epsilon");
    check(transduce(empty_fst(), "") == "<none>", "empty machine accepts nothing");
}

static void union_tests() {
    Wfst f = union_of({rule("1", {"one"}), rule("2", {"two"})});
    check(transduce(f, "1") == "one" && transduce(f, "2") == "two" && transduce(f, "3") == "<none>",
          "union accepts each branch and nothing else");

    // Same input, two outputs, different costs: the cheaper one wins.
    Wfst ambiguous = union_of({rule("1", {"one"}, 2.0f), rule("1", {"ace"}, 0.5f)});
    check(transduce(ambiguous, "1") == "ace", "shortest path resolves an ambiguous union");
}

static void concat_tests() {
    Wfst f = concat_of({rule("1", {"one"}), rule("2", {"two"})});
    check(transduce(f, "12") == "one two", "concatenation chains two rules");
    check(transduce(f, "1") == "<none>", "concatenation requires both parts");

    Wfst with_optional = concat_of({rule("1", {"one"}), optional_of(rule("2", {"two"}))});
    check(transduce(with_optional, "1") == "one" && transduce(with_optional, "12") == "one two",
          "optional_of makes a part skippable");
}

static void closure_tests() {
    Wfst digit = union_of({rule("0", {"oh"}), rule("1", {"one"})});
    Wfst star = closure(digit);
    check(transduce(star, "") == "", "closure accepts the empty string");
    check(transduce(star, "0110") == "oh one one oh", "closure repeats its argument");
}

static void weight_tests() {
    Wfst cheap = reweight(rule("1", {"one"}), 0.25f);
    Path p = best_transduction(cheap, chars_of("1"));
    check(p.found && p.weight == 0.25f, "reweight adds a constant to every path");

    Wfst f = union_of({reweight(rule("1", {"one"}), 1.0f), rule("1", {"unit"}, 0.5f)});
    check(transduce(f, "1") == "unit", "reweight participates in best-path selection");
}

static void invert_tests() {
    Wfst f = rule("25", {"twenty-five"});
    Wfst back = invert(f);
    Path p = best_transduction(back, words_of({"twenty-five"}));
    check(p.found && render(p.olabels) == "25", "inversion swaps the tapes");

    Wfst twice = invert(invert(f));
    check(transduce(twice, "25") == "twenty-five", "inversion is an involution");

    // Weights survive inversion, so the same costs settle both directions.
    Wfst weighted = union_of({rule("1", {"one"}, 0.5f), rule("2", {"one"}, 2.0f)});
    Path q = best_transduction(invert(weighted), words_of({"one"}));
    check(q.found && render(q.olabels) == "1" && q.weight == 0.5f,
          "inversion preserves weights");
}

static void projection_tests() {
    Wfst f = rule("12", {"twelve"});
    Path in = best_transduction(project_input(f), chars_of("12"));
    check(in.found && render(in.olabels) == "12", "input projection copies the lower tape");
    Path out = best_transduction(invert(project_output(f)), words_of({"twelve"}));
    check(out.found && render(out.olabels) == "twelve",
          "output projection copies the upper tape");
}

static void compose_tests() {
    Wfst digits_to_words = union_of({rule("1", {"one"}), rule("2", {"two"})});
    // Word-level second stage: "one" -> "first".
    Wfst ordinalise = transducer(words_of({"one"}), words_of({"first"}));
    Wfst composed = compose(digits_to_words, ordinalise);
    check(transduce(composed, "1") == "first", "composition chains two relations");
    check(transduce(composed, "2") == "<none>",
          "composition rejects what the second stage cannot read");

    // Weights add across composition.
    Wfst a = rule("1", {"one"}, 1.5f);
    Wfst b = transducer(words_of({"one"}), words_of({"first"}), 0.25f);
    Path p = best_transduction(compose(a, b), chars_of("1"));
    check(p.found && p.weight == 1.75f, "composition adds weights in the tropical semiring");

    // Epsilon on both tapes: the filter must yield exactly one path.
    Wfst insert = transducer({}, words_of({"and"}));
    Wfst deleting = transducer(words_of({"and"}), {});
    Wfst round = compose(concat_of({rule("1", {"one"}), insert}),
                         concat_of({transducer(words_of({"one"}), words_of({"uno"})), deleting}));
    check(transduce(round, "1") == "uno", "epsilon filter composes insertions with deletions");
}

static void connect_tests() {
    Wfst f;
    int s0 = f.add_state();
    int s1 = f.add_state(0.0f);
    int dead = f.add_state();          // reachable, but never final
    f.set_start(s0);
    Arc a;
    a.ilabel = ch("1");
    a.olabel = word("one");
    a.next = s1;
    f.add_arc(s0, a);
    Arc b;
    b.ilabel = ch("9");
    b.next = dead;
    f.add_arc(s0, b);
    Wfst trimmed = connect(f);
    check(trimmed.num_states() == 2, "connect drops non-coaccessible states");
    check(transduce(trimmed, "1") == "one", "connect keeps the surviving path");
}

static void shortest_path_tests() {
    Wfst f = union_of({rule("1", {"a"}, 3.0f), rule("1", {"b"}, 1.0f), rule("1", {"c"}, 2.0f)});
    Wfst lattice = compose(transducer(chars_of("1"), chars_of("1")), f);
    Path p = shortest_path(lattice);
    check(p.found && render(p.olabels) == "b" && p.weight == 1.0f,
          "shortest path picks the minimum-cost branch");

    // Final-state weights count towards the total.
    Wfst g;
    int s0 = g.add_state();
    int s1 = g.add_state(4.0f);
    g.set_start(s0);
    Arc arc;
    arc.ilabel = ch("1");
    arc.olabel = word("one");
    arc.weight = 1.0f;
    arc.next = s1;
    g.add_arc(s0, arc);
    Path q = best_transduction(g, chars_of("1"));
    check(q.found && q.weight == 5.0f, "final weights are part of the path cost");
}

// The engine laws the grammars lean on, checked over random small machines.
static void algebraic_property_tests() {
    prop::Property<long long> involution;
    involution.generate = [](prop::Rng& r) { return r.in_range(0, 9999); };
    involution.shrink = prop::shrink_integer;
    involution.show = [](const long long& n) { return std::to_string(n); };
    involution.holds = [](const long long& n) {
        std::string s = std::to_string(n);
        Wfst f = transducer(chars_of(s), words_of({"n" + s}));
        Path p = best_transduction(invert(invert(f)), chars_of(s));
        return p.found && render(p.olabels) == "n" + s;
    };
    std::string witness;
    check(prop::check_property(involution, 20260803, 200, witness),
          "property: invert(invert(f)) == f" + (witness.empty() ? "" : " [" + witness + "]"));

    prop::Property<long long> assoc;
    assoc.generate = [](prop::Rng& r) { return r.in_range(100, 999); };
    assoc.shrink = [](const long long& n) {
        std::vector<long long> out;
        if (n > 100) out.push_back(n - 1);
        if (n / 2 >= 100) out.push_back(n / 2);
        return out;
    };
    assoc.show = [](const long long& n) { return std::to_string(n); };
    assoc.holds = [](const long long& n) {
        std::string s = std::to_string(n);
        std::vector<Wfst> parts;
        for (char c : s) parts.push_back(rule(std::string(1, c), {std::string(1, c)}));
        Wfst left = concat_of({concat_of({parts[0], parts[1]}), parts[2]});
        Wfst right = concat_of({parts[0], concat_of({parts[1], parts[2]})});
        return transduce(left, s) == s.substr(0, 1) + " " + s.substr(1, 1) + " " +
                                         s.substr(2, 1) &&
               transduce(left, s) == transduce(right, s);
    };
    witness.clear();
    check(prop::check_property(assoc, 20260803, 200, witness),
          "property: concatenation is associative" +
              (witness.empty() ? "" : " [" + witness + "]"));
}

int main() {
    primitive_tests();
    union_tests();
    concat_tests();
    closure_tests();
    weight_tests();
    invert_tests();
    projection_tests();
    compose_tests();
    connect_tests();
    shortest_path_tests();
    algebraic_property_tests();
    std::cout << "\nTests run:" << run << "\nTests failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
