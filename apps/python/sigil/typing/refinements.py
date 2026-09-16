"""Explicit Python input contracts for erased native binding signatures.

Property-backed keyword records use their setter types. The kit and brush
field helpers share documented conversions, which are mirrored here; other
method conversions are named explicitly and fail generation when removed.
"""

from __future__ import annotations

import ast
import copy

ERASED: dict[str, list[str]] = {}
RETURNS: dict[str, str] = {}
SIGNATURES: dict[str, str] = {}
PARAMETERS: dict[str, dict[str, str]] = {}
ATTRIBUTES: dict[str, str] = {}
SEEN: set[str] = set()


def erased(prefix: str, names: str, *types: str) -> None:
    for name in names.split():
        ERASED[prefix + "." + name] = list(types)


def returns(prefix: str, names: str, value: str) -> None:
    for name in names.split():
        RETURNS[prefix + "." + name] = value


def signatures(prefix: str, name: str, text: str) -> None:
    SIGNATURES[prefix + "." + name] = text


# Shared value conversions.
erased("_sigil.Context", "background", "_t.ColorLike")
returns("_sigil.Context", "size", "tuple[float, float]")
erased("_sigil.skia.Color", "__init__", "_t.ColorLike")
erased("_sigil.skia.Paint", "setColor", "_t.ColorLike")
erased("_sigil.skia.Path", "Oval Rect", "_t.RectLike")
erased("_sigil.skia.PathBuilder", "addArc addOval addRect", "_t.RectLike")
erased("_sigil.weave.Type", "color", "_t.ColorLike | None")
returns("_sigil.io.Hub", "blob fetch", "bytes | None")
returns("_sigil.io.Feed", "latest", "bytes | None")
erased("_sigil.io.Hub", "write", "collections.abc.Buffer")
erased("_sigil.io.Arrival", "__init__ bytes", "collections.abc.Buffer")
erased("_sigil.io.Feed", "deliver send sendTo", "collections.abc.Buffer")
erased("_sigil.io.SharedMemoryWriter", "write", "collections.abc.Buffer")
for path in (
    "_sigil.io.Hub.mount",
    "_sigil.io.Hub.setNetworkCacheDirectory",
    "_sigil.io.Feed.record",
    "_sigil.io.RecordingWriter.__init__",
    "_sigil.io.readRecording",
):
    PARAMETERS[path] = {"path": "os.PathLike[str] | os.PathLike[bytes] | str | bytes"}
returns("_sigil.skia.Path", "getBounds", "_sigil.skia.Rect")
returns("_sigil.skia.Picture", "cullRect", "_sigil.skia.Rect")
PARAMETERS["_sigil.skia.Point.__init__"] = {
    "arg0": "collections.abc.Sequence[_t.FloatLike]"
}
PARAMETERS["_sigil.skia.Rect.__init__"] = {
    "arg0": "collections.abc.Sequence[_t.FloatLike]"
}
returns("_sigil.sketch.Assets", "database", "_sigil.data.Database | None")
ATTRIBUTES["_sigil.weave.Type.textTransform"] = "TextTransform | None"
ATTRIBUTES["_sigil.weave.Type.verticalForm"] = "VerticalForm | None"
returns("_sigil.weave.Type", "features", "list[FontFeature] | None")
returns("_sigil.weave.Type", "variations", "list[FontVariation]")
for field in ("size", "track", "wordSpacing"):
    signatures(
        "_sigil.weave.Type",
        field,
        f"""@property
def {field}(self) -> Length | None: ...
@{field}.setter
def {field}(self, value: Length | float | int | None) -> None: ...
""",
    )
PARAMETERS["_sigil.weave.Type.features"] = {
    "arg1": "collections.abc.Sequence[FontFeature] | None"
}
PARAMETERS["_sigil.weave.Type.variations"] = {
    "arg1": "collections.abc.Sequence[FontVariation]"
}

# Retained declarations.
element = "_sigil.compose.Element"
erased(
    element,
    "width height minWidth minHeight maxWidth maxHeight basis left top right bottom gap",
    "_t.DimensionLike",
)
erased(element, "size", "_t.DimensionLike", "_t.DimensionLike")
erased(element, "inset", *(["_t.DimensionLike"] * 4))
erased(
    element,
    "opacity rotate rotateX rotateY rotateZ scale scaleX scaleY scaleZ skewX skewY translateX translateY translateZ perspective",
    "_t.ScalarLike",
)
erased(element, "fontSize fontTrack", "_t.FloatLike | _sigil.weave.Length")
erased(element, "fill", "_t.PaintLike")
erased(element, "ink", "_t.ElementInkLike")
erased(element, "alignItems alignSelf", "_t.AlignLike")
erased(element, "justify", "_t.JustifyLike")
erased(element, "cellAlign", "_t.AlignLike", "_t.AlignLike")
erased(element, "at centerAt transformOriginPx", "_t.PointLike")
erased(element, "rect region", "_t.RectLike")
erased(element, "shape", "_t.ShapeLike")
erased(element, "background foreground overlay stroke", "_t.DecorationLike")
erased(element, "textStroke", "_t.ColorLike")
erased(element, "echo", "_t.PointLike", "_t.ColorLike")
erased(element, "var", "_t.DimensionLike | _t.ColorLike")
for edge in ("padding", "margin"):
    signatures(
        element,
        edge,
        f"""@typing.overload
def {edge}(self, all: _t.DimensionLike, /) -> Element: ...
@typing.overload
def {edge}(self, horizontal: _t.DimensionLike, vertical: _t.DimensionLike, /) -> Element: ...
@typing.overload
def {edge}(self, left: _t.DimensionLike, top: _t.DimensionLike, right: _t.DimensionLike, bottom: _t.DimensionLike, /) -> Element: ...
""",
    )
erased("_sigil.compose.Composer", "hitTest", "_t.PointLike")
erased("_sigil.compose.Decoration", "__init__", "_t.DecorationLike")
erased("_sigil.compose.Dimension", "__init__", "_t.DimensionLike")
erased("_sigil.compose.Fill", "color", "_t.ColorLike")
erased("_sigil.compose.Fill", "__init__", "_t.FillLike")
erased("_sigil.compose.MotionPath", "__init__", "_t.ShapeLike", "_t.ScalarLike")
erased("_sigil.compose.MotionPath", "t", "_t.ScalarLike")
erased(
    "_sigil.compose.PathFormat", "dashPhaseBinding trimPhase", "_t.ScalarLike | None"
)
erased("_sigil.compose.PathFormat", "strokeFill", "_t.SurfacePaintLike")
erased("_sigil.compose.Shadow", "color", "_t.ColorLike")
erased("_sigil.compose.Shadow", "offset", "_t.PointLike")
erased("_sigil.compose.Shape", "__init__", "_t.ShapeLike")
erased("_sigil.compose.SurfacePaint", "__init__", "_t.SurfacePaintLike")
erased("_sigil.compose", "shadow", "_t.ColorLike", "_t.PointLike")
erased("_sigil.compose", "shape", "_t.ShapeLike")
erased("_sigil.compose", "stroke", "_t.SurfacePaintLike")
erased(
    "_sigil.compose",
    "text",
    "_t.FloatLike | _sigil.weave.Length | None",
    "_t.ColorLike | None",
)
for name in ("pen", "graphics"):
    PARAMETERS[f"_sigil.compose.{name}"] = {"program": "_t.DrawCallback"}
signatures(
    "_sigil.compose",
    "memo",
    "def memo[Model](properties: Model, describe: collections.abc.Callable[[Model], Element]) -> Element: ...",
)
erased("_sigil.compose.spans", "upTo", "_t.ScalarLike")
erased("_sigil.compose.spans", "range wrap", "_t.ScalarLike", "_t.ScalarLike")

# Canvas and pen conversions retain native paint/color distinctions.
erased("_sigil.draw.Canvas", "clear", "_t.ColorLike")
erased("_sigil.draw.Canvas", "clipRect drawRect", "_t.RectLike")
erased("_sigil.draw.Canvas", "drawPoints", "_t.PointBatch")
returns("_sigil.draw.Graphics", "extent", "tuple[float, float]")
PARAMETERS["_sigil.draw.Graphics.draw"] = {"program": "_t.DrawCallback"}
PARAMETERS["_sigil.draw.on"] = {"program": "_t.DrawCallback"}
erased("_sigil.draw.Pen", "lerpColor", "_t.ColorLike", "_t.ColorLike")
erased("_sigil.draw", "lerpColor", "_t.ColorLike", "_t.ColorLike")
erased("_sigil.draw.Pen", "circle point", "_t.PointLike")
erased("_sigil.draw.Pen", "line", "_t.PointLike", "_t.PointLike")
erased("_sigil.draw.Pen", "element", "_t.RectLike")
erased("_sigil.draw.Pen", "inherit", "_t.ColorLike")
erased("_sigil.draw.Pen", "shape", "_sigil.skia.Path")
PARAMETERS["_sigil.draw.Pen.clip"] = {"shape": "_t.DrawCallback"}
PARAMETERS["_sigil.draw.Pen.image"] = {"arg0": "Graphics"}
for method in ("background", "fill", "stroke", "color"):
    result = "_sigil.skia.Color" if method == "color" else "None"
    paint = " | _sigil.material.skia.Paint" if method in ("fill", "stroke") else ""
    signatures(
        "_sigil.draw.Pen",
        method,
        f"""@typing.overload
def {method}(self, value: _t.ColorLike{paint}, /) -> {result}: ...
@typing.overload
def {method}(self, gray: _t.FloatLike, alpha: _t.FloatLike = ..., /) -> {result}: ...
@typing.overload
def {method}(self, red: _t.FloatLike, green: _t.FloatLike, blue: _t.FloatLike, alpha: _t.FloatLike = ..., /) -> {result}: ...
""",
    )

# Material recipes keep value uniforms distinct from animated effects.
erased("_sigil.material.skia.Effect", "glow", "_t.ColorLike")
PARAMETERS["_sigil.material.skia.Effect.blur"] = {"arg0": "Paint"}
erased(
    "_sigil.material.skia.Effect",
    "uniform",
    "_t.ScalarLike | collections.abc.Sequence[_t.FloatLike]",
)
paint = "_sigil.material.skia.Paint"
erased(paint, "solid", "_t.ColorLike")
erased(paint, "uniform", "_t.UniformValue")
erased(paint, "sksl", "str | _sigil.skia.RuntimeEffect")
PARAMETERS[paint + ".sksl"] = {"uniforms": "dict[str, _t.UniformValue]"}
PARAMETERS["_sigil.material.skia.Effect.slot"] = {"arg1": "Paint"}
erased(paint, "conical linear linearUnit", "_t.PointLike", "_t.PointLike")
erased(paint, "glowUnit radial radialUnit sweep", "_t.PointLike")
for name, arg in (
    ("conical", "arg4"),
    ("linear", "stops"),
    ("linearUnit", "arg2"),
    ("glowUnit", "arg2"),
    ("radial", "stops"),
    ("radialUnit", "arg2"),
    ("sweep", "stops"),
):
    PARAMETERS[paint + "." + name] = {arg: "_t.GradientStops"}
erased("_sigil.material.pattern", "checker", "_t.ColorLike", "_t.ColorLike")
erased("_sigil.material.pattern", "gridLines halftone stripes", "_t.ColorLike")

# Data callbacks preserve the model and result types supplied by Python.
erased("_sigil.data.Column", "__init__", "_sigil.data.ColumnType | None")
PARAMETERS["_sigil.data.Column.__init__"] = {
    "values": "collections.abc.Iterable[_t.CellInput]"
}
returns("_sigil.data.Column", "__getitem__ at", "_t.CellValue")
returns("_sigil.data.Column", "values", "list[_t.CellValue]")
returns("_sigil.data.Group", "key", "_t.CellValue")
returns("_sigil.data.Database", "open fromBytes", "Database | None")
PARAMETERS["_sigil.data.Database.open"] = {
    "arg0": "os.PathLike[str] | os.PathLike[bytes] | str | bytes"
}
PARAMETERS["_sigil.data.engineOf"] = {
    "arg0": "os.PathLike[str] | os.PathLike[bytes] | str | bytes"
}
erased("_sigil.data.Json", "__init__", "_t.JsonInput")
erased("_sigil.data.Json", "__getitem__", "str | typing.SupportsInt")
PARAMETERS["_sigil.data.Json.object"] = {
    "arg0": "collections.abc.Iterable[tuple[str, _t.JsonInput]]"
}
returns("_sigil.data.Json", "to_python", "_t.JsonValue")
erased("_sigil.data", "encodeJson tableFromJson", "_t.JsonInput")
returns("_sigil.data", "tableFromJson", "Table | None")
erased("_sigil.data.Table", "add derive", "ColumnType | None")
PARAMETERS["_sigil.data.Table.add"] = {
    "values": "collections.abc.Iterable[_t.CellInput]"
}
PARAMETERS["_sigil.data.Table.derive"] = {
    "value": "collections.abc.Callable[[int], _t.CellInput]"
}
PARAMETERS["_sigil.data.Table.filter"] = {
    "arg0": "collections.abc.Callable[[int], bool]"
}
returns("_sigil.data.Table", "cell", "_t.CellValue")
returns("_sigil.data.Table", "row", "dict[str, _t.CellValue]")
erased(
    "_sigil.data.Scale",
    "domain range",
    "Interval | collections.abc.Sequence[_t.FloatLike]",
)
signatures(
    "_sigil.data.Scale",
    "through",
    "def through[Result](self, arg0: _t.FloatLike, arg1: collections.abc.Callable[[float], Result]) -> Result: ...",
)

# Kit fields reuse the conversion policy implemented by field<T>.
erased("_sigil.compose.kit", "at", "_t.DimensionLike", "_t.DimensionLike")
erased("_sigil.compose.kit", "disc", "_t.PointLike")
erased("_sigil.compose.kit", "dot", "_t.PointLike", "_t.FillLike")
erased("_sigil.compose.kit", "ring", "_t.PointLike")
erased("_sigil.sketch.kit", "stage", "_sigil.Context")
erased("_sigil.sketch.kit.Theme", "font style", "_t.ColorLike")
erased(
    "_sigil.sketch.kit.Provide",
    "__exit__",
    "type[BaseException] | None",
    "BaseException | None",
    "types.TracebackType | None",
)
for cls, names in {
    "Caption": "label note readingLine",
    "Panel": "eyebrowLine noteLine titleLine",
    "Sheet": "footerLine subtitleLine titleLine",
}.items():
    for name in names.split():
        returns(
            "_sigil.compose.kit." + cls,
            name,
            f"collections.abc.Callable[[str, {cls}], _sigil.compose.Element]",
        )
        erased(
            "_sigil.compose.kit." + cls,
            name,
            f"collections.abc.Callable[[], _sigil.compose.Element] | collections.abc.Callable[[str], _sigil.compose.Element] | collections.abc.Callable[[str, {cls}], _sigil.compose.Element] | None",
        )
erased("_sigil.compose.layouts.AlongPath", "path", "_t.ShapeLike")
returns("_sigil.compose.layouts.AlongPath", "path", "_t.ShapeFunction")

# Brush geometry and callback seams.
brush = "_sigil.draw.brush"
for cls in ("Curl", "Direction", "Vortex", "Wave"):
    erased(brush + "." + cls, "__call__", "_t.PointLike")
erased(brush + ".Direction", "__init__", "_t.DirectionLike")
for cls in ("Input", "Sample"):
    erased(brush + "." + cls, "__init__", "_t.PointLike")
erased(brush + ".Line", "__init__", "_t.PointLike", "_t.PointLike")
erased(brush + ".Plot", "path", "_t.PointLike")
erased(brush + ".Position", "__init__", "_t.DirectionLike", "_t.RectLike | None")
erased(brush + ".Position", "angle field moveTo", "_t.DirectionLike")
erased(brush + ".Engine", "addField", "_t.DirectionLike")
erased(brush + ".Engine", "beginStroke", "_t.PointLike")
erased(brush + ".Engine", "clip", "_t.RectLike")
erased(brush + ".Engine", "flowLine", "_t.PointLike")
erased(brush + ".Engine", "line", "_t.PointLike", "_t.PointLike")
erased(brush + ".Engine", "fill hatchStyle mass set stroke wash", "_t.ColorLike")
for cls in ("Curve", "Pressure"):
    erased(brush + "." + cls, "curve", "_t.ScalarFunction | None")
erased(brush, "charcoal marker pencil spray watercolor", "_t.ColorLike")
erased(brush, "line segment", "_t.PointLike", "_t.PointLike")
erased(brush, "flowLine trace", "_t.PointLike", "_t.DirectionLike")
erased(brush, "warp", "_t.DirectionLike")
returns(brush, "stockFields", "dict[str, Direction]")
for key, parameter, value in [
    ("Polygon.__init__", "vertices", "collections.abc.Iterable[_t.PointLike]"),
    ("Polygon.vertices", "arg1", "collections.abc.Iterable[_t.PointLike]"),
    ("Engine.paint", "arg1", "collections.abc.Iterable[_t.SampleLike]"),
    ("Engine.polygon", "arg1", "collections.abc.Iterable[_t.PointLike]"),
    ("paint", "arg2", "collections.abc.Iterable[_t.SampleLike]"),
    ("wash", "arg2", "collections.abc.Iterable[_t.PointLike]"),
    ("warp", "polygon", "collections.abc.Iterable[_t.PointLike]"),
]:
    PARAMETERS[brush + "." + key] = {parameter: value}
for name in ("Engine.spline", "Plot.fromStroke", "spline"):
    PARAMETERS[brush + "." + name] = {
        "controls": "collections.abc.Iterable[_t.SampleLike]",
        "stroke": "collections.abc.Iterable[_t.SampleLike]",
    }
for name in ("hatch", "mass"):
    PARAMETERS[brush + "." + name] = {
        "polygon": "collections.abc.Iterable[_t.PointLike]"
    }
for name in ("draw", "fill", "hatch", "mass", "show", "wash"):
    PARAMETERS[brush + ".Polygon." + name] = {"arg1": "Engine"}
    PARAMETERS[brush + ".Plot." + name] = {"engine": "Engine"}
ATTRIBUTES[brush + ".Tool.customTip"] = (
    "collections.abc.Callable[[], None] | _t.DrawCallback | collections.abc.Callable[[_sigil.draw.Pen, Dab], None] | None"
)


# Motion factory dispatch preserves the corresponding value family.
erased("_sigil.motion.Animatable", "__init__", "_t.ScalarLike")
erased("_sigil.motion.Bound", "map wave", "_t.EaseLike")
erased("_sigil.motion.Transition", "__init__ ease", "_t.EaseLike")
erased("_sigil.motion", "ramp", "_t.EaseLike")
PARAMETERS["_sigil.motion.Ticker.add"] = {"function": "_t.TickCallback"}
PARAMETERS["_sigil.motion.Ticker.addFixed"] = {
    "function": "collections.abc.Callable[[], bool | None]"
}
for prefix, input_type, value_type in (
    ("", "_t.FloatLike", "float"),
    ("Color", "_t.ColorLike", "_sigil.skia.Color"),
    ("Fill", "_t.FillLike", "_sigil.compose.Fill"),
):
    erased("_sigil.motion." + prefix + "From", "to", input_type)
    returns("_sigil.motion." + prefix + "From", "to", prefix + "FromTo")
    erased("_sigil.motion." + prefix + "Output", "__init__ set value", input_type)
    returns(
        "_sigil.motion." + prefix + "Output", "__call__ endValue get value", value_type
    )
    returns("_sigil.motion." + prefix + "Transitioned", "value", value_type)
    returns(
        "_sigil.motion." + prefix + "Transitioned", "from_value", value_type + " | None"
    )
    returns(
        "_sigil.motion." + prefix + "Transitioned",
        "waypoints",
        "list[tuple[float, " + value_type + "]]",
    )

for name in ("from_", "to", "entrance", "transition", "through", "animate"):
    declarations = []
    for prefix, value in (
        ("", "float"),
        ("Color", "_t.ColorLike"),
        ("Fill", "_sigil.compose.Fill"),
    ):
        timing = "duration: _t.FloatLike = 0.25, delay: _t.FloatLike = 0.0, ease: _t.EaseLike = None"
        if name == "from_":
            args, result = f"arg0: {value}, /", prefix + "From"
        elif name == "to":
            args, result = f"arg0: {value}, /", prefix + "To"
        elif name == "entrance":
            stop = "_t.FillLike" if prefix == "Fill" else value
            args, result = (
                f"start: {value}, stop: {stop}, {timing}",
                prefix + "Transitioned",
            )
        elif name == "transition":
            args, result = f"target: {value}, {timing}", prefix + "Transitioned"
        elif name == "through":
            args, result = (
                f"arg0: collections.abc.Iterable[tuple[_t.FloatLike, {value}]], /",
                prefix + "Waypoints",
            )
        else:
            args = f"path: {prefix}FromTo | {prefix}To | {prefix}Waypoints, spec: Transition | None = None, *, ease: _t.EaseLike = None"
            result = prefix + "Transitioned"
        declarations.append(f"@typing.overload\ndef {name}({args}) -> {result}: ...")
    signatures("_sigil.motion", name, "\n".join(declarations))

# GLM's binding caster reports only 'tuple', so dimensionality is explicit.
mesh = "_sigil.geometry.mesh"
PARAMETERS[mesh + ".box"] = {"lo": "_t.Vec3Like", "hi": "_t.Vec3Like"}
PARAMETERS[mesh + ".superellipsoid"] = {"radii": "_t.Vec3Like"}
PARAMETERS[mesh + ".revolve"] = {"profile": "collections.abc.Sequence[_t.Vec2Like]"}
PARAMETERS[mesh + ".grid"] = {
    "surface": "collections.abc.Callable[[float, float], _t.Vec3Like]"
}
returns(mesh + ".Mesh", "bounds", "tuple[_t.Vec3, _t.Vec3]")
for name, dim in (("colors", 4), ("normals", 3), ("positions", 3), ("uvs", 2)):
    returns(mesh + ".Mesh", name, f"list[_t.Vec{dim}]")
    PARAMETERS[mesh + ".Mesh." + name] = {
        "arg0": f"collections.abc.Sequence[_t.Vec{dim}Like]"
    }
PARAMETERS[mesh + ".camera.Camera.project"] = {"arg0": "_t.Vec3Like"}
PARAMETERS[mesh + ".camera.faceCamera"] = {
    x: "_t.Vec3Like" for x in ("eye", "at", "up")
}
PARAMETERS[mesh + ".camera.place"] = {"position": "_t.Vec3Like"}
PARAMETERS[mesh + ".render.Light.__init__"] = {"direction": "_t.Vec3Like"}
erased(mesh + ".render", "drawImagePanel drawMesh", "_sigil.draw.Pen")


def expression(text: str) -> ast.expr:
    return ast.parse(text, mode="eval").body


def show(node: ast.AST | None) -> str:
    return ast.unparse(node) if node is not None else ""


def erased_annotation(node: ast.AST | None) -> bool:
    text = show(node)
    return any(
        token in text for token in ("typing.Any", "...", "os.PathLike |")
    ) or text in {
        "collections.abc.Callable",
        "collections.abc.Iterable",
        "collections.abc.Sequence",
        "tuple",
        "list",
        "dict",
        "list[tuple]",
        "collections.abc.Sequence[tuple]",
    }


def setter_type(annotation: ast.expr, module: str) -> ast.expr:
    text = show(annotation)
    mappings = {"float": "_t.FloatLike", "int": "typing.SupportsInt"}
    if module.startswith(
        ("_sigil.compose.kit", "_sigil.sketch.kit", "_sigil.compose.layouts")
    ):
        mappings.update(
            {
                "_sigil.skia.Color": "_t.ColorLike",
                "_sigil.skia.Point": "_t.PointLike",
                "_sigil.skia.Size": "_t.SizeLike",
                "_sigil.compose.Dimension": "_t.DimensionLike",
                "_sigil.compose.Fill": "_t.FillLike",
                "_sigil.compose.SurfacePaint": "_t.SurfacePaintLike",
                "_sigil.compose.Align": "_t.AlignLike",
                "_sigil.compose.Justify": "_t.JustifyLike",
            }
        )
    elif module == "_sigil.draw.brush":
        mappings.update(
            {"_sigil.skia.Color": "_t.ColorLike", "_sigil.skia.Point": "_t.PointLike"}
        )
    if text in mappings:
        return expression(mappings[text])
    if text.endswith(" | None"):
        inner = setter_type(expression(text.removesuffix(" | None")), module)
        return expression(show(inner) + " | None")
    if text.startswith("list["):
        return expression("collections.abc.Sequence[" + text[5:])
    return copy.deepcopy(annotation)


def refine(module: str, tree: ast.Module) -> None:
    def visit(body: list[ast.stmt], path: str, class_name: str | None = None) -> None:
        getters = {
            n.name: n
            for n in body
            if isinstance(n, ast.FunctionDef)
            and any(show(d) == "property" for d in n.decorator_list)
        }
        output: list[ast.stmt] = []
        replaced: set[str] = set()
        for node in body:
            if isinstance(node, (ast.FunctionDef, ast.ClassDef)):
                SEEN.add(path + "." + node.name)
            elif isinstance(node, ast.AnnAssign):
                SEEN.add(path + "." + show(node.target))
            if isinstance(node, ast.ClassDef):
                if path == "_sigil.draw.brush.Pressure" and node.name in {
                    "Gaussian",
                    "Variation",
                }:
                    output.extend(
                        ast.parse(f"{node.name} = _sigil.draw.brush.{node.name}").body
                    )
                    continue
                visit(node.body, path + "." + node.name, node.name)
            elif (
                isinstance(node, ast.AnnAssign)
                and path + "." + show(node.target) in SIGNATURES
            ):
                output.extend(
                    ast.parse(SIGNATURES[path + "." + show(node.target)]).body
                )
                continue
            elif isinstance(node, ast.AnnAssign) and show(node.annotation) == "tuple":
                dim = 4 if path.endswith(".BoxOptions") else 3
                name = show(node.target)
                output.extend(
                    ast.parse(
                        f"@property\ndef {name}(self) -> _t.Vec{dim}: ...\n@{name}.setter\ndef {name}(self, value: _t.Vec{dim}Like) -> None: ..."
                    ).body
                )
                continue
            elif (
                isinstance(node, ast.AnnAssign)
                and path + "." + show(node.target) in ATTRIBUTES
            ):
                node.annotation = expression(ATTRIBUTES[path + "." + show(node.target)])
            elif isinstance(node, ast.FunctionDef):
                if node.name.startswith("__keyword_"):
                    continue
                full = path + "." + node.name
                if full in SIGNATURES:
                    if full not in replaced:
                        output.extend(ast.parse(SIGNATURES[full]).body)
                        replaced.add(full)
                    continue
                all_args = [
                    *node.args.posonlyargs,
                    *node.args.args,
                    *node.args.kwonlyargs,
                ]
                unknown = [a for a in all_args if show(a.annotation) == "typing.Any"]
                if node.name in {"__eq__", "__ne__"}:
                    for arg in all_args:
                        if arg.arg != "self":
                            arg.annotation = expression("builtins.object")
                elif full in ERASED:
                    values = ERASED[full]
                    if unknown:
                        if len(values) != len(unknown):
                            raise RuntimeError(
                                f"{full}: expected {len(values)} erased arguments, found {len(unknown)}"
                            )
                        for arg, type_ in zip(unknown, values, strict=True):
                            arg.annotation = expression(type_)
                elif (
                    any(show(d).endswith(".setter") for d in node.decorator_list)
                    and unknown
                ):
                    getter = getters[node.name]
                    if getter.returns is not None:
                        unknown[-1].annotation = setter_type(getter.returns, module)
                if full in PARAMETERS:
                    for arg in all_args:
                        if arg.arg in PARAMETERS[full] and erased_annotation(
                            arg.annotation
                        ):
                            arg.annotation = expression(PARAMETERS[full][arg.arg])
                if full in RETURNS and not any(
                    show(d).endswith(".setter") for d in node.decorator_list
                ):
                    node.returns = expression(RETURNS[full])
                positional = [*node.args.posonlyargs, *node.args.args]
                for arg, default in zip(
                    positional[-len(node.args.defaults) :], node.args.defaults
                ):
                    if (
                        isinstance(default, ast.Constant)
                        and default.value is None
                        and arg.annotation is not None
                        and "None" not in show(arg.annotation)
                    ):
                        arg.annotation = expression(show(arg.annotation) + " | None")
                if (
                    isinstance(node.returns, ast.Constant)
                    and node.returns.value is Ellipsis
                ):
                    raise RuntimeError(f"Unregistered native return type: {full}")
            output.append(node)
        body[:] = output
        # Record constructors assign the same exposed properties as edits do.
        setters = {
            show(n.target): n.annotation
            for n in body
            if isinstance(n, ast.AnnAssign)
            and not show(n.target).startswith("_")
            and "ClassVar" not in show(n.annotation)
        }
        setters.update(
            {
                n.name: n.args.args[-1].annotation
                for n in body
                if isinstance(n, ast.FunctionDef)
                and any(show(d).endswith(".setter") for d in n.decorator_list)
            }
        )
        for node in body:
            if (
                isinstance(node, ast.FunctionDef)
                and node.name == "__init__"
                and node.args.kwarg
            ):
                node.args.kwarg = None
                node.args.kwonlyargs = [
                    ast.arg(arg=name, annotation=copy.deepcopy(value))
                    for name, value in setters.items()
                ]
                node.args.kw_defaults = [ast.Constant(Ellipsis) for _ in setters]

    visit(tree.body, module)
    if module == "_sigil.draw.brush":
        tree.body[:] = [
            n
            for n in tree.body
            if not (isinstance(n, ast.ImportFrom) and n.module == "builtins")
        ]
        tree.body.append(ast.parse("Stroke: typing.TypeAlias = list[Sample]").body[0])
    imports = ast.parse(
        "import _sigil\nimport _sigil._types as _t\nimport builtins\nimport collections.abc\nimport typing\nimport types\nimport os\n"
    ).body
    tree.body[1:1] = imports
    used = {node.id for node in ast.walk(tree) if isinstance(node, ast.Name)}
    used.update(
        show(node) for node in ast.walk(tree) if isinstance(node, ast.Attribute)
    )
    exports: set[str] = set()
    for node in tree.body:
        if (
            isinstance(node, ast.AnnAssign)
            and show(node.target) == "__all__"
            and isinstance(node.value, ast.List)
        ):
            exports.update(
                item.value
                for item in node.value.elts
                if isinstance(item, ast.Constant) and isinstance(item.value, str)
            )
    imported: set[str] = set()
    clean: list[ast.stmt] = []
    for node in tree.body:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            text = show(node)
            if text in imported:
                continue
            imported.add(text)
            if isinstance(node, ast.Import):
                node.names[:] = [
                    alias
                    for alias in node.names
                    if (alias.asname or alias.name) in used
                ]
            elif node.module != "__future__":
                node.names[:] = [
                    alias
                    for alias in node.names
                    if (alias.asname or alias.name) in used | exports
                ]
            if not node.names:
                continue
        clean.append(node)
    tree.body[:] = clean


def validate() -> None:
    expected = (
        ERASED.keys()
        | RETURNS.keys()
        | SIGNATURES.keys()
        | PARAMETERS.keys()
        | ATTRIBUTES.keys()
    )
    absent = expected - SEEN
    if absent:
        raise RuntimeError(
            "Refinements no longer present in the extension: "
            + ", ".join(sorted(absent))
        )
