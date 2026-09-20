"""The borrowed canvas every binding that draws through Skia is handed.

Input contracts for the erased signatures of the draw/canvas-seam
package, and nothing else: a fragment is one author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.erased("_sigil.draw.Canvas", "clear", "_t.ColorLike")
    table.erased("_sigil.draw.Canvas", "clipRect drawRect", "_t.RectLike")
    table.erased("_sigil.draw.Canvas", "drawPoints", "_t.PointBatch")
