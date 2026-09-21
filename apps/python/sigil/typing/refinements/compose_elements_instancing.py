"""Instance pools, cell sheets, flights and the stamping leaf.

Input contracts for the erased signatures of the
compose-elements/instancing package, and nothing else: a fragment is one
author's alone.

Most of the package states itself: a lane is registered ahead of the
accessor that answers it, a flight ahead of the lane that holds it, and a
tint is a colour without a word here. What is left is the points, sizes
and rectangles read through a shared conversion, the ease whose absence
is a value, the recipe whose answer decides a variant's size, and the two
doors of a lane that pybind11 cannot describe: what a slice is assigned
from, and the buffer protocol's C slots.
"""

from __future__ import annotations

from .table import Table

INSTANCING = "_sigil.compose.instancing"
POOL = INSTANCING + ".Pool"
FLIGHT = POOL + ".Flight"
SHEET = INSTANCING + ".CellSheet"

BUFFER = "collections.abc.Buffer"
# A recipe answers a node of any kind, so what it hands back is the union
# every other node slot takes rather than the element alone.
NODE = "_t.NodeLike"
TREE = (
    "collections.abc.Callable[[], {answer}] | collections.abc.Callable[[int], {answer}]"
)
SIZED = f"tuple[{NODE}, _t.SizeLike]"

# Each lane class, with what one item reads back as, what a single
# assignment accepts where pybind11 erased it, what an item of an assigned
# iterable is, and what a row of its buffer holds.
LANES = {
    "PointLane": (
        "_sigil.skia.Point",
        "_t.PointLike",
        "_t.PointLike",
        "an x and a y",
    ),
    "NumberLane": ("builtins.float", None, "_t.FloatLike", "one float"),
    "ColorLane": (
        "_sigil.material.Color",
        None,
        "_t.ColorLike",
        "red, green, blue and alpha",
    ),
    "FrameLane": (
        "builtins.int",
        None,
        "typing.SupportsInt",
        "one whole number of four bytes",
    ),
    "SizeLane": (
        "_sigil.skia.Size",
        "_t.SizeLike",
        "_t.SizeLike",
        "a width and a height",
    ),
    "RectLane": (
        "_sigil.skia.Rect",
        "_t.RectLike",
        "_t.RectLike",
        "an x, a y, a width and a height",
    ),
    "FlightLane": (
        FLIGHT,
        None,
        FLIGHT,
        "a flight's twelve fields in the order it declares them",
    ),
}


def point_field(name: str) -> str:
    """A flight's point: read back as the class, written however a point is."""
    return (
        f"@property\ndef {name}(self) -> _sigil.skia.Point: ...\n"
        f"@{name}.setter\ndef {name}(self, value: _t.PointLike) -> None: ...\n"
    )


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # Where an instance stands, and where a pick is asked about, are points
    # however they are spelled.
    table.erased(POOL, "add", "_t.PointLike")
    table.erased(INSTANCING, "pick", "_t.PointLike")
    # No ease leaves each instance's progress as it is, so the absence is
    # part of the union rather than beside it.
    table.erased(POOL, "fly", "_t.EaseLike")
    # A flight's two points are written through the shared point reading.
    # `from` is a keyword, so `from_` is the spelling a declaration can
    # carry; the native name stays reachable through keywords and getattr.
    for name in ("from_", "to"):
        table.declares(FLIGHT, name, point_field(name))

    table.erased(SHEET, "cell", "_t.SizeLike")
    # The baked sheet does not exist until the first stamp.
    table.returns(SHEET, "image", "_sigil.skia.Image | None")
    # What the recipe answers decides how a variant is sized, so the two
    # arities differ in what `make` may answer: beside a shared size a tree
    # alone is enough, and without one every variant brings its own. The
    # index is offered, and a recipe that does not read it names nothing.
    table.declares(
        SHEET,
        "variants",
        "@typing.overload\n"
        "def variants(self, count: typing.SupportsInt, logicalSize: _t.SizeLike, "
        f"make: {TREE.format(answer=f'{NODE} | {SIZED}')}) -> int:\n"
        '    """Several bakes of one recipe, answering the first frame. A recipe\n'
        "    that answers a tree is registered at the shared size, and one that\n"
        '    answers a tree and a size brings its own."""\n'
        "@typing.overload\n"
        "def variants(self, count: typing.SupportsInt, "
        f"make: {TREE.format(answer=SIZED)}) -> int:\n"
        '    """The same, where every variant answers a tree and its own size."""\n',
    )

    for lane, (item, value, element, row) in LANES.items():
        path = INSTANCING + "." + lane
        table.returns(path, "__iter__", f"collections.abc.Iterator[{item}]")
        # A slice is assigned from as many items, or from a buffer of their
        # numbers; a single assignment is named only where the binding reads
        # it through a conversion pybind11 cannot describe.
        contracts = {"values": f"{BUFFER} | collections.abc.Iterable[{element}]"}
        if value is not None:
            contracts["value"] = value
        table.parameters(path + ".__setitem__", **contracts)
        # The buffer protocol's two slots are C slots, so they carry no
        # signature of their own. The view is a copy that belongs to the
        # consumer, which is why it is read-only and why releasing it gives
        # nothing back to the pool.
        table.declares(
            path,
            "__buffer__",
            "def __buffer__(self, flags: int) -> memoryview:\n"
            f'    """A read-only copy of the lane, one row per instance: {row}."""\n',
        )
        table.declares(
            path,
            "__release_buffer__",
            "def __release_buffer__(self, buffer: memoryview) -> None:\n"
            '    """Frees the copy a view of the lane was handed."""\n',
        )
