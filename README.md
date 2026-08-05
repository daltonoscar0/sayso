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

Eight semiotic classes: `CARDINAL`, `ORDINAL`, `DECIMAL`, `FRACTION`, `MONEY`, `MEASURE`,
`DATE`, `TIME`. Everything else passes through byte for byte.

```
Input   →  "The 2.5 km run starts at 6:45 am; 3/4 of the field paid £12.50."
TTS     →  "The two point five kilometers run starts at six forty-five a m;
            three quarters of the field paid twelve pounds and fifty pence."
```

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
│                                                              │
│  Each grammar is run once per span *start*, not once per     │
│  span: every arc leaving state i comes from one composition. │
└────────────────────┬─────────────────────────────────────────┘
                     │  spans: one of eight classes, or PLAIN
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  per-class grammar — a composition of WFSTs                  │
│                                                              │
│    CARDINAL  cardinal ∘ hyphenator                           │
│    ORDINAL   suffix-stripper ∘ cardinal ∘ (ordinaliser ∘ h.) │
│    DECIMAL   sign · cardinal · point · digits, hyphenated    │
│    FRACTION  sign · numerator · denominator-table, hyph.     │
│    MONEY     currency (with its numeric fields hyphenated)   │
│    MEASURE   (cardinal | decimal) · unit-table, hyphenated   │
│    DATE      written-format-rewrites ∘ canonical-date        │
│    TIME      hour · minute · meridiem, hyphenated            │
│                                                              │
│  Small compositions are done once at build time; the large   │
│  ones are a Cascade, composed against the input instead.     │
│                                                              │
│  Every one of these is written so that a shared prefix is    │
│  concatenated onto a union rather than into each of its      │
│  branches: A·(B ∪ C), never A·B ∪ A·C. See "Keeping the      │
│  grammars small" below — it is worth more than it sounds.    │
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
make          # sayso + all six test binaries
make test     # build and run the suite
```

Or by hand, in Scalpel's style:

```bash
g++ -std=c++17 -O2 -o sayso \
    sayso_main.cpp wfst.cpp symbols.cpp numbers.cpp money.cpp dates.cpp \
    decimals.cpp fractions.cpp times.cpp measures.cpp \
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
| `DECIMAL` | `3.14`, `0.5`, `.5`, `-2.75` | `three point one four`, `zero point five` |
| `FRACTION` | `1/2`, `3/4`, `15/16`, `3/64`, `2 1/2` | `one half`, `three quarters`, `two and a half` |
| `MONEY` | `$25`, `$25.10`, `$0.99`, `€40`, `£1.50`, `$2.5 million` | `twenty-five dollars and ten cents`, `fifty pence` |
| `MEASURE` | `5 km`, `3.5 kg`, `100 mph`, `50%`, `25°C`, `500 MB` | `five kilometers`, `fifty percent`, `twenty-five degrees celsius` |
| `DATE` | `3/14/2015`, `2015-03-14`, `March 14, 2015`, `14 March 2015` | `march fourteenth twenty fifteen` |
| `TIME` | `3:30`, `03:30`, `3:05`, `3:00`, `3:30 pm`, `3:30 p.m.`, `19:30` | `three thirty`, `three oh five`, `three o'clock`, `three thirty p m` |

Three of these stop short of where a table could have gone on, and each time for the same
reason — the classifier has no context, so a grammar that reads more also mis-reads more.
Fraction denominators stop at the ones with single-word ordinals. Times have no seconds
field, because English has no settled way of saying one for the grammar to canonicalise to.
And the measure table deliberately omits every unit whose written form is also an ordinary
English word: `in`, `s`, `h`, `t`, `a`. Without that, `3 in the morning` is three inches.

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
| `kZeroPadPenalty` | 0.3 | reads `03/14`, writes `3/14`; also `03:30` |
| `kIso / kMonthName / kDayFirstFormatPenalty` | 0.5 / 0.6 / 0.7 | reads four date formats, writes `M/D/YYYY` |
| `kTwoThousandsPenalty` | 1.2 | `2005` is `two thousand five`, `1905` is `nineteen oh five` |
| `kCardinalYearPenalty` | 0.6 | `2015` is `twenty fifteen`, not `two thousand fifteen` |
| `kUnspokenLeadingZeroPenalty` | 0.4 | reads `0.5` as `point five` |
| `kUnwrittenLeadingZeroPenalty` | 0.5 | reads `.5`; dearer, so `point five` writes `0.5` |
| `kArticleNumeratorPenalty` | 0.25 | `one half` alone, `and a half` after a whole number |
| `kAltDenominatorPenalty` | 0.3 | reads `fourths`, writes `quarters` |
| `kBareHourPenalty` | 0.35 | `3:00` is `three o'clock`, `3:00 pm` is `three p m` |
| `kDottedMeridiemPenalty` | 0.4 | reads `p.m.`, writes `pm` |
| `kMeridiemSpacingPenalty` | 0.3 | reads `3:30pm`, writes `3:30 pm` |
| `kMeasureSpacingPenalty` | 0.3 | reads `5km` and `50 %`, writes `5 km` and `50%` |
| `kAltUnitSpellingPenalty` | 0.3 | reads `lbs`, writes `lb` |
| `kFoldedCasePenalty` | 0.1 | reads `°c` and `mhz`, writes `°C` and `MHz` |
| `kCurrencyHomographPenalty` | 0.25 | `three pounds` is sterling, `5 lb` is weight |
| `kPlainPerChar` | 1.0 | per character of untouched text; this is what makes long spans win |
| class entry costs | 0.05 – 0.2 | `MONEY($25)` beats `PLAIN($) + CARDINAL(25)` |

Three of the newer entries are the same idiom seen from an odd angle. `kBareHourPenalty` is
one weight leaning opposite ways in two branches, because whether a round hour is spoken
depends on whether a meridiem follows it — and `kArticleNumeratorPenalty` does the same for
`one half` against `and a half`. `kFoldedCasePenalty` exists because CLASSIFY folds a span's
case before any grammar sees it, so for a unit like `°C` the folded spelling is the only one
TTS can ever match and the canonical one the only one ITN will ever write; the weight is what
makes the second of those true. And `kCurrencyHomographPenalty` is an ambiguity the grammars
cannot settle at all — `three pounds` fits MONEY and MEASURE equally, so the measure reading is
priced to lose the spoken form while keeping `5 lb`, which no currency writes.

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
| CARDINAL | 63 | 100.0% | 100.0% | 19.0% | 14.3% |
| ORDINAL | 63 | 100.0% | 100.0% | 28.6% | 28.6% |
| DECIMAL | 62 | 100.0% | 100.0% | 0.0% | 4.8% |
| FRACTION | 62 | 100.0% | 100.0% | 17.7% | 0.0% |
| MONEY | 63 | 100.0% | 100.0% | 0.0% | 1.6% |
| MEASURE | 62 | 100.0% | 100.0% | 0.0% | 11.3% |
| DATE | 63 | 100.0% | 100.0% | 1.6% | 0.0% |
| TIME | 62 | 100.0% | 100.0% | 9.7% | 0.0% |
| **all** | **500** | **100.0%** | **100.0%** | **9.6%** | **7.6%** |

**Those NeMo numbers are not a quality measurement and should not be read as one.** The slice's
reference spoken forms follow Sayso's conventions, so Sayso scores 100% on it close to by
construction, and NeMo is penalised for every place it makes a different but equally correct
choice — `thirty fourth` for `thirty-fourth`, `nine hundred and forty two` for
`nine hundred forty-two`. Its ITN is also fed hyphenated text, which no speech recogniser
produces and its grammars do not expect.

### Relaxed scoring — hyphens, "and", "o"/"oh" and case folded away, ITN input de-hyphenated

| class | n | Sayso TTS | Sayso ITN | NeMo TTS | NeMo ITN |
|---|---:|---:|---:|---:|---:|
| CARDINAL | 63 | 100.0% | 100.0% | 49.2% | 85.7% |
| ORDINAL | 63 | 100.0% | 100.0% | 100.0% | 100.0% |
| DECIMAL | 62 | 100.0% | 100.0% | 100.0% | 100.0% |
| FRACTION | 62 | 100.0% | 100.0% | 87.1% | 0.0% |
| MONEY | 63 | 100.0% | 100.0% | 100.0% | 69.8% |
| MEASURE | 62 | 100.0% | 100.0% | 83.9% | 64.5% |
| DATE | 63 | 100.0% | 100.0% | 39.7% | 0.0% |
| TIME | 62 | 100.0% | 100.0% | 41.9% | 0.0% |
| **all** | **500** | **100.0%** | **100.0%** | **75.2%** | **52.6%** |

NeMo scoring 100% on ORDINAL, DECIMAL and MONEY-TTS is the useful signal here: where the two
systems share a convention, the harness is fair and NeMo gets everything right. What remains
splits into three kinds of thing, and only the third is an error.

**Differences of convention**, which the folding does not reach:

- **TIME TTS** — NeMo writes `five forty nine PM`, one token, where the slice has `p m`, two.
  The relaxed mode folds case, so this is not a capitalisation difference; it is a real
  disagreement about whether the meridiem is one spoken word or two, and neither is wrong.
- **FRACTION TTS** — NeMo says `eleven one hundredths` for `11/100` where the slice has
  `eleven hundredths`.
- **CARDINAL TTS** — NeMo reads a bare `61795` digit by digit. On four-digit numbers the two
  systems now agree: Sayso reads `1880` as `eighteen eighty` too, which it did not before the
  real slice was scored.
- **DATE TTS** — NeMo says `the second of december nineteen fifty eight`, a British
  day-first reading. Correct English, different from the slice's reference.

**Things NeMo's inverse normaliser does not attempt.** Every 0.0% in the ITN column is one of
these, not a wrong answer:

- **DATE ITN** — writes `october 1 1975`; it does not reassemble a spoken date into a numeric
  written form at all.
- **TIME ITN** — writes `1102` for `eleven oh two`, reading the words as a cardinal rather
  than a clock time.
- **FRACTION ITN** — leaves `fifty-two tenths` untouched.
- **MONEY ITN** — writes `54942 pounds` rather than `£54942`; it does not map a currency word
  back to its symbol.

**Coverage gaps**, where NeMo does attempt the class and comes up short: it leaves `MB`
unexpanded in `797 MB`, and turns `three thousand nine hundred sixty-nine centimeters` into
`3900 sixty-nine centimeters`.

The fair summary is unchanged from when this comparison covered four classes: Sayso
canonicalises harder in the ITN direction — numeric dates and times, currency symbols, no
thousands separators — because that is what its weighting scheme is for, and NeMo covers far
more of English than eight semiotic classes.

### Throughput

Single-threaded, `-O2`, Apple M-series, machine otherwise idle. Both rows are medians of four
runs, alternating between the two builds over the *same* 500-sentence slice, because absolute
figures on a laptop move by a factor of two with thermal state and background load — measure
them against each other in one sitting or not at all:

| | classes | cold, every sentence novel | warm, span cache populated |
|---|---:|---:|---:|
| four-class build (previous revision) | 4 | 257/s | 60,700/s |
| this build | **8** | **276/s** | **74,100/s** |

Twice the semiotic classes, and still faster: about 7% on the cold path and 22% on the warm
one.

Two things had already got the classifier most of the way there. **FIRST- and LAST-set
prefilters** answer most of its questions from the first and last symbol of a span, without
composing anything. And **CLASSIFY runs each grammar once per span start** rather than once
per span: composing against an all-final input chain and reading the best path off every input
position (`apply_prefixes`) answers for all of a start's spans at once, instead of re-walking
their shared prefixes.

That second one is why the span caps can be generous. Cost grows with the longest span a
grammar can actually match, not with the cap, so raising the spoken cap from 16 to 24
tokens — enough to hold `nine hundred ninety nine billion ... dollars and ninety nine cents`
unhyphenated, which is what a speech recogniser hands you — cost nothing measurable.

Two more changes came in with the new classes, and between them they are why the count could
double while the clock went the other way.

#### Keeping the grammars small

Concatenation distributes over union: `A·B ∪ A·C` and `A·(B ∪ C)` accept the same strings with
the same weights. Written the first way, a grammar holds one copy of `A` per branch. Written
the second way, it holds one. When `A` is the cardinal machine and the branches are thirty-odd
measure units, or the seven tails a currency amount can take, that is not a detail — it is
most of what the grammar costs to compose against a span.

The measure grammar was written the first way to begin with. The currency grammar had been
carrying eighteen copies of the cardinal machine since it was written, and was by a wide
margin the most expensive thing in the library. Benching the old and new sources on the same
machine, microseconds per span:

| MONEY | TTS | ITN |
|---|---:|---:|
| eighteen copies of the cardinal machine | 102 | 1,615 |
| factored | **34** | **571** |

About threefold in both directions, on what was half the cost of a cold sentence.

Where every grammar stands now — same conditions, variance under two percent:

| | MONEY | DATE | MEASURE | FRACTION | DECIMAL | TIME | ORDINAL | CARDINAL |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| TTS | 34 | 94 | 154 | 169 | 79 | 31 | 41 | 83 |
| ITN | 571 | 864 | 203 | 181 | 79 | 73 | 31 | 86 |

DATE, ORDINAL and CARDINAL were benched against both builds and did not move, which is the
control: nothing was factored out of them, and the year branch the cardinal grammar gained
costs nothing measurable.

Two smaller identities do the same work elsewhere. An optional sign is concatenated onto the
grammar rather than distributed over three copies of it. And `reweight` may be moved inside a
concatenation — it prepends a single epsilon arc, and the tropical semiring sums along a path,
so the cost lands on the same total wherever it sits — which is what lets a penalised branch
share its prefix with an unpenalised one instead of duplicating it.

The remaining change is in the classifier: the span cache now holds every class's answers
under one key rather than one entry per class. Building the span-text key once per span start
instead of once per (span start, class) is most of what the warm path costs once the grammars
are all cache hits, and it is worth about 1.6× on that path.

DATE is now the most expensive grammar in the library and is not helped by any of this — its
cost is its size, four written formats composed against a canonical machine, and factoring has
nothing to take out. It is the obvious place to look next.

### On the real Google TN slice

Everything above is the synthetic slice. This is 500 sentences of Wikipedia from
`output-00099-of-00100` — a held-out *test* shard, per the dataset's own split — normalised by
Google's Kestrel and used as the reference. It is the only number here that measures agreement
with something other than Sayso's own conventions.

**TTS 62.8%. ITN 17.0%**, relaxed scoring.

| class | n | TTS | ITN |
|---|---:|---:|---:|
| CARDINAL | 96 | 71.9% | 45.8% |
| DATE | 378 | 59.3% | 6.6% |
| MEASURE | 8 | 87.5% | 62.5% |
| ORDINAL | 11 | 100.0% | 90.9% |
| DECIMAL | 4 | 75.0% | 25.0% |
| MONEY / FRACTION / TIME | 1 each | 0.0% | 0.0% |
| **all** | **500** | **62.8%** | **17.0%** |

**Read the ITN column as close to meaningless.** The corpus has no canonical-written column,
so ITN is scored against the written one — and that column is natural prose, where "scoring
three goals" leaves "three" a word. Nothing in the spoken input says which numbers Wikipedia
chose to spell out, so this measures conformance to a house style, not normalisation. It is
the caveat `eval/README.md` already raises, in its sharpest form.

The TTS column is real, and it moved. It was **40.0%** before the year reading was licensed
for a bare span; that one change was 147 of 300 mismatches. Categorising all 186 that remain:

| count | cause |
|---:|---|
| 134 | **day-first spoken dates** — Kestrel says "the twenty-eighth of february twenty sixteen" |
| 10 | decades — `1970s`, which Sayso does not implement |
| 10 | abbreviations with no table entry — `mr`→mister, `ltd`→limited, `ha`→hectares |
| 9 | numeric ranges — `5 - 0` is "five **to** zero" |
| 8 | abbreviated month names — `Jun 29, 2011` |
| 6 | Roman numerals — `World War II` is "world war two" |
| 5 | British→American spelling — `harbour`→`harbor`, `organisation`→`organization` |
| 10 | everything else |

Three things are worth saying plainly about that table.

The first row is a **declined** design decision, not a defect. Kestrel preserves written order
in speech; Sayso canonicalises every written format to one spoken form, which is what makes
the ITN direction canonicalise at all and is pinned by both a test and a round-trip property.
Matching Kestrel would be worth roughly another 27 points and would cost that property.
`docs/DECISIONS.md` records the trade.

The last row is **not normalisation at all**. Kestrel Americanises spelling on its way past;
that is a property of the reference, not a thing a text normaliser should reproduce.

And the middle rows are the honest to-do list — decades, Roman numerals, ranges and
abbreviation tables are real gaps, each small, each a table-plus-combinators file.

The harness needed one change before any of this was measurable. Kestrel marks punctuation
`sil` — written but not spoken — so the reference spoken column has none, while Sayso copies
punctuation through verbatim, which is what a TTS front end must do. Compared naively, every
class scored 0.0% with almost every sentence failing on a comma. Relaxed scoring now drops
standalone punctuation from both sides.

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
are why the NeMo comparison has to be read with the care set out above.

The real slice is what answers that, and "On the real Google TN slice" above reports it: the
100% becomes 62.8%, and the gap is almost entirely one declined design decision plus four
small missing classes. Any claim made from the synthetic numbers alone should be read against
that section.

---

## Testing

```bash
make test
```

```
== wfst_tests            engine: semiring, rational operations, composition, best path
== number_tests          cardinals and ordinals, with round-trip properties
== money_date_tests      currency and dates, with round-trip properties
== quantity_tests        decimals, fractions and measures, with round-trip properties
== clock_tests           times, with a round-trip property over the whole day
== pipeline_tests        Scalpel bridge, CLASSIFY, end-to-end normalisation
```

128 tests. The grammar suites test a cascade on its own, so a case there says what a machine
can read and write; where two classes want the same span, the arbitration is the classifier's
and lives in `pipeline_tests` instead.

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
├── decimals.h/.cpp         # decimal grammar; the amount measures embed
├── fractions.h/.cpp        # fraction grammar, denominator table, mixed numbers
├── money.h/.cpp            # currency grammar
├── measures.h/.cpp         # measure grammar and the unit table
├── dates.h/.cpp            # canonical date grammar + written-format normalisers
├── times.h/.cpp            # clock-time grammar
├── classify.h/.cpp         # CLASSIFY: the token-span lattice
├── text.h/.cpp             # Scalpel bridge and offset repair
├── normalizer.h/.cpp       # the pipeline, end to end
├── property.h              # seeded property-test harness with shrinking
├── sayso_main.cpp          # CLI
├── *_tests.cpp             # six test binaries
├── docs/DECISIONS.md       # every design call, with its alternative and rationale
└── eval/
    ├── eval_main.cpp         # accuracy both directions, per class, throughput
    ├── generate_slice.cpp    # synthetic slice, from an independent verbaliser
    ├── convert_google_tn.py  # raw Google TN shard -> slice format
    └── nemo_compare.py       # the same slice scored by NeMo; dev deps live here
```

---

## Next steps

- **A neural reranker.** The classifier has no context beyond the span itself, and some
  ambiguity genuinely needs it. `I second the motion` becomes `I 2nd the motion`, because
  nothing in a finite-state grammar can tell the ordinal from the verb. The natural shape is
  to have CLASSIFY emit an n-best lattice rather than a single best path, and rerank it with a
  contextual model — which keeps the grammars as the source of what is *possible* and lets the
  model choose only among things that already are.
- **More semiotic classes.** `DECIMAL`, `FRACTION`, `MEASURE` and `TIME` are in, and adding
  them needed nothing new from the engine or CLASSIFY, which was the claim. What is left is
  the classes whose written forms are not built out of numbers: telephone numbers, addresses,
  electronic (URLs, emails), `LETTERS` and `VERBATIM`. Those are a different shape — mostly
  character-level spelling-out rather than field structure — and the last two in particular
  are what a real corpus is full of: they were the largest group the converter dropped.
- **The gaps the real slice named.** In order of how often they cost a sentence: decades
  (`1970s`), abbreviations the measure table has no entry for (`ltd`, `ha`, `mr`), numeric
  ranges (`5 - 0` as "five to zero"), abbreviated month names, and Roman numerals. None is
  more than ten sentences in five hundred, and each is another small table.
- **Optional written-format output.** ITN currently canonicalises hard — every spoken date
  becomes `M/D/YYYY`. Making the target format a parameter is a matter of moving the format
  penalties into a runtime table.
- **A faster cold path still.** Prefix composition and then the factoring described above have
  taken the remaining cost down to real composition work rather than repetition, and it is now
  concentrated in one place: DATE is roughly half of what a cold sentence costs, and unlike
  the others it has no duplicated sub-machine to take out. Weighted determinisation of it, or
  caching per token rather than per span text, are the next places to look.
- **Determinisation and minimisation.** Sayso resolves ambiguity by best path and never needs
  a deterministic machine, but weighted determinisation would shrink the date grammar.
- **Languages other than English.** The engine is language-neutral; the grammars are tables of
  English words. A second language is a second set of tables.
