# sayso

Bidirectional weighted-FST text normaliser, built on [Scalpel](https://github.com/daltonoscar0/Scalpel).

One grammar per semiotic class, written once in the speaking direction. Reading it forwards
verbalises text for TTS; reading the *same machine* backwards does dictation-style inverse
normalisation. There is no second grammar, no rule list, and no model weights.

```
Input   →  "It costs $25.10 on 3/14/2015."
TTS     →  "It costs twenty-five dollars and ten cents on march fourteenth twenty fifteen."

Input   →  "it costs twenty-five dollars and ten cents on march fourteenth twenty fifteen."
ITN     →  "it costs $25.10 on 3/14/2015."
```

Four semiotic classes: `CARDINAL`, `ORDINAL`, `MONEY`, `DATE`. Everything else passes through
byte for byte.

---

## How it works

Kestrel-style: tokenise, tag spans with a semiotic class, hand each span to that class's
grammar, put the sentence back together.

```
┌──────────────────────────────────────────┐
│  Raw text  (std::string)                 │
└────────────────────┬─────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  Scalpel Tokenizer  (FSM)         + offset-repair adapter    │
│  typed tokens with byte offsets that bracket their own text  │
└────────────────────┬─────────────────────────────────────────┘
                     │  std::vector<Token>
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  CLASSIFY   — a weighted FST over token boundaries           │
│                                                              │
│    state i ──PLAIN, cost = chars(token i)──▶ state i+1       │
│    state i ──CLASS, cost = entry + grammar(span)──▶ state j  │
│                                                              │
│  Every path spans the whole sentence, so best-path decoding  │
│  gives longest-match/lowest-cost tagging. Longest match is   │
│  not a rule; it is what pricing PLAIN per character does.    │
└────────────────────┬─────────────────────────────────────────┘
                     │  spans: CARDINAL | ORDINAL | MONEY | DATE | PLAIN
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  per-class grammar — a composition of WFSTs                  │
│                                                              │
│    CARDINAL  cardinal ∘ hyphenator                           │
│    ORDINAL   suffix-stripper ∘ cardinal ∘ (ordinaliser ∘ h.) │
│    MONEY     currency (with its numeric fields hyphenated)   │
│    DATE      written-format-rewrites ∘ canonical-date        │
│                                                              │
│  Small compositions are done once at build time; the large   │
│  ones are a Cascade, composed against the input instead.     │
└────────────────────┬─────────────────────────────────────────┘
                     │  best path through the composed lattice
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  serialise — segment outputs, gaps copied verbatim by offset │
└──────────────────────────────────────────────────────────────┘
```

The ITN direction is the same picture with every cascade inverted:

```
spoken text → tokenise → CLASSIFY (grammars read backwards) → invert(grammar) → written text
```

`invert(A ∘ B) = invert(B) ∘ invert(A)`, so inverting a cascade reverses it and inverts each
stage. Weights are preserved, which is the whole point: one set of costs decides both
directions, and the two can never drift apart because there is only one grammar.

### Why weighted, and not just finite-state

Ambiguity is everywhere in normalisation and none of it is resolvable by structure alone.
`$1` fits both a singular and a plural currency path. `twenty five` and `twenty-five` are both
readable. `2015-03-14` is a date and also, if you squint, three numbers and two minus signs.
Every one of those is settled the same way: the tropical semiring makes a path's weight the
sum of its arcs, and the answer is the cheapest path. Nothing else in the pipeline arbitrates.

### Why one grammar rather than two

A hand-written inverse grammar is a second thing to keep in sync, and it will not stay in
sync. Deriving ITN by inversion means every fix to the verbaliser is a fix to the inverse, and
every weight that expresses a preference for TTS output expresses the matching preference for
what ITN will accept. Ordinal suffix agreement is the clearest case: the machine that *strips*
`st`/`nd`/`rd`/`th` while remembering the last digit becomes, inverted, the machine that
*chooses* the agreeing suffix. `21st` and `11th` come out right without a lookup table.

---

## Building

| Tool | Version |
|---|---|
| C++ compiler | C++17 (g++ 7+, clang++ 5+) |
| Scalpel | checked out at `../Scalpel` |

No external libraries. No build system required, though there is a Makefile.

```bash
make          # sayso + all four test binaries
make test     # build and run the suite
```

Or by hand, in Scalpel's style:

```bash
g++ -std=c++17 -O2 -o sayso \
    sayso_main.cpp wfst.cpp symbols.cpp numbers.cpp money.cpp dates.cpp \
    text.cpp classify.cpp normalizer.cpp \
    ../Scalpel/tokenizer.cpp ../Scalpel/char_classes.cpp -I.
```

---

## Usage

```bash
echo 'It costs $25.10 on 3/14/2015.' | ./sayso normalize --direction tts
```
```
It costs twenty-five dollars and ten cents on march fourteenth twenty fifteen.
```

```bash
echo 'he finished twenty-first out of one hundred forty-three' | ./sayso normalize --direction itn
```
```
he finished 21st out of 143
```

`--explain` prints the CLASSIFY tagging before the output:

```bash
echo 'Born 1999-12-25, she paid £3.' | ./sayso normalize --direction tts --explain
```
```
 [Born] PLAIN -> [Born]
 [1999-12-25] DATE -> [december twenty-fifth nineteen ninety-nine]
 [,] PLAIN -> [,]
 [she] PLAIN -> [she]
 [paid] PLAIN -> [paid]
 [£3] MONEY -> [three pounds]
 [.] PLAIN -> [.]
Born december twenty-fifth nineteen ninety-nine, she paid three pounds.
```

### What each class covers

| Class | Written | Spoken |
|---|---|---|
| `CARDINAL` | `25`, `-42`, `1,234`, up to twelve digits; digit-by-digit past that | `twenty-five`, `minus forty-two` |
| `ORDINAL` | `1st`, `21st`, `11th`, `1000000th` | `first`, `twenty-first`, `one millionth` |
| `MONEY` | `$25`, `$25.10`, `$0.99`, `€40`, `£1.50`, `$2.5 million` | `twenty-five dollars and ten cents`, `fifty pence` |
| `DATE` | `3/14/2015`, `2015-03-14`, `March 14, 2015`, `14 March 2015` | `march fourteenth twenty fifteen` |

---

## The weighting scheme

Every weight lives in [`weights.h`](weights.h), under two rules.

**The canonical form of anything costs zero.** Every variant Sayso must *accept* but would
rather not *produce* carries a positive cost. Because inversion preserves weights, one number
governs both directions: the cost on a written thousands comma is simultaneously what lets TTS
read `1,234` and what makes ITN write `1234`.

**Costs are small inside a grammar and larger between them.** A class should not win a span by
accumulating cheap internal decisions.

| Weight | Cost | What it buys |
|---|---|---|
| `kCommaCost` | 0.75 | reads `1,234`, writes `1234` |
| `kSpacedCompoundPenalty` | 0.01 | reads `twenty five`, writes `twenty-five` |
| `kAltSignPenalty` | 0.5 | reads `negative`, writes `minus` |
| `kDigitByDigitPerDigit` | 0.5 | per digit; the fallback for `007` and 13-digit strings |
| `kPluralPreferenceMargin` | 0.01 | reads `one dollars`, writes `one dollar` |
| `kZeroDollarsPenalty` | 1.0 | reads `zero dollars and ninety-nine cents`, writes `$0.99` |
| `kZeroPadPenalty` | 0.3 | reads `03/14`, writes `3/14` |
| `kIso / kMonthName / kDayFirstFormatPenalty` | 0.5 / 0.6 / 0.7 | reads four date formats, writes `M/D/YYYY` |
| `kTwoThousandsPenalty` | 1.2 | `2005` is `two thousand five`, `1905` is `nineteen oh five` |
| `kCardinalYearPenalty` | 0.6 | `2015` is `twenty fifteen`, not `two thousand fifteen` |
| `kPlainPerChar` | 1.0 | per character of untouched text; this is what makes long spans win |
| class entry costs | 0.05 – 0.2 | `MONEY($25)` beats `PLAIN($) + CARDINAL(25)` |

Two idioms recur. **Priority union by margin**: where a specific branch and a general branch
both fit, a hair of weight on the general one makes the specific one win, without needing a
`difference` operation. **Canonicalisation by penalty**: several written forms map to one
spoken form, and the cheapest written form is the one ITN produces.

Where nondeterminism arises from inversion — `twenty five` against `twenty-five`, `$3` against
`$3.00` — it is always resolved this way. Sayso never determinises; the best path does the work.

---

## Accuracy

Run `cd eval && ./generate_slice && ./sayso_eval`. Numbers below are from the 500-example
synthetic slice (see the caveat under it).

**TTS verbalisation** (written → spoken)

| class | n | accuracy |
|---|---:|---:|
| CARDINAL | 125 | 100.0% |
| ORDINAL | 125 | 100.0% |
| MONEY | 125 | 100.0% |
| DATE | 125 | 100.0% |
| **all** | **500** | **100.0%** |

**ITN** (spoken → written), scored against the canonical written form

| class | n | Sayso | NeMo |
|---|---:|---:|---:|
| CARDINAL | 125 | 100.0% | — |
| ORDINAL | 125 | 100.0% | — |
| MONEY | 125 | 100.0% | — |
| DATE | 125 | 100.0% | — |
| **all** | **500** | **100.0%** | **—** |

**Throughput**, single-threaded, `-O2`, Apple M-series:

| | sentences | time | rate |
|---|---:|---:|---:|
| cold, every sentence novel | 1,000 | 3.94 s | 254/s |
| warm, span cache populated | 10,000 | 0.93 s | 10,773/s |

Ten thousand sentences take about forty seconds if every one is unlike anything seen
before, and about a second once the corpus has shown its vocabulary of numbers, amounts and
dates — which is what a real corpus does within the first few hundred sentences. The cold
figure is what it is because CLASSIFY asks every grammar about every span; FIRST/LAST-set
prefilters answer most of those questions without composing anything, and cut that path by
about 5x.

### What that slice is, and is not

`eval/data/google_tn_slice.tsv` is meant to hold a 500-example slice of the Google text
normalisation dataset. It is not distributed here. When it is absent,
`eval/generate_slice.cpp` writes a synthetic slice in its place, whose spoken side comes from
a small reference verbaliser written as ordinary arithmetic and string concatenation — *not*
from the transducers under test, which would score 100% and measure nothing.

So the numbers above measure agreement between two independent implementations of the same
conventions. They do not measure agreement with the Google TN dataset's conventions. Place the
real slice (`eval/README.md` says how) for a number that does.

### NeMo comparison

NeMo is not installed in this environment, so the NeMo column above is empty. The blocker is
`pynini`, the OpenFst binding NeMo's text-processing package is built on. Three attempts, with
the exact failures recorded in [`eval/README.md`](eval/README.md):

1. `pip install nemo_text_processing` — the `pynini` wheel build fails with
   `fatal error: 'fst/util.h' file not found`: no OpenFst headers on the system.
2. With `brew install openfst` (1.8.4) and `CPPFLAGS=-I/opt/homebrew/include`, the build gets
   past that and fails with `error: no member named 'StringJoin' in namespace 'fst'; did you
   mean 'StrJoin'?`. pynini 2.1.6 targets OpenFst 1.8.3, which renamed that helper in 1.8.4.
3. `pynini==2.1.7` against OpenFst 1.8.4 compiles, but its extension modules take well over an
   hour on this machine and had not finished when this was written.

The upstream-recommended path is `conda install -c conda-forge pynini`, which ships a prebuilt
wheel against a matching OpenFst; there is no conda on this machine.

[`eval/nemo_compare.py`](eval/nemo_compare.py) is written and ready. It reads the same slice,
scores both directions the same way, and prints a table in the same shape as `./sayso_eval`.
On a machine with a working pynini it is one command:

```bash
./.venv/bin/python nemo_compare.py
```


---

## Testing

```bash
make test
```

```
== wfst_tests            engine: semiring, rational operations, composition, best path
== number_tests          cardinals and ordinals, with round-trip properties
== money_date_tests      currency and dates, with round-trip properties
== pipeline_tests        Scalpel bridge, CLASSIFY, end-to-end normalisation
```

Each grammar is held to a round-trip property over seeded random input:
`itn(tts(n)) == n` for integers and ordinals, for amounts in cents, for dates; and
`tts(itn(s)) == s` for well-formed spoken forms, which is the property that pins the
*canonical* spoken form rather than merely a readable one. The property harness
(`property.h`) is about eighty lines — a seeded xorshift generator and a shrinker — so the
test build has the same zero-dependency story as the library.

---

## Project structure

```
sayso/
├── wfst.h/.cpp             # tropical-semiring WFSTs; union, concat, closure,
│                           #   projection, inversion, composition, best path, Cascade
├── symbols.h/.cpp          # shared symbol table; written chars and spoken words
├── weights.h               # the entire weighting scheme, in one file
├── numbers.h/.cpp          # cardinal and ordinal grammars, hyphenator
├── money.h/.cpp            # currency grammar
├── dates.h/.cpp            # canonical date grammar + written-format normalisers
├── classify.h/.cpp         # CLASSIFY: the token-span lattice
├── text.h/.cpp             # Scalpel bridge and offset repair
├── normalizer.h/.cpp       # the pipeline, end to end
├── property.h              # seeded property-test harness with shrinking
├── sayso_main.cpp          # CLI
├── *_tests.cpp             # four test binaries
├── docs/DECISIONS.md       # every design call, with its alternative and rationale
└── eval/                   # evaluation harness; the only place dev deps may live
```

---

## Next steps

- **A neural reranker.** The classifier has no context beyond the span itself, and some
  ambiguity genuinely needs it. `I second the motion` becomes `I 2nd the motion`, because
  nothing in a finite-state grammar can tell the ordinal from the verb. The natural shape is
  to have CLASSIFY emit an n-best lattice rather than a single best path, and rerank it with a
  contextual model — which keeps the grammars as the source of what is *possible* and lets the
  model choose only among things that already are.
- **More semiotic classes.** Times, measures, fractions, telephone numbers, addresses,
  electronic (URLs, emails), decimals as a class in their own right. Each is another
  table-plus-combinators file next to `money.cpp`; the engine and CLASSIFY need nothing new.
- **Optional written-format output.** ITN currently canonicalises hard — every spoken date
  becomes `M/D/YYYY`. Making the target format a parameter is a matter of moving the format
  penalties into a runtime table.
- **Determinisation and minimisation.** Sayso resolves ambiguity by best path and never needs
  a deterministic machine, but weighted determinisation would shrink the date grammar and
  speed up cold-cache classification.
- **Languages other than English.** The engine is language-neutral; the grammars are tables of
  English words. A second language is a second set of tables.
