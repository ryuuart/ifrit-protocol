"""A moving circle and a greeting: a first Python sketch to edit and save.

EDIT THESE FIRST
  COLOR — the circle's fill.
  SPEED — how quickly it moves; zero holds it still.
  The text inside draw() — your greeting.

TAGS: Runtime/Starter, Drawing/Primitives, Motion/Animation
"""

from math import sin

from sigil.draw import Pen
from sigil.sketch import kit, sketch

COLOR = "#efb87e"
SPEED = 1.2


@sketch(size=(640, 420), captureAt=1.0, theme=kit.featureTheme(kit.Density.Spacious))
class Hello:
    def draw(self, pen: Pen) -> None:
        look = kit.theme()
        t = pen.millis() / 1000
        pen.background(look.palette.ground)
        pen.noStroke()
        pen.fill(look.palette.ink)
        pen.textFont(look.font(look.type.title))
        pen.text("Hello, Python.", 40, 82)
        pen.fill(look.palette.ash)
        pen.textFont(look.font(look.type.subtitle))
        pen.text("A little code. Something moving.", 42, 114)

        pen.fill(COLOR)
        pen.circle(pen.width / 2 + 140 * sin(t * SPEED), 245, 76)

        pen.fill(look.palette.ash)
        pen.textFont(look.font(look.type.footer))
        pen.text("Edit COLOR or SPEED, then save.", 42, 380)
