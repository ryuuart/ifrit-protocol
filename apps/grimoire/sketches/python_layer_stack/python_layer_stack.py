"""A picture built from layers: gilt rings, words baked from a compose
tree and squashed, a hatching shown only inside the words, and a disc that
erases everything beneath it as it circles.

The picture is a `draw.Stack` built once in setup, four named layers at the
canvas's size composited bottom to top. Each layer is drawn with a pen of
its own or holds a compose tree baked into it, and carries how it meets the
layers beneath it: a blend, an opacity, a placement and a mask. `draw` moves
two placements and puts the stack down with `pen.stack`; the panel at the
left reads each layer's settings back from the stack.

EDIT THESE FIRST
  RADII — the gilt rings, in canvas units from the centre.
  WORDS — what the field layer bakes.
  SQUASH — the field's placement: its width and height factors.
  ORBIT — how far from the centre the erasing disc circles.

TAGS: Python, Drawing/Generative, Materials/Compositing
"""

from math import cos, pi, sin

from sigil import compose, draw, material
from sigil.draw import Pen
from sigil.sketch import sketch

NAME = "Layer stack"
CATEGORY = "Draw · Generative"

WIDTH, HEIGHT = 1200, 800
CENTER = (WIDTH / 2, HEIGHT / 2)
GROUND = "#0c0e13"

RADII = (120, 180, 240)
GOLD = "#f3d27a"
UMBER = "#a8752a"
TICK = "#c9a24a"

WORDS = "666 666 666"
FIELD_INK = "#7fd1c7"
FIELD_FACE = "Avenir Next, Avenir, Helvetica Neue, sans-serif"
SQUASH = (0.85, 1.0)

HATCH_INK = "#ff7a6b"
HATCH_SPACING = 11
HATCH_SPEED = 14

CUT_DIAMETER = 210
ORBIT = 150
ORBIT_SECONDS = 16

PANEL_FACE = "Menlo"
PANEL_INK = "#d9dee8"
PANEL_QUIET = "#7d8697"


def drawRings(pen: Pen) -> None:
    """The gilt rings, swept by one conic gradient round the centre, inside
    a dial of ticks."""
    pen.noFill()
    pen.stroke(material.conicGradient([GOLD, UMBER, GOLD], from_=-40))
    pen.strokeWeight(10)
    for radius in RADII:
        pen.circle(*CENTER, 2 * radius)
    pen.stroke(TICK)
    for index in range(120):
        angle = 2 * pi * index / 120
        inner = 262
        outer = 292 if index % 10 == 0 else 276
        pen.strokeWeight(3 if index % 10 == 0 else 1.5)
        pen.line(
            CENTER[0] + inner * cos(angle),
            CENTER[1] + inner * sin(angle),
            CENTER[0] + outer * cos(angle),
            CENTER[1] + outer * sin(angle),
        )


def drawHatching(pen: Pen) -> None:
    """Slanted lines across the whole canvas, one spacing wider on each side
    so the layer can slide by a spacing without showing its edge."""
    pen.stroke(HATCH_INK)
    pen.strokeWeight(3.5)
    slant = HEIGHT * 0.45
    offset = -slant - HATCH_SPACING
    while offset < WIDTH + HATCH_SPACING:
        pen.line(offset, 0, offset + slant, HEIGHT)
        offset += HATCH_SPACING


def fieldTree() -> compose.Element:
    words = (
        compose.text(WORDS, size=150, color=FIELD_INK)
        .fontFamily(FIELD_FACE)
        .fontWeight(900)
    )
    return compose.box(words).alignItems("center").justifyContent("center")


def describeLayer(layer: draw.Layer) -> str:
    parts = [layer.blend().name.lower()]
    if layer.opacity < 1:
        parts.append(f"{layer.opacity:.1f}")
    mask = layer.mask()
    if mask is not None:
        parts.append(f"mask {mask.name}")
    return " · ".join(parts)


def buildStack() -> draw.Stack:
    """The picture: four layers, each drawn or baked once, bottom to top.
    The same lines typed at a prompt build the same stack."""
    stack = draw.Stack((WIDTH, HEIGHT))

    rings = stack.layer("rings")
    with rings.pen() as pen:
        drawRings(pen)

    field = stack.layer("field")
    field.bake(fieldTree())
    field.transform(scale=SQUASH, origin=CENTER)
    field.blend("screen")
    field.opacity = 0.8

    # The hatching shows only where the field has ink, wherever the field's
    # own placement puts that ink.
    hatch = stack.layer("hatch")
    with hatch.pen() as pen:
        drawHatching(pen)
    hatch.mask(field)

    # A layer that removes deletes what lies beneath it wherever it has ink:
    # the disc cuts through the rings, the words and the hatching.
    cut = stack.layer("cut")
    with cut.pen() as pen:
        pen.noStroke()
        pen.fill(255)
        pen.circle(*CENTER, CUT_DIAMETER)
    cut.blend("remove")
    return stack


@sketch(size=(WIDTH, HEIGHT), background=GROUND, captureAt=2.5)
class LayerStack:
    def setup(self) -> None:
        self.stack = buildStack()

    def draw(self, pen: Pen) -> None:
        seconds = pen.millis() / 1000
        turn = 2 * pi * seconds / ORBIT_SECONDS
        self.stack.find("cut").transform(
            translate=(ORBIT * cos(turn), ORBIT * sin(turn))
        )
        slide = (seconds * HATCH_SPEED) % HATCH_SPACING
        self.stack.find("hatch").transform(translate=(slide, 0))

        pen.background(GROUND)
        pen.stack(self.stack)
        self.drawPanel(pen)

    def drawPanel(self, pen: Pen) -> None:
        """The stack's layers as a layers panel lists them, top first, each
        with the settings it meets the layers beneath it with."""
        pen.noStroke()
        pen.textFont(PANEL_FACE, 13)
        pen.fill(PANEL_QUIET)
        pen.text("LAYERS, TOP FIRST", 48, 620)
        for row, layer in enumerate(reversed(self.stack.layers)):
            baseline = 650 + row * 24
            pen.fill(PANEL_INK)
            pen.text(layer.name, 48, baseline)
            pen.fill(PANEL_QUIET)
            pen.text(describeLayer(layer), 120, baseline)
