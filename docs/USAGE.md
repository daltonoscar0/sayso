# Building and using sayso

Part of the [sayso](../README.md) documentation.

## Building

| Tool | Version |
|---|---|
| C++ compiler | C++17 (g++ 7+, clang++ 5+) |
| Scalpel | checked out at `../Scalpel` |

No external libraries. No build system required, though there is a Makefile.

```bash
make          # sayso + all seven test binaries
make test     # build and run the suite
```

Sources compile once into `build/` and link into whatever needs them. Each binary used to name
its own sources, which meant the library was recompiled seven times over: half an hour for a
one-line change to one grammar. An incremental build is now a couple of seconds.

The Python package wraps the compiled verbaliser and adds nothing to it:

```bash
pip install -e .          # needs Python 3.11
pytest                    # the contract, the telephone cases, the round-trip properties
```

| Tool | Version |
|---|---|
| Python | 3.11 (the package), 3.9 (the corpus eval's own venv) |
| pytest, hypothesis | test dependencies only; nothing at runtime |

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

`--style phone` asks for the reading a voice gives a written form rather than the one a page
does. Both readings are in the same machine and both stay invertible; the option moves a cost,
it does not select a grammar.

```bash
echo 'It is $4.99 at 18:00, 1/2 mile away.' | ./sayso normalize --style phone
echo 'It is $4.99 at 18:00, 1/2 mile away.' | ./sayso normalize --style plain
```
```
It is four ninety-nine at six in the evening, half a mile away.
It is four dollars and ninety-nine cents at eighteen hundred, one half mile away.
```

`--json` prints the pipeline contract's `StageResult`, one document per line of input: the
output, the tokens, every non-trivial decision with the readings it beat and their weights, and
a `meta` block carrying the pronunciation hints and name spans. This is what the Python package
returns, and there is only one implementation of it.

```bash
python -m sayso "I read the 2nd Dr. Lee lead a live band on Reading Rd at 10:30"
```
```json
{
  "stage": "sayso",
  "output": "I read the second Doctor Lee lead a live band on Reading Road at ten thirty",
  "decisions": [
    {
      "span": [15, 22], "surface": "Dr. Lee", "result": "Doctor Lee",
      "kind": "name", "rule": "sayso.name", "score": 0.04,
      "alternatives": [
        {"result": "doctor", "score": 0.05, "kind": "abbrev", "span": [15, 17]},
        {"result": "second drive", "score": 0.05, "kind": "address", "span": [11, 17]}
      ],
      "note": null
    }
  ],
  "meta": {
    "pron_hints": {"Reading": {"arpabet": "R EH1 D IH0 NG", "respell": "RED-ing"}},
    "name_spans": [[18, 28], [49, 61]]
  }
}
```

An alternative carries its span as well as its score, because the two only mean anything
together: these readings cover different numbers of characters, so a cheaper one is not a
better one. `2nd Dr` reads as `ADDRESS` "second drive" for 0.05 against `ORDINAL` "second" for
0.1, and loses anyway, because the sentence still has to pay for `Lee` and the address reading
has eaten the doctor.

From Python:

```python
from sayso import run
run("that will be $19.95", style="phone")["output"]   # 'that will be nineteen ninety-five'
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
| `DATE` | `3/14/2015`, `2015-03-14`, `March 14, 2015`, `14 March 2015`, `Jun 29, 2011` | `march fourteenth twenty fifteen` |
| `TIME` | `3:30`, `3:05`, `3:00`, `3:30 pm`, `10.30pm`, `5pm`, `18:00` | `three thirty`, `three oh five`, `three o'clock`, `eighteen hundred` |
| `DECADE` | `1970s`, `1900s`, `2000s`, `'80s` | `nineteen seventies`, `nineteen hundreds`, `two thousands`, `eighties` |
| `RANGE` | `1-10`, `5 - 0`, `1729-1811` | `one to ten`, `seventeen twenty-nine to eighteen eleven` |
| `ABBREV` | `St`, `vol`, `Dr`, `vs`, `Mt`, `Ltd` | `saint`, `volume`, `doctor`, `versus` |
| `LETTERS` | `U.S.`, `D.C.`, `e.g.`, `PDF`, `BBC` | `u s`, `d c`, `e g`, `p d f`, `b b c` |
| `TELEPHONE` | `978-0-131-40221-8` | `nine seven eight o one three one …` |
| `ADDRESS` | `221 Baker St`, `Reading Rd`, `5th Ave`, `E 42nd St`, `Apt 4B` | `two hundred twenty-one Baker Street`, `fifth avenue`, `apartment four B` |
| `NAME` | `Dr. Lee`, `St. Helens`, `J.R.R. Tolkien`, `King Henry III` | `Doctor Lee`, `Saint Helens`, `J R R Tolkien`, `King Henry the third` |
| `STATE` | `Little Rock, AR`, `Springfield, IL 62701` | `Little Rock, Arkansas`, `Springfield, Illinois six two seven oh one` |
| `PHONE` | `(555) 121-2121`, `+1 555-121-2121`, `1-800-FLOWERS` | `five five five, one two one, two one two one`, `one, eight oh oh, F L O W E R S` |

Roman numerals live inside `CARDINAL` rather than in a class of their own, because they are a
written-side spelling of a small integer: `World War II` is `world war two`.

Several of these stop short of where a table could have gone on, and always for the same
reason, the classifier has no context, so a grammar that reads more also mis-reads more:

- **Fraction denominators** stop at the ones with single-word ordinals.
- **Times** have no seconds field: English has no settled way of saying one to canonicalise to.
- **The measure table** omits every unit whose written form is an ordinary English word,
  `in`, `s`, `h`, `t`, `a`. Without that, `3 in the morning` is three inches.
- **The abbreviation table** omits every abbreviation commoner as a word. `no` expands to
  `number` 288 times in the corpus shard and is the word "no" 8,805 times, so it is out; `co`,
  `min` and `col` go with it.
- **`LETTERS` reads dotted shapes, and a table of acronyms.** A general "two or more capitals"
  rule is 70% right on the corpus, the other 30% being Roman numerals, acronyms English says
  as words (`FIFA`, `NASA`), and caps-lock prose, which would make it by a wide margin the
  least reliable thing here. The table is the 396 all-caps types among the corpus's 400
  commonest that the reference spells out at least 95% of the time. It is also the one grammar
  that asks for the span with its case intact: `US` is an acronym and `us` is a pronoun.
- **Roman numerals are a table, not a parser.** A parser over `M`, `D`, `C`, `L`, `X`, `V`, `I`
  accepts `MIX`, `DID` and `CIVIC`; single letters are excluded outright, since `I` alone is
  overwhelmingly the pronoun.
- **`VERBATIM` was declined entirely.** See [What is deliberately absent](#what-is-deliberately-absent).

---

## What is deliberately absent

Three things a reasonable person would expect to find here, and the evidence for leaving each
out. All three counts are from the held-out corpus shard.

**`VERBATIM`.** By token count it looks like the second-largest unimplemented class. It is
four unrelated transformations wearing one label, and none survived scrutiny. `&` → "and" is
40% of its mass and, inverted, would rewrite 189,026 ordinary "and" tokens into ampersands,
and inversion is not optional for a bidirectional grammar. Its Greek-letter table is 91.5% a
tokeniser artefact: whole Greek *words* shredded into letters and read out one at a time,
against which a symbol table would have rewritten 593 English words into Greek. What remains
is a letter-spellout, which belongs to `LETTERS`. Nothing durable was left.

**Bare all-caps acronyms.** `PDF` → "p d f" is the single largest shape in the data. Spans
reach a grammar case-folded, so no rule here can distinguish `PDF` from `pdf`, and a bare-run
branch would spell out ordinary lower-case words. The dots are the only signal that survives
folding, so `LETTERS` reads `U.S.` and leaves `PDF` alone.

**Dialect conversion.** The reference turns `colour` into `color`, `metres` into `meters`,
`organisation` into `organization`, 12,380 tokens across 540 types. That is not text
normalisation, and Sayso does not do it. It costs about 1% of the corpus score, and the
[accuracy section](#on-the-real-google-tn-slice) counts it separately for that reason.
