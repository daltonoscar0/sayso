# Testing and project layout

Part of the [sayso](../README.md) documentation.

## Testing

Two suites. The C++ one tests the grammars and the classifier; the Python one tests the
contract the rest of the front end is written against.

```bash
make test     # 210 C++ tests, no dependencies
pytest        # 43 Python tests: the contract, the telephone cases, the properties
```

```
== wfst_tests            engine: semiring, rational operations, composition, best path
== number_tests          cardinals and ordinals, with round-trip properties
== money_date_tests      currency and dates, with round-trip properties
== quantity_tests        decimals, fractions and measures, with round-trip properties
== clock_tests           times, with a round-trip property over the whole day
== lexical_tests         decades, abbreviations, acronyms, Roman numerals, ranges
== pipeline_tests        Scalpel bridge, CLASSIFY, end-to-end normalisation
```

210 tests. The grammar suites test a cascade on its own, so a case there says what a machine
can read and write; where two classes want the same span, the arbitration is the classifier's
and lives in `pipeline_tests` instead, as does whether a one-directional class is offered at
all in ITN, which is a property of the registry rather than of any grammar.

A good number of these tests exist because a corpus found the bug first. The ones that pin a
*refusal* are worth reading as a set: `10s` is not a decade because "tens" is a quantifier,
`s.` is not an acronym because it is a sentence ending, `mix` is not a Roman numeral, `2011-11`
is not a range, and `3 in` is not a measure.

Each grammar is held to a round-trip property over seeded random input:
`itn(tts(n)) == n` for integers and ordinals, for amounts in cents, for dates; and
`tts(itn(s)) == s` for well-formed spoken forms, which is the property that pins the
*canonical* spoken form rather than merely a readable one. The property harness
(`property.h`) is about eighty lines (a seeded xorshift generator and a shrinker) so the
test build has the same zero-dependency story as the library.

On the Python side, `tests/test_contract.py` checks the shape of a `StageResult` against the
pipeline contract, that the demo sentence verbalises exactly, and that the four decisions it is
supposed to record are there with the paths they beat. `tests/test_phone_cases.py` scores the
306 hand-checked rows against a floor per class. `tests/test_roundtrip.py` runs hypothesis over
the invertible classes.

One of those properties is weaker than it looks and says so in its own docstring. A bare clock
time is asserted to keep its *digits*, not its colon, because "twelve thirty" written back is
`1230`: read backwards a clock and a year are the same words, `TIME` therefore enters dearer
than `CARDINAL`, and 588 years used to fail to round-trip before it did. The property was
written strict, failed on `11:01`, and the failure turned out to be the trade `weights.h`
already documents rather than anything the new classes broke, which is what a property test is
for.

---

## Known issue: the Python package is not in the repository

`pytest` cannot run from a clean clone. `src/sayso/` — `__init__.py`,
`__main__.py`, `api.py`, `binary.py`, `evaluate.py`, all five listed in
`src/sayso.egg-info/SOURCES.txt` — is absent from version control.

The cause is a `.gitignore` pattern. The first line of the binaries section was
`sayso`, intended for the compiled CLI at the repo root, but a pattern with no
slash matches a file *or a directory* at *any* depth, so it also matched
`src/sayso/` and silently excluded the whole package. `src/sayso.egg-info/` was
tracked because its name is not exactly `sayso`, which is why the packaging
metadata is in the repo while the code it describes is not.

The pattern is now `/sayso`, anchored to the root, so the directory is no longer
ignored and the sources can be committed.

TODO(oscar): commit `src/sayso/*.py` from a working copy that has it. Until then
the `python` CI job fails at import, and `make test` (210 C++ tests) is the only
suite that runs.

## Project structure

```
sayso/
├── wfst.h/.cpp             # tropical-semiring WFSTs; union, concat, closure,
│                           #   projection, inversion, composition, best path, Cascade
├── symbols.h/.cpp          # shared symbol table; written chars and spoken words
├── weights.h               # the entire weighting scheme, in one file
├── numbers.h/.cpp          # cardinals, ordinals, years, Roman numerals, hyphenator
├── decimals.h/.cpp         # decimal grammar; the amount measures embed
├── fractions.h/.cpp        # fraction grammar, denominator table, mixed numbers
├── money.h/.cpp            # currency grammar
├── measures.h/.cpp         # measure grammar and the unit table
├── dates.h/.cpp            # canonical date grammar + written-format normalisers
├── times.h/.cpp            # clock-time grammar
├── decades.h/.cpp          # decade grammar
├── ranges.h/.cpp           # numeric ranges          - speaking only
├── abbrev.h/.cpp           # lexical abbreviations   - speaking only
├── letters.h/.cpp          # dotted acronyms         - speaking only
├── telephone.h/.cpp        # hyphen-joined serials   - speaking only
├── style.h                 # the one option a caller has: a page's reading or a voice's
├── proper.h/.cpp           # the capitalised-word machinery ADDRESS, NAME and STATE share
├── addresses.h/.cpp        # street addresses        - speaking only
├── names.h/.cpp            # titles, initials, suffixes - speaking only
├── states.h/.cpp           # postal codes, and the pronunciation-hint table - speaking only
├── phones.h/.cpp           # telephone numbers       - speaking only
├── classify.h/.cpp         # CLASSIFY: the token-span lattice
├── text.h/.cpp             # Scalpel bridge and offset repair
├── normalizer.h/.cpp       # the pipeline, end to end
├── report.h/.cpp           # the pipeline contract's StageResult, as JSON
├── property.h              # seeded property-test harness with shrinking
├── sayso_main.cpp          # CLI
├── *_tests.cpp             # seven test binaries
├── docs/DECISIONS.md       # every design call, with its alternative and rationale
├── src/sayso/              # the Python package; a wrapper, not a second implementation
│   ├── api.py                # run(text, style="phone") -> StageResult
│   ├── __main__.py           # python -m sayso "text", and --eval
│   ├── evaluate.py           # the telephone cases, scored
│   └── binary.py             # finds the compiled verbaliser, and builds it once if absent
├── tests/                  # pytest: the contract, the cases, the round-trip properties
│   └── data/phone_cases.tsv  # 306 rows, hand-checked, not generated
└── eval/
    ├── eval_main.cpp         # accuracy both directions, per class, throughput
    ├── generate_slice.cpp    # synthetic slice, from an independent verbaliser
    ├── convert_google_tn.py  # raw Google TN shard -> slice format
    └── nemo_compare.py       # the same slice scored by NeMo; dev deps live here
```

---
