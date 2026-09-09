#!/usr/bin/env python3
"""Score NVIDIA NeMo's text normalisation on the same slice Sayso is scored on.

This lives in eval/ and is never imported by the library: Sayso itself has no
runtime dependencies, and NeMo brings a large tree of them. Run it inside
eval/.venv (see eval/README.md), which is the only place those dependencies
are allowed to exist.

    python nemo_compare.py [data/google_tn_slice.tsv] [--relaxed]

--relaxed folds away conventions two correct normalisers may simply disagree
about, hyphens inside compounds, the British "and", "o" for "oh", letter case, and
strips hyphens from
the ITN input, which inverse normalisers do not expect because ASR output has
none. It is the only mode in which a cross-system number means anything.

Prints a table in the same shape as ./sayso_eval so the two can be read side
by side, and writes nemo_results.tsv with the per-example outputs.
"""
import sys
from collections import defaultdict


def squeeze(s):
    return " ".join(s.split())


def fold_style(s):
    # "eleven o two" and "eleven oh two" are two spellings of one spoken word.
    # Case is not a spoken distinction: "degrees Celsius" and "degrees
    # celsius" are the same reading.
    return " ".join("oh" if t == "o" else t
                    for t in s.lower().replace("-", " ").split() if t != "and")


def dehyphenate(s):
    return s.replace("-", " ")


def load(path):
    rows = []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if len(fields) < 3:
                continue
            semiotic, written, spoken = fields[0], fields[1], fields[2]
            canonical = fields[3] if len(fields) > 3 else written
            rows.append((semiotic, written, spoken, canonical))
    return rows


def report(title, per_class):
    print()
    print(title)
    print("-" * len(title))
    print(f"{'class':<12}{'n':>8}{'accuracy':>12}")
    right_total = total_total = 0
    for name in sorted(per_class):
        right, total = per_class[name]
        right_total += right
        total_total += total
        print(f"{name:<12}{total:>8}{100.0 * right / total:>11.1f}%")
    if total_total:
        print(f"{'ALL':<12}{total_total:>8}{100.0 * right_total / total_total:>11.1f}%")


def main():
    args = sys.argv[1:]
    relaxed = "--relaxed" in args
    positional = [a for a in args if not a.startswith("--")]
    path = positional[0] if positional else "data/google_tn_slice.tsv"
    compare = (lambda s: fold_style(squeeze(s))) if relaxed else squeeze
    rows = load(path)
    if not rows:
        sys.exit(f"no examples in {path}")

    try:
        from nemo_text_processing.text_normalization.normalize import Normalizer
        from nemo_text_processing.inverse_text_normalization.inverse_normalize import (
            InverseNormalizer,
        )
    except ImportError as error:
        sys.exit(
            f"NeMo is not importable in this environment: {error}\n"
            "See eval/README.md for the install path and for the failure this "
            "machine hit."
        )

    normalizer = Normalizer(input_case="cased", lang="en")
    inverse = InverseNormalizer(lang="en")

    tts = defaultdict(lambda: [0, 0])
    itn = defaultdict(lambda: [0, 0])
    with open("nemo_results.tsv", "w", encoding="utf-8") as out:
        out.write("#class\tdirection\tinput\tnemo\treference\n")
        for semiotic, written, spoken, canonical in rows:
            got = compare(normalizer.normalize(written, verbose=False))
            tts[semiotic][1] += 1
            if got == compare(spoken):
                tts[semiotic][0] += 1
            out.write(f"{semiotic}\ttts\t{written}\t{got}\t{compare(spoken)}\n")

            spoken_in = dehyphenate(spoken) if relaxed else spoken
            got = squeeze(inverse.inverse_normalize(spoken_in, verbose=False))
            itn[semiotic][1] += 1
            if got == squeeze(canonical):
                itn[semiotic][0] += 1
            out.write(f"{semiotic}\titn\t{spoken_in}\t{got}\t{canonical}\n")

    print(f"slice: {path}  ({len(rows)} examples)")
    mode = "relaxed (hyphens, 'and', 'o'/'oh' and case folded away)" if relaxed else "strict (exact match)"
    print("scoring: " + mode)
    report("NeMo TTS verbalisation (written -> spoken)", tts)
    report("NeMo ITN (spoken -> written)", itn)
    print("\nper-example outputs written to nemo_results.tsv")


if __name__ == "__main__":
    main()
