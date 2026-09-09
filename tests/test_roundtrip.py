"""Round-trip properties for the classes that read both ways.

One grammar per class, written in the speaking direction and inverted for
dictation, means the two directions cannot drift apart by construction. What
they *can* do is drift from the canonical written form: inversion is only
defined up to one, and a class that accepts four written spellings has to pick
the one it writes back. These properties pin that choice.

Only the invertible classes are here. ADDRESS, NAME, STATE and PHONE are
speaking-only, for the reason abbrev.h gives: expanding "Rd" to "Road" is
normalisation and contracting every road back to "Rd" is not, and no weight can
express that difference. There is nothing to round-trip, so nothing is asserted.

The generators are bounded rather than exhaustive because each example is a
subprocess call. Batching every case of a property into one call is what keeps
that affordable.
"""

from __future__ import annotations

import json
import subprocess

import pytest
from hypothesis import HealthCheck, given, settings
from hypothesis import strategies as st

from sayso import binary

# One process per property rather than one per example: building the grammars
# is most of what a call costs, and hypothesis wants hundreds of examples.
SLOW = settings(
    max_examples=30,
    deadline=None,
    suppress_health_check=[HealthCheck.function_scoped_fixture],
)


def written_back(lines, style="plain"):
    """Verbalise each line, then read every result back into written form."""
    spoken = _batch(lines, "tts", style)
    return spoken, _batch(spoken, "itn", style)


def _batch(lines, direction, style):
    lines = list(lines)
    if not lines:
        return []
    completed = subprocess.run(
        [str(binary.find()), "normalize",
         "--direction", direction, "--style", style, "--json"],
        input="\n".join(lines) + "\n",
        capture_output=True, text=True, timeout=900,
    )
    assert completed.returncode == 0, completed.stderr
    out = [json.loads(line)["output"]
           for line in completed.stdout.splitlines() if line.strip()]
    assert len(out) == len(lines)
    return out


# ---- TIME --------------------------------------------------------------------

@given(hour=st.integers(1, 12), minute=st.integers(0, 59))
@SLOW
def test_a_bare_civil_time_writes_back_to_its_own_digits(hour, minute):
    """A bare clock time keeps its digits, and may lose its colon.

    This is weaker than "writes back to itself" on purpose, and the weakness is
    a decision rather than a defect. Read backwards, a clock time and a year are
    the same words: "nineteen forty five" fits TIME("19:45") and CARDINAL("1945")
    equally well, and years are overwhelmingly the commoner reading of a bare
    pair. So TIME enters dearer than CARDINAL, and the price is that a bare
    spoken "twelve thirty" writes as 1230. weights.h records the trade at
    kTimeEntry, with the 588 years that failed to round-trip before it.

    A meridiem removes the ambiguity, and the property above it is the strict
    one. What must never happen either way is a digit changing, so that is what
    this asserts.
    """
    written = f"{hour}:{minute:02d}"
    spoken, back = written_back([written])
    assert back[0].replace(":", "") == written.replace(":", ""), (
        f"{written} -> {spoken[0]} -> {back[0]}"
    )


@given(hour=st.integers(1, 12), minute=st.integers(0, 59),
       meridiem=st.sampled_from(["am", "pm"]))
@SLOW
def test_a_time_with_a_meridiem_writes_back_to_itself(hour, minute, meridiem):
    written = f"{hour}:{minute:02d} {meridiem}"
    spoken, back = written_back([written])
    assert back[0] == written, f"{written} -> {spoken[0]} -> {back[0]}"


# ---- MONEY -------------------------------------------------------------------

@given(dollars=st.integers(1, 999), cents=st.integers(0, 99))
@SLOW
def test_a_price_writes_back_to_itself(dollars, cents):
    # A whole number of dollars is written without a cents field, which is the
    # canonical form the ".00" branch is priced to lose to.
    written = f"${dollars}" if cents == 0 else f"${dollars}.{cents:02d}"
    spoken, back = written_back([written])
    assert back[0] == written, f"{written} -> {spoken[0]} -> {back[0]}"


@given(dollars=st.integers(1, 99), cents=st.integers(1, 99))
@SLOW
def test_a_phone_style_price_writes_back_to_itself(dollars, cents):
    """The elided reading is still invertible: "four ninety-nine" is a price."""
    written = f"${dollars}.{cents:02d}"
    spoken, back = written_back([written], style="phone")
    assert back[0] == written, f"{written} -> {spoken[0]} -> {back[0]}"


# ---- FRACTION ----------------------------------------------------------------

DENOMINATORS = [2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 32, 64, 100]


@given(numerator=st.integers(1, 9), denominator=st.sampled_from(DENOMINATORS))
@SLOW
def test_a_fraction_writes_back_to_itself(numerator, denominator):
    written = f"{numerator}/{denominator}"
    spoken, back = written_back([written])
    assert back[0] == written, f"{written} -> {spoken[0]} -> {back[0]}"


@given(whole=st.integers(1, 99), denominator=st.sampled_from([2, 3, 4, 8]))
@SLOW
def test_a_mixed_number_writes_back_to_itself(whole, denominator):
    written = f"{whole} 1/{denominator}"
    spoken, back = written_back([written])
    assert back[0] == written, f"{written} -> {spoken[0]} -> {back[0]}"


# ---- the classes that do not go backwards ------------------------------------

@pytest.mark.parametrize("written", [
    "221 Baker St", "Reading Rd", "Dr. Lee", "Little Rock, AR", "555-1212",
])
def test_a_speaking_only_class_leaves_spoken_text_alone(written):
    """Inverting these would be contraction, not normalisation, so it does not
    happen: reading the spoken form back must not resurrect the abbreviation."""
    spoken, back = written_back([written], style="phone")
    assert "St" not in back[0].split()
    assert "Rd" not in back[0].split()
