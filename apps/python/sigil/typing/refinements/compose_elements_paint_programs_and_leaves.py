"""PaintContext, keyless paint programs, keyed shapes, asset and paragraph leaves.

Input contracts for the erased signatures of the
compose-elements/paint-programs-and-leaves package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A program names the parameters it reads, so its type is the union of
    # the arities the binding accepts; every overload carries `program`.
    table.parameters("_sigil.compose.custom", program="_t.PaintProgram")
    for leaf in ("pen", "graphics"):
        table.parameters(f"_sigil.compose.{leaf}", program="_t.PenProgram")
    # The key is declared as an object by the binding itself; only the
    # generator beside it is erased.
    table.parameters("_sigil.compose.keyedShape", function="_t.KeyedShapeFunction")
    for node in ("Element", "Text", "Image", "Band"):
        table.parameters(
            f"_sigil.compose.{node}.shape", function="_t.KeyedShapeFunction"
        )
    table.erased(
        "_sigil.compose", "linearGradient", "_t.PointLike", "_t.PointLike"
    )
    table.erased("_sigil.compose", "radialGradient", "_t.PointLike")
    table.erased(
        "_sigil.compose.VarTable",
        "find __contains__",
        "_sigil.compose.VarRef | builtins.str",
    )
    table.erased("_sigil.compose.PaintContext.Pointer", "at", "_t.PointLike")
