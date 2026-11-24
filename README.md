# sayso

[![tests](https://github.com/daltonoscar0/sayso/actions/workflows/tests.yml/badge.svg)](https://github.com/daltonoscar0/sayso/actions/workflows/tests.yml)

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

Seventeen semiotic classes: `CARDINAL`, `ORDINAL`, `DECIMAL`, `FRACTION`, `MONEY`, `MEASURE`,
`DATE`, `TIME`, `DECADE`, `RANGE`, `ABBREV`, `LETTERS`, `TELEPHONE`, `ADDRESS`, `NAME`,
`STATE`, `PHONE`. Everything else passes through byte for byte.

The last four are what a voice needs and a page does not. They exist because a conversational
product reads names, addresses and phone numbers aloud, and every one of those breaks on an
abbreviation that means two things:

```
Input   →  "I read the 2nd Dr. Lee lead a live band on Reading Rd at 10:30"
TTS     →  "I read the second Doctor Lee lead a live band on Reading Road at ten thirty"
```

`Dr.` is a doctor and `Rd` is a road in the same sentence, and neither is decided by a table.
See [Reading the abbreviation both ways](docs/ARCHITECTURE.md#reading-the-abbreviation-both-ways).

```
Input   →  "The 2.5 km run starts at 6:45 am; 3/4 of the field paid £12.50."
TTS     →  "The two point five kilometers run starts at six forty-five a m;
            three quarters of the field paid twelve pounds and fifty pence."

Input   →  "St Paul's, vol 3, pp 1-10, e.g. the 1980s."
TTS     →  "Saint Paul's, volume three, pp one to ten, e g the nineteen eighties."
```

Nine of the seventeen are bidirectional. The other eight (`RANGE`, `ABBREV`, `LETTERS`,
`TELEPHONE`, `ADDRESS`, `NAME`, `STATE`, `PHONE`) only speak; see
[One-directional classes](docs/ARCHITECTURE.md#one-directional-classes) for why that is a property of those
classes rather than a hole in the design.

---

## Quickstart

Needs a C++17 compiler and the [Scalpel](https://github.com/daltonoscar0/Scalpel)
tokeniser checked out alongside this repository at `../Scalpel`.

```bash
make                                    # builds the CLI and the test binaries
echo "It costs $25.10 on 3/14/2015." | ./sayso
```

Full flag reference, the per-class breakdown and what is deliberately left out:
[docs/USAGE.md](docs/USAGE.md).

## Accuracy

The number to judge Sayso by is the first one: it is the only one measured
against a reference that is not Sayso's own convention.

| What | Measured against | Result |
|---|---|---|
| Google TN, held-out shard 00098 | Google Kestrel | **93.6%** TTS |
| Google TN, shard 00099 (developed against) | Google Kestrel | 93.9% TTS |
| Hand-written telephone cases, 306 rows | Read aloud by hand | 93.5% |
| Synthetic slice, 500 rows | Sayso's own conventions | 100.0% TTS / 98.8% ITN |

The last row is close to 100% by construction and **is not a quality
measurement** — the slice's reference spoken forms follow Sayso's conventions.
The same applies to the NeMo comparison columns in the full writeup: NeMo is
penalised there for making different but equally correct choices. Both caveats
are spelled out where the numbers live.

Per-class tables, the failure cases, the throughput figures and the full NeMo
comparison: [docs/ACCURACY.md](docs/ACCURACY.md).

## Testing

```bash
make test     # 210 C++ tests, no dependencies
pytest        # 43 Python tests: the contract, the telephone cases, the properties
```

See [docs/TESTING.md](docs/TESTING.md) for what each suite covers and the
project layout.

> **Note:** `pytest` does not currently work from a clean clone. The Python
> package under `src/sayso/` is missing from the repository — the `.gitignore`
> entry for the compiled `sayso` binary also matched the `src/sayso/` directory,
> so it was never committed. The C++ suite is unaffected. See
> [docs/TESTING.md](docs/TESTING.md).

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — how it works, why weighted,
  reading abbreviations both ways, one-directional classes, the weighting scheme
- [docs/USAGE.md](docs/USAGE.md) — building, CLI usage, per-class coverage
- [docs/ACCURACY.md](docs/ACCURACY.md) — all accuracy and throughput numbers
- [docs/TESTING.md](docs/TESTING.md) — the two suites, project structure
- [docs/DECISIONS.md](docs/DECISIONS.md) — design decisions as they were taken

## Next steps

- **A neural reranker.** The classifier has no context beyond the span itself, and some
  ambiguity genuinely needs it. `I second the motion` becomes `I 2nd the motion`, because
  nothing in a finite-state grammar can tell the ordinal from the verb. The natural shape is
  to have CLASSIFY emit an n-best lattice rather than a single best path, and rerank it with a
  contextual model, which keeps the grammars as the source of what is *possible* and lets the
  model choose only among things that already are.
- **More semiotic classes.** Eight were added after the first four, and none of them needed
  anything new from the engine or CLASSIFY, which was the architecture's claim, now tested.
  What is left is `TELEPHONE`, `ADDRESS` and `ELECTRONIC`: 1.9% of semiotic tokens between
  them, and a different shape entirely, since the corpus spells a URL out character by
  character with a `_letter` suffix on each. `TELEPHONE` is the one worth having, and not for
  its own sake; it is the class that would stop `RANGE` mis-reading an ISBN.
- **Bare all-caps acronyms, if CLASSIFY ever sees case.** `LETTERS` reads dotted shapes only
  because spans are case-folded before a grammar sees them. Giving the classifier the
  unfolded span alongside the folded one would unlock the largest single shape in the data.
  That is a change to the pipeline rather than to any grammar, which is why it has not been
  made lightly.
- **Optional written-format output, which is now the biggest single win available.** ITN
  canonicalises hard: every spoken date becomes `M/D/YYYY`, and every written date is spoken
  month-first. That second half is 84% of everything the corpus still disagrees with, 1,318
  of 1,567 mismatches, because Kestrel preserves written order in speech and Sayso does not.
  Making the format a runtime parameter, defaulting to today's behaviour, would let a caller
  ask for the order-preserving reading without giving up the property that one spoken form
  round-trips to one written form. The penalties are already isolated in `weights.h`; moving
  them into a table is most of the work.
- **A faster cold path still.** Prefix composition and then the factoring described above have
  taken the remaining cost down to real composition work rather than repetition, and it is now
  concentrated in one place: DATE is roughly half of what a cold sentence costs, and unlike
  the others it has no duplicated sub-machine to take out. Weighted determinisation of it, or
  caching per token rather than per span text, are the next places to look.
- **Determinisation and minimisation.** Sayso resolves ambiguity by best path and never needs
  a deterministic machine, but weighted determinisation would shrink the date grammar.
- **Languages other than English.** The engine is language-neutral; the grammars are tables of
  English words. A second language is a second set of tables.

## License

MIT. See [LICENSE](LICENSE).
