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

Run `cd eval && ./generate_slice && ./sayso_eval` (add `--relaxed` for the second table).
Numbers are from the 500-example synthetic slice; read the caveat below before drawing
conclusions from the NeMo columns.

### Strict scoring — exact match

| class | n | Sayso TTS | Sayso ITN | NeMo TTS | NeMo ITN |
|---|---:|---:|---:|---:|---:|
| CARDINAL | 125 | 100.0% | 100.0% | 16.0% | 9.6% |
| ORDINAL | 125 | 100.0% | 100.0% | 43.2% | 43.2% |
| MONEY | 125 | 100.0% | 100.0% | 0.8% | 3.2% |
| DATE | 125 | 100.0% | 100.0% | 12.8% | 0.0% |
| **all** | **500** | **100.0%** | **100.0%** | **18.2%** | **14.0%** |

**Those NeMo numbers are not a quality measurement and should not be read as one.** The slice's
reference spoken forms follow Sayso's conventions, so Sayso scores 100% on it close to by
construction, and NeMo is penalised for every place it makes a different but equally correct
choice — `thirty fourth` for `thirty-fourth`, `nine hundred and forty two` for
`nine hundred forty-two`. Its ITN is also fed hyphenated text, which no speech recogniser
produces and its grammars do not expect.

### Relaxed scoring — hyphens and "and" folded away, ITN input de-hyphenated

| class | n | Sayso TTS | Sayso ITN | NeMo TTS | NeMo ITN |
|---|---:|---:|---:|---:|---:|
| CARDINAL | 125 | 100.0% | 100.0% | 40.8% | 85.6% |
| ORDINAL | 125 | 100.0% | 100.0% | 100.0% | 100.0% |
| MONEY | 125 | 100.0% | 100.0% | 100.0% | 60.8% |
| DATE | 125 | 100.0% | 100.0% | 53.6% | 0.0% |
| **all** | **500** | **100.0%** | **100.0%** | **73.6%** | **61.6%** |

NeMo scoring 100% on ORDINAL in both directions is the useful signal here: where the two
systems share a convention, the harness is fair and NeMo gets everything right. The remaining
gaps are, on inspection of `nemo_results.tsv`, mostly still differences of target rather than
errors:

- **CARDINAL TTS** — NeMo reads a bare `61795` digit by digit and `1880` as a year
  (`eighteen eighty`). Sayso reads both as cardinals. Neither is wrong about a bare number;
  they guess differently.
- **DATE TTS** — NeMo says `the second of december nineteen fifty eight`, a British
  day-first reading. Correct English, different from the slice's reference.
- **DATE ITN** — NeMo writes `october 1 1975`. It does not reassemble a spoken date into a
  numeric written form at all, which is the thing Sayso's format weights exist to do.
- **MONEY ITN** — NeMo writes `54942 pounds` rather than `£54942`; it does not map a
  currency word back to its symbol.

The fair summary is that Sayso canonicalises harder in the ITN direction — numeric dates,
currency symbols, no thousands separators — because that is what its weighting scheme is for,
and that NeMo covers far more of English than four semiotic classes.

### Throughput

Single-threaded, `-O2`, Apple M-series, machine otherwise idle:

| | sentences | time | rate |
|---|---:|---:|---:|
| cold, every sentence novel | 1,000 | 16.2 s | 61/s |
| warm, span cache populated | 10,000 | 2.6 s | 3,900/s |

Ten thousand sentences take under three seconds once the corpus has shown its vocabulary of
numbers, amounts and dates — which a real corpus does within its first few hundred sentences —
and about two and a half minutes if every single one is unlike anything seen before.

The cold figure is dominated by CLASSIFY asking every grammar about every span. FIRST- and
LAST-set prefilters answer most of those questions without composing anything. The remaining
cost is genuine long spans of number words, and it is sensitive to the spoken span cap:
raising that cap from 12 to 16 tokens — needed so that unhyphenated ASR output like
`eight hundred forty six million one hundred sixty eight thousand nine hundred forty nine`
is classified as one number rather than three — cost about fourfold. Fixing that properly
means composing once per span *start* and reading every prefix off the result, rather than
once per span; see next steps.

### Reproducing the NeMo columns

`eval/nemo_compare.py` scores NVIDIA NeMo on the same slice, in the same shape, and writes
per-example outputs to `nemo_results.tsv`. Getting NeMo installed took three attempts — it
pins `pynini==2.1.6.post1`, which needs OpenFst **1.8.3** specifically, and PyPI has no arm64
wheel for it. The route that worked and the two that did not are written up in
[`eval/README.md`](eval/README.md).

### What that slice is, and is not

`eval/data/google_tn_slice.tsv` is meant to hold a 500-example slice of the Google text
normalisation dataset. It is not distributed here. When it is absent,
`eval/generate_slice.cpp` writes a synthetic slice in its place, whose spoken side comes from
a small reference verbaliser written as ordinary arithmetic and string concatenation — *not*
from the transducers under test, which would score 100% and measure nothing.

So Sayso's numbers measure agreement between two independent implementations of the same
conventions. They do not measure agreement with the Google TN dataset's conventions, and they
are why the NeMo comparison has to be read with the care set out above. Place the real slice
(`eval/README.md` says how) for numbers that mean more.

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
- **One composition per span start, not per span.** CLASSIFY currently asks each grammar about
  each span separately, so a sentence containing a long spelled-out numeral re-composes its
  prefixes over and over. Composing once for the longest span and reading the best path off
  every intermediate input position would collapse that to one composition per (start, class),
  and is the fix for the cold-path figure above.
- **Spelled-out billions in ITN.** The spoken span cap is sixteen tokens, which covers every
  unhyphenated cardinal below a billion; larger ones are classified in pieces. Raising the cap
  is a one-line change once the composition above makes it affordable.
- **Determinisation and minimisation.** Sayso resolves ambiguity by best path and never needs
  a deterministic machine, but weighted determinisation would shrink the date grammar and
  speed up cold-cache classification.
- **Languages other than English.** The engine is language-neutral; the grammars are tables of
  English words. A second language is a second set of tables.
