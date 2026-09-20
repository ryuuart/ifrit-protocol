"""Primitive lanes, faces, extrusion caps and the normal grid.

Input contracts for the erased signatures of the
geometry-mesh/mesh-currency package, and nothing else: a fragment is one
author's alone.

GLM's caster reports only 'tuple', so every vector says how many numbers
it holds, on the way in and on the way back. A primitive lane is one
float4 per triangle: it is read as a copy, so the reading is a list and
the writing takes any sequence of four-number values.
"""

from __future__ import annotations

from .table import Table

MESH = "_sigil.geometry.mesh"
MESH_CLASS = MESH + ".Mesh"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.parameters(MESH + ".box", lo="_t.Vec3Like", hi="_t.Vec3Like")
    table.parameters(MESH + ".superellipsoid", radii="_t.Vec3Like")
    table.parameters(
        MESH + ".revolve", profile="collections.abc.Sequence[_t.Vec2Like]"
    )
    # WHAT THE FORMULA ANSWERS DECIDES WHICH GRID IS CALLED: a position
    # on its own is differenced, and a position with its normal is taken
    # at its word, which is what `normals=True` asks for.
    table.parameters(
        MESH + ".grid",
        surface=(
            "collections.abc.Callable[[float, float],"
            " _t.Vec3Like | tuple[_t.Vec3Like, _t.Vec3Like]]"
        ),
    )
    table.returns(MESH_CLASS, "bounds", "tuple[_t.Vec3, _t.Vec3]")
    for name, dimension in (
        ("colors", 4),
        ("normals", 3),
        ("positions", 3),
        ("uvs", 2),
    ):
        table.returns(MESH_CLASS, name, f"list[_t.Vec{dimension}]")
        table.parameters(
            MESH_CLASS + "." + name,
            value=f"collections.abc.Sequence[_t.Vec{dimension}Like]",
        )
    table.returns(MESH_CLASS, "primitive", "list[_t.Vec4]")
    table.parameters(MESH_CLASS + ".primitive", fill="_t.Vec4Like")
    table.returns(MESH_CLASS, "primitiveIf", "list[_t.Vec4] | None")
    table.parameters(
        MESH_CLASS + ".setPrimitive",
        values="collections.abc.Sequence[_t.Vec4Like]",
    )
    table.parameters(
        MESH + ".normalized", vector="_t.Vec3Like", fallback="_t.Vec3Like"
    )
    table.returns(MESH, "normalized", "_t.Vec3")
    table.parameters(
        MESH + ".basisFor", direction="_t.Vec3Like", up="_t.Vec3Like"
    )
    table.returns(MESH, "basisFor", "tuple[_t.Vec3, _t.Vec3, _t.Vec3]")
    table.returns(MESH, "faceCentroid faceNormal", "_t.Vec3")
    table.parameters(MESH + ".faceUp", up="_t.Vec3Like")
