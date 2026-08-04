# Evaluation

Everything in this directory is development tooling. Nothing here is compiled into, linked
against, or imported by the Sayso library or CLI — those have no dependencies at all, and this
is the only place a dependency is permitted to exist.

## Running it

```bash
make -C .. eval      # builds generate_slice and sayso_eval
./generate_slice     # only if you have no real slice; see below
./sayso_eval
```

Output is sentence-level and per-class accuracy in both directions, a sample of mismatches,
and two throughput numbers.

## The slice

`sayso_eval` reads `data/google_tn_slice.tsv`, one example per line, `#` for comments:

```
semiotic class <TAB> written <TAB> spoken [<TAB> canonical written]
```

The fourth column is optional and exists because **inverse normalisation is only defined up to
a canonical written form**. "march fourteenth twenty fifteen" is written `3/14/2015`,
`2015-03-14`, `March 14, 2015` and `14 March 2015`; Sayso reads all four and writes the first.
Scoring ITN against whichever form a corpus happened to use measures format preference, not
accuracy — on an early run that alone put DATE at 24%, with every single "error" being a
correctly normalised date in the wrong format. Where the column is present, ITN is scored
against it. Where it is absent, ITN is scored against the written column and the report says
so on its first line.

### Placing the real slice

The Google text normalisation dataset (Sproat & Jaitly, 2016) is not distributed with Sayso.
To use it:

1. Get the English data from <https://www.kaggle.com/datasets/google-nlu/text-normalization>
   or <https://github.com/rwsproat/text-normalization-data>.
2. Its format is one *token* per line — `semiotic-class <TAB> before <TAB> after`, with
   `<eos>` marking sentence ends and `<self>` meaning "unchanged". Reassemble 500 sentences
   into this directory's sentence-level format: join tokens with spaces, replace `<self>` with
   the `before` token, and label each sentence with the semiotic class it exercises (or the
   first non-`PLAIN` class it contains).
3. Save the result as `data/google_tn_slice.tsv`. Only the first three columns are needed; if
   you add a fourth, make it the written form you expect ITN to produce.

`data/*.tsv` is gitignored, so the file will not be committed by accident.

### The synthetic fallback

With no real slice present, `generate_slice.cpp` writes a 500-example one so the harness runs
end to end. Its spoken side comes from a small reference verbaliser in that file — ordinary
arithmetic and string concatenation, deliberately *not* the transducers under test, since a
slice generated from the grammars being scored would report 100% and measure nothing.

What the synthetic numbers mean: two independent implementations of the same conventions agree.
What they do not mean: that those conventions match the Google TN dataset's. Only the real
slice tells you that.

## NeMo comparison

`nemo_compare.py` scores [NVIDIA NeMo](https://github.com/NVIDIA/NeMo-text-processing)'s
normaliser and inverse normaliser on the same slice, printing a table in the same shape as
`sayso_eval` and writing per-example outputs to `nemo_results.tsv`.

```bash
python3 -m venv .venv
./.venv/bin/pip install nemo_text_processing
./.venv/bin/python nemo_compare.py
```

### Status in this environment

pynini would not build here, so the NeMo half has not been run. The exact failures, in order:

**1. No OpenFst headers.**

```
$ ./.venv/bin/pip install nemo_text_processing
  building '_pywrapfst' extension
  extensions/_pywrapfst.cpp:1201:10: fatal error: 'fst/util.h' file not found
   1201 | #include <fst/util.h>
  ERROR: Failed building wheel for pynini
```

**2. OpenFst 1.8.4 installed, but pynini 2.1.6 targets 1.8.3.**

```
$ brew install openfst                       # 1.8.4
$ CPPFLAGS=-I/opt/homebrew/include LDFLAGS=-L/opt/homebrew/lib \
      ./.venv/bin/pip install nemo_text_processing
  extensions/stringmap.h:185:26: error: no member named 'StringJoin' in namespace 'fst';
                                 did you mean 'StrJoin'?
  ERROR: Failed building wheel for pynini
```

OpenFst renamed `StringJoin` to `StrJoin` in 1.8.4. pynini 2.1.6 and OpenFst 1.8.4 are not
API-compatible; the versions have to be matched.

**3. Version-matched source build, too slow to complete.**

```
$ CPPFLAGS=-I/opt/homebrew/include LDFLAGS=-L/opt/homebrew/lib \
      ./.venv/bin/pip install "pynini==2.1.7"
```

This one compiles — no errors — but pynini's extension modules are large enough that the build
was still running after an hour of wall clock on this machine (Python 3.9, macOS, arm64), and
was abandoned rather than blocking the rest of the work.

**The path that is known to work** is the one upstream recommends, a prebuilt binary rather
than a source build:

```bash
conda install -c conda-forge pynini
pip install nemo_text_processing
```

There is no conda on this machine. On a machine that has one, `nemo_compare.py` runs
unchanged.


`.venv/` is gitignored. Nothing in it is on any include or link path used by `make`.
