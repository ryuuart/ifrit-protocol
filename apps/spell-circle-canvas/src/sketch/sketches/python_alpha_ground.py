"""A mark on NOTHING, for whoever composites it — alpha_ground in Python.

``background=(0, 0, 0, 0)`` is a colour with no colour and no coverage.
The pixels this does not draw on carry nothing, so another application
can lay the whole frame over its own scene and see its own scene through
it. A frame offered under ``--publish`` is this canvas, at this size,
over this ground — so what a subscriber receives is a 640 by 360 overlay
and not a picture of the window it was watched in.

    Sketchbook --sketch python_alpha_ground --publish Overlay
    Receiver Overlay --grab overlay.png

EDIT THESE FIRST
  INK — what the mark is drawn in.
  TURN — seconds for one turn of the ring.
  TICKS — how many marks go round it.

TAGS: Drawing/Primitives, Materials/Compositing
"""

from math import tau

from sigil.draw import Pen
from sigil.sketch import sketch

INK = "#f5d66e"
TURN = 6.0
TICKS = 24
RADIUS = 84


@sketch(size=(640, 360), background=(0, 0, 0, 0), capture_at=1.5)
class AlphaGround:
    def draw(self, pen: Pen) -> None:
        # Nothing lays a ground down: what this leaves untouched is what a
        # subscriber sees its own scene through.
        cx = pen.width / 2
        cy = pen.height / 2

        pen.noStroke()
        pen.fill(INK)
        pen.circle(cx, cy, 26)

        pen.noFill()
        pen.stroke(INK)
        pen.strokeWeight(2)
        pen.push()
        pen.translate(cx, cy)
        pen.rotate(pen.millis() / 1000 / TURN * tau)
        for tick in range(TICKS):
            # A long mark every sixth, so the turn reads at a glance.
            length = 18 if tick % 6 == 0 else 8
            pen.line(RADIUS - length, 0, RADIUS, 0)
            pen.rotate(tau / TICKS)
        pen.pop()

        pen.strokeWeight(1)
        pen.line(cx - RADIUS, cy + RADIUS + 24, cx + RADIUS, cy + RADIUS + 24)
