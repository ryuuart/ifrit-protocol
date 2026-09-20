"""Camera matrices, frustum extent and a readable Matrix.

Input contracts for the erased signatures of the
geometry-mesh/camera package, and nothing else: a fragment is one
author's alone.

GLM's caster reports only 'tuple', so every vector says how many numbers
it holds. A MATRIX IS COUNTED BY COLUMNS: one index answers a column of
four, a pair answers a column and the row within it, and the sixteen
values and the buffer stand in that same order.
"""

from __future__ import annotations

from .table import Table

MODULE = "_sigil.geometry.mesh.camera"
CAMERA = MODULE + ".Camera"
MATRIX = MODULE + ".Matrix"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.parameters(
        MATRIX + ".fromColumns",
        column0="_t.Vec4Like",
        column1="_t.Vec4Like",
        column2="_t.Vec4Like",
        column3="_t.Vec4Like",
    )
    table.parameters(MATRIX + ".setColumn", column="_t.Vec4Like")
    table.returns(MATRIX, "column", "_t.Vec4")
    # Only the column overload is erased; writing one element takes a
    # number, which pybind11 already spells.
    table.parameters(MATRIX + ".__setitem__", value="_t.Vec4Like")
    # The two readings answer different things, so each overload is
    # written out rather than given one shared return.
    table.declares(
        MATRIX,
        "__getitem__",
        "@typing.overload\n"
        "def __getitem__(self, index: typing.SupportsInt) -> _t.Vec4: ...\n"
        "@typing.overload\n"
        "def __getitem__(self, index: tuple[typing.SupportsInt,"
        " typing.SupportsInt]) -> float: ...\n",
    )
    table.declares(
        MATRIX,
        "__matmul__",
        "@typing.overload\n"
        "def __matmul__(self, other: Matrix) -> Matrix: ...\n"
        "@typing.overload\n"
        "def __matmul__(self, other: _t.Vec4Like) -> _t.Vec4: ...\n",
    )
    # The buffer protocol's two slots are C slots, so they carry no
    # signature of their own: the reading hands out the sixteen floats
    # where the matrix already keeps them, and the release gives that
    # view back.
    table.declares(
        MATRIX,
        "__buffer__",
        "def __buffer__(self, flags: int) -> memoryview:\n"
        '    """The sixteen floats as they stand in memory, column by'
        ' column."""\n',
    )
    table.declares(
        MATRIX,
        "__release_buffer__",
        "def __release_buffer__(self, buffer: memoryview) -> None:\n"
        '    """Gives back a view taken over the matrix\'s own floats."""\n',
    )
    table.parameters(CAMERA + ".project", point="_t.Vec3Like", viewport="_t.SizeLike")
    table.parameters(CAMERA + ".viewProjection", viewport="_t.SizeLike")
    table.parameters(MODULE + ".place", position="_t.Vec3Like")
    table.parameters(
        MODULE + ".faceCamera",
        eye="_t.Vec3Like",
        at="_t.Vec3Like",
        up="_t.Vec3Like",
    )
