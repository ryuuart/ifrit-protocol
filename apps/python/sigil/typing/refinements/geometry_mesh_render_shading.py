"""The draw runtime seam and drawPanel.

Input contracts for the erased signatures of the
geometry-mesh/render-shading package, and nothing else: a fragment is one
author's alone.

GLM's caster reports only 'tuple', so every vector says how many numbers
it holds.
"""

from __future__ import annotations

from .table import COLOR, Table

MODULE = "_sigil.geometry.mesh.render"
LIGHT = MODULE + ".Light"
STYLE = MODULE + ".MeshStyle"
IMAGE = "_sigil.skia.Image"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # Every draw is handed the pen the frame is being drawn with; the
    # binding takes it as a handle and checks it, which leaves nothing in
    # the signature to say what it is.
    table.erased(MODULE, "drawImagePanel drawMesh drawPanel", "_sigil.draw.Pen")
    # A panel's body draws through a pen of its own, as every other
    # drawing callback does.
    table.parameters(MODULE + ".drawPanel", draw="_t.DrawCallback")
    table.parameters(LIGHT + ".__init__", direction="_t.Vec3Like")

    # A texture that was never given reads back as absent. The painter
    # keeps it as a picture and the binding crosses it as the image Python
    # holds, which is why both halves are stated.
    table.accessor(STYLE, "texture", IMAGE + " | None", IMAGE + " | None")
    # A texture's placement is the path tier's 2D map, bound beside the
    # outline it also moves.
    table.attribute(STYLE + ".uvTransform", "_sigil.geometry.path.Transform")
    # The painter's colours are four straight floats, read back as the
    # colour class and written with any spelling of a colour.
    table.accessor(LIGHT, "color", COLOR, "_t.ColorLike")
    table.accessor(STYLE, "baseColor", COLOR, "_t.ColorLike")
    table.accessor(STYLE, "ambient", COLOR, "_t.ColorLike")
