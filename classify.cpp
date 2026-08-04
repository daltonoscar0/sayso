#include "classify.h"

#include <unordered_map>

#include "numbers.h"
#include "text.h"
#include "weights.h"

namespace sayso {

const char* class_name(SemioticClass c) {
    switch (c) {
        case SemioticClass::CARDINAL: return "CARDINAL";
        case SemioticClass::ORDINAL:  return "ORDINAL";
        case SemioticClass::MONEY:    return "MONEY";
        case SemioticClass::DATE:     return "DATE";
        case SemioticClass::PLAIN:    break;
    }
    return "PLAIN";
}

const std::vector<Grammar>& grammars() {
    // Ordered most specific first; the order is cosmetic, the entry costs do
    // the deciding.
    static const std::vector<Grammar> registry = {
        {SemioticClass::ORDINAL,  &ordinal_fst(),  kOrdinalEntry},
        {SemioticClass::CARDINAL, &cardinal_fst(), kCardinalEntry},
    };
    return registry;
}

namespace {

// The grammars are stored written->spoken; ITN reads the same machines
// backwards. Inverting is cheap but not free, so each one is done once.
const std::vector<Wfst>& inverted_grammars() {
    static const std::vector<Wfst> inverted = [] {
        std::vector<Wfst> v;
        for (const Grammar& g : grammars()) v.push_back(invert(*g.fst));
        return v;
    }();
    return inverted;
}

// Written spans keep their punctuation and internal spacing (whitespace runs
// collapse to one space); spoken spans are just their words.
std::vector<Label> span_labels(const std::string& source,
                               const std::vector<Token>& tokens, size_t begin,
                               size_t end, Direction dir) {
    if (dir == Direction::ITN) {
        std::vector<std::string> words;
        for (size_t k = begin; k < end; ++k) words.push_back(fold_case(tokens[k].text));
        return words_of(words);
    }
    std::string flat;
    bool in_space = false;
    for (char c : fold_case(source)) {
        bool space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (space) {
            if (!in_space) flat += ' ';
        } else {
            flat += c;
        }
        in_space = space;
    }
    return chars_of(flat);
}

struct Candidate {
    SemioticClass type;
    size_t begin, end;
    std::string output;
    float cost;
};

}  // namespace

std::vector<Segment> classify(const std::string& input,
                              const std::vector<Token>& tokens, Direction dir) {
    std::vector<Segment> result;
    size_t n = tokens.size();
    if (n == 0) return result;

    // A span's verbalisation depends only on its class, its text and the
    // direction, so it is worth remembering across a document.
    static std::unordered_map<std::string, std::pair<float, std::string>> cache;

    std::vector<Candidate> candidates;
    Wfst lattice;
    std::vector<int> state(n + 1);
    for (size_t i = 0; i <= n; ++i) state[i] = lattice.add_state();
    lattice.set_start(state[0]);
    lattice.set_final(state[n], 0.0f);

    for (size_t i = 0; i < n; ++i) {
        // PLAIN: leave this token alone.
        Arc plain;
        plain.weight = kPlainPerChar * static_cast<float>(codepoint_length(tokens[i].text));
        plain.olabel = 0;  // candidate 0 is reserved for "no candidate"
        plain.next = state[i + 1];
        lattice.add_arc(state[i], plain);

        for (size_t j = i + 1; j <= n && j - i <= kMaxSpanTokens; ++j) {
            size_t from = static_cast<size_t>(tokens[i].start_index);
            size_t to = static_cast<size_t>(tokens[j - 1].end_index);
            std::string source = input.substr(from, to - from);
            std::vector<Label> labels = span_labels(source, tokens, i, j, dir);
            if (labels.empty()) continue;

            for (size_t g = 0; g < grammars().size(); ++g) {
                const Grammar& grammar = grammars()[g];
                std::string key = std::string(1, dir == Direction::TTS ? 't' : 'i') +
                                  class_name(grammar.type) + "\t" + source;
                auto hit = cache.find(key);
                if (hit == cache.end()) {
                    const Wfst& machine =
                        dir == Direction::TTS ? *grammar.fst : inverted_grammars()[g];
                    Path p = best_transduction(machine, labels);
                    hit = cache.emplace(key, std::make_pair(
                                                 p.found ? p.weight : kInfinity,
                                                 p.found ? render(p.olabels) : std::string()))
                              .first;
                }
                if (hit->second.first == kInfinity) continue;

                Candidate c{grammar.type, i, j, hit->second.second,
                            grammar.entry_cost + hit->second.first};
                candidates.push_back(c);
                Arc arc;
                arc.weight = c.cost;
                arc.olabel = static_cast<Label>(candidates.size());
                arc.next = state[j];
                lattice.add_arc(state[i], arc);
            }
        }
    }

    Path best = shortest_path(lattice);
    if (!best.found) return result;

    // Output labels are indices into `candidates` (never rendered as text);
    // a PLAIN step contributes no label, so walk the tokens alongside them.
    size_t at = 0, next_label = 0;
    while (at < n) {
        bool claimed = false;
        if (next_label < best.olabels.size()) {
            const Candidate& c = candidates[best.olabels[next_label] - 1];
            if (c.begin == at) {
                Segment s;
                s.type = c.type;
                s.begin = c.begin;
                s.end = c.end;
                s.start_byte = static_cast<size_t>(tokens[c.begin].start_index);
                s.end_byte = static_cast<size_t>(tokens[c.end - 1].end_index);
                s.source = input.substr(s.start_byte, s.end_byte - s.start_byte);
                s.output = c.output;
                s.cost = c.cost;
                result.push_back(s);
                at = c.end;
                ++next_label;
                claimed = true;
            }
        }
        if (claimed) continue;
        Segment s;
        s.type = SemioticClass::PLAIN;
        s.begin = at;
        s.end = at + 1;
        s.start_byte = static_cast<size_t>(tokens[at].start_index);
        s.end_byte = static_cast<size_t>(tokens[at].end_index);
        s.source = input.substr(s.start_byte, s.end_byte - s.start_byte);
        s.output = s.source;
        s.cost = kPlainPerChar * static_cast<float>(codepoint_length(s.source));
        result.push_back(s);
        ++at;
    }
    return result;
}

}  // namespace sayso
