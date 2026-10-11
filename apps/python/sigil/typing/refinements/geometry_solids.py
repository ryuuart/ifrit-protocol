"""The solid values: a revolution's profile and a loft's sections.

Input contracts for the erased signatures of the geometry-solids package,
and nothing else: a fragment is one author's alone.
"""

from __future__ import annotations

from .table import Table

SOLIDS = "_sigil.geometry.solids"
REVOLUTION = SOLIDS + ".Revolution"
LOFT = SOLIDS + ".Loft"

PROFILE = "collections.abc.Sequence[_t.Vec2Like]"
SECTIONS = "collections.abc.Sequence[collections.abc.Sequence[_t.Vec3Like]]"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A profile is (radius, height) points and a loft's sections are rings
    # of points in space; both are vectors the bindings read as tuples.
    table.returns(REVOLUTION, "profile", "list[_t.Vec2]")
    table.parameters(REVOLUTION + ".profile", value=PROFILE)
    table.parameters(REVOLUTION + ".__init__", profile=PROFILE)
    table.returns(LOFT, "sections", "list[list[_t.Vec3]]")
    table.parameters(LOFT + ".sections", value=SECTIONS)
    table.parameters(LOFT + ".__init__", sections=SECTIONS)
