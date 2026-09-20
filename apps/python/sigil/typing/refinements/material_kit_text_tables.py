"""The six animated text paints, the chrome-type ramps, the named colormaps and the gel tables.

Input contracts for the erased signatures of the
material/kit-text-tables package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
