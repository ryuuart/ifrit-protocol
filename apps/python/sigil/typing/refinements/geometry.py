"""Conversion seams of the headless mesh renderer.

GLM's binding caster reports only 'tuple', so dimensionality is explicit.
"""

from __future__ import annotations

from .table import Table

MESH = "_sigil.geometry.mesh"


def register(table: Table) -> None:
    table.parameters(MESH + ".render.Light.__init__", direction="_t.Vec3Like")
    table.erased(MESH + ".render", "drawImagePanel drawMesh", "_sigil.draw.Pen")
