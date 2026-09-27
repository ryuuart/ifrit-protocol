"""Tile programs and mapping, the sequence and speckle generators, the woven cloth, and the lattice sources.

Input contracts for the erased signatures of the
material/pattern package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.erased("_sigil.material.pattern.Tile", "offset", "_t.PointLike")
    table.erased("_sigil.material.pattern", "scanlines", "_t.ColorLike")
    table.erased("_sigil.material.pattern", "stipple", "_t.ColorLike")
