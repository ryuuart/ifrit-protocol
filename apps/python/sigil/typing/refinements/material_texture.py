"""Textures over their three picture kinds, the image leaf, the flipbook and the flow map.

Input contracts for the erased signatures of the
material/texture package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

MATERIAL = "_sigil.material"
TEXTURE = MATERIAL + ".Texture"
FLIPBOOK = MATERIAL + ".FlipbookOptions"
# A texture is made over any picture Python holds.
PICTURE = "_sigil.skia.Image | _sigil.media.Image | _sigil.media.Video"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.parameters(TEXTURE + ".__init__", source=PICTURE)
    table.parameters(TEXTURE + ".at", origin="_t.Vec2Like")
    table.parameters(MATERIAL + ".image", pixels=PICTURE)
    table.returns(TEXTURE, "size", "tuple[int, int]")
    # The uv matrix is three columns of three numbers, written and read
    # in the same order.
    table.declares(
        TEXTURE,
        "uv",
        "@typing.overload\n"
        "def uv(self, matrix: collections.abc.Sequence[_t.Vec3Like]) -> Texture: ...\n"
        "@typing.overload\n"
        "def uv(self) -> tuple[_t.Vec3, _t.Vec3, _t.Vec3]: ...\n",
    )
    # A bound frame or progress reads back as the animatable that drives
    # it, and None hands the frame back to the clock.
    for name in ("frame", "progress"):
        table.accessor(
            FLIPBOOK,
            name,
            "_sigil.motion.Animatable | None",
            "_t.ScalarLike | None",
        )
