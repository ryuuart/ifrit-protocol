"""Pattern fills, the auto Table and Python layout schemes.

Input contracts for the erased signatures of the
compose-elements/pattern-and-layout-schemes package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # The grid's gutters are a vector: the column gap in x, the row gap in y.
    table.accessor("_sigil.compose.layouts.Grid", "gap", "_t.Vec2", "_t.Vec2Like")
