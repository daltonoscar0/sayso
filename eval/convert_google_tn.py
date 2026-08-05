#!/usr/bin/env python3
"""Turn a raw Google text-normalisation shard into a Sayso evaluation slice.

The dataset (Sproat & Jaitly, 2016) is one *token* per line:

    semiotic-class <TAB> before <TAB> after

with `<eos>` marking sentence ends, `<self>` meaning "unchanged", and `sil`
meaning "not spoken" (which is what punctuation is). This reassembles those
into the sentence-level format eval/sayso_eval reads:

    semiotic-class <TAB> written sentence <TAB> spoken sentence

Usage:

    python3 convert_google_tn.py output-00000-of-00100 > data/google_tn_slice.tsv

Only sentences whose semiotic content Sayso actually implements are kept, and
the count of everything dropped is reported on stderr — scoring a twelve-class
normaliser on sentences containing ELECTRONIC or TELEPHONE would measure the absence
of those classes, not the quality of the twelve that exist. Pass --classes to
change the set, or --keep-all to disable the filter and see what the twelve
classes score on unrestricted text.

One property of the reconstruction is worth knowing: the dataset stores tokens,
not sentences, so the written side comes out space-separated — "The 2006 ."
rather than "The 2006." Every system under evaluation is handed the same
string, so the comparison stays fair, but it is not quite natural text and
Sayso's serialiser will preserve that spacing in its output.

This is dev tooling: it needs only the standard library, and nothing in the
Sayso library or CLI imports it.
"""
import argparse
import sys
from collections import Counter

# Classes that carry no semiotic content of their own and so never disqualify
# a sentence.
NEUTRAL = {"PLAIN", "PUNCT"}

DEFAULT_CLASSES = ["CARDINAL", "ORDINAL", "DECIMAL", "FRACTION",
                   "MONEY", "MEASURE", "DATE", "TIME"]


def sentences(handle):
    """Yield lists of (semiotic_class, before, after) per sentence."""
    current = []
    for line in handle:
        line = line.rstrip("\n")
        if not line:
            continue
        fields = line.split("\t")
        if fields[0] == "<eos>":
            if current:
                yield current
            current = []
            continue
        if len(fields) < 3:
            continue
        current.append((fields[0], fields[1], fields[2]))
    if current:
        yield current


def render(tokens):
    """Written and spoken sides of one sentence."""
    written, spoken = [], []
    for semiotic, before, after in tokens:
        written.append(before)
        if after == "<self>":
            spoken.append(before)
        elif after == "sil":
            pass  # punctuation: written but not spoken
        else:
            spoken.append(after)
    return " ".join(written), " ".join(spoken)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shard", help="a raw output-NNNNN-of-00100 file")
    parser.add_argument("-n", "--count", type=int, default=500,
                        help="how many sentences to emit (default 500)")
    parser.add_argument("--classes", default=",".join(DEFAULT_CLASSES),
                        help="comma-separated classes to keep (default %s)"
                             % ",".join(DEFAULT_CLASSES))
    parser.add_argument("--keep-all", action="store_true",
                        help="emit every sentence, whatever classes it contains")
    args = parser.parse_args()

    wanted = {c.strip().upper() for c in args.classes.split(",") if c.strip()}
    kept, dropped = 0, Counter()

    print("# Slice reassembled from %s by eval/convert_google_tn.py." % args.shard)
    print("# Columns: semiotic class, written sentence, spoken sentence.")

    with open(args.shard, encoding="utf-8") as handle:
        for tokens in sentences(handle):
            if kept >= args.count:
                break
            present = {semiotic for semiotic, _, _ in tokens} - NEUTRAL
            if not args.keep_all:
                if not present:
                    dropped["no semiotic content"] += 1
                    continue
                unsupported = present - wanted
                if unsupported:
                    for c in unsupported:
                        dropped[c] += 1
                    continue
            written, spoken = render(tokens)
            if not written or not spoken:
                dropped["empty after rendering"] += 1
                continue
            label = sorted(present & wanted)[0] if (present & wanted) else "PLAIN"
            print("%s\t%s\t%s" % (label, written, spoken))
            kept += 1

    print("kept %d sentences" % kept, file=sys.stderr)
    if dropped:
        print("dropped, by reason:", file=sys.stderr)
        for reason, count in dropped.most_common():
            print("  %-24s %d" % (reason, count), file=sys.stderr)
    if kept < args.count:
        print("warning: wanted %d, the shard yielded %d — pass another shard "
              "or --keep-all" % (args.count, kept), file=sys.stderr)


if __name__ == "__main__":
    main()
