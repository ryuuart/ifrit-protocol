"""Triangulations, hulls, streamlines, symmetry, value noise and the cellular sheet.

Input contracts for the erased signatures of the
geometry-path-shapes/structures package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.attribute("_sigil.geometry.shapes.Hatch.origin", "_t.PointLike | None")
    table.parameters("_sigil.geometry.shapes.Hatch.__init__", origin="_t.PointLike | None")
