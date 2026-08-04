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
   or <https://github.com/rwsproat/text-normalization-data>. It is about 1.1 GB across a
   hundred shards named `output-NNNNN-of-00100`; one shard is far more than 500 sentences, so
   there is no need to download all of them.
2. Convert one shard:

   ```bash
   python3 convert_google_tn.py output-00000-of-00100 > data/google_tn_slice.tsv
   ```

   The raw format is one *token* per line — `semiotic-class <TAB> before <TAB> after`, with
   `<eos>` marking sentence ends, `<self>` meaning "unchanged" and `sil` meaning "not spoken"
   (which is what punctuation is). The script reassembles sentences and keeps only those whose
   semiotic content Sayso implements, reporting on stderr how many it dropped and why. Scoring
   a four-class normaliser on sentences containing TIME or MEASURE would measure the absence of
   those classes rather than the quality of the four that exist; pass `--keep-all` if that is
   what you want to see.

That is all — `sayso_eval` reads `data/google_tn_slice.tsv` with no further arguments. The
dataset has no canonical-written column, so the report will say ITN is scored against the
written column; see the note above on what that costs.

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

### Getting it installed

This works, and is what produced the NeMo columns in the top-level README:

```bash
python3 -m venv .venv
brew install openfst                                    # 1.8.4
CPPFLAGS=-I/opt/homebrew/include LDFLAGS=-L/opt/homebrew/lib \
    ./.venv/bin/pip install "pynini==2.1.7"             # ~2 hours to compile
./.venv/bin/pip install --no-deps nemo_text_processing
./.venv/bin/pip install joblib pandas inflect regex tqdm wget sacremoses wrapt \
    editdistance transformers cdifflib "setuptools>=65.5.1"
./.venv/bin/python nemo_compare.py --relaxed
```

Two things about that are worth knowing before repeating it.

**`--no-deps` is load-bearing.** `nemo_text_processing` pins `pynini==2.1.6.post1`, so a plain
install throws away a working pynini and tries to build 2.1.6 from source, which fails. pip
warns about the version mismatch on every subsequent command; NeMo runs correctly against
2.1.7 regardless. The trailing package list is everything `--no-deps` skipped, discovered by
importing and reading the `ModuleNotFoundError`s one at a time.

**Version matching is the whole problem.** pynini pins an exact OpenFst version and PyPI has
no arm64 wheel, so pip always builds from source and the two have to agree. The two failures:

```
$ ./.venv/bin/pip install nemo_text_processing          # no OpenFst at all
  extensions/_pywrapfst.cpp:1201:10: fatal error: 'fst/util.h' file not found

$ brew install openfst && CPPFLAGS=-I/opt/homebrew/include \
      ./.venv/bin/pip install nemo_text_processing      # OpenFst 1.8.4 vs pynini 2.1.6
  extensions/stringmap.h:185:26: error: no member named 'StringJoin' in namespace 'fst';
                                 did you mean 'StrJoin'?
```

OpenFst renamed `StringJoin` to `StrJoin` in 1.8.4. pynini 2.1.6 targets 1.8.3; pynini 2.1.7
targets 1.8.4, which is why the third attempt compiled. The upstream-recommended route,
`conda install -c conda-forge pynini`, ships a prebuilt binary and avoids all of this — there
is no conda on this machine.

### Reading the results

Run both modes. `--relaxed` is the only one in which a cross-system number means anything:
strict scoring penalises NeMo for hyphenation and for the British "and", and feeds its inverse
normaliser hyphenated text that no speech recogniser produces. Even relaxed, several remaining
gaps are differences of target rather than errors — the top-level README enumerates them
against `nemo_results.tsv`.

`.venv/` is gitignored. Nothing in it is on any include or link path used by `make`.
