"""The kit's text paints: a field, a moment and a box.

Input contracts for the erased signatures of the compose-kit/text-paint
package, and nothing else: a fragment is one author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    kit = "_sigil.compose.kit"
    box = "_sigil.geometry.path.Rect | _t.RectLike"
    table.parameters(f"{kit}.textPaint", time="_t.ScalarLike", box=f"{box} | None")
    table.parameters(f"{kit}.TextPaint.__init__", time="_t.ScalarLike", box=box)
    table.accessor(f"{kit}.TextPaint", "time", "_sigil.motion.Animatable", "_t.ScalarLike")
    table.accessor(f"{kit}.TextPaint", "box", "_sigil.geometry.path.Rect", box)
