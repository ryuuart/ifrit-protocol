"""Conversion seams of the retained 3D scene, its lights and surface presets."""

from __future__ import annotations

from .table import Table

ELEMENT = "_sigil.world.Element"
LIGHT = "_sigil.world.light"
# A light's tint is four numbers as the renderer stores them, so a light
# brighter than white stays writable, and it is also a colour, so the
# spellings every other colour accepts reach it.
TINT = "_t.ColorLike | _t.Vec4Like"


def register(table: Table) -> None:
    table.declares(
        ELEMENT,
        "children",
        """@typing.overload
def children(self, children: collections.abc.Iterable[Element], /) -> Element: ...
@typing.overload
def children(self, *children: Element) -> Element: ...
""",
    )
    table.erased(
        ELEMENT,
        "translateX translateY translateZ rotateX rotateY rotateZ scale scaleX scaleY scaleZ intensity exposure rotate",
        "_t.ScalarLike",
    )
    table.erased(ELEMENT, "emission", "_t.ScalarLike", "_t.ScalarLike", "_t.ScalarLike")
    table.parameters(ELEMENT + ".at", position="_t.Vec3Like")
    table.parameters(ELEMENT + ".transformOrigin", origin="_t.Vec3Like")
    table.parameters(ELEMENT + ".rotate", axis="_t.Vec3Like")
    for name in ("reads", "writes"):
        table.declares(
            "_sigil.world.Pass",
            name,
            f"""@typing.overload
def {name}(self, resources: collections.abc.Iterable[str], /) -> Pass: ...
@typing.overload
def {name}(self, *resources: str) -> Pass: ...
""",
        )
    table.erased("_sigil.world.Pass", "clear levels", "_t.ColorLike")
    table.erased("_sigil.world.Scene", "image", "_t.ColorLike")
    for record, fields in (
        ("_sigil.world.light.Light", {"color": 4, "direction": 3, "position": 3}),
        ("_sigil.world.kit.Rig", {"at": 3, "color": 4}),
        ("_sigil.world.kit.Turntable", {"at": 3}),
    ):
        for name, dimension in fields.items():
            written = TINT if name == "color" else f"_t.Vec{dimension}Like"
            table.declares(
                record,
                name,
                f"""@property
def {name}(self) -> _t.Vec{dimension}: ...
@{name}.setter
def {name}(self, value: {written}) -> None: ...
""",
            )
    table.parameters(LIGHT + ".sun", direction="_t.Vec3Like", color=TINT)
    table.parameters(LIGHT + ".point", position="_t.Vec3Like", color=TINT)
    table.parameters(
        LIGHT + ".spot",
        position="_t.Vec3Like",
        direction="_t.Vec3Like",
        color=TINT,
    )
    table.parameters(LIGHT + ".attenuation", at="_t.Vec3Like")
    table.returns(LIGHT, "radiance", "_t.Vec3")
    surface = "_sigil.material.kit.SurfaceParameters"
    table.erased(
        surface, "baseColor emissive absorption metal dielectric", "_t.ColorLike"
    )
    # workaround: stubgen interprets this class as the legacy typing.Set alias.
    table.declares(
        "_sigil.world.kit.Set",
        "__init__",
        "def __init__(self, **kwargs: typing.Any) -> None: ...",
    )
    table.declares("_sigil.world.kit.Set", "copy", "def copy(self) -> Set: ...")
    table.declares(
        "_sigil.world.kit",
        "litSet",
        "def litSet(subject: _sigil.world.Element, set: Set = ..., "
        "seconds: _t.FloatLike = 0) -> _sigil.world.Element: ...",
    )
