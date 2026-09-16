"""A moving circle and a greeting: a first Python sketch to edit and save.

EDIT THESE FIRST
  COLOR — the circle's fill.
  SPEED — how quickly it moves; zero holds it still.
  The text inside draw() — your greeting.

TAGS: Runtime/Starter, Drawing/Primitives, Motion/Animation
"""

from math import sin

from sigil.draw import Pen
from sigil.sketch import sketch

COLOR = "#efb87e"
SPEED = 1.2


@sketch(size=(640, 420), capture_at=1.0)
class Hello:
    def draw(self, pen: Pen) -> None:
        t = pen.millis() / 1000
        pen.background("#18252e")
        pen.noStroke()
        pen.fill("#f4eee4")
        pen.textSize(42)
        pen.text("Hello, Python.", 40, 82)
        pen.fill("#a7b7bf")
        pen.textSize(16)
        pen.text("A little code. Something moving.", 42, 114)

        pen.fill(COLOR)
        pen.circle(pen.width / 2 + 140 * sin(t * SPEED), 245, 76)

        pen.fill("#a7b7bf")
        pen.textSize(13)
        pen.text("Edit COLOR or SPEED, then save.", 42, 380)
