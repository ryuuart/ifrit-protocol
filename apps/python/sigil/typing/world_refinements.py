"""Typed conversion seams for native World declarations and material presets."""


def register(
    erased: dict[str, list[str]],
    returns: dict[str, str],
    signatures: dict[str, str],
    parameters: dict[str, dict[str, str]],
) -> None:
    element = "_sigil.world.Element"
    signatures[element + ".children"] = """@typing.overload
def children(self, children: collections.abc.Iterable[Element], /) -> Element: ...
@typing.overload
def children(self, *children: Element) -> Element: ...
"""
    for name in [
        "translateX",
        "translateY",
        "translateZ",
        "rotateX",
        "rotateY",
        "rotateZ",
        "scale",
        "scaleX",
        "scaleY",
        "scaleZ",
        "intensity",
        "exposure",
    ]:
        erased[element + "." + name] = ["_t.ScalarLike"]
    erased[element + ".rotate"] = ["_t.ScalarLike"]
    erased[element + ".emission"] = ["_t.ScalarLike"] * 3
    parameters[element + ".at"] = {"position": "_t.Vec3Like"}
    parameters[element + ".transformOrigin"] = {"origin": "_t.Vec3Like"}
    parameters[element + ".rotate"] = {"axis": "_t.Vec3Like"}

    for name in ("reads", "writes"):
        signatures["_sigil.world.Pass." + name] = f"""@typing.overload
def {name}(self, resources: collections.abc.Iterable[str], /) -> Pass: ...
@typing.overload
def {name}(self, *resources: str) -> Pass: ...
"""
    for path in ("Pass.clear", "Pass.levels", "Scene.image"):
        erased["_sigil.world." + path] = ["_t.ColorLike"]

    for record, fields in (
        ("_sigil.world.light.Light", {"color": 4, "direction": 3, "position": 3}),
        ("_sigil.world.kit.Rig", {"at": 3, "color": 4}),
        ("_sigil.world.kit.Turntable", {"at": 3}),
    ):
        for name, dimension in fields.items():
            signatures[record + "." + name] = f"""@property
def {name}(self) -> _t.Vec{dimension}: ...
@{name}.setter
def {name}(self, value: _t.Vec{dimension}Like) -> None: ...
"""
    light = "_sigil.world.light"
    parameters[light + ".sun"] = {"direction": "_t.Vec3Like", "color": "_t.Vec4Like"}
    parameters[light + ".point"] = {"position": "_t.Vec3Like", "color": "_t.Vec4Like"}
    parameters[light + ".spot"] = {
        "position": "_t.Vec3Like",
        "direction": "_t.Vec3Like",
        "color": "_t.Vec4Like",
    }
    parameters[light + ".attenuation"] = {"at": "_t.Vec3Like"}
    returns[light + ".radiance"] = "_t.Vec3"

    surface = "_sigil.material.kit.SurfaceParameters"
    for name in ("baseColor", "emissive", "absorption", "metal", "dielectric"):
        erased[surface + "." + name] = ["_t.ColorLike"]

    # workaround: stubgen interprets this class as the legacy typing.Set alias.
    signatures["_sigil.world.kit.Set.__init__"] = (
        "def __init__(self, **kwargs: typing.Any) -> None: ..."
    )
    signatures["_sigil.world.kit.Set.copy"] = "def copy(self) -> Set: ..."
    signatures["_sigil.world.kit.litSet"] = (
        "def litSet(subject: _sigil.world.Element, set: Set = ..., "
        "seconds: _t.FloatLike = 0) -> _sigil.world.Element: ..."
    )
