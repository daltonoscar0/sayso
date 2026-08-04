#include "classify.h"

#include <unordered_map>

#include "../Scalpel/utf8.h"
#include "dates.h"
#include "money.h"
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
        {SemioticClass::MONEY,    &money_grammar(),    kMoneyEntry},
        {SemioticClass::DATE,     &date_grammar(),     kDateEntry},
        {SemioticClass::ORDINAL,  &ordinal_grammar(),  kOrdinalEntry},
        {SemioticClass::CARDINAL, &cardinal_grammar(), kCardinalEntry},
    };
    return registry;
}

namespace {

// The labels of the *longest* span starting at a token, plus the label
// position of each shorter span's end. One label sequence answers for every
// span from that start, which is what lets the grammar be run once.
struct SpanLabels {
    std::vector<Label> labels;
    std::vector<size_t> boundary;  // boundary[k] = labels covering tokens [begin, begin+k)
};

// Written spans keep their punctuation and internal spacing (whitespace runs
// collapse to one space); spoken spans are just their words. Both are case
// folded, since the grammars are written in one case.
SpanLabels span_labels(const std::string& input, const std::vector<Token>& tokens,
                       size_t begin, size_t end, Direction dir) {
    SpanLabels out;
    out.boundary.push_back(0);

    if (dir == Direction::ITN) {
        for (size_t k = begin; k < end; ++k) {
            out.labels.push_back(word(fold_case(tokens[k].text)));
            out.boundary.push_back(out.labels.size());
        }
        return out;
    }

    size_t from = static_cast<size_t>(tokens[begin].start_index);
    size_t to = static_cast<size_t>(tokens[end - 1].end_index);
    size_t next_token = begin;
    bool in_space = false;
    size_t i = from, len = 0;
    while (i < to) {
        uint32_t cp = utf8::decode(input, i, len);
        bool space = cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r';
        if (space) {
            if (!in_space) out.labels.push_back(ch(" "));
        } else {
            std::string one;
            utf8::append(one, utf8::to_lower(cp));
            out.labels.push_back(ch(one));
        }
        in_space = space;
        i += len;
        while (next_token < end &&
               i >= static_cast<size_t>(tokens[next_token].end_index)) {
            out.boundary.push_back(out.labels.size());
            ++next_token;
        }
    }
    while (out.boundary.size() < end - begin + 1) out.boundary.push_back(out.labels.size());
    return out;
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

    // A grammar's readings of every prefix of a span depend only on its
    // class, that span's text and the direction, so they are worth
    // remembering across a document.
    using Readings = std::vector<std::pair<float, std::string>>;
    static std::unordered_map<std::string, Readings> cache;

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

        // Every span from this start is answered by one run per grammar over
        // the longest of them.
        size_t last = std::min(n, i + max_span_tokens(dir));
        SpanLabels span = span_labels(input, tokens, i, last, dir);
        if (span.labels.empty()) continue;
        size_t from = static_cast<size_t>(tokens[i].start_index);
        std::string longest = input.substr(
            from, static_cast<size_t>(tokens[last - 1].end_index) - from);

        for (const Grammar& grammar : grammars()) {
            std::string key = std::string(1, dir == Direction::TTS ? 't' : 'i') +
                              class_name(grammar.type) + "\t" + longest;
            auto hit = cache.find(key);
            if (hit == cache.end()) {
                std::vector<Path> paths =
                    dir == Direction::TTS ? grammar.grammar->apply_prefixes(span.labels)
                                          : grammar.grammar->apply_prefixes_inverse(span.labels);
                Readings readings;
                readings.reserve(paths.size());
                for (const Path& p : paths)
                    readings.emplace_back(p.found ? p.weight : kInfinity,
                                          p.found ? render(p.olabels) : std::string());
                hit = cache.emplace(std::move(key), std::move(readings)).first;
            }
            const Readings& readings = hit->second;

            for (size_t k = 1; k + i <= last; ++k) {
                size_t at = span.boundary[k];
                if (at == 0 || at >= readings.size()) continue;
                float weight = readings[at].first;
                if (weight == kInfinity) continue;

                candidates.push_back({grammar.type, i, i + k, readings[at].second,
                                      grammar.entry_cost + weight});
                Arc arc;
                arc.weight = candidates.back().cost;
                arc.olabel = static_cast<Label>(candidates.size());
                arc.next = state[i + k];
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
