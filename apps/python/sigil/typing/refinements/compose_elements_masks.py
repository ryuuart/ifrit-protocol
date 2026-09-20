"""Region, parts, by gates and Element.mask.

Input contracts for the erased signatures of the
compose-elements/masks package, and nothing else: a fragment is one
author's alone.

Most of this package states itself: a selection, a region, a run of the
boundary and a coverage paint are classes the signatures name, and the
paint is widened to a recipe instance wherever it is written. What is
left is the rectangle a region is cut from, which is read through the
shared conversion, and the fraction of an edge gate, which is a number
or anything a number animates from.
"""

from __future__ import annotations

from .table import Table

MODULE = "_sigil.compose"
REGION = MODULE + ".Region"
GATE = MODULE + ".Gate"
BY = MODULE + ".by"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A region's rectangle is read the way every other rect is read.
    table.parameters(REGION + ".rect", bounds="_t.RectLike")
    table.parameters(REGION + ".oval", bounds="_t.RectLike")
    # The factory and the field take the same animatable number; the
    # field reads back as the animatable itself, so only its setter is
    # erased.
    table.parameters(BY + ".edge", fraction="_t.ScalarLike")
    table.erased(GATE, "fraction", "_t.ScalarLike")
