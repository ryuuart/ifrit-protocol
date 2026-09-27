"""The outline and its algebra, the transform, the path through points, the
point patterns, and the general shapes over a box.

Input contracts for the erased signatures of the geometry outline and shape
bindings, and nothing else: a fragment is one author's alone.
"""

from __future__ import annotations

from .table import Table

PATH = "_sigil.geometry.path"
SHAPES = "_sigil.geometry.shapes"
MESH = "_sigil.geometry.mesh"
VEC2 = "_t.Vec2"
VEC2_LIKE = "_t.Vec2Like"
POINTS = "collections.abc.Sequence[_t.Vec2Like]"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    outline = PATH + ".Outline"
    table.returns(outline, "pointAt tangentAt normalAt", VEC2)
    table.returns(outline, "resampled", "list[list[_t.Vec2]]")
    table.parameters(outline + ".contains", point=VEC2_LIKE)
    table.parameters(outline + ".nearest", point=VEC2_LIKE)

    rect = PATH + ".Rect"
    table.parameters(rect + ".of", origin=VEC2_LIKE, size=VEC2_LIKE)
    table.parameters(rect + ".centredOn", centre=VEC2_LIKE, size=VEC2_LIKE)
    table.returns(rect, "size centre", VEC2)
    table.parameters(rect + ".contains", point=VEC2_LIKE)
    table.parameters(rect + ".translated", offset=VEC2_LIKE)
    for field in ("min", "max"):
        table.accessor(rect, field, VEC2, VEC2_LIKE)

    for owner, fields in ((PATH + ".Pose", ("position", "tangent", "normal")),
                          (PATH + ".Nearest", ("position",))):
        for field in fields:
            table.accessor(owner, field, VEC2, VEC2_LIKE)

    transform = PATH + ".Transform"
    table.parameters(transform + ".translate", offset=VEC2_LIKE)
    table.parameters(transform + ".rotate", about=VEC2_LIKE)
    table.parameters(transform + ".scale", factors=VEC2_LIKE, about=VEC2_LIKE)
    table.parameters(transform + ".skew", degrees=VEC2_LIKE)
    table.parameters(transform + ".__call__", point=VEC2_LIKE)
    table.returns(transform, "__call__", VEC2)

    table.parameters(PATH + ".through", points=POINTS)
    table.parameters(PATH + ".curveThrough", points=POINTS)
    table.returns(PATH, "points", "list[_t.Vec2]")
    # A width is a number of px, or a law through `(along, width)` stops.
    width = ("typing.SupportsFloat | collections.abc.Sequence["
             "tuple[typing.SupportsFloat, typing.SupportsFloat]]")
    table.parameters(PATH + ".offset", width=width)
    table.parameters(PATH + ".band", width=width)

    for shape in ("Radial", "Ellipse", "Fitted", "Blob", "Parallelogram",
                  "Arrow", "Chevron"):
        table.parameters(SHAPES + "." + shape + ".outline", size=VEC2_LIKE)
    for shape in ("Radial", "Ellipse", "Fitted"):
        table.parameters(SHAPES + "." + shape + ".at", centre=VEC2_LIKE)
    table.parameters(SHAPES + ".Radial.points", size=VEC2_LIKE)
    table.returns(SHAPES + ".Radial", "points", "list[_t.Vec2]")

    table.declares(
        MESH,
        "loft",
        "def loft(sections: collections.abc.Sequence["
        "collections.abc.Sequence[_t.Vec3Like]], "
        "segmentsBetween: typing.SupportsInt = 0, closed: bool = False, "
        "capEnds: bool = True) -> Mesh: ...\n",
    )
