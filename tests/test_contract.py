"""The pipeline contract: the shape of a StageResult, and the demo sentence."""

from __future__ import annotations

import json
import subprocess
import sys

import pytest

from sayso import run
from sayso.api import STAGE

DEMO = "I read the 2nd Dr. Lee lead a live band on Reading Rd at 10:30"
DEMO_SPOKEN = (
    "I read the second Doctor Lee lead a live band on Reading Road at ten thirty"
)


@pytest.fixture(scope="module")
def demo():
    return run(DEMO)


def test_stage_result_has_every_contract_key(demo):
    assert set(demo) >= {"stage", "input", "output", "tokens", "decisions", "meta"}
    assert demo["stage"] == STAGE
    assert demo["input"] == DEMO
    assert isinstance(demo["output"], str)
    assert isinstance(demo["meta"], dict)


def test_stage_result_is_json_serialisable(demo):
    json.dumps(demo)


def test_tokens_are_the_whitespace_tokens_of_output(demo):
    assert demo["tokens"] == demo["output"].split()


def test_demo_sentence_verbalises_exactly(demo):
    assert demo["output"] == DEMO_SPOKEN


@pytest.mark.parametrize("surface", ["2nd", "Dr.", "Rd", "10:30"])
def test_demo_sentence_records_a_decision_for(demo, surface):
    surfaces = [d["surface"] for d in demo["decisions"]]
    assert any(surface in s for s in surfaces), surfaces


def test_every_decision_has_the_contract_fields(demo):
    assert demo["decisions"]
    for decision in demo["decisions"]:
        assert set(decision) >= {
            "span", "out_span", "surface", "result", "kind", "rule",
            "alternatives", "score",
        }
        start, end = decision["span"]
        assert 0 <= start < end <= len(demo["input"])
        # The span is offsets into `input`, so it must actually bracket the
        # surface it claims to have read.
        assert demo["input"][start:end] == decision["surface"]
        # And `out_span` is offsets into `output`, bracketing the result. It
        # cannot be derived from `span`: the segments before this one changed
        # length, so the two coordinate systems have drifted by then.
        out_start, out_end = decision["out_span"]
        assert 0 <= out_start < out_end <= len(demo["output"])
        assert demo["output"][out_start:out_end] == decision["result"]
        assert isinstance(decision["score"], (int, float))
        assert "note" in decision


def test_decisions_are_only_the_non_trivial_spots(demo):
    """Copying a token through unchanged is not a decision."""
    for decision in demo["decisions"]:
        assert decision["surface"] != decision["result"]


def test_alternatives_carry_the_losing_paths_and_their_weights(demo):
    by_surface = {d["surface"]: d for d in demo["decisions"]}
    # "Rd" could have been ABBREV's "road" and was ADDRESS's "Reading Road".
    road = next(d for s, d in by_surface.items() if "Rd" in s)
    assert road["alternatives"], "the Rd decision should show what it beat"
    for alternative in road["alternatives"]:
        assert set(alternative) >= {"result", "score"}
        assert isinstance(alternative["score"], (int, float))


def test_alternatives_are_ranked_by_score(demo):
    for decision in demo["decisions"]:
        scores = [a["score"] for a in decision["alternatives"]]
        assert scores == sorted(scores)


def test_pron_hints_reports_reading(demo):
    hints = demo["meta"]["pron_hints"]
    assert "Reading" in hints
    assert hints["Reading"]["arpabet"] == "R EH1 D IH0 NG"


def test_name_spans_cover_the_name(demo):
    spans = demo["meta"]["name_spans"]
    assert spans
    covered = [demo["output"][s:e] for s, e in spans]
    assert any("Lee" in c for c in covered), covered


def test_module_cli_prints_valid_contract_json():
    completed = subprocess.run(
        [sys.executable, "-m", "sayso", DEMO],
        capture_output=True, text=True, timeout=300,
    )
    assert completed.returncode == 0, completed.stderr
    payload = json.loads(completed.stdout)
    assert payload["output"] == DEMO_SPOKEN
    assert payload["stage"] == STAGE


def test_module_cli_eval_prints_a_table():
    completed = subprocess.run(
        [sys.executable, "-m", "sayso", "--eval"],
        capture_output=True, text=True, timeout=900,
    )
    assert completed.returncode == 0, completed.stderr
    assert "accuracy" in completed.stdout
    assert "ALL" in completed.stdout


def test_empty_input_is_not_an_error():
    result = run("")
    assert result["output"] == ""
    assert result["decisions"] == []


def test_style_changes_the_reading():
    assert run("$4.99", style="phone")["output"] == "four ninety-nine"
    assert (
        run("$4.99", style="plain")["output"]
        == "four dollars and ninety-nine cents"
    )


def test_unknown_option_is_refused():
    with pytest.raises(TypeError):
        run("hello", voice="phone")


def test_unknown_style_is_refused():
    with pytest.raises(ValueError):
        run("hello", style="shouty")
