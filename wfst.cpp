#include "wfst.h"

#include <algorithm>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace sayso {

int Wfst::add_state(float final_weight) {
    arcs_.emplace_back();
    final_.push_back(final_weight);
    return static_cast<int>(final_.size()) - 1;
}

size_t Wfst::num_arcs() const {
    size_t n = 0;
    for (const auto& v : arcs_) n += v.size();
    return n;
}

namespace {

// Copy `src` into `dst`, returning the offset its state ids were shifted by.
int splice(Wfst& dst, const Wfst& src) {
    int offset = dst.num_states();
    for (int s = 0; s < src.num_states(); ++s) dst.add_state(src.final_weight(s));
    for (int s = 0; s < src.num_states(); ++s)
        for (const Arc& a : src.arcs(s)) {
            Arc b = a;
            b.next = a.next + offset;
            dst.add_arc(s + offset, b);
        }
    return offset;
}

}  // namespace

// ---- primitive machines -----------------------------------------------------

Wfst empty_string_fst(float w) {
    Wfst f;
    int s = f.add_state(w);
    f.set_start(s);
    return f;
}

Wfst empty_fst() {
    Wfst f;
    f.set_start(f.add_state());
    return f;
}

Wfst transducer(const std::vector<Label>& in, const std::vector<Label>& out,
                float w) {
    Wfst f;
    int prev = f.add_state();
    f.set_start(prev);
    size_t n = std::max(in.size(), out.size());
    if (n == 0) {
        f.set_final(prev, w);
        return f;
    }
    for (size_t i = 0; i < n; ++i) {
        int next = f.add_state();
        Arc a;
        a.ilabel = i < in.size() ? in[i] : kEpsilon;
        a.olabel = i < out.size() ? out[i] : kEpsilon;
        a.weight = i == 0 ? w : 0.0f;
        a.next = next;
        f.add_arc(prev, a);
        prev = next;
    }
    f.set_final(prev, 0.0f);
    return f;
}

// ---- rational operations ----------------------------------------------------

Wfst union_of(const std::vector<Wfst>& parts) {
    Wfst f;
    int start = f.add_state();
    f.set_start(start);
    for (const Wfst& p : parts) {
        if (p.start() < 0) continue;
        int offset = splice(f, p);
        Arc a;
        a.next = p.start() + offset;
        f.add_arc(start, a);
    }
    return f;
}

Wfst concat_of(const std::vector<Wfst>& parts) {
    if (parts.empty()) return empty_string_fst();
    Wfst f;
    int offset = splice(f, parts[0]);
    f.set_start(parts[0].start() + offset);
    std::vector<int> tails;
    for (int s = 0; s < parts[0].num_states(); ++s)
        if (parts[0].is_final(s)) tails.push_back(s + offset);
    std::vector<float> tail_weights;
    for (int s = 0; s < parts[0].num_states(); ++s)
        if (parts[0].is_final(s)) tail_weights.push_back(parts[0].final_weight(s));

    for (size_t i = 1; i < parts.size(); ++i) {
        const Wfst& p = parts[i];
        int off = splice(f, p);
        for (size_t k = 0; k < tails.size(); ++k) {
            Arc a;
            a.weight = tail_weights[k];
            a.next = p.start() + off;
            f.add_arc(tails[k], a);
            f.set_final(tails[k], kInfinity);
        }
        tails.clear();
        tail_weights.clear();
        for (int s = 0; s < p.num_states(); ++s)
            if (p.is_final(s)) {
                tails.push_back(s + off);
                tail_weights.push_back(p.final_weight(s));
            }
    }
    return f;
}

Wfst closure(const Wfst& a) {
    Wfst f;
    int start = f.add_state(0.0f);
    f.set_start(start);
    if (a.start() < 0) return f;
    int offset = splice(f, a);
    Arc enter;
    enter.next = a.start() + offset;
    f.add_arc(start, enter);
    for (int s = 0; s < a.num_states(); ++s)
        if (a.is_final(s)) {
            Arc back;
            back.weight = a.final_weight(s);
            back.next = start;
            f.add_arc(s + offset, back);
            f.set_final(s + offset, kInfinity);
        }
    return f;
}

Wfst optional_of(const Wfst& a) { return union_of({a, empty_string_fst()}); }

Wfst reweight(const Wfst& a, float w) {
    Wfst f;
    int start = f.add_state();
    f.set_start(start);
    int offset = splice(f, a);
    Arc e;
    e.weight = w;
    e.next = a.start() + offset;
    f.add_arc(start, e);
    return f;
}

// ---- relational operations --------------------------------------------------

Wfst invert(const Wfst& a) {
    Wfst out;
    for (int s = 0; s < a.num_states(); ++s) out.add_state(a.final_weight(s));
    out.set_start(a.start());
    for (int s = 0; s < a.num_states(); ++s)
        for (const Arc& arc : a.arcs(s)) {
            Arc b = arc;
            std::swap(b.ilabel, b.olabel);
            out.add_arc(s, b);
        }
    return out;
}

namespace {

Wfst project(const Wfst& a, bool to_input) {
    Wfst out;
    for (int s = 0; s < a.num_states(); ++s) out.add_state(a.final_weight(s));
    out.set_start(a.start());
    for (int s = 0; s < a.num_states(); ++s)
        for (const Arc& arc : a.arcs(s)) {
            Arc b = arc;
            if (to_input) b.olabel = b.ilabel;
            else b.ilabel = b.olabel;
            out.add_arc(s, b);
        }
    return out;
}

}  // namespace

Wfst project_input(const Wfst& a) { return project(a, true); }
Wfst project_output(const Wfst& a) { return project(a, false); }

Wfst compose(const Wfst& a, const Wfst& b, std::vector<int>* left_of) {
    Wfst out;
    if (left_of) left_of->clear();
    if (a.start() < 0 || b.start() < 0) return empty_fst();

    // A composed state is (state in a, state in b, filter). The filter keeps
    // an epsilon on a's output tape from being paired with an epsilon on b's
    // input tape in two different orders, which would duplicate paths.
    //
    // The triple is packed into one 64-bit key: composition is the hot loop of
    // the whole library, and a hashed integer costs a fraction of a
    // std::map<std::tuple<...>> lookup.
    auto pack = [](int sa, int sb, int filter) {
        return (static_cast<uint64_t>(sa) << 34) | (static_cast<uint64_t>(sb) << 2) |
               static_cast<uint64_t>(filter);
    };
    std::unordered_map<uint64_t, int> index;
    std::vector<uint64_t> stack;

    auto state_for = [&](int sa, int sb, int filter) {
        uint64_t key = pack(sa, sb, filter);
        auto it = index.find(key);
        if (it != index.end()) return it->second;
        float fw = kInfinity;
        if (a.is_final(sa) && b.is_final(sb))
            fw = a.final_weight(sa) + b.final_weight(sb);
        int id = out.add_state(fw);
        if (left_of) left_of->push_back(sa);
        index[key] = id;
        stack.push_back(key);
        return id;
    };

    out.set_start(state_for(a.start(), b.start(), 0));
    while (!stack.empty()) {
        uint64_t cur = stack.back();
        stack.pop_back();
        int sa = static_cast<int>(cur >> 34);
        int sb = static_cast<int>((cur >> 2) & 0xFFFFFFFFull);
        int filter = static_cast<int>(cur & 0x3);
        int here = index[cur];

        for (const Arc& x : a.arcs(sa)) {
            if (x.olabel != kEpsilon) {
                for (const Arc& y : b.arcs(sb)) {
                    if (y.ilabel != x.olabel) continue;
                    Arc c;
                    c.ilabel = x.ilabel;
                    c.olabel = y.olabel;
                    c.weight = x.weight + y.weight;
                    c.next = state_for(x.next, y.next, 0);
                    out.add_arc(here, c);
                }
            } else {
                // a writes nothing: either advance a alone, or pair the move
                // with an epsilon read on b's side (allowed only from the
                // neutral filter state, which is what stops the two orders of
                // the same pair from both surviving).
                if (filter == 0)
                    for (const Arc& y : b.arcs(sb)) {
                        if (y.ilabel != kEpsilon) continue;
                        Arc c;
                        c.ilabel = x.ilabel;
                        c.olabel = y.olabel;
                        c.weight = x.weight + y.weight;
                        c.next = state_for(x.next, y.next, 0);
                        out.add_arc(here, c);
                    }
                if (filter == 0 || filter == 1) {
                    Arc c;
                    c.ilabel = x.ilabel;
                    c.weight = x.weight;
                    c.next = state_for(x.next, sb, 1);
                    out.add_arc(here, c);
                }
            }
        }
        for (const Arc& y : b.arcs(sb)) {
            if (y.ilabel != kEpsilon) continue;
            if (filter != 0 && filter != 2) continue;
            Arc c;
            c.olabel = y.olabel;
            c.weight = y.weight;
            c.next = state_for(sa, y.next, 2);
            out.add_arc(here, c);
        }
    }
    return out;
}

// When nothing survives, the result is empty_fst(), which has one state, not
// none. `survivor_of` has to gain an entry for it or the invariant callers
// rely on, one entry per state of the result, silently breaks: run_prefix_
// stages indexed mapping[0] into an empty vector on every span no grammar
// matched, which AddressSanitizer catches and an ordinary build does not.
//
// The entry names the original start. That state does survive, in the only
// sense left: it is still the start, with everything unreachable from it or
// unable to reach a final state trimmed away.
Wfst connect(const Wfst& a, std::vector<int>* survivor_of) {
    int n = a.num_states();
    if (survivor_of) survivor_of->clear();
    auto nothing_survives = [&] {
        if (survivor_of) survivor_of->assign(1, a.start() < 0 ? 0 : a.start());
        return empty_fst();
    };
    if (a.start() < 0 || n == 0) return nothing_survives();

    std::vector<char> reachable(n, 0), coreachable(n, 0);
    std::vector<std::vector<int>> back(n);
    std::vector<int> stack{a.start()};
    reachable[a.start()] = 1;
    while (!stack.empty()) {
        int s = stack.back();
        stack.pop_back();
        for (const Arc& arc : a.arcs(s)) {
            back[arc.next].push_back(s);
            if (!reachable[arc.next]) {
                reachable[arc.next] = 1;
                stack.push_back(arc.next);
            }
        }
    }
    for (int s = 0; s < n; ++s)
        if (reachable[s] && a.is_final(s)) {
            coreachable[s] = 1;
            stack.push_back(s);
        }
    while (!stack.empty()) {
        int s = stack.back();
        stack.pop_back();
        for (int p : back[s])
            if (!coreachable[p]) {
                coreachable[p] = 1;
                stack.push_back(p);
            }
    }

    std::vector<int> remap(n, -1);
    Wfst out;
    for (int s = 0; s < n; ++s)
        if (reachable[s] && coreachable[s]) {
            remap[s] = out.add_state(a.final_weight(s));
            if (survivor_of) survivor_of->push_back(s);
        }
    if (remap[a.start()] < 0) return nothing_survives();
    out.set_start(remap[a.start()]);
    for (int s = 0; s < n; ++s) {
        if (remap[s] < 0) continue;
        for (const Arc& arc : a.arcs(s)) {
            if (remap[arc.next] < 0) continue;
            Arc b = arc;
            b.next = remap[arc.next];
            out.add_arc(remap[s], b);
        }
    }
    return out;
}

// ---- search -----------------------------------------------------------------

// Single-source shortest distance, kept separate from the backtrace so that
// one Dijkstra can answer for many destinations.
ShortestDistance shortest_distance(const Wfst& a) {
    ShortestDistance out;
    int n = a.num_states();
    if (a.start() < 0 || n == 0) return out;

    out.dist.assign(n, kInfinity);
    out.from.assign(n, -1);
    out.via.assign(n, Arc());
    using Entry = std::pair<float, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    out.dist[a.start()] = 0.0f;
    out.start = a.start();
    queue.push({0.0f, a.start()});

    std::vector<char> done(n, 0);
    while (!queue.empty()) {
        auto [d, s] = queue.top();
        queue.pop();
        if (done[s]) continue;
        done[s] = 1;
        for (const Arc& arc : a.arcs(s)) {
            float nd = d + arc.weight;
            if (nd < out.dist[arc.next]) {
                out.dist[arc.next] = nd;
                out.from[arc.next] = s;
                out.via[arc.next] = arc;
                queue.push({nd, arc.next});
            }
        }
    }
    return out;
}

Path backtrace(const ShortestDistance& distance, int state, float cost) {
    Path result;
    std::vector<Arc> path;
    for (int s = state; s != distance.start; s = distance.from[s])
        path.push_back(distance.via[s]);
    std::reverse(path.begin(), path.end());
    for (const Arc& arc : path) {
        if (arc.ilabel != kEpsilon) result.ilabels.push_back(arc.ilabel);
        if (arc.olabel != kEpsilon) result.olabels.push_back(arc.olabel);
    }
    result.weight = cost;
    result.found = true;
    return result;
}

Path shortest_path(const Wfst& a) {
    ShortestDistance distance = shortest_distance(a);
    if (distance.dist.empty()) return Path();

    int best = -1;
    float best_cost = kInfinity;
    for (int s = 0; s < a.num_states(); ++s) {
        if (!a.is_final(s) || distance.dist[s] == kInfinity) continue;
        float total = distance.dist[s] + a.final_weight(s);
        if (total < best_cost) {
            best_cost = total;
            best = s;
        }
    }
    if (best < 0) return Path();
    return backtrace(distance, best, best_cost);
}

Path best_transduction(const Wfst& a, const std::vector<Label>& input) {
    Wfst chain = transducer(input, input);
    return shortest_path(compose(chain, a));
}

Cascade& Cascade::then(const Wfst& stage) {
    stages_.push_back(&stage);
    inverted_.clear();
    inverted_pointers_.clear();
    forward_.computed = backward_.computed = false;
    return *this;
}

namespace {

// Labels that can be the first symbol read: walk the input-epsilon arcs out
// of the start state and collect every real label they lead to.
std::unordered_set<Label> compute_first_labels(const Wfst& f) {
    std::unordered_set<Label> first;
    if (f.start() < 0) return first;
    std::vector<int> stack{f.start()};
    std::unordered_set<int> seen{f.start()};
    while (!stack.empty()) {
        int s = stack.back();
        stack.pop_back();
        for (const Arc& a : f.arcs(s)) {
            if (a.ilabel != kEpsilon) {
                first.insert(a.ilabel);
            } else if (seen.insert(a.next).second) {
                stack.push_back(a.next);
            }
        }
    }
    return first;
}

// Labels that can be the last symbol read: the mirror image, over states from
// which a final state is reachable without reading anything more.
std::unordered_set<Label> compute_last_labels(const Wfst& f) {
    std::unordered_set<Label> last;
    int n = f.num_states();
    if (n == 0) return last;

    std::vector<std::vector<int>> back(n);
    for (int s = 0; s < n; ++s)
        for (const Arc& a : f.arcs(s))
            if (a.ilabel == kEpsilon) back[a.next].push_back(s);

    std::vector<char> can_finish(n, 0);
    std::vector<int> stack;
    for (int s = 0; s < n; ++s)
        if (f.is_final(s)) {
            can_finish[s] = 1;
            stack.push_back(s);
        }
    while (!stack.empty()) {
        int s = stack.back();
        stack.pop_back();
        for (int p : back[s])
            if (!can_finish[p]) {
                can_finish[p] = 1;
                stack.push_back(p);
            }
    }

    for (int s = 0; s < n; ++s)
        for (const Arc& a : f.arcs(s))
            if (a.ilabel != kEpsilon && can_finish[a.next]) last.insert(a.ilabel);
    return last;
}

}  // namespace

const Cascade::Boundary& Cascade::boundary(bool inverse) const {
    Boundary& b = inverse ? backward_ : forward_;
    if (b.computed) return b;
    if (!stages_.empty()) {
        // The stage that reads the input: the first one forwards, the last one
        // inverted backwards. Later stages constrain further, but the cheap
        // test only needs a necessary condition.
        Wfst held;
        const Wfst* reader = stages_.front();
        if (inverse) {
            held = invert(*stages_.back());
            reader = &held;
        }
        b.first = compute_first_labels(*reader);
        b.last = compute_last_labels(*reader);
    }
    b.computed = true;
    return b;
}

namespace {

// connect() between stages keeps a dead branch from being multiplied by the
// next stage. On the last stage there is no next stage, and shortest_path
// ignores dead branches anyway, so the trim is pure cost.
Path run_stages(const std::vector<Label>& input,
                const std::vector<const Wfst*>& stages) {
    Wfst lattice = transducer(input, input);
    for (size_t i = 0; i < stages.size(); ++i) {
        lattice = compose(lattice, *stages[i]);
        if (i + 1 < stages.size()) lattice = connect(lattice);
    }
    return shortest_path(lattice);
}

}  // namespace

Path Cascade::apply(const std::vector<Label>& input) const {
    const Boundary& b = boundary(false);
    if (!input.empty() && (!b.first.count(input.front()) || !b.last.count(input.back())))
        return Path();
    return run_stages(input, stages_);
}

Path Cascade::apply_inverse(const std::vector<Label>& input) const {
    const Boundary& b = boundary(true);
    if (!input.empty() && (!b.first.count(input.front()) || !b.last.count(input.back())))
        return Path();
    return run_stages(input, inverse_stages());
}

const std::vector<const Wfst*>& Cascade::inverse_stages() const {
    if (inverted_.size() != stages_.size()) {
        inverted_.clear();
        inverted_pointers_.clear();
        for (auto it = stages_.rbegin(); it != stages_.rend(); ++it)
            inverted_.push_back(invert(**it));
        for (const Wfst& stage : inverted_) inverted_pointers_.push_back(&stage);
    }
    return inverted_pointers_;
}

namespace {

// An input chain whose every state is final, so that a path may stop at any
// input position and still be accepted. This is what makes one composition
// answer for every prefix at once.
Wfst prefix_chain(const std::vector<Label>& input) {
    Wfst f;
    int prev = f.add_state(0.0f);
    f.set_start(prev);
    for (Label l : input) {
        int next = f.add_state(0.0f);
        Arc a;
        a.ilabel = a.olabel = l;
        a.next = next;
        f.add_arc(prev, a);
        prev = next;
    }
    return f;
}

// Compose the cascade against a prefix chain, carrying each lattice state's
// input position along, then decode every position from a single Dijkstra.
std::vector<Path> run_prefix_stages(const std::vector<Label>& input,
                                    const std::vector<const Wfst*>& stages) {
    std::vector<Path> result(input.size() + 1);

    Wfst lattice = prefix_chain(input);
    std::vector<int> position(lattice.num_states());
    for (int s = 0; s < lattice.num_states(); ++s) position[s] = s;

    std::vector<int> mapping;
    for (const Wfst* stage : stages) {
        Wfst composed = compose(lattice, *stage, &mapping);
        std::vector<int> composed_position(composed.num_states());
        for (int s = 0; s < composed.num_states(); ++s)
            composed_position[s] = position[mapping[s]];

        Wfst trimmed = connect(composed, &mapping);
        std::vector<int> trimmed_position(trimmed.num_states());
        for (int s = 0; s < trimmed.num_states(); ++s)
            trimmed_position[s] = composed_position[mapping[s]];

        lattice = std::move(trimmed);
        position = std::move(trimmed_position);
        if (lattice.num_states() == 0) return result;
    }

    ShortestDistance distance = shortest_distance(lattice);
    if (distance.dist.empty()) return result;

    // Cheapest final state per input position, then one backtrace each.
    std::vector<int> best(input.size() + 1, -1);
    std::vector<float> best_cost(input.size() + 1, kInfinity);
    for (int s = 0; s < lattice.num_states(); ++s) {
        if (!lattice.is_final(s) || distance.dist[s] == kInfinity) continue;
        size_t at = static_cast<size_t>(position[s]);
        float total = distance.dist[s] + lattice.final_weight(s);
        if (at < best_cost.size() && total < best_cost[at]) {
            best_cost[at] = total;
            best[at] = s;
        }
    }
    for (size_t at = 0; at < best.size(); ++at)
        if (best[at] >= 0) result[at] = backtrace(distance, best[at], best_cost[at]);
    return result;
}

}  // namespace

std::vector<Path> Cascade::apply_prefixes(const std::vector<Label>& input) const {
    if (!input.empty() && !boundary(false).first.count(input.front()))
        return std::vector<Path>(input.size() + 1);
    return run_prefix_stages(input, stages_);
}

std::vector<Path> Cascade::apply_prefixes_inverse(const std::vector<Label>& input) const {
    if (!input.empty() && !boundary(true).first.count(input.front()))
        return std::vector<Path>(input.size() + 1);
    return run_prefix_stages(input, inverse_stages());
}

}  // namespace sayso
