"""Representative direct binding contracts, checked without running sketches."""

from array import array
from dataclasses import dataclass
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
assert_type(
    tree.padding(all=8).margin(left=1, top=2, right=3, bottom=4), compose.Element
)
assert_type(tree.alignItems("auto").justifyContent("space_between"), compose.Element)
assert_type(
    tree.alignItems(compose.Align.Auto).justifyContent(compose.Justify.End),
    compose.Element,
)
assert_type(motion.from_(0).to(1), motion.FromTo)
assert_type(motion.animate(motion.from_(0).to(1)), motion.Transitioned)
assert_type(motion.entrance("#000", "#fff"), motion.ColorTransitioned)
assert_type(
    motion.animate(motion.from_(compose.Fill.none()).to("#fff")),
    motion.FillTransitioned,
)
assert_type(motion.through([(0.0, 1.0), (0.4, 2.0)]), motion.Waypoints)

ink = material.Paint.linear((0, 0), (40, 0), [(0, "#fff"), (1, "#123")])
shader = material.Paint.sksl(
    "uniform float gain; half4 main(float2 p) { return half4(gain); }", {"gain": 0.5}
)
assert_type(shader.uniform("gain", 0.7), material.Paint)
tree.fill(ink).opacity(motion.bind(motion.Output(1.0)))
outline = compose.stroke(2, ink)
outline.strokeFill = ink
outline.trimPhase = None
outline.dashPhaseBinding = motion.Output(0)

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
    pen.background(material.kit.unlit(material.kit.SurfaceParameters()))
    pen.fill(material.kit.unlit(material.kit.SurfaceParameters(baseColor="#e75a31")))

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
    ctx.ticker.add(lambda dt, elapsed: elapsed < 2)
    ctx.ticker.addFixed(60, lambda: None)
    hub = ctx.assets.hub()
    assert_type(hub, io.Hub)
    feed = hub.feed("udp://:27021", io.FeedPolicy(capacity=64))
    assert_type(feed, io.Feed)
    assert_type(feed.latest(), bytes | None)
    assert_type(feed.receive(), io.Arrival | None)
    if (arrival := feed.newest()) is not None:
        assert_type(arrival.bytes, bytes)
        assert_type(arrival.from_, str)
        assert_type(feed.sendTo(arrival.from_, memoryview(arrival.bytes)), bool)
    assert_type(hub.write("output.json", b"{}"), bool)


standalone = io.Hub()
io.registerUdp(standalone)
assert_type(standalone.poll(), bool)
standalone.dispatch(0.5)
