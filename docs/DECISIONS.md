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

## Money and dates

- **Grammars are lazily-composed `Cascade`s, not eagerly composed machines** — alt: composing every stage at build time; the date grammar's formats composed against its canonical machine came to 1.2M states, of which any one input visits a few hundred, so stages are composed against the input lattice instead.
- **Priority union is expressed as a hair of weight, not as set difference** — alt: implementing `difference` and subtracting the specific case from the general one; a 0.01 margin on the general branch makes the specific one win wherever both apply, in both directions, for one line of code.
- **`hyphenated()` reads its vocabulary off the machine it is given** — alt: a hand-maintained word list; a missing identity arc does not pass a word through, it kills the path, and that bug cost real debugging time before the vocabulary became derived.
- **One canonical date grammar (M/D/YYYY) plus character-level format normalisers** — alt: one machine per written format; reordering a field costs one path per value of that field, and holding the day (31) or year (200) in a small rewrite machine is far cheaper than holding it across a copy of the day and year verbalisers.
- **ISO dates are limited to years 1900-2099** — alt: all four-digit years; each year is a distinct path through the reorder machine, and 200 covers every realistic date at a tenth the size.
- **Dates require all three fields** — alt: making the year optional; "March 14" is common, but an optional year makes "may second" a date and the classifier has no context to tell that from the verb, so the ambiguity is refused rather than guessed at.
- **The dollar is the canonical currency; others pay a hair** — alt: leaving the tie; "ninety-nine cents" fits dollars and euros equally, and an arbitrary tie-break is a coin flip that changes between builds.
- **Format penalties are small (0.5-0.7), not large** — alt: the 2.0-3.0 they started at; they only have to beat the canonical format's zero, and a large penalty made a date span look expensive to the classifier, which then read "2015-03-14" as three cardinals separated by minus signs.
- **Homographs always convert in ITN ("I second the motion" -> "I 2nd the motion")** — alt: a stop-word list; the classifier models no context, so this is exactly the job deferred to the neural reranker, and the behaviour is pinned by a test rather than left to be discovered.

## Pipeline

- **Scalpel is the tokeniser; Sayso adds an offset-repair adapter, not a fork** — alt: patching Scalpel, reimplementing tokenisation; Scalpel's `START` state ignores HYPHEN/APOSTROPHE/OTHER characters, so `-` in `2015-03-14` and `€` in `€40` are dropped, and comparing token offsets against the input recovers them generically.
- **CLASSIFY is a lattice WFST over token boundaries, decoded by the same `shortest_path`** — alt: a longest-match scan; making it a lattice means longest-match is an emergent consequence of pricing PLAIN per character rather than a separate rule.
- **Span length is capped, and the cap differs by direction (8 written, 16 spoken)** — alt: one unbounded cap; the classifier's cost grows sharply with it, and the two directions genuinely differ — the longest written span is "$1,234,567.89" at eight tokens while an unhyphenated spoken cardinal below a billion runs to fifteen.
- **The spoken cap is 16, not 12, and that cost fourfold throughput** — alt: keeping 12 and being fast; relaxed scoring showed 12 splitting "eight hundred forty six million ..." into three numbers, and speech recognisers do not hyphenate, so 12 was wrong for the direction that actually consumes ASR output. Correctness first; the real fix (one composition per span start) is in the README's next steps.
- **Span verbalisations are memoised per (direction, class, text)** — alt: recomputing; the classifier asks the same question of the same span text constantly across a corpus, and the cache is what gets the throughput number into the thousands of sentences per second.
- **Spans are case-folded before reaching a grammar** — alt: case variants inside every grammar; the spoken side is lowercase by construction and the canonical written forms Sayso generates contain no letters that need case.
- **Serialisation copies the gaps between segments verbatim from the input** — alt: a detokeniser; byte offsets make the untouched text exactly untouched, so quoting, spacing and stray punctuation cannot be damaged by normalisation.
- **FIRST- and LAST-set prefilters on every cascade** — alt: composing and letting it fail; the classifier asks every grammar about every span, and the overwhelming majority of those questions are settled by the first and last symbols alone — no currency grammar has anything to say about a span starting with a digit, and no number grammar about one ending in "summary".
- **Composed states are packed into a 64-bit key in an `unordered_map`** — alt: `std::map<std::tuple<int,int,int>>`; composition is the library's hot loop and the tuple comparison was showing up as a constant factor on every call.

## Evaluation

- **The eval harness is C++ and zero-dependency; only the NeMo comparison is Python** — alt: a Python harness for both; the Sayso half then could not be run without the dev environment, which defeats the point of having one.
- **An optional fourth "canonical written" column in the slice** — alt: scoring ITN against the corpus's written column; inverse normalisation is only defined up to a canonical form, and scoring against an arbitrary one put DATE at 24% where every "error" was a correctly normalised date in a different format.
- **The synthetic slice is generated by an independent reference verbaliser, not by the grammars** — alt: generating it from the FSTs; that scores 100% by construction and measures nothing, and the README says plainly what the synthetic numbers do and do not mean.
- **Two throughput numbers, cold and warm** — alt: quoting the warm one; they differ by an order of magnitude and quoting only the flattering one would be misleading.
- **Two scoring modes, strict and relaxed** — alt: strict only; strict penalises another system for writing "thirty fourth" or "nine hundred and forty two", which are not errors, so a cross-system number under strict scoring measures conformance to this slice's conventions rather than quality.
- **The relaxed mode de-hyphenates the ITN input** — alt: feeding both systems hyphenated text; inverse normalisers consume ASR output, which has no hyphens, so hyphenated input tests something no deployment ever sees — and de-hyphenating exposed a genuine Sayso bug in the span cap.
- **The NeMo comparison is reported with its caveats in the table, not below it** — alt: quoting 100% against 18%; the slice's reference conventions are Sayso's own, NeMo scores 100% on ORDINAL where the conventions coincide, and most remaining gaps are differences of target rather than errors.
