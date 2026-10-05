"""The artist's Builder, the sweeps and the sinks.

Input contracts for the erased signatures of the
geometry-mesh/pop-builder package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

POP = "_sigil.geometry.mesh.pop"
BUILDER = POP + ".Builder"
# A lane is named by a conventional Attribute or by any string.
LANE = "Attribute | str"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.parameters(POP + ".on", loop="collections.abc.Sequence[_t.Vec3Like]")
    for member, lanes in (
        ("jitter", ("attribute",)),
        ("noise", ("attribute",)),
        ("vary", ("attribute",)),
        ("fill", ("attribute",)),
        ("rampBy", ("source", "to")),
        ("orderBy", ("by",)),
        ("promote", ("source",)),
        ("peak", ("along",)),
        ("mix", ("first", "second", "to")),
        ("mixBy", ("first", "second", "to")),
        ("copy", ("source", "to")),
        ("normal", ("attribute",)),
    ):
        table.parameters(BUILDER + "." + member, **{lane: LANE for lane in lanes})
    table.parameters(BUILDER + ".fade", start="_t.Vec4Like", end="_t.Vec4Like")
    table.parameters(BUILDER + ".tint", color="_t.Vec4Like")
    table.parameters(BUILDER + ".fill", value="_t.Vec4Like")
    table.parameters(BUILDER + ".lookAt", target="_t.Vec3Like")
    table.parameters(BUILDER + ".move", offset="_t.Vec3Like")
    table.parameters(BUILDER + ".rampBy", stops="collections.abc.Sequence[_t.Vec4Like]")
    table.parameters(BUILDER + ".order", axis="_t.Vec3Like")
    table.parameters(BUILDER + ".select", center="_t.Vec3Like", size="_t.Vec3Like")
    for member in ("twist", "taper"):
        table.parameters(
            BUILDER + "." + member, axis="_t.Vec3Like", origin="_t.Vec3Like"
        )
    table.parameters(
        BUILDER + ".bend",
        axis="_t.Vec3Like",
        direction="_t.Vec3Like",
        origin="_t.Vec3Like",
    )
    table.parameters(BUILDER + ".normal", center="_t.Vec3Like")
