"""Conversion seams of meshes, cameras and the headless mesh renderer.

GLM's binding caster reports only 'tuple', so dimensionality is explicit.
"""

from __future__ import annotations

from .table import Table

MESH = "_sigil.geometry.mesh"


def register(table: Table) -> None:
    table.parameters(MESH + ".box", lo="_t.Vec3Like", hi="_t.Vec3Like")
    table.parameters(MESH + ".superellipsoid", radii="_t.Vec3Like")
    table.parameters(MESH + ".revolve", profile="collections.abc.Sequence[_t.Vec2Like]")
    table.parameters(
        MESH + ".grid",
        surface="collections.abc.Callable[[float, float], _t.Vec3Like]",
    )
    table.returns(MESH + ".Mesh", "bounds", "tuple[_t.Vec3, _t.Vec3]")
    for name, dimension in (
        ("colors", 4),
        ("normals", 3),
        ("positions", 3),
        ("uvs", 2),
    ):
        table.returns(MESH + ".Mesh", name, f"list[_t.Vec{dimension}]")
        table.parameters(
            MESH + ".Mesh." + name,
            value=f"collections.abc.Sequence[_t.Vec{dimension}Like]",
        )
    table.parameters(MESH + ".camera.Camera.project", point="_t.Vec3Like")
    table.parameters(
        MESH + ".camera.faceCamera",
        eye="_t.Vec3Like",
        at="_t.Vec3Like",
        up="_t.Vec3Like",
    )
    table.parameters(MESH + ".camera.place", position="_t.Vec3Like")
    table.parameters(MESH + ".render.Light.__init__", direction="_t.Vec3Like")
    table.erased(MESH + ".render", "drawImagePanel drawMesh", "_sigil.draw.Pen")
