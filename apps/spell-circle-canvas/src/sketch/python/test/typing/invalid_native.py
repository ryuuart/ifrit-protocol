"""Each marked line must produce its specified strict diagnostic."""

from sigil.native import compose, data, draw, io, material, motion, weave

compose.box().width({"pixels": 20})  # error: reportArgumentType
compose.box().opacity("opaque")  # error: reportArgumentType
compose.box().fill(object())  # error: reportArgumentType
compose.box().padding(1, 2, 3)  # error: reportCallIssue
compose.memo(2, lambda value: str(value))  # error: reportArgumentType
weave.Type(misspelled=12)  # error: reportCallIssue
material.skia.Paint.sksl("code", {"gain": object()})  # error: reportArgumentType
motion.animate(42)  # error: reportCallIssue,reportArgumentType
data.Table().filter(lambda row: "yes")  # error: reportArgumentType
io.Hub().write("output.json", {"value": 1})  # error: reportArgumentType
io.Hub().feed(42)  # error: reportArgumentType
io.Hub().feed("udp://:27021").send("text")  # error: reportArgumentType
io.FeedPolicy(capacity="unbounded")  # error: reportArgumentType


def invalid(pen: draw.Pen) -> None:
    pen.line("left", "right")  # error: reportArgumentType
    canvas = pen.canvas()
    canvas.drawPoints(draw.PointMode.Lines, [], "paint")  # error: reportArgumentType
