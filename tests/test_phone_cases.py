"""Accuracy over the hand-checked telephone cases.

The thresholds below are the measured numbers rounded down, so the suite fails
on a regression rather than on a rounding wobble. They are not aspirations: the
README's failure-case section lists, verbatim, every row that is failing at the
number recorded here, and says why.
"""

from __future__ import annotations

import pytest

from sayso.evaluate import CASES, load, per_class, score

# class -> the accuracy floor, in percent.
FLOORS = {
    "ADDRESS": 97.0,   # measured 100.0
    "STATE": 90.0,     # measured  93.2
    "TIME": 97.0,      # measured 100.0
    "FRACTION": 86.0,  # measured  88.6
    "PHONE": 80.0,     # measured  83.3
    "MONEY": 97.0,     # measured 100.0
    "NAME": 86.0,      # measured  88.6
}
OVERALL_FLOOR = 91.0   # measured  93.5


@pytest.fixture(scope="module")
def outcomes():
    return score()


def test_the_case_file_is_the_size_it_claims_to_be():
    cases = load()
    assert len(cases) >= 300, f"{CASES} has {len(cases)} rows"


def test_every_new_class_has_at_least_forty_rows():
    counts = {}
    for case in load():
        counts[case.semiotic_class] = counts.get(case.semiotic_class, 0) + 1
    thin = {k: v for k, v in counts.items() if v < 40}
    assert not thin, f"classes with fewer than forty rows: {thin}"


def test_both_styles_are_exercised():
    styles = {case.style for case in load()}
    assert styles == {"phone", "plain"}


@pytest.mark.parametrize("semiotic_class", sorted(FLOORS))
def test_per_class_accuracy(outcomes, semiotic_class):
    passed, total = per_class(outcomes)[semiotic_class]
    accuracy = 100.0 * passed / total
    assert accuracy >= FLOORS[semiotic_class], (
        f"{semiotic_class} {accuracy:.1f}% is below its floor of "
        f"{FLOORS[semiotic_class]:.1f}%"
    )


def test_overall_accuracy(outcomes):
    passed = sum(1 for o in outcomes if o.passed)
    accuracy = 100.0 * passed / len(outcomes)
    assert accuracy >= OVERALL_FLOOR, f"{accuracy:.1f}% overall"


def test_the_canonical_ambiguities_are_resolved_the_right_way(outcomes):
    """The rows the whole exercise was about, pinned individually.

    These are the pairs where the same characters mean different things, and a
    class that got them right by accident would still be wrong tomorrow.
    """
    by_text = {}
    for o in outcomes:
        by_text.setdefault((o.case.text, o.case.style), o)

    def spoken(text, style="phone"):
        return by_text[(text, style)].actual

    assert spoken("Dr. Lee") == "Doctor Lee"
    assert spoken("Lee Dr") == "Lee Drive"
    assert spoken("St. Helens") == "Saint Helens"
    assert spoken("Wall St") == "Wall Street"
    assert spoken("Little Rock, AR") == "Little Rock, Arkansas"
    # The requirement is that a bare code is never the state. Sayso leaves it
    # as "AR" rather than spelling it "A R", because spelling bare capitals is
    # LETTERS' business and its table is corpus-derived; see the README's
    # failure-case section.
    assert "Arkansas" not in spoken("AR")
    assert spoken("1/2/2026") == "january second twenty twenty-six"
    assert spoken("Reading Rd") == "Reading Road"
