#pragma once
// A property-test harness small enough to keep Sayso dependency-free.
//
// A Property bundles a generator, the predicate under test, a shrinker and a
// printer. check_property() draws `trials` values from a seeded RNG, and on
// the first failure walks the shrinker downhill until no smaller
// counterexample fails, so a report names the simplest input that breaks,
// not the random one that happened to.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace prop {

// xorshift64*: deterministic across platforms, which matters because a
// failing seed printed on one machine has to reproduce on another.
class Rng {
  public:
    explicit Rng(uint64_t seed) : state_(seed ? seed : 0x9E3779B97F4A7C15ull) {}

    uint64_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1Dull;
    }

    // Uniform in [lo, hi].
    long long in_range(long long lo, long long hi) {
        if (hi <= lo) return lo;
        return lo + static_cast<long long>(next() % static_cast<uint64_t>(hi - lo + 1));
    }

    size_t pick(size_t n) { return static_cast<size_t>(next() % n); }

  private:
    uint64_t state_;
};

template <typename T>
struct Property {
    std::function<T(Rng&)> generate;
    std::function<bool(const T&)> holds;
    std::function<std::vector<T>(const T&)> shrink;
    std::function<std::string(const T&)> show;
};

// Halve towards zero, and try the neighbours; enough structure to reduce a
// nine-digit counterexample to a two-digit one in a handful of steps.
inline std::vector<long long> shrink_integer(long long n) {
    std::vector<long long> out;
    if (n == 0) return out;
    if (n < 0) out.push_back(-n);
    out.push_back(0);
    if (n / 2 != n) out.push_back(n / 2);
    if (n > 0) out.push_back(n - 1);
    if (n < 0) out.push_back(n + 1);
    return out;
}

template <typename T>
bool check_property(const Property<T>& p, uint64_t seed, int trials,
                    std::string& counterexample) {
    Rng rng(seed);
    for (int i = 0; i < trials; ++i) {
        T value = p.generate(rng);
        if (p.holds(value)) continue;

        T smallest = value;
        for (int step = 0; step < 256; ++step) {
            bool improved = false;
            for (const T& candidate : p.shrink(smallest))
                if (!p.holds(candidate)) {
                    smallest = candidate;
                    improved = true;
                    break;
                }
            if (!improved) break;
        }
        counterexample = p.show(smallest);
        return false;
    }
    return true;
}

}  // namespace prop
