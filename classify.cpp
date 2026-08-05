#include "classify.h"

#include <unordered_map>

#include "../Scalpel/utf8.h"
#include "abbrev.h"
#include "dates.h"
#include "decades.h"
#include "decimals.h"
#include "fractions.h"
#include "letters.h"
#include "measures.h"
#include "money.h"
#include "numbers.h"
#include "ranges.h"
#include "telephone.h"
#include "text.h"
#include "times.h"
#include "weights.h"

namespace sayso {

const char* class_name(SemioticClass c) {
    switch (c) {
        case SemioticClass::CARDINAL: return "CARDINAL";
        case SemioticClass::ORDINAL:  return "ORDINAL";
        case SemioticClass::DECIMAL:  return "DECIMAL";
        case SemioticClass::FRACTION: return "FRACTION";
        case SemioticClass::MONEY:    return "MONEY";
        case SemioticClass::MEASURE:  return "MEASURE";
        case SemioticClass::DATE:     return "DATE";
        case SemioticClass::TIME:     return "TIME";
        case SemioticClass::DECADE:   return "DECADE";
        case SemioticClass::ABBREV:   return "ABBREV";
        case SemioticClass::LETTERS:  return "LETTERS";
        case SemioticClass::RANGE:    return "RANGE";
        case SemioticClass::TELEPHONE: return "TELEPHONE";
        case SemioticClass::PLAIN:    break;
    }
    return "PLAIN";
}

const std::vector<Grammar>& grammars(DateSpeech speech) {
    // Ordered most specific first; the order is cosmetic, the entry costs do
    // the deciding. Two registries, differing only in which date cascade they
    // point at.
    auto build = [](DateSpeech order) {
        return std::vector<Grammar>{
        {SemioticClass::MONEY,    &money_grammar(),    kMoneyEntry},
        {SemioticClass::DATE,     &date_grammar(order), kDateEntry},
        {SemioticClass::TIME,     &time_grammar(),     kTimeEntry},
        {SemioticClass::DECADE,   &decade_grammar(),   kDecadeEntry},
        {SemioticClass::MEASURE,  &measure_grammar(),  kMeasureEntry},
        {SemioticClass::DECIMAL,  &decimal_grammar(),  kDecimalEntry},
        {SemioticClass::FRACTION, &fraction_grammar(), kFractionEntry},
        {SemioticClass::ORDINAL,  &ordinal_grammar(),  kOrdinalEntry},
        {SemioticClass::CARDINAL, &cardinal_grammar(), kCardinalEntry},
        // One-directional; see the note on Grammar::speaking_only.
        {SemioticClass::LETTERS,  &letters_grammar(),  kLettersEntry, true, true},
        {SemioticClass::ABBREV,   &abbreviation_grammar(), kAbbreviationEntry, true},
        {SemioticClass::RANGE,    &range_grammar(),    kRangeEntry, true},
        {SemioticClass::TELEPHONE, &telephone_grammar(), kTelephoneEntry, true},
        };
    };
    static const std::vector<Grammar> canonical = build(DateSpeech::Canonical);
    static const std::vector<Grammar> preserving =
        build(DateSpeech::PreserveWrittenOrder);
    return speech == DateSpeech::Canonical ? canonical : preserving;
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
                       size_t begin, size_t end, Direction dir, bool fold) {
    SpanLabels out;
    out.boundary.push_back(0);

    if (dir == Direction::ITN) {
        for (size_t k = begin; k < end; ++k) {
            out.labels.push_back(
                word(fold ? fold_case(tokens[k].text) : tokens[k].text));
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
            utf8::append(one, fold ? utf8::to_lower(cp) : cp);
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
                              const std::vector<Token>& tokens, Direction dir,
                              DateSpeech speech) {
    std::vector<Segment> result;
    size_t n = tokens.size();
    if (n == 0) return result;

    // A grammar's readings of every prefix of a span depend only on its
    // class, that span's text and the direction, so they are worth
    // remembering across a document.
    //
    // One entry holds every class's answers rather than one entry per class:
    // the span text is the expensive part of the key, and building it once per
    // span start instead of once per (span start, class) is most of what the
    // warm path costs once the grammars themselves are all cache hits.
    // The map is a function-static and nothing trims it, so a long-lived
    // process would grow without bound on novel text. The cap is generous —
    // a real corpus shows its vocabulary of numbers, amounts and dates within
    // its first few hundred sentences, so the warm path is warm long before
    // this — and clearing outright rather than evicting one entry keeps the
    // bookkeeping to a counter.
    using Readings = std::vector<std::pair<float, std::string>>;
    using SpanReadings = std::vector<Readings>;  // indexed as grammars()
    static std::unordered_map<std::string, SpanReadings> cache;
    constexpr size_t kMaxCachedSpans = 200000;
    if (cache.size() > kMaxCachedSpans) cache.clear();

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
        SpanLabels span = span_labels(input, tokens, i, last, dir, true);
        if (span.labels.empty()) continue;
        // Folding is per codepoint, so the two label sequences agree on
        // length and on every token boundary; only the labels differ.
        SpanLabels cased = span_labels(input, tokens, i, last, dir, false);
        size_t from = static_cast<size_t>(tokens[i].start_index);
        std::string longest = input.substr(
            from, static_cast<size_t>(tokens[last - 1].end_index) - from);

        std::string key = std::string(1, dir == Direction::TTS ? 't' : 'i') +
                          (speech == DateSpeech::Canonical ? 'c' : 'p') + "\t" + longest;
        auto hit = cache.find(key);
        if (hit == cache.end()) {
            SpanReadings entry;
            entry.reserve(grammars(speech).size());
            for (const Grammar& grammar : grammars(speech)) {
                if (dir == Direction::ITN && grammar.speaking_only) {
                    entry.emplace_back();
                    continue;
                }
                const std::vector<Label>& labels =
                    grammar.wants_case ? cased.labels : span.labels;
                std::vector<Path> paths =
                    dir == Direction::TTS ? grammar.grammar->apply_prefixes(labels)
                                          : grammar.grammar->apply_prefixes_inverse(labels);
                // Only up to the last prefix this grammar actually read. The
                // overwhelming majority of (span, class) pairs match nothing
                // at all, and storing a full-length vector of misses for each
                // of them was 99.95% of what the cache held — 4.5 GB over
                // thirty thousand lines of corpus. A short vector reads the
                // same, because the lookup below already treats a position
                // past the end as no candidate.
                size_t last_found = 0;
                for (size_t k = 0; k < paths.size(); ++k)
                    if (paths[k].found) last_found = k + 1;

                Readings readings;
                readings.reserve(last_found);
                for (size_t k = 0; k < last_found; ++k)
                    readings.emplace_back(
                        paths[k].found ? paths[k].weight : kInfinity,
                        paths[k].found ? render(paths[k].olabels) : std::string());
                entry.push_back(std::move(readings));
            }
            hit = cache.emplace(std::move(key), std::move(entry)).first;
        }

        for (size_t g = 0; g < grammars(speech).size(); ++g) {
            const Grammar& grammar = grammars(speech)[g];
            const Readings& readings = hit->second[g];

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
