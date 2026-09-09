"""Make `pytest` work from a clean checkout.

`pip install -e .` is the supported way in, and after it this file does nothing.
Without it, the package still has to be importable, because the contract says
tests under tests/ run with plain pytest.
"""

from __future__ import annotations

import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))
