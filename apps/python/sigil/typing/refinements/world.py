"""Conversion seams of the retained 3D scene, its nodes, lights and presets."""

from __future__ import annotations

from .table import Table

WORLD = "_sigil.world"
KINDS = ("Group", "Null", "Mesh", "Light", "Camera")
# A light's tint is four numbers as the renderer stores them, so a light
# brighter than white stays writable, and it is also a colour, so the
# spellings every other colour accepts reach it.
TINT = "_t.ColorLike | _t.Vec4Like"


def register(table: Table) -> None:
    table.declares(
        WORLD,
        "group",
        """@typing.overload
def group(children: collections.abc.Iterable[Node], /) -> Group: ...
@typing.overload
def group(*children: Node) -> Group: ...
""",
    )
    for kind in KINDS:
        node = f"{WORLD}.{kind}"
        table.erased(
            node,
            "translateX translateY translateZ rotateX rotateY rotateZ scale scaleX scaleY scaleZ rotate",
            "_t.ScalarLike",
        )
        table.erased(node, "transition", "_sigil.motion.Tween | _t.DurationLike")
        table.parameters(node + ".at", position="_t.Vec3Like")
        table.parameters(node + ".transformOrigin", origin="_t.Vec3Like")
        table.parameters(node + ".rotate", axis="_t.Vec3Like")
    for name in ("lightLink", "shadowLink"):
        table.erased(f"{WORLD}.Light", name, "str | Collection")
    table.erased(f"{WORLD}.Pass", "only", "str | Collection")
    table.erased(
        WORLD,
        "directionalLight pointLight spotLight ambientLight",
        TINT,
        "_t.ScalarLike",
    )
    table.erased(WORLD, "hemisphereLight", TINT, TINT, "_t.ScalarLike")
    # A number answers a number and a live value a value following it.
    table.declares(
        WORLD,
        "intensityAt",
        "def intensityAt(strength: _t.ScalarLike, distance: typing.SupportsFloat)"
        " -> float | _sigil.motion.Animatable: ...\n",
    )
    table.parameters(WORLD + ".directionalLight", direction="_t.Vec3Like")
    table.parameters(WORLD + ".spotLight", direction="_t.Vec3Like")
    table.erased(WORLD, "camera", "_t.ScalarLike")
    table.erased(f"{WORLD}.Frame", "environment", *(["_t.ScalarLike"] * 8))
    table.declares(
        f"{WORLD}.LightDescription",
        "direction",
        """@property
def direction(self) -> _t.Vec3: ...
""",
    )
    table.declares(
        f"{WORLD}.PlacedLight",
        "position",
        """@property
def position(self) -> _t.Vec3: ...
""",
    )
    table.parameters(WORLD + ".attenuation", at="_t.Vec3Like")
    table.returns(WORLD, "radiance", "_t.Vec3")
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
    table.erased("_sigil.world.Pass", "clear", "_t.ColorLike")
    table.parameters("_sigil.world.Pass.levels", tint="_t.ColorLike")
    table.declares(
        "_sigil.world.kit.Rig",
        "color",
        f"""@property
def color(self) -> _sigil.material.Color: ...
@color.setter
def color(self, value: {TINT}) -> None: ...
""",
    )
    for name in ("fillColor", "backColor", "ambientColor"):
        table.declares(
            "_sigil.world.kit.Rig",
            name,
            f"""@property
def {name}(self) -> _sigil.material.Color: ...
@{name}.setter
def {name}(self, value: _t.ColorLike) -> None: ...
""",
        )
    table.declares(
        "_sigil.world.kit.Rig",
        "intensity",
        """@property
def intensity(self) -> float: ...
@intensity.setter
def intensity(self, value: _t.ScalarLike) -> None: ...
""",
    )
    table.declares(
        "_sigil.world.kit.Turntable",
        "at",
        """@property
def at(self) -> _t.Vec3: ...
@at.setter
def at(self, value: _t.Vec3Like) -> None: ...
""",
    )
    for name in ("centre", "size"):
        table.declares(
            "_sigil.world.kit.SoftBox",
            name,
            f"""@property
def {name}(self) -> _t.Vec2: ...
@{name}.setter
def {name}(self, value: _t.Vec2Like) -> None: ...
""",
        )
    table.declares(
        "_sigil.world.kit.SoftBox",
        "radiance",
        """@property
def radiance(self) -> _t.Vec3: ...
@radiance.setter
def radiance(self, value: _t.Vec3Like) -> None: ...
""",
    )
    table.parameters(
        "_sigil.world.kit.environmentMap", zenith="_t.Vec3Like", nadir="_t.Vec3Like"
    )
    surface = "_sigil.material.surface.SurfaceParameters"
    table.erased(surface, "baseColor emissive absorption", "_t.ColorLike")
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
        "def litSet(subject: _sigil.world.Node, set: Set = ..., "
        "now: _t.FloatLike = 0) -> _sigil.world.Group: ...",
    )
