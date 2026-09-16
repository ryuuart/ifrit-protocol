"""Random samples expose the same sine-composed field as observable_flowfield_3."""

from math import cos, sin, tau

from sigil.core import chance
from sigil.draw import SQUARE, PointMode
from sigil.sketch import sketch


@sketch(size=(720, 720), capture_at=0.05)
class FlowField:
    def setup(self, ctx):
        self.samples = [
            (
                chance.Stream.mix64(i * 2 + 0x6BCECD).unit() * ctx.width,
                chance.Stream.mix64(i * 2 + 0x883A00).unit() * ctx.height,
            )
            for i in range(14_000)
        ]

    def draw(self, pen):
        pen.background(0)
        pen.noFill()
        pen.strokeCap(SQUARE)
        pen.stroke(255, 190)
        pen.strokeWeight(0.55)
        t = pen.millis() / 1000
        lines = []
        for x, y in self.samples:
            angle = (sin(x * 0.01 + t * 0.22) + sin(y * 0.01 - t * 0.19)) * tau
            lines.extend(((x, y), (x + cos(angle) * 20, y + sin(angle) * 20)))
        pen.canvas().drawPoints(PointMode.Lines, lines, pen.strokePaint())
