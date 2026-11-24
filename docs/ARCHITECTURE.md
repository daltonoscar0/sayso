# How sayso works

Part of the [sayso](../README.md) documentation.

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
│  CLASSIFY  : a weighted FST over token boundaries            │
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
                     │  spans: one of seventeen classes, or PLAIN
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  per-class grammar: a composition of WFSTs                   │
│                                                              │
│    CARDINAL  (cardinal | year | roman) ∘ hyphenator          │
│    ORDINAL   suffix-stripper ∘ cardinal ∘ (ordinaliser ∘ h.) │
│    DECIMAL   sign · cardinal · point · digits, hyphenated    │
│    FRACTION  sign · numerator · denominator-table, hyph.     │
│    MONEY     currency (with its numeric fields hyphenated)   │
│    MEASURE   (cardinal | decimal) · unit-table, hyphenated   │
│    DATE      written-format-rewrites ∘ canonical-date        │
│    TIME      hour · minute · meridiem, hyphenated            │
│    DECADE    century-pair · pluralised-tens, hyphenated      │
│    RANGE     number · hyphen→"to" · number, hyphenated       │
│    ABBREV    a lexical table            ── speaking only     │
│    LETTERS   dotted initials, acronym table ─ speaking only  │
│    TELEPHONE digits-hyphen-digits ×4    ── speaking only     │
│                                                              │
│  and the four a voice needs, each of which reads its own     │
│  context because a grammar cannot see outside its span:      │
│                                                              │
│    ADDRESS   [number] [dir] Name · suffix-table ── speak only│
│    NAME      title-table · Name [· suffix]      ── speak only│
│    STATE     Name · "," · code-table [· zip]    ── speak only│
│    PHONE     [+1] ddd · ddd · dddd | vanity     ── speak only│
│                                                              │
│  Small compositions are done once at build time; the large   │
│  ones are a Cascade, composed against the input instead.     │
│                                                              │
│  Every one of these is written so that a shared prefix is    │
│  concatenated onto a union rather than into each of its      │
│  branches: A·(B ∪ C), never A·B ∪ A·C. See "Keeping the      │
│  grammars small" below; it is worth more than it sounds,     │
│  and ADDRESS cost a factor of four by forgetting it.         │
└────────────────────┬─────────────────────────────────────────┘
                     │  best path through the composed lattice
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  serialise: segment outputs, gaps copied verbatim by offset  │
└────────────────────┬─────────────────────────────────────────┘
                     │  text, and the readings that lost
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  report: the pipeline contract's StageResult (--json)        │
│    decisions[]  span, surface, result, kind, rule, score     │
│    alternatives[]  what each decision beat, with its weight  │
│    meta  pron_hints for the words G2P gets wrong, name_spans │
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

### Reading the abbreviation both ways

`St` is `saint` 3,205 times in the corpus `ABBREV` was built from and `street` 156 times. `Dr`
is `doctor` 1,302 times and a drive not once. A table has to pick, and picking is wrong every
time the token is the other one. This is the ambiguity a conversational voice trips over
hardest, because addresses and names are most of what it reads.

No weight can settle it either, because the evidence is not in the token. It is in the tokens
either side, and a Sayso grammar sees only its own span.

So the fix is to make the span bigger. `ADDRESS` claims the street name in front of the
suffix, and the house number in front of that; `NAME` claims the surname behind the title.
That puts the context *inside* the span, where a grammar can read it, and leaves the
classifier's lattice to price the two readings against each other exactly as it prices `MONEY`
against `CARDINAL`.

Most of the ambiguity then disappears before any weight is consulted, because the two classes
want different word orders:

| written | parses as | because |
|---|---|---|
| `Dr. Lee` | `NAME` → "Doctor Lee" | an abbreviation *before* a capitalised name is a title |
| `Lee Dr` | `ADDRESS` → "Lee Drive" | an abbreviation *after* one is a street suffix |
| `St. Helens` | `NAME` → "Saint Helens" | same rule, same direction |
| `Wall St` | `ADDRESS` → "Wall Street" | same rule, other direction |

`Dr. Lee` has no parse in the address grammar and `Lee Dr` has none in the name grammar. Where
both do parse, one hair of weight prefers the title. Two structural constraints keep the
classes out of ordinary prose: a street name and a surname must be **capitalised**, and an
ambiguous suffix with no number in front of it pays for the privilege.

The same shape settles the postal codes. `AR` is Arkansas in `Little Rock, AR` and the letters
A R everywhere else, and every two-letter code is a word somewhere: `IN`, `OR`, `HI`, `ME`,
`OK` and `PA` are all states and all something commoner. `STATE` demands the one shape only an
address has: a capitalised place name, a comma, then the code. A bare `AR` has no parse at all.

### Telling the next stage what it cannot know

Expanding `AR` to `Arkansas` hands the grapheme-to-phoneme stage a word it will very likely
mispronounce, because `Arkansas` does not rhyme with `Kansas` and nothing in the spelling says
so. A verbaliser that replaces a token the next stage could have spelled out with one it will
get wrong has made the sentence worse.

So Sayso says what it knows. Any output word in a small curated table is reported in
`meta.pron_hints` with its pronunciation, and the next stage may take the hint or ignore it:

```json
"pron_hints": {"Reading": {"arpabet": "R EH1 D IH0 NG", "respell": "RED-ing"}}
```

Fifty-eight entries, every one of them read out of the [CMU Pronouncing
Dictionary](https://github.com/cmusphinx/cmudict) rather than written from memory, which was
worth doing, since three of the first drafted from memory were wrong. `Reading` is the entry
that makes the case: cmudict's default is the verb, `R IY1 D IH0 NG`, and the town is the
second variant. A model handed the bare word takes the first.

The list is deliberately short. Anything longer is a pronunciation dictionary, and shipping one
of those from the text normaliser would be doing the next stage's job with none of its
evidence. `NAME`, `ADDRESS` and `STATE` also report their spans in `meta.name_spans`, so a
heteronym disambiguator can skip the regions where it has nothing to go on.

### One-directional classes

Nine classes are read in both directions. Eight are not, and the reason is worth setting out,
because it is the one place the "one grammar, read both ways" story needs a footnote.

Expanding `St` to `saint` is normalisation. Contracting every `saint` back to `St` is not. The
asymmetry is semantic, and no weight can express it here: CLASSIFY prices `PLAIN` per
character, so a five-letter word costs 1.0 × 5 against a class entry cost of 0.05. A class
whose written side is drawn from ordinary English can *always* outbid leaving the word alone,
whichever way it is pointing. Making the penalty large enough to lose in one direction would
lose it in both.

So the registry carries a flag, and `classify()` skips those grammars when asked for ITN. This
is not a second grammar and not a weakening of the rule; each machine is still written once
and still inverted for everything else. It records that a *class* is one-directional.

Only classes with an English written side need it. `RANGE` joins them for a related reason:
`5-10` spoken as "five to ten" is normalisation, but "five to ten" written as `5-10` changes
the sentence's syntax rather than its spelling, "increased from five to ten percent" would
become "increased from 5-10 percent".

`ADDRESS`, `NAME` and `STATE` join for the first reason and not a new one: their written sides
are ordinary capitalised English. Contracting every spoken `road` back to `Rd` is the `saint`
problem again, and a caller who wants `Reading Rd` on the page wants a house style rather than
a normalisation. `PHONE` joins because the grouping is lost in speech: "five five five, one two
one two" is as much `5551212` as `555-1212`, and the class has no way to choose.

Roman numerals are the counter-example that proves the rule. They live *inside* `CARDINAL`
rather than in a class of their own, and there a weight does suffice: ITN's spoken "two" meets
`2` at a hair and `II` at a real penalty, so the digits win. A separate `ROMAN` class would
have had to beat `PLAIN` instead, and could not.

### Why one grammar rather than two

A hand-written inverse grammar is a second thing to keep in sync, and it will not stay in
sync. Deriving ITN by inversion means every fix to the verbaliser is a fix to the inverse, and
every weight that expresses a preference for TTS output expresses the matching preference for
what ITN will accept. Ordinal suffix agreement is the clearest case: the machine that *strips*
`st`/`nd`/`rd`/`th` while remembering the last digit becomes, inverted, the machine that
*chooses* the agreeing suffix. `21st` and `11th` come out right without a lookup table.

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
| `kIso / kMonthName / kDayFirstFormatPenalty` | 0.3 / 0.6 / 0.7 | reads four date formats, writes `M/D/YYYY` |
| `kTwoThousandsPenalty` | 1.2 | `2005` is `two thousand five`, `1905` is `nineteen oh five` |
| `kCardinalYearPenalty` | 0.6 | `2015` is `twenty fifteen`, not `two thousand fifteen` |
| `kUnspokenLeadingZeroPenalty` | 0.4 | reads `0.5` as `point five` |
| `kUnwrittenLeadingZeroPenalty` | 0.5 | reads `.5`; dearer, so `point five` writes `0.5` |
| `kArticleNumeratorPenalty` | 0.25 | `one half` alone, `and a half` after a whole number |
| `kAltDenominatorPenalty` | 0.05 | reads `fourths`, writes `quarters` |
| `kBareHourPenalty` | 0.35 | `3:00` is `three o'clock`, `3:00 pm` is `three p m` |
| `kDottedMeridiemPenalty` | 0.4 | reads `p.m.`, writes `pm` |
| `kMeridiemSpacingPenalty` | 0.3 | reads `3:30pm`, writes `3:30 pm` |
| `kMeasureSpacingPenalty` | 0.3 | reads `5km` and `50 %`, writes `5 km` and `50%` |
| `kAltUnitSpellingPenalty` | 0.05 | reads `lbs`, writes `lb` |
| `kFoldedCasePenalty` | 0.1 | reads `°c` and `mhz`, writes `°C` and `MHz` |
| `kCurrencyHomographPenalty` | 0.25 | `three pounds` is sterling, `5 lb` is weight |
| `kPlainPerChar` | 1.0 | per character of untouched text; this is what makes long spans win |
| class entry costs | 0.05, 0.5 | `MONEY($25)` beats `PLAIN($) + CARDINAL(25)` |

Three of the newer entries are the same idiom seen from an odd angle. `kBareHourPenalty` is
one weight leaning opposite ways in two branches, because whether a round hour is spoken
depends on whether a meridiem follows it, and `kArticleNumeratorPenalty` does the same for
`one half` against `and a half`. `kFoldedCasePenalty` exists because CLASSIFY folds a span's
case before any grammar sees it, so for a unit like `°C` the folded spelling is the only one
TTS can ever match and the canonical one the only one ITN will ever write; the weight is what
makes the second of those true. And `kCurrencyHomographPenalty` is an ambiguity the grammars
cannot settle at all, `three pounds` fits MONEY and MEASURE equally, so the measure reading is
priced to lose the spoken form while keeping `5 lb`, which no currency writes.

Two idioms recur. **Priority union by margin**: where a specific branch and a general branch
both fit, a hair of weight on the general one makes the specific one win, without needing a
`difference` operation. **Canonicalisation by penalty**: several written forms map to one
spoken form, and the cheapest written form is the one ITN produces.

Where nondeterminism arises from inversion (`twenty five` against `twenty-five`, `$3` against
`$3.00`) it is always resolved this way. Sayso never determinises; the best path does the work.

---
