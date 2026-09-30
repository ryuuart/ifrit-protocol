"""Study · Diagram — a material and decoding study of the Voyager record cover.

An engraved cover, symbolic playback instructions, a radial pulsar diagram,
hydrogen states and a live vertical image raster share an archival plate.

TAGS: Studies/Science, Studies/Diagrams, Materials/Metal, Drawing/Engraving
TAGS: Typography/Technical, Motion/Signals, Media/Transmission
"""

from math import cos, pi, sin, sqrt
from pathlib import Path
from random import Random

from sigil.compose import Cache, box, pen
from sigil.draw import CENTER, CLOSE, LEFT, Pen
from sigil.material import shader
from sigil.sketch import SketchContext, sketch

WIDTH, HEIGHT = 1560, 1100
CX, CY, RADIUS = 486, 550, 397
ETCH = "#f6dfa2"
QUIET = "#929f9f"
PAPER = "#d5ded8"
GROUND = "#111d23"


def label(
    p: Pen,
    words: str,
    x: float,
    y: float,
    size: float = 13,
    color: str = PAPER,
    center: bool = False,
    family: str = "Menlo",
) -> None:
    p.noStroke()
    p.fill(color)
    p.textFont(family, size)
    p.textAlign(CENTER if center else LEFT)
    p.text(words, x, y)


def poly(p: Pen, points, closed=False) -> None:
    p.beginShape()
    for x, y in points:
        p.vertex(x, y)
    if closed:
        p.endShape(CLOSE)
    else:
        p.endShape()


def arrow(p: Pen, x1, y1, x2, y2, size=6) -> None:
    p.line(x1, y1, x2, y2)
    dx, dy = x2 - x1, y2 - y1
    d = sqrt(dx * dx + dy * dy)
    if d:
        ux, uy = dx / d, dy / d
        p.line(
            x2, y2, x2 - size * ux + size * uy * 0.55, y2 - size * uy - size * ux * 0.55
        )
        p.line(
            x2, y2, x2 - size * ux - size * uy * 0.55, y2 - size * uy + size * ux * 0.55
        )


def binary(p: Pen, value: str, x, y, angle=0, spacing=5, color=ETCH) -> None:
    p.push()
    p.translate(x, y)
    p.rotate(angle)
    p.noFill()
    p.stroke(color)
    p.strokeWeight(0.9)
    for i, bit in enumerate(value):
        bx = i * spacing
        p.line(bx, -4 if bit == "1" else 0, bx, 4 if bit == "1" else 0)
        if bit == "0":
            p.line(bx - 1.2, 0, bx + 1.2, 0)
    p.pop()


def playback(p: Pen) -> None:
    x, y, r = 314, 356, 106
    p.noFill()
    p.stroke(ETCH)
    p.strokeWeight(1.55)
    p.circle(x, y, r * 2)
    p.circle(x, y, 17)
    p.circle(x, y, 4)
    for i in range(6):
        p.strokeWeight(0.38)
        p.circle(x, y, (r - 6 - i * 3) * 2)
    p.strokeWeight(1.4)
    for i in range(52):
        a = i / 52 * 2 * pi
        if -0.25 < a < 0.3:
            continue
        s = r + 6
        t = s + (7 if i % 3 == 0 else 3)
        p.line(x + cos(a) * s, y + sin(a) * s, x + cos(a) * t, y + sin(a) * t)
    # The stylus contact begins on the outer groove, beside a pickup body.
    poly(
        p,
        [
            (431, 316),
            (438, 314),
            (444, 323),
            (444, 363),
            (437, 367),
            (431, 356),
            (431, 316),
        ],
    )
    p.rect(434, 328, 6, 20)
    p.line(437, 367, 423, 385)
    arrow(p, 411, 293, 396, 279, 5)
    binary(p, "100110001011001101101110000000000", 225, 221, spacing=5)
    p.line(209, 509, 419, 509)
    p.line(214, 513, 415, 513)
    poly(p, [(417, 493), (424, 491), (426, 505), (422, 516), (417, 507)])
    arrow(p, 313, 530, 417, 530, 5)
    binary(p, "1010011001101011011000100000000000", 244, 555, spacing=4.7)
    label(p, "01", 215, 468, 11, ETCH)


def cover_raster(p: Pen) -> None:
    p.noFill()
    p.stroke(ETCH)
    p.strokeWeight(1.3)
    # A square synchronising pulse introduces three illustrative image lines.
    points = []
    for i in range(15):
        x = 527 + i * 5
        points.extend([(x, 268 if i % 2 else 302), (x + 5, 268 if i % 2 else 302)])
    points += [(602, 302), (602, 324), (625, 324), (625, 286), (644, 286)]
    for i in range(62):
        x = 644 + i * 2.6
        y = 286 - sin(i * 0.73) * (11 + sin(i * 0.26) * 8)
        points.append((x, y))
    poly(p, points)
    for i in range(3):
        binary(p, bin(i + 1)[2:], 661 + i * 61, 257, spacing=6)
    poly(
        p,
        [
            (551, 350),
            (595, 390),
            (618, 337),
            (658, 384),
            (681, 332),
            (723, 385),
            (747, 331),
            (790, 379),
        ],
    )
    for i, x in enumerate((596, 658, 723)):
        p.circle(x, 387, 5)
        binary(p, bin(i + 1)[2:], x - 3, 408, spacing=5)
    p.rect(572, 444, 156, 109)
    for i in range(18):
        p.strokeWeight(0.46)
        p.line(576 + i * 2.3, 447, 576 + i * 2.3, 550)
    p.strokeWeight(1.3)
    arrow(p, 577, 432, 716, 432, 5)
    binary(p, "1000000000", 625, 419, spacing=5)
    p.rect(572, 578, 156, 109)
    p.circle(650, 633, 72)
    label(p, "02", 740, 519, 11, ETCH)


def pulsars(p: Pen) -> None:
    ox, oy = 337, 718
    angles = [
        -0.05,
        0.32,
        0.61,
        0.99,
        1.33,
        1.81,
        2.34,
        2.86,
        3.24,
        3.63,
        4.11,
        4.48,
        4.81,
        5.12,
    ]
    lengths = [333, 205, 232, 190, 215, 188, 175, 157, 154, 166, 180, 210, 239, 190]
    p.noFill()
    p.stroke(ETCH)
    p.strokeWeight(1.15)
    for i, (a, reach) in enumerate(zip(angles, lengths)):
        ex, ey = ox + cos(a) * reach, oy + sin(a) * reach
        p.line(ox, oy, ex, ey)
        p.line(ex - sin(a) * 3, ey + cos(a) * 3, ex + sin(a) * 3, ey - cos(a) * 3)
        # Bit strings are illustrative visual data rather than navigation data.
        bits = format((17831 + i * 1147) & 65535, "016b")
        binary(
            p,
            bits,
            ox + cos(a) * (reach * 0.48) - sin(a) * 6,
            oy + sin(a) * (reach * 0.48) + cos(a) * 6,
            a,
            spacing=3.1,
        )
    p.circle(ox, oy, 9)
    label(p, "03", 685, 751, 11, ETCH)


def hydrogen(p: Pen, x, y, r=23, color=ETCH) -> None:
    p.noFill()
    p.stroke(color)
    p.strokeWeight(1.15)
    for i in range(2):
        cx = x + i * 79
        p.circle(cx, y, 2 * r)
        p.circle(cx, y, 3)
        p.line(cx, y - r - 12, cx, y - r + 6)
        p.line(cx, y + r - 6, cx, y + r + 10)
        arrow(p, cx, y - 13, cx, y - 3, 3)
        arrow(p, cx, y + 4 if i else y + 15, cx, y + 15 if i else y + 4, 3)
    p.line(x + r, y, x + 79 - r, y)
    binary(p, "1", x + 39, y + 18)


def plate(p: Pen) -> None:
    label(p, "VOYAGER", 69, 84, 52, PAPER, family="Helvetica Neue")
    label(p, "AN INSTRUCTION PLATE FOR AN UNKNOWN READER", 74, 119, 13)
    label(p, "INTERSTELLAR MESSAGE / 1977", 1150, 70, 13, QUIET)
    p.stroke(QUIET)
    p.strokeWeight(0.6)
    p.line(72, 147, 1488, 147)
    p.line(934, 173, 934, 954)
    for i in range(120):
        a = i * 2 * pi / 120
        s = RADIUS + 9
        t = s + (10 if i % 10 == 0 else 3)
        p.stroke("#63777a" if i % 10 == 0 else "#334a51")
        p.line(CX + cos(a) * s, CY + sin(a) * s, CX + cos(a) * t, CY + sin(a) * t)
    label(p, "GOLD-PLATED RECORD / ENGRAVED COVER", 486, 999, 12, QUIET, True)
    p.push()
    p.clip(lambda: p.circle(CX, CY, RADIUS * 2 - 7))
    playback(p)
    cover_raster(p)
    pulsars(p)
    hydrogen(p, 666, 827)
    p.noFill()
    p.stroke(ETCH)
    p.strokeWeight(1.3)
    p.circle(473, 565, 22)
    p.circle(473, 565, 12)
    label(p, "04", 732, 885, 11, ETCH)
    p.pop()

    # The reading column separates clock, playback, raster and location scales.
    x = 974
    sections = [
        (195, "01 / ESTABLISH A CLOCK", "Hydrogen supplies a shared time unit."),
        (373, "02 / FOLLOW THE GROOVE", "A stylus reads from the edge inward."),
        (567, "03 / RECONSTRUCT AN IMAGE", "Vertical lines unfold into a raster."),
        (803, "04 / LOCATE THE SOURCE", "Radial directions form a pulsar diagram."),
    ]
    for y, heading, prose in sections:
        label(p, heading, x, y, 16, PAPER)
        label(p, prose, x, y + 27, 13, QUIET, family="Helvetica Neue")
        p.stroke("#3c5158")
        p.strokeWeight(0.6)
        p.line(x, y + 44, 1488, y + 44)
    hydrogen(p, 1008, 287, 21, PAPER)
    label(p, "t₀ = 0.70 × 10⁻⁹ s", 1138, 282, 17)
    label(p, "ONE TRANSITION / ONE UNIT", 1138, 311, 10, QUIET)
    p.noFill()
    p.stroke(PAPER)
    p.strokeWeight(1)
    p.circle(1036, 473, 102)
    p.circle(1036, 473, 10)
    arrow(p, 1085, 460, 1077, 444)
    p.line(1090, 432, 1103, 460)
    p.line(1103, 460, 1085, 460)
    label(p, "3.6 s / REVOLUTION", 1138, 461, 17)
    label(p, "OUTSIDE → INSIDE", 1138, 489, 12, QUIET)
    p.noFill()
    p.stroke(PAPER)
    p.rect(983, 632, 188, 119)
    p.circle(1077, 691, 74)
    for i in range(46):
        p.stroke("#425a62")
        p.strokeWeight(0.5)
        p.line(986 + i * 4, 635, 986 + i * 4, 748)
    label(p, "512 VERTICAL LINES", 1195, 660, 17)
    label(p, "VERIFY ASPECT RATIO", 1195, 689, 12, QUIET)
    label(p, "A CIRCLE MUST STAY ROUND", 1195, 714, 10, QUIET)
    label(p, "14 PULSAR DIRECTIONS", 985, 883, 17)
    label(p, "BIT PATTERNS HERE ARE ILLUSTRATIVE", 985, 911, 11, QUIET)
    p.stroke("#425a62")
    p.strokeWeight(0.6)
    p.line(72, 1034, 1488, 1034)
    label(p, "TRANSMISSION / SOUND → SIGNAL → GEOMETRY", 73, 1065, 11, QUIET)
    label(p, "SCHEMATIC RECONSTRUCTION                 PLATE 01", 1001, 1065, 11, QUIET)


def raster_motion(p: Pen) -> None:
    phase = (p.millis() / 1000 / 4) % 1
    x = 986 + phase * 182
    p.stroke("#b7eee4")
    p.strokeWeight(1.5)
    p.line(x, 635, x, 748)
    p.noStroke()
    p.fill("#b7eee435")
    p.rect(max(986, x - 7), 635, min(7, x - 986), 113)
    p.fill("#b7eee4")
    p.circle(x, 756, 3)


@sketch(size=(WIDTH, HEIGHT), background=GROUND, capture_at=2.4)
class VoyagerInstructionPlate:
    def setup(self, ctx: SketchContext) -> None:
        source = Path(__file__).with_name("gold.sksl").read_text()
        disc = (
            box()
            .absolute()
            .left(CX - RADIUS)
            .top(CY - RADIUS)
            .width(RADIUS * 2)
            .height(RADIUS * 2)
            .borderRadius(RADIUS)
            .fill(shader(source))
        )
        rim = pen("voyager.rim", self.rim, cache=Cache.Picture).absolute().inset(0)
        engraving = (
            pen("voyager.engraving", plate, cache=Cache.Picture).absolute().inset(0)
        )
        signal = pen("voyager.raster", raster_motion).absolute().inset(0)
        ctx.render(
            box().width(WIDTH).height(HEIGHT).children(disc, rim, engraving, signal)
        )

    @staticmethod
    def rim(p: Pen) -> None:
        p.noFill()
        for inset, weight, color in (
            (0, 2.4, "#d4a658"),
            (3, 0.6, "#f1d796"),
            (6, 0.7, "#49381d"),
        ):
            p.stroke(color)
            p.strokeWeight(weight)
            p.circle(CX, CY, (RADIUS - inset) * 2)
        # Scratches are fixed cuts, with shorter cuts crossing the grain.
        rng = Random(1977)
        p.push()
        p.clip(lambda: p.circle(CX, CY, (RADIUS - 6) * 2))
        for i in range(460):
            x, y = rng.uniform(92, 880), rng.uniform(155, 944)
            length = rng.uniform(4, 49)
            p.stroke("#ffe9b311" if i % 3 else "#19140d19")
            p.strokeWeight(rng.uniform(0.22, 0.65))
            p.line(x, y, x + length, y - length * 0.52)
        p.pop()
