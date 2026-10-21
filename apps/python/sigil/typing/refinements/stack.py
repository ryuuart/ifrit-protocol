"""Conversion seams of the layer stack.

A layer is named by its name or given as itself; a size, a placement's
points and a scale are pairs, and a scale or a shear may be one number;
a blend is one of the pen's blend constants or its word.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    table.parameters("_sigil.draw.Stack.__init__", size="_t.SizeLike")
    for member in ("remove", "reorder", "__contains__"):
        table.parameters(f"_sigil.draw.Stack.{member}", layer="str | Layer")
    table.returns("_sigil.draw.Stack", "size", "tuple[float, float]")
    table.returns("_sigil.draw.Stack", "_repr_png_", "bytes")
    table.returns("_sigil.draw.Layer", "_repr_png_", "bytes | None")
    table.parameters("_sigil.draw.Layer.bake", size="_t.SizeLike | None")
    table.parameters("_sigil.draw.Layer.blend", mode="Constant | str")
    table.parameters("_sigil.draw.Layer.clip", shape="_t.DrawCallback | None")
    table.parameters(
        "_sigil.draw.Layer.transform",
        scale="_t.FloatLike | _t.PointLike | None",
        shear="_t.FloatLike | _t.PointLike | None",
        translate="_t.PointLike | None",
        origin="_t.PointLike | None",
    )
    table.parameters(
        "_sigil.draw.LayerTransform.__init__",
        origin="_t.PointLike | None",
        translation="_t.PointLike | None",
        scale="_t.FloatLike | _t.PointLike | None",
        shear="_t.FloatLike | _t.PointLike | None",
    )
    table.returns(
        "_sigil.draw.LayerTransform",
        "origin translation scale shear",
        "tuple[float, float]",
    )
    table.declares(
        "_sigil.draw.LayerFrame",
        "__exit__",
        "def __exit__(self, *exception: object) -> bool: ...\n",
    )
