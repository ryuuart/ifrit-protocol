"""Liquid nibs, wet fibres and pigment blooms over a generated drafting grid.

The composition follows the native liquid-layer brush example. Python describes
moving control points; spline sampling, nibs, fibres and spray execute natively.
TAGS: Drawing/Brushes, Drawing/Generative, Motion/Animation
"""

from math import cos, pi, sin, tau

from sigil.draw import ADD, CLOSE, ROUND, SCREEN, Pen, brush
from sigil.material import pattern
from sigil.sketch import SketchContext, sketch

PIGMENTS = [
    (0.08, 0.82, 0.88, 1),
    (0.56, 0.18, 0.94, 1),
    (1, 0.36, 0.16, 1),
    (0.96, 0.72, 0.14, 1),
]
RIBBONS = 6


def controls(ribbon, seconds, width, height):
    """Six pressure-bearing knots describe an entire native brush ribbon."""
    return [
        (
            width * (-0.10 + along * 1.20),
            height
            * (
                (ribbon + 1) / (RIBBONS + 1)
                + 0.105
                * sin(
                    along * tau * 1.35
                    + ribbon * 0.8
                    + seconds * (0.34 + ribbon * 0.018)
                )
                + 0.032 * sin(seconds * 0.53 + point + ribbon)
            ),
            0.22 + 0.85 * sin(along * pi),
        )
        for point in range(6)
        for along in [point / 5]
    ]


def liquid_nib(color, width):
    return brush.Tool(
        tip=brush.Tip.Nib,
        color=color,
        width=width,
        spacing=6,
        opacity=0.19,
        pressure=brush.Pressure(0.08, 1.18, 0.10),
        blend=SCREEN,
        sharpness=0.16,
        noise=0.12,
    )


@sketch(size=(960, 700), background="#040912", capture_at=0.8)
class LiquidLayers:
    def setup(self, ctx: SketchContext) -> None:
        self.grid = [
            pattern.gridLines(34, 1.15, (0.18, 0.46, 0.58, 0.34)).paint(),
            pattern.gridLines(136, 2.2, (0.70, 0.82, 0.88, 0.22)).paint(),
        ]

    def draw(self, pen: Pen) -> None:
        seconds = pen.millis() / 1000
        width, height = pen.width, pen.height
        cx, cy = width / 2, height / 2
        pen.randomSeed(0x11A71D)
        pen.background("#050912")
        pen.noStroke()
        for grid in self.grid:
            pen.fill(grid)
            pen.rect(0, 0, width, height)

        pen.strokeCap(ROUND)
        pen.strokeJoin(ROUND)
        pen.noFill()
        pen.stroke(218, 235, 242, 92)
        pen.strokeWeight(2)
        for ring in range(7):
            pen.circle(cx, cy, 95 + ring * 76)
        pen.beginShape()
        for point in range(12):
            angle = seconds * 0.08 + point * tau / 12
            radius = 292 if point % 2 == 0 else 102
            pen.vertex(cx + radius * cos(angle), cy + radius * sin(angle))
        pen.endShape(CLOSE)

        for ribbon in range(RIBBONS):
            pigment = PIGMENTS[ribbon % len(PIGMENTS)]
            path = controls(ribbon, seconds, width, height)
            body = liquid_nib(pigment, 70 + 10 * (ribbon % 3))
            body.pressure.variation = None
            brush.spline(pen, body, path, 0.74)
            wet = brush.watercolor(pigment, body.width * 0.74)
            wet.spacing, wet.bristles = 8, 10
            wet.opacity, wet.scatter = 0.12, 0.52
            wet.pressure, wet.blend = body.pressure, SCREEN
            brush.spline(pen, wet, path, 0.74)

        bloom = brush.spray((1, 0.68, 0.24, 1), 65)
        bloom.opacity, bloom.blend = 0.10, ADD
        for mark in range(14):
            angle = seconds * 0.12 + tau * mark / 14
            x, y = cx + 210 * cos(angle), cy + 162 * sin(angle)
            brush.line(pen, bloom, (x, y), (x + 10 * cos(angle), y + 10 * sin(angle)))
