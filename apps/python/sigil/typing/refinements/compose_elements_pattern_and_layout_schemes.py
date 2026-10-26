"""Pattern fills, the auto Table and Python layout schemes.

Input contracts for the erased signatures of the
compose-elements/pattern-and-layout-schemes package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # The grid's gutters are a Gap, written as one or as its two lengths:
    # across between columns, down between rows.
    table.accessor(
        "_sigil.compose.layouts.Grid",
        "gap",
        "_sigil.compose.layouts.Gap",
        "_sigil.compose.layouts.Gap | tuple[_t.DimensionLike, _t.DimensionLike]",
    )
