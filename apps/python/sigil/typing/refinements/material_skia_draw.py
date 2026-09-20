"""skia::fill and shader, the two ramp crossings, the palette images and the stock warm-up.

Input contracts for the erased signatures of the
material/skia-draw package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
