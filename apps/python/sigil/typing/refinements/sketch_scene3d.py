"""What a Python scene3d sketch is handed at setup.

Input contracts for the erased signatures of the scene3d context and the
texture scene it hands out, and nothing else: a fragment is one author's
alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from the scene3d context's signatures."""
    # Colours convert through the shared colour conversion, as the scene2d
    # context's do; a texture scene states no background and is clear.
    table.erased("_sigil.sketch.Scene3DContext", "background", "_t.ColorLike")
    table.parameters(
        "_sigil.sketch.Scene3DContext.textureScene",
        background="_t.ColorLike | None",
    )
    table.returns("_sigil.sketch.Scene3DContext", "size", "tuple[float, float]")
    table.returns("_sigil.compose.TextureScene", "size", "tuple[int, int]")
