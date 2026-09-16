"""Random samples expose the same sine-composed field as observable_flowfield_3.

Requires the optional ``studies`` package extra.
"""

# TAGS: Drawing/Generative

from math import tau

import numpy as np
from sigil.core import chance
from sigil.draw import SQUARE, PointMode
from sigil.sketch import sketch

REQUIRES = ("numpy",)


@sketch(size=(720, 720), capture_at=0.05)
class FlowField:
    def setup(self, ctx):
        self.samples = np.array(
            [
                (
                    chance.Stream.mix64(i * 2 + 0x6BCECD).unit() * ctx.width,
                    chance.Stream.mix64(i * 2 + 0x883A00).unit() * ctx.height,
                )
                for i in range(14_000)
            ]
        )
        self.lines = np.empty((len(self.samples) * 2, 2), dtype=np.float32)
        self.lines[::2] = self.samples

    def draw(self, pen):
        pen.background(0)
        pen.noFill()
        pen.strokeCap(SQUARE)
        pen.stroke(255, 190)
        pen.strokeWeight(0.55)
        t = pen.millis() / 1000
        x, y = self.samples.T
        angle = (np.sin(x * 0.01 + t * 0.22) + np.sin(y * 0.01 - t * 0.19)) * tau
        self.lines[1::2, 0] = x + np.cos(angle) * 20
        self.lines[1::2, 1] = y + np.sin(angle) * 20
        pen.canvas().drawPoints(PointMode.Lines, self.lines, pen.strokePaint())
