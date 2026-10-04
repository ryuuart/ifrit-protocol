"""Representative direct binding contracts, checked without running sketches."""

from array import array
from dataclasses import dataclass
from datetime import timedelta
from typing import assert_type

from sigil import (
    compose,
    data,
    draw,
    geometry,
    io,
    material,
    motion,
    sketch,
    skia,
    weave,
)


@dataclass(frozen=True)
class Model:
    count: int


def describe(model: Model) -> compose.Text:
    return compose.text(str(model.count)).fontSize(weave.em(1.2))


tree = (
    compose.memo(Model(3), describe).width("100%").padding(horizontal=12, vertical=16)
)
assert_type(tree, compose.Element)
key = material.Light(
    kind=material.LightKind.Point, position=(0, 0, 180), intensity=motion.animatable(1)
)
assert_type(compose.light(source=key).translateX(12), compose.Element)
assert_type(compose.scene([compose.light(key), compose.text("Lit")]), compose.Element)
assert_type(compose.scene(compose.light(key), compose.text("Lit")), compose.Element)
lighting = material.Lighting(lights=(key, material.studio()))
assert_type(lighting, material.Lighting)
assert_type(lighting.frame, material.LightingFrame)
assert_type(
    material.Lighting(key, frame=material.LightingFrame.Scene), material.Lighting
)
tree.lighting(key)
tree.lighting(lighting)
around = material.environment(material.Material("#ffffff"), size=(32, 16))
assert_type(compose.scene().environment(around), compose.Element)
assert_type(
    material.Lighting(around, frame=material.LightingFrame.Scene), material.Lighting
)
tree.lighting(around)
assert_type(
    tree.padding(all=8).margin(left=1, top=2, right=3, bottom=4), compose.Element
)
assert_type(tree.alignItems("auto").justifyContent("space_between"), compose.Element)
assert_type(
    tree.alignItems(compose.Align.Auto).justifyContent(compose.Justify.End),
    compose.Element,
)
assert_type(motion.Tween(from_=0, to=1), motion.Tween)
assert_type(motion.animate(motion.Tween(from_=0, to=1)), motion.Animatable)
assert_type(motion.animate(from_=0, to=1), motion.Animatable)
assert_type(motion.animate(from_="#000", to="#fff"), motion.ColorAnimatable)
assert_type(
    motion.animate(from_=compose.Fill.none(), to=compose.Fill.color("#fff")),
    motion.FillAnimatable,
)
assert_type(motion.animatable(0.5), motion.Animatable)
assert_type(motion.stagger(0.04), motion.Staggered)
assert_type(motion.phase(timedelta(seconds=1), 2), float)

ink = material.Paint.linearGradient(
    (0, 0),
    (40, 0),
    [(0, "#fff"), (1, "#123")],
    material.GradientOptions(units=material.GradientUnits.Pixels),
)
shader = material.Paint.sksl(
    "uniform float gain; half4 main(float2 p) { return half4(gain); }", {"gain": 0.5}
)
assert_type(shader.set("gain", 0.7), material.Paint)
tree.fill(ink).opacity(motion.bind(motion.animatable(1.0), to=(0, 1)))
outline = compose.stroke(2, ink)
outline.strokeFill = ink
outline.trimPhase = None
outline.dashPhaseBinding = motion.animatable(0)
assert_type(shader.bind("gain", 0.25), material.Paint)

glass = material.Filter.glass(
    material.GlassOptions(ior=1.33, thickness=24, normal=material.Material("#8080ff"))
)
assert_type(glass, material.Filter)
height = material.Material("#808080")
dented = material.surface.normalFromHeight(
    height, material.surface.HeightNormalOptions(depth=-2, step=0.5)
)
assert_type(dented, material.Material)
assert_type(
    material.surface.blendNormals(
        height, dented, material.surface.NormalBlendOptions(detailDirectX=True)
    ),
    material.Material,
)
tinted = material.Material("#c0c0c0")
assert_type(tinted.bind("tint", "#ff8040"), material.Material)
assert_type(tinted.bind("tint", (1, 0.5, 0.25)), material.Material)
assert_type(tinted.bind("tint", motion.animatable("#ff8040")), material.Material)
finish = material.Material("#c0c0c0")
assert_type(finish.surface(roughness=0.4, lighting=key), material.Material)
assert_type(finish.surface(lighting=around), material.Material)
assert_type(finish.surface(lighting=lighting), material.Material)
assert_type(compose.relief(finish, shoulder=3, depth=-0.5), compose.Decoration)
assert_type(tree.background(compose.relief(finish)), compose.Element)
glyphs = weave.PaintStyle(foregroundMaterial=finish)
assert_type(glyphs.foregroundMaterial, material.Material | None)
assert_type(
    sketch.CanvasSpecification(paintDiscardedFrames=False).paintDiscardedFrames, bool
)

style = weave.Type(size=weave.Length(16), features=[weave.FontFeature("liga", 1)])
label = compose.text("Native", weave.textStyle(style))
panel = compose.kit.panel(label, compose.kit.Panel(title="Station"))
assert_type(panel, compose.Element)
assert_type(skia.Path.Rect((0, 0, 20, 20)).getBounds(), skia.Rect)

table = data.Table()
table.add("reading", [1.0, None, 3.0])
table.derive("index", lambda index: index)
assert_type(table.filter(lambda index: index != 1), data.Table)
assert_type(table.cell("reading", 0), None | bool | int | float | str | data.Instant)
assert_type(data.Flag(True) < False, bool)
assert_type(data.Flag(True) == data.Flag(False), bool)
assert_type(sorted([data.Instant(1), data.Instant(0)]), list[data.Instant])
assert_type({data.Flag(True): data.Instant(0)}, dict[data.Flag, data.Instant])
document = data.Json({"ok": [True, None]}).to_python()
if isinstance(document, dict):
    assert_type(next(iter(document)), str)
assert_type(
    data.Scale(domain=(0, 10), range=(0, 1)).through(2.0, lambda x: str(x)), str
)


def paint(pen: draw.Pen) -> None:
    points = array("f", [0, 0, 30, 10])
    paint = skia.Paint()
    pen.canvas().drawPoints(draw.PointMode.Lines, points, paint)
    pen.canvas().drawPoints(draw.PointMode.Points, [(1, 2), skia.Point(3, 4)], paint)
    pen.fill(ink)
    pen.stroke(32, 64, 96, 255)
    pen.line((0, 0), (20, 30))
    pen.fill("#f1bb7b")
    pen.fill(ink, draw.CANVAS)
    pen.stroke(ink, draw.SHAPE)
    pen.background(ink)
    pen.background(material.surface.unlit(material.surface.SurfaceParameters()))
    pen.fill(
        material.surface.unlit(material.surface.SurfaceParameters(baseColor="#e75a31"))
    )

    class Squircle:
        def path(self, size: tuple[float, float]) -> skia.Path:
            return skia.Path.Rect((0, 0, size[0], size[1]))

    pen.shape(Squircle(), 0, 0, 40, 40)
    pen.shape(skia.Path.Rect((0, 0, 10, 10)))
    brush = draw.brush.pencil("#555")
    brush.customTip = lambda p, dab: p.circle(dab.position, 2)
    draw.brush.paint(pen, brush, [draw.brush.Sample((0, 0)), ((20, 10), 0.6)])
    geometry.mesh.render.drawMesh(
        pen,
        geometry.mesh.box((-1, -1, -1), (1, 1, 1)),
        geometry.mesh.camera.Matrix(),
        geometry.mesh.camera.Camera(),
    )


def setup(ctx: sketch.SketchContext) -> None:
    ctx.canvas(320, 240)
    ctx.render(compose.graphics("ink", paint))
    assert_type(ctx.engine.timer(lambda delta, elapsed: elapsed < 2), motion.Timer)
    ctx.engine.timer(lambda: None, stepRate=60)
    hub = ctx.assets.hub()
    assert_type(hub, io.Hub)
    feed = hub.listen("udp://:27021", io.ListenOptions(capacity=64))
    assert_type(feed, io.Feed)
    assert_type(feed.latest(), io.Message | None)
    assert_type(feed.receive(), io.Message | None)
    if (arrival := feed.latest()) is not None:
        assert_type(arrival.payload, bytes)
        assert_type(arrival.sender, str)
        assert_type(feed.send(memoryview(arrival.payload), to=arrival.sender), bool)
    assert_type(hub.write("output.json", b"{}"), bool)


standalone = io.Hub()
io.registerTransports(standalone, ["udp"])
assert_type(standalone.poll(), bool)
standalone.advance(0.5)
