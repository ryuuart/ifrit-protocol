"""Splines, parallel-transport frames, hung rails and poses.

Input contracts for the erased signatures of the
geometry-mesh/curve package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

SPLINE = "_sigil.geometry.mesh.curve.Spline3"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # GLM's caster reports only 'tuple': a spline's control points and
    # everything it answers are three numbers each.
    points = "collections.abc.Sequence[_t.Vec3Like]"
    table.parameters(SPLINE + ".__init__", points=points)
    table.accessor(SPLINE, "points", "list[_t.Vec3]", points)
    table.returns(SPLINE, "position tangent", "_t.Vec3")
    table.returns(SPLINE, "sample sampleArcLength", "list[_t.Vec3]")
