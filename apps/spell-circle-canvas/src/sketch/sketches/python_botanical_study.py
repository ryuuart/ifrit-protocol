"""A field specimen: one leaf silhouette receives wash, dry mass, hatch and veins.

The branch and leaf construction follow the native botanical brush example.
The arrangement is invented; pigment deposition is the native brush engine.
TAGS: Drawing/Brushes, Drawing/Generative
"""

from math import cos, pi, sin

from sigil.draw import CENTER, LEFT, Pen, brush
from sigil.sketch import SketchContext, sketch

LEAVES = [
    ((286, 626), 184, 48, -2.45, -0.10),
    ((354, 584), 196, 54, 0.34, 0.13),
    ((434, 536), 205, 57, -2.48, 0.09),
    ((503, 487), 192, 51, 0.12, -0.11),
    ((584, 431), 211, 61, -2.57, -0.08),
    ((650, 373), 185, 47, -0.06, 0.12),
    ((720, 310), 180, 50, -2.62, 0.10),
    ((779, 248), 157, 43, -0.46, -0.08),
    ((829, 190), 139, 39, -2.68, 0.06),
    ((865, 147), 115, 33, -0.63, -0.06),
]
PIGMENTS = [
    (0.10, 0.31, 0.22, 1),
    (0.20, 0.47, 0.31, 1),
    (0.05, 0.40, 0.42, 1),
    (0.47, 0.53, 0.13, 1),
    (0.73, 0.42, 0.10, 1),
]
BRANCH = [
    (145, 718, 0.24),
    (292, 624, 1),
    (452, 526, 0.72),
    (624, 407, 0.96),
    (778, 258, 0.54),
    (906, 102, 0.10),
]
BERRIES = [
    (903, 111),
    (936, 128),
    (918, 151),
    (957, 160),
    (888, 166),
    (934, 190),
    (971, 119),
]


def leaf_geometry(pen, base, length, width, angle, bend):
    """A local leaf coordinate system serves both the silhouette and its veins."""
    axis, normal = (cos(angle), sin(angle)), (-sin(angle), cos(angle))

    def at(t, side=0, roughness=1):
        envelope = max(0, sin(pi * t)) ** 0.72
        across = sin(pi * t) * bend * length
        across += side * width * envelope * (1 + side * 0.08 * sin(pi * t)) * roughness
        return (
            base[0] + axis[0] * length * t + normal[0] * across,
            base[1] + axis[1] * length * t + normal[1] * across,
        )

    silhouette = [
        at(step / 22, side, pen.random(0.975, 1.025))
        for side, steps in [(1, range(23)), (-1, range(22, -1, -1))]
        for step in steps
    ]
    return brush.Polygon(silhouette), at


def veins(pen, at, pigment):
    lead = brush.pencil(pigment, 1.25)
    lead.opacity, lead.scatter = 0.62, 0.08
    brush.spline(
        pen,
        lead,
        [
            (at(t), pressure)
            for t, pressure in [(0, 0.18), (0.34, 0.95), (0.72, 0.58), (1, 0.08)]
        ],
        0.72,
    )
    lead.width, lead.opacity = 0.72, 0.38
    for index in range(1, 6):
        t = 0.14 + index * 0.12
        for side in (-0.83, 0.83):
            brush.line(pen, lead, at(t), at(t + 0.055, side), 0.68, 0.06)


@sketch(size=(1100, 780), background="#f7f1de", capture_at=0)
class BotanicalStudy:
    def setup(self, ctx: SketchContext) -> None:
        self.brushes = brush.Engine()
        self.brushes.scaleBrushes(1.15)

    def draw(self, pen: Pen) -> None:
        pen.noLoop()
        pen.randomSeed(0xB07A11CA)
        pen.noiseSeed(0xB07A11CA)
        pen.background("#f7f1de")
        pen.stroke(80, 61, 40, 13)
        for _ in range(2800):
            pen.strokeWeight(pen.random(0.25, 0.9))
            pen.point(pen.random(pen.width), pen.random(pen.height))

        stem = brush.charcoal((0.27, 0.20, 0.12, 1), 6.4)
        stem.opacity, stem.bristles = 0.48, 17
        brush.spline(pen, stem, BRANCH, 0.84)
        twig = brush.pencil((0.30, 0.22, 0.13, 1), 2.1)
        twig.opacity = 0.66
        for base, _, _, angle, _ in LEAVES:
            join = (base[0] + 24 * cos(angle), base[1] + 24 * sin(angle))
            brush.line(pen, twig, base, join, 0.88, 0.18)

        brushes = self.brushes
        for index, spec in enumerate(LEAVES):
            polygon, at = leaf_geometry(pen, *spec)
            pigment = PIGMENTS[index % len(PIGMENTS)]
            edge = (pigment[0] * 0.47, pigment[1] * 0.48, pigment[2] * 0.43, 1)
            brushes.push()
            brushes.set("HB", edge, 0.78)
            brushes.wash(pigment, 0.055)
            if index % 3 == 1:
                brushes.noFill()
                brushes.noHatch()
                brushes.mass(
                    "pastel",
                    pigment,
                    brush.Mass(
                        precision=0.56,
                        strength=0.42,
                        gradient=0.24 if index % 2 == 0 else -0.18,
                        outline=False,
                    ),
                )
            else:
                brushes.noMass()
                brushes.fill(pigment, 0.13)
                brushes.fillBleed(0.14, brush.BleedDirection.Out, spec[3])
                brushes.fillTexture(0.58, 0.52, True)
                brushes.hatchStyle("2H", edge, 0.42)
                brushes.hatch(
                    brush.Hatch(
                        spacing=12 + index % 3 * 2,
                        angle=spec[3],
                        jitter=0.12,
                        gradient=0.16 if index % 2 == 0 else -0.12,
                    )
                )
            polygon.show(pen, brushes)
            brushes.pop()
            veins(pen, at, edge)

        fruit_stem = brush.pencil((0.30, 0.23, 0.13, 1), 1.2)
        fruit_stem.opacity = 0.62
        for berry in BERRIES:
            brush.line(pen, fruit_stem, (902, 111), berry, 0.72, 0.12)
        brushes.set("2B", (0.42, 0.08, 0.07, 1), 0.65)
        brushes.fill((0.74, 0.12, 0.10, 1), 0.19)
        brushes.fillBleed(0.18)
        brushes.fillTexture(0.72, 0.62, True)
        brushes.noWash()
        brushes.noHatch()
        brushes.noMass()
        for index, (x, y) in enumerate(BERRIES):
            brushes.circle(pen, x, y, 12 + index % 3 * 2.5, 0.28)

        pen.noStroke()
        pen.fill(55, 48, 38, 210)
        pen.textAlign(LEFT, CENTER)
        pen.textSize(15)
        pen.text("FIELD STUDY  /  WET + DRY", 58, 50)
        pen.textSize(12)
        pen.fill(92, 83, 62, 180)
        pen.text("NATURAL MEDIA    /    10 LEAVES, 5 PIGMENTS", 715, 727)
