"""Where item i of n goes: the run, the ring and its placement, the grid of
modules.

Input contracts for the erased signatures of the arrangement bindings, and
nothing else: a fragment is one author's alone.
"""

from __future__ import annotations

from .table import Table

ARRANGE = "_sigil.geometry.arrange"
VEC2 = "_t.Vec2"
VEC2_LIKE = "_t.Vec2Like"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.returns(ARRANGE, "direction onEllipse onRing moduleSize", VEC2)
    table.parameters(ARRANGE + ".heading", vector=VEC2_LIKE)
    table.parameters(ARRANGE + ".onEllipse", center=VEC2_LIKE, radii=VEC2_LIKE)
    table.parameters(ARRANGE + ".placeOnEllipse", center=VEC2_LIKE,
                     radii=VEC2_LIKE)
    table.parameters(ARRANGE + ".placeAlong", position=VEC2_LIKE,
                     tangent=VEC2_LIKE)
    table.parameters(ARRANGE + ".moduleSize", container=VEC2_LIKE,
                     gap=VEC2_LIKE)
    table.parameters(ARRANGE + ".cellRect", module=VEC2_LIKE)
    for owner, fields in ((ARRANGE + ".Ring", ("center", "radii")),
                          (ARRANGE + ".Placement", ("position",)),
                          (ARRANGE + ".CellBlock", ("gap", "origin"))):
        for field in fields:
            table.accessor(owner, field, VEC2, VEC2_LIKE)
