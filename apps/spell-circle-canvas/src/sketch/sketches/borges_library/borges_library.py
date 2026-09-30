"""An original architectural atlas after Borges: rooms, books and uncertain connections.

TAGS: Studies/Cultural, Studies/Literature, Typography/Paragraphs, Drawing/Generative, Diagrams/Architecture, Runtime/Python
"""

from itertools import pairwise
from math import cos, pi, sin
from random import Random

from sigil.compose import Cache, box, text
from sigil.compose import pen as canvas
from sigil.draw import CLOSE, Pen
from sigil.sketch import SketchContext, sketch
from sigil.weave import FrameOptions, Leading, Type

SIZE = (1400, 1000)
PAPER = "#eee3c9"
INK = "#263b42"
PALE = "#c2c3ad"
RED = "#a54532"
GOLD = "#bd9860"
TYPE = "Baskerville, Georgia, serif"
LABEL = "Avenir Next, Helvetica Neue, sans-serif"
MONO = "Menlo, monospace"


def polygon(p: Pen, points, fill=None, weight=0.7, edge=INK):
    if fill is None:
        p.noFill()
    else:
        p.fill(fill)
    if edge is None:
        p.noStroke()
    else:
        p.stroke(edge)
        p.strokeWeight(weight)
    p.beginShape()
    for x, y in points:
        p.vertex(x, y)
    p.endShape(CLOSE)


def hexagon(cx, cy, radius, squash=1):
    return [
        (
            cx + radius * cos(pi / 6 + k * pi / 3),
            cy + radius * squash * sin(pi / 6 + k * pi / 3),
        )
        for k in range(6)
    ]


def along(a, b, t, dy=0):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t + dy)


def line(p, a, b, color=INK, weight=0.65):
    p.stroke(color)
    p.strokeWeight(weight)
    p.line(*a, *b)


def paper(p: Pen):
    p.noStroke()
    p.fill(PAPER)
    p.rect(0, 0, *SIZE)
    rng = Random(711)
    # The uniform grain material has no directional fibre construction;
    # sparse native lines give the stock a physical direction at this scale.
    for _ in range(5200):
        x, y = rng.uniform(0, SIZE[0]), rng.uniform(0, SIZE[1])
        p.stroke(96, 75, 42, rng.randrange(3, 15))
        p.strokeWeight(rng.uniform(0.18, 0.5))
        p.line(x, y, x + rng.uniform(0.4, 3.8), y + rng.uniform(-0.4, 0.4))
    for inset in range(0, 16, 2):
        p.noFill()
        p.stroke(110, 78, 29, 4)
        p.strokeWeight(2)
        p.rect(inset, inset, SIZE[0] - inset * 2, SIZE[1] - inset * 2)


def shelf_wall(p, a, b, height, seed):
    rng = Random(seed)
    polygon(
        p, [a, b, (b[0], b[1] - height), (a[0], a[1] - height)], "#d2c5a7", weight=1.0
    )
    shades = ("#243940", "#596264", "#765541", "#a24431", "#a88955", "#c7b693")
    for row in range(5):
        lo = -height + 4 + row * (height - 5) / 5
        hi = lo + (height - 5) / 5 - 2
        for book in range(32):
            t0, t1 = (book + 0.09) / 32, (book + 0.9) / 32
            drop = rng.uniform(0, 1.5)
            polygon(
                p,
                [
                    along(a, b, t0, lo + drop),
                    along(a, b, t1, lo + drop),
                    along(a, b, t1, hi),
                    along(a, b, t0, hi),
                ],
                shades[rng.randrange(len(shades))],
                edge=None,
            )
            if book % 4 == 0:
                line(
                    p,
                    along(a, b, t0, lo + 3.2),
                    along(a, b, t1, lo + 3.2),
                    "#dacdad",
                    0.3,
                )
        line(p, along(a, b, 0, hi + 1.2), along(a, b, 1, hi + 1.2), INK, 0.7)
    for t in (0, 1 / 3, 2 / 3, 1):
        line(p, along(a, b, t), along(a, b, t, -height), INK, 1.2)


def room(p, cx, cy, radius, index):
    outside = hexagon(cx, cy, radius, 0.25)
    inside = hexagon(cx, cy, radius * 0.46, 0.25)
    polygon(p, outside, "#d9ceb1", 1.05)
    polygon(p, inside, "#293f46", 0.85)
    # Each annular floor is six quads. The central opening stays empty
    # without relying on a contour winding convention in the Python seam.
    for k in range(6):
        polygon(
            p,
            [outside[k], outside[(k + 1) % 6], inside[(k + 1) % 6], inside[k]],
            "#e8ddc2",
            edge=None,
        )
        for t in (0.25, 0.5, 0.75):
            line(
                p,
                along(inside[k], outside[k], t),
                along(inside[(k + 1) % 6], outside[(k + 1) % 6], t),
                "#c8bea5",
                0.35,
            )
    # The two foreground sides are removed for the section. Four intact
    # walls retain five shelves and thirty-two spines on every shelf.
    for k in (2, 3, 4, 5):
        shelf_wall(p, outside[k], outside[(k + 1) % 6], 44, 110 + index * 31 + k)
    for k in range(6):
        a, b = inside[k], inside[(k + 1) % 6]
        line(p, a, b, INK, 1.4)
        line(p, (a[0], a[1] - 11), (b[0], b[1] - 11), INK, 0.85)
        for t in (0, 0.25, 0.5, 0.75):
            q = along(a, b, t)
            line(p, q, (q[0], q[1] - 11), INK, 0.65)
    for k in (0, 1, 2):
        a, b = outside[k], outside[(k + 1) % 6]
        polygon(p, [a, b, (b[0], b[1] + 6), (a[0], a[1] + 6)], "#bcae8d", 0.5)
        for t in range(13):
            q = along(a, b, t / 12)
            line(p, q, (q[0] + 4, q[1] + 5), "#887c64", 0.45)
    for x in (cx - radius * 0.48, cx + radius * 0.48):
        p.noStroke()
        p.fill(GOLD)
        p.circle(x, cy - 27, 7)
        p.noFill()
        p.stroke("#bea66d")
        p.strokeWeight(0.35)
        p.circle(x, cy - 27, 14)
    if index % 2 == 0:
        x, y = cx - 119, cy + 5
        p.noStroke()
        p.fill(INK)
        p.circle(x, y - 15, 4)
        line(p, (x, y - 12), (x + 1, y - 5), INK, 2.0)
        line(p, (x, y - 10), (x + 7, y - 8), INK, 1.0)
        line(p, (x + 1, y - 5), (x - 3, y), INK, 1.0)
        line(p, (x + 1, y - 5), (x + 4, y), INK, 1.0)
        polygon(
            p,
            [(x + 7, y - 12), (x + 11, y - 11), (x + 10, y - 7), (x + 6, y - 8)],
            RED,
            edge=None,
        )


def tower(p: Pen):
    # Axonometric coordinates expose the shaft and repeated rooms. This
    # is an original explanatory projection rather than a spatial model.
    for cx, cy, radius in ((485, 287, 75), (917, 283, 74), (735, 260, 73)):
        for offset in (0, 26, 52):
            polygon(p, hexagon(cx, cy + offset, radius, 0.47), None, 0.45, "#a9ad9b")
            polygon(
                p, hexagon(cx, cy + offset, radius * 0.43, 0.47), None, 0.45, "#a9ad9b"
            )
        line(p, (cx, cy - 33), (cx, cy + 85), "#a9ad9b", 0.4)
    for x in (529, 851):
        line(p, (x, 270), (x, 772), "#abb1a4", 0.5)
    for y in (744, 604, 464, 324):
        room(p, 690, y, 188, y // 108)
    # A narrow vestibule continues right. The spiral is displaced beside
    # the room shaft so its circulation can be read in the section.
    for y in (744, 604, 464, 324):
        polygon(
            p,
            [(851, y - 23), (943, y - 49), (967, y - 36), (875, y - 9)],
            "#d9cdb0",
            0.9,
        )
        line(p, (851, y - 29), (943, y - 55), INK, 0.75)
        for step in range(6):
            q = along((851, y - 23), (943, y - 49), step / 5)
            line(p, q, (q[0], q[1] - 6), INK, 0.65)
    line(p, (960, 299), (960, 755), INK, 1.3)
    for i in range(74):
        theta = i * 0.43
        y = 744 - i * 5.7
        a = (960 + 6 * cos(theta), y + 3 * sin(theta))
        b = (960 + 33 * cos(theta), y + 14 * sin(theta))
        polygon(
            p,
            [a, b, (b[0], b[1] - 3), (a[0], a[1] - 3)],
            "#9d927a" if sin(theta) > 0 else "#d7cbb0",
            0.5,
        )
        line(p, b, (b[0], b[1] - 8), INK, 0.4)


def diagrams(p: Pen):
    # A network sketch shows one selected route through many possible
    # neighbour relations; it does not claim a unique map of the story.
    for row in range(4):
        for col in range(3):
            x = 1134 + col * 68 + (row % 2) * 34
            y = 280 + row * 58
            polygon(p, hexagon(x, y, 38), None, 0.75, "#7b8c88")
            polygon(p, hexagon(x, y, 18), None, 0.4, "#9da18d")
            p.noStroke()
            p.fill("#b7b89e")
            p.circle(x, y, 3)
    route = [(1134, 280), (1168, 338), (1236, 338), (1270, 396), (1236, 454)]
    for a, b in pairwise(route):
        line(p, a, b, RED, 2.2)
    for q in route:
        p.noStroke()
        p.fill(RED)
        p.circle(*q, 6)

    points = hexagon(1214, 630, 83)
    polygon(p, points, None, 1.1)
    polygon(p, hexagon(1214, 630, 38), "#d0c7ab", 0.8)
    for k in (0, 2, 3, 5):
        a, b = points[k], points[(k + 1) % 6]
        for offset in range(5):
            inset_a = along(a, (1214, 630), 0.045 * (offset + 1))
            inset_b = along(b, (1214, 630), 0.045 * (offset + 1))
            line(p, inset_a, inset_b, INK, 0.8)
    for k in (1, 4):
        mid = along(points[k], points[(k + 1) % 6], 0.5)
        line(p, mid, (mid[0], mid[1] + (26 if k == 1 else -26)), RED, 1.8)
    p.noStroke()
    p.fill(RED)
    p.circle(1175, 630, 5)
    p.circle(1253, 630, 5)


def page(p: Pen):
    p.stroke(INK)
    p.strokeWeight(0.85)
    p.line(44, 172, 1356, 172)
    p.line(44, 819, 1356, 819)
    p.line(44, 940, 1356, 940)
    p.stroke("#bcb9a2")
    p.strokeWeight(0.7)
    p.line(341, 204, 341, 786)
    p.line(1046, 204, 1046, 786)
    for x in range(44, 1357, 8):
        p.line(x, 177, x, 181 if (x - 44) % 40 else 187)
    # Four-wall folio count as a real grid: twenty shelves, 640 volumes.
    for wall in range(4):
        start = 439 + wall * 231
        for row in range(5):
            for book in range(32):
                p.noStroke()
                p.fill(INK if (book + row + wall) % 7 else RED)
                p.rect(start + book * 6.4, 863 + row * 9.6, 4.5, 7.5)
    for a, b, c in (
        ((389, 408), (464, 408), (535, 421)),
        ((884, 246), (813, 246), (746, 324)),
        ((911, 577), (996, 552), (1019, 552)),
        ((970, 780), (856, 780), (780, 717)),
    ):
        line(p, a, b, RED, 0.65)
        line(p, b, c, RED, 0.65)
        p.noStroke()
        p.fill(RED)
        p.circle(*c, 4)


def label(
    value,
    x,
    y,
    size=12,
    width=260,
    height=42,
    family=LABEL,
    color=INK,
    tracking=0,
    weight=400,
    leading=None,
):
    node = (
        text(value)
        .fontFamily(family)
        .font(Type(size=size, weight=weight))
        .ink(color)
        .absolute()
        .left(x)
        .top(y)
        .width(width)
        .height(height)
        .letterSpacing(tracking)
        .textFirstBaseline(FrameOptions.FirstBaseline.CapHeight)
    )
    if leading:
        node.lineHeight(Leading.absolute(leading))
    return node


@sketch(size=SIZE, background=PAPER, capture_at=0)
class BorgesLibrary:
    def setup(self, ctx: SketchContext) -> None:
        words = [
            label("ATLAS DE ESPACIOS IMAGINADOS", 44, 34, 12, 780, tracking=2.4),
            label("LA BIBLIOTECA", 42, 69, 75, 980, 80, TYPE, tracking=-1.3),
            label("DE BABEL", 1023, 84, 42, 335, 66, TYPE, color=RED),
            label(
                "J. L. BORGES / ARCHITECTURAL READING", 46, 148, 11, 890, tracking=1.7
            ),
            label(
                "FOLIO 06  •  SECTION / PLAN / COUNT", 1060, 149, 10, 298, tracking=0.8
            ),
            label("01  /  LA REGLA", 44, 215, 12, color=RED, tracking=1.5),
            label(
                "Una habitación.\nTodos los mundos.",
                44,
                250,
                31,
                273,
                91,
                TYPE,
                leading=34,
            ),
            label(
                "La galería hexagonal se repite arriba, abajo y más allá del pasillo. La sala conserva su forma; los libros agotan las combinaciones de un alfabeto limitado.",
                44,
                352,
                17,
                260,
                128,
                TYPE,
                leading=23,
            ),
            label(
                "A room becomes a cosmology. Repetition offers a measure; the search for meaning exceeds it. This cutaway makes one possible route legible.",
                44,
                493,
                16,
                260,
                124,
                TYPE,
                leading=22,
            ),
            label("READING THE SECTION", 44, 639, 10, tracking=1.5),
            label(
                "A   Repeated galleries\nB   Central ventilation shaft\nC   Vestibule + spiral stair\nD   Two spherical lamps",
                44,
                670,
                12,
                269,
                102,
                MONO,
                leading=23,
            ),
            label(
                "02  /  EXPLODED AXONOMETRIC CUTAWAY",
                382,
                215,
                11,
                600,
                color=RED,
                tracking=1.5,
            ),
            label("A", 386, 386, 16, 20, family=TYPE, color=RED),
            label("B", 881, 226, 16, 20, family=TYPE, color=RED),
            label("C", 1004, 529, 16, 20, family=TYPE, color=RED),
            label("D", 966, 757, 16, 20, family=TYPE, color=RED),
            label("LEVEL +02", 374, 319, 9, 80, family=MONO, color="#788379"),
            label("LEVEL +01", 374, 459, 9, 80, family=MONO, color="#788379"),
            label("LEVEL  00", 374, 599, 9, 80, family=MONO, color="#788379"),
            label("LEVEL −01", 374, 739, 9, 80, family=MONO, color="#788379"),
            label("THE TWO FRONT WALLS ARE CUT AWAY", 416, 801, 8, 520, tracking=1.4),
            label("03  /  CONTINUIDAD", 1080, 215, 11, color=RED, tracking=1.4),
            label("One selected itinerary", 1096, 494, 14, 245, family=TYPE),
            label(
                "Connections are interpretive;\nthe story leaves their geometry open.",
                1096,
                518,
                11,
                245,
                43,
                family=TYPE,
                leading=15,
            ),
            label("04  /  PLANTA", 1080, 566, 11, color=RED, tracking=1.4),
            label("Four walls × five shelves", 1096, 732, 14, 250, family=TYPE),
            label(
                "Central void / two free sides\nCirculation drawn as an assumption",
                1096,
                757,
                11,
                250,
                40,
                family=TYPE,
                leading=15,
            ),
            label("05  /  THE VOLUME", 44, 841, 11, color=RED, tracking=1.4),
            label("410", 44, 863, 42, 130, 55, TYPE),
            label("PAGES", 160, 880, 10, 98, tracking=1.4),
            label(
                "40 lines × approximately 80 characters", 46, 918, 11, 350, family=TYPE
            ),
            label(
                "20 SHELVES  /  32 BOOKS EACH  /  640 VOLUMES PER ROOM",
                439,
                842,
                10,
                900,
                tracking=1.1,
            ),
            label("I", 439, 918, 10, 80, family=TYPE),
            label("II", 670, 918, 10, 80, family=TYPE),
            label("III", 901, 918, 10, 80, family=TYPE),
            label("IV", 1132, 918, 10, 80, family=TYPE),
            label(
                "ORIGINAL INTERPRETATION  /  NATIVE GEOMETRY + TYPE  /  NOT A UNIQUE MAP OF THE STORY",
                44,
                961,
                9,
                1050,
                tracking=1.2,
            ),
            label(
                "LITERARY STUDY     06 / ∞", 1160, 959, 11, 199, family=TYPE, color=RED
            ),
        ]
        artwork = [
            canvas("paper", paper, Cache.Picture),
            canvas("architecture", tower, Cache.Picture),
            canvas("diagrams", diagrams, Cache.Picture),
            canvas("ruling", page, Cache.Picture),
        ]
        for element in artwork:
            element.absolute().inset(0)
        ctx.render(box(*artwork, *words).width(SIZE[0]).height(SIZE[1]))
