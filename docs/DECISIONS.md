# Decisions

One line each: what was decided, what else was on the table, why.

## Engine

- **C++17, zero runtime dependencies, flat layout, `g++` one-liners plus a Makefile** — alt: CMake, a header-only design; matches Scalpel's build story and morphology's Makefile, which is the same author's pattern for a project that consumes Scalpel.
- **Tropical semiring (min, +) with `float` arc and final weights** — alt: log semiring, integer costs; tropical gives best-path decoding directly, which is the only decoding Sayso needs.
- **One process-wide `SymbolTable`** — alt: per-FST tables with relabelling on composition; every grammar composes with every other, so a shared table removes an entire class of bug for the cost of a singleton.
- **Written side is one symbol per character, spoken side one symbol per word** — alt: characters throughout, words throughout; digits have no word boundaries and number words have no useful character structure, so each side gets the granularity it actually has.
- **Mohri's three-state epsilon filter in `compose`** — alt: remove epsilons first; the filter is ~15 lines and keeps grammars readable, since insertions (`ε:dollars`) and deletions (`$:ε`) are how currency and date grammars express themselves.
- **Dijkstra for shortest path, weights asserted non-negative** — alt: Bellman-Ford, generic shortest-distance over the semiring; every cost in Sayso is a penalty, so negative weights would be a bug rather than a feature.
- **Hand-rolled property harness (`property.h`, xorshift64\* + shrinking)** — alt: a test framework as a dev dependency; a seeded generator plus a shrinker is ~80 lines, and keeping it in-tree means the test build has the same zero-dependency story as the library.

## Grammars

- **ITN is `invert()` of the verbalisation grammar, never a second grammar** — alt: hand-written inverse rules; a second grammar drifts, and inversion makes the weighting scheme govern both directions by construction.
- **Hyphenation is a separate word-level FST composed onto every number** — alt: enumerating "twenty-five" as an atom in the cardinal table; a 9x8 table becomes one machine with 8 states, and inverting it is what lets ITN read both "twenty five" and "twenty-five".
- **Ordinals are `suffix_stripper ∘ cardinal ∘ ordinaliser`** — alt: a parallel ordinal number table; composition expresses the actual generalisation (only the last word of a numeral inflects) and makes suffix agreement (21st, not 21th) fall out of inversion instead of a lookup.
- **Digit-by-digit fallback priced per digit (0.5), not as a flat entry cost** — alt: a flat penalty; per-digit keeps the fallback under the per-character cost of leaving a long token alone while never outbidding a real reading of a short one.
- **Thousands commas are optional on the written side at a cost** — alt: a separate comma-stripping pass; one weighted arc makes TTS accept "1,234" and makes ITN emit "1234" from the same machine.
- **`minus` is generated for a leading `-`, `negative` merely accepted** — alt: accepting only one; the weight records the preference without narrowing what ITN can read.
- **Cardinals reach 999,999,999,999 (twelve digits)** — alt: stopping at the required nine digits; the scale loop is the same code either way, so billions came free.

## Pipeline

- **Scalpel is the tokeniser; Sayso adds an offset-repair adapter, not a fork** — alt: patching Scalpel, reimplementing tokenisation; Scalpel's `START` state ignores HYPHEN/APOSTROPHE/OTHER characters, so `-` in `2015-03-14` and `€` in `€40` are dropped, and comparing token offsets against the input recovers them generically.
- **CLASSIFY is a lattice WFST over token boundaries, decoded by the same `shortest_path`** — alt: a longest-match scan; making it a lattice means longest-match is an emergent consequence of pricing PLAIN per character rather than a separate rule.
- **Spans are case-folded before reaching a grammar** — alt: case variants inside every grammar; the spoken side is lowercase by construction and the canonical written forms Sayso generates contain no letters that need case.
- **Serialisation copies the gaps between segments verbatim from the input** — alt: a detokeniser; byte offsets make the untouched text exactly untouched, so quoting, spacing and stray punctuation cannot be damaged by normalisation.
