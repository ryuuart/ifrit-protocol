"""An original illuminated folio after the Shah Tahmasp manuscript tradition.

TAGS: Studies/Manuscripts, Studies/Cultural, Typography/RTL, Runtime/Python
TAGS: Drawing/Geometry, Drawing/Organic, Materials/Gilding, Motion/Bindings
"""

import math
import random

from sigil.compose import Cache, pen, positioned, text
from sigil.compose.kit import at
from sigil.draw import CLOSE, SHAPE
from sigil.material import Paint
from sigil.motion import animate
from sigil.sketch import sketch
from sigil.weave import FrameOptions, TextAlignment, Type, rich

W, H = 1100, 1560
INK = "#322e30"
GOLD = "#c79b43"
LAPIS = "#222c66"
PAPER = "#eee2bb"
GILT = Paint.linearGradient(
    (0, 0),
    (1, 1),
    [
        (0, "#77502a"),
        (0.20, "#d2a843"),
        (0.47, "#ffebaa"),
        (0.55, "#b88d38"),
        (0.77, "#e8c878"),
        (1, "#6f4f2b"),
    ],
)


def line(p, a, b, color=INK, weight=0.7):
    p.noFill()
    p.stroke(color)
    p.strokeWeight(weight)
    p.line(*a, *b)


def polygon(p, points, color, stroke=INK, weight=0.7):
    p.fill(color)
    p.stroke(stroke)
    p.strokeWeight(weight)
    p.beginShape()
    for xy in points:
        p.vertex(*xy)
    p.endShape(CLOSE)


def curve(p, start, spans, color, stroke=INK, weight=0.7):
    p.fill(color)
    p.stroke(stroke)
    p.strokeWeight(weight)
    p.beginShape()
    p.vertex(*start)
    for span in spans:
        p.bezierVertex(*span)
    p.endShape(CLOSE)


def disk(p, x, y, d, color, stroke=INK, weight=0.6):
    p.fill(color)
    p.stroke(stroke)
    p.strokeWeight(weight)
    p.circle(x, y, d)


def leaf(p, x, y, size, angle, color):
    p.push()
    p.translate(x, y)
    p.rotate(angle)
    curve(
        p,
        (0, 0),
        [
            (size * 0.35, -size * 0.3, size * 0.8, -size * 0.3, size, 0),
            (size * 0.7, size * 0.28, size * 0.25, size * 0.25, 0, 0),
        ],
        color,
        "#575b42",
        0.45,
    )
    line(p, (0, 0), (size * 0.9, 0), "#d8c484", 0.35)
    p.pop()


def flower(p, x, y, r, petals=6, color="#b54157"):
    for n in range(petals):
        angle = n * math.tau / petals
        disk(
            p,
            x + math.cos(angle) * r * 0.55,
            y + math.sin(angle) * r * 0.55,
            r * 0.9,
            color,
            "#dec994",
            0.45,
        )
    disk(p, x, y, r * 0.6, "#eaca77", "#785232", 0.45)
    disk(p, x, y, r * 0.17, "#59343b", "#59343b", 0.2)


def rosette(p, x, y, radius, points=8):
    vertices = []
    for n in range(points * 2):
        a = n * math.pi / points - math.pi / 2
        r = radius if n % 2 == 0 else radius * 0.54
        vertices.append((x + math.cos(a) * r, y + math.sin(a) * r))
    polygon(p, vertices, LAPIS, GOLD, 0.9)
    p.fill(GILT, SHAPE)
    p.stroke("#6a542f")
    p.strokeWeight(0.55)
    inner = []
    for n in range(points * 2):
        a = n * math.pi / points - math.pi / 2
        r = radius * 0.63 if n % 2 == 0 else radius * 0.35
        inner.append((x + math.cos(a) * r, y + math.sin(a) * r))
    p.beginShape()
    for xy in inner:
        p.vertex(*xy)
    p.endShape(CLOSE)
    disk(p, x, y, radius * 0.40, "#654961", "#f2d88d", 0.65)
    flower(p, x, y, radius * 0.18, 5, "#f0deac")


def scroll(p, x, y, w, h, color=GOLD):
    p.noFill()
    p.stroke(color)
    p.strokeWeight(0.8)
    p.bezier(x, y + h * 0.8, x + w * 0.4, y + h, x + w * 0.7, y, x + w, y + h * 0.2)
    p.bezier(
        x + w * 0.2,
        y + h * 0.68,
        x + w * 0.6,
        y + h * 0.6,
        x + w * 0.8,
        y + h * 0.83,
        x + w * 0.56,
        y + h * 0.86,
    )
    for n in range(6):
        t = (n + 1) / 7
        leaf(
            p,
            x + w * t,
            y + h * (0.85 - 0.65 * t),
            min(w, h) * 0.32,
            -58 if n % 2 else 143,
            color,
        )


def paper(p):
    rng = random.Random(12344)
    p.noStroke()
    p.fill("#202c30")
    p.rect(0, 0, W, H)
    for inset, color in [(0, "#121c22"), (5, "#1a2529"), (12, "#243235")]:
        p.fill(color)
        p.rect(55 + inset, 52 + inset, 1002 - inset * 2, 1483 - inset * 2, 5)
    p.fill(
        Paint.linearGradient(
            (0, 0),
            (1, 1),
            [(0, "#dfd0a8"), (0.3, "#f5ebcf"), (0.7, "#eadbb5"), (1, "#d4c393")],
        ),
        SHAPE,
    )
    p.rect(65, 38, 970, 1460, 3)
    p.strokeWeight(0.4)
    for n in range(12500):
        x = rng.uniform(71, 1029)
        y = rng.uniform(44, 1492)
        p.stroke(rng.choice(["#c3ac7643", "#99784720", "#e5d4a440", "#fef1d655"]))
        p.line(x, y, x + rng.uniform(-1.8, 1.8), y + rng.uniform(0.2, 1.6))
    p.noFill()
    for x, y, w, h, color in [
        (75, 48, 950, 1440, "#957854"),
        (80, 53, 940, 1430, "#e7d8b4"),
        (106, 93, 888, 1346, "#c8af77"),
        (116, 103, 868, 1326, "#a58549"),
    ]:
        p.stroke(color)
        p.strokeWeight(0.6)
        p.rect(x, y, w, h)


def border(p):
    p.fill(LAPIS)
    p.stroke("#775b35")
    p.strokeWeight(0.7)
    p.rect(128, 124, 844, 1292)
    p.fill("#d3b779")
    p.rect(163, 159, 774, 1222)
    p.fill(PAPER)
    p.rect(177, 173, 746, 1194)
    for off, c in [(0, "#d9b15b"), (5, "#d9b15b"), (10, "#243360"), (16, "#75563b")]:
        p.noFill()
        p.stroke(c)
        p.strokeWeight(0.65)
        p.rect(128 + off, 124 + off, 844 - 2 * off, 1292 - 2 * off)
    for x in range(193, 908, 34):
        scroll(p, x, 130, 26, 20)
        scroll(p, x, 1389, 26, 20)
        disk(p, x + 13, 153, 2.5, "#f5d995", "#f5d995", 0.2)
        disk(p, x + 13, 1387, 2.5, "#f5d995", "#f5d995", 0.2)
    for y in range(185, 1375, 34):
        p.push()
        p.translate(136, y)
        p.rotate(90)
        scroll(p, 0, 0, 27, 18)
        p.pop()
        p.push()
        p.translate(963, y + 28)
        p.rotate(-90)
        scroll(p, 0, 0, 27, 18)
        p.pop()
    for x, y in [(149, 145), (951, 145), (149, 1395), (951, 1395)]:
        rosette(p, x, y, 23)
    for x in (171, 929):
        for y in range(192, 1360, 14):
            polygon(
                p,
                [(x, y - 4), (x + 4, y), (x, y + 4), (x - 4, y)],
                "#35446d",
                GOLD,
                0.3,
            )
    # Gold-speckled margins stay quieter than the painted field.
    rng = random.Random(37)
    for n in range(1750):
        x = rng.uniform(89, 1010)
        y = rng.uniform(77, 1449)
        if 121 < x < 980 and 115 < y < 1424:
            continue
        disk(
            p,
            x,
            y,
            rng.uniform(0.6, 1.9),
            rng.choice(["#b89856", "#c3a463", "#ddd0a0"]),
            "#c3ac79",
            0.2,
        )
    for y in (370, 712, 1075):
        rosette(p, 109, y, 20, 8)
        p.push()
        p.translate(1002, y)
        p.rotate(90)
        rosette(p, 0, 0, 20, 8)
        p.pop()
        line(p, (109, y - 37), (109, y - 21), GOLD, 0.7)
        line(p, (109, y + 21), (109, y + 37), GOLD, 0.7)
        disk(p, 109, y - 42, 5, LAPIS, GOLD, 0.7)
        disk(p, 109, y + 42, 5, LAPIS, GOLD, 0.7)


def headings(p):
    # The headings are ornaments and live Unicode text occupies their centres.
    p.fill("#283366")
    p.stroke(GOLD)
    p.strokeWeight(1)
    p.rect(203, 191, 694, 91)
    for x in (215, 885):
        rosette(p, x, 236, 29, 8)
    for x in range(251, 869, 19):
        flower(p, x, 202, 3.8, 4, "#dfd0a0")
        flower(p, x, 270, 3.8, 4, "#dfd0a0")
    for y in (207, 264):
        line(p, (252, y), (847, y), GOLD, 0.55)
    scroll(p, 238, 290, 105, 30, "#b9954e")
    scroll(p, 750, 290, 105, 30, "#b9954e")
    p.noFill()
    p.stroke(GOLD)
    p.strokeWeight(0.65)
    p.rect(191, 336, 718, 757)
    p.rect(196, 341, 708, 747)
    for x in range(204, 907, 16):
        polygon(
            p,
            [(x, 335), (x + 4, 329), (x + 8, 335), (x + 4, 341)],
            "#d8b16a",
            "#9e7437",
            0.3,
        )


def mountain(p, x, y, s, color):
    p.push()
    p.translate(x, y)
    p.scale(s)

    def outline():
        curve(
            p,
            (-51, 87),
            [
                (-79, 82, -79, 56, -58, 47),
                (-84, 21, -47, 13, -48, -6),
                (-70, -23, -28, -26, -29, -44),
                (-49, -62, -5, -61, 9, -94),
                (35, -100, 28, -67, 39, -59),
                (61, -68, 44, -35, 57, -20),
                (83, -17, 62, 11, 72, 26),
                (89, 43, 87, 59, 61, 71),
                (71, 96, 23, 102, -6, 105),
                (-22, 101, -31, 104, -51, 87),
            ],
            color,
            "#796378",
            0.8,
        )

    outline()
    p.push()
    p.clip(outline)
    # Hand-authored mineral bands, curling fault lines and broken stipple.
    rng = random.Random(int(x * 7 + y))
    for n in range(9):
        xx = -72 + n * 20
        curve(
            p,
            (xx, 115),
            [
                (xx - 20, 76, xx + 22, 54, xx + 9, 24),
                (xx - 7, 3, xx + 29, -10, xx + 15, -37),
                (xx - 1, -59, xx + 30, -89, xx + 32, -106),
                (xx + 35, -85, xx + 7, -58, xx + 24, -35),
                (xx + 41, -9, xx + 7, 2, xx + 26, 22),
                (xx + 39, 47, xx + 6, 71, xx + 20, 115),
            ],
            "#ebd0d222" if n % 2 else "#59487518",
            "#88789244",
            0.45,
        )
    for n in range(180):
        xx = rng.uniform(-78, 82)
        yy = rng.uniform(-97, 101)
        p.noFill()
        p.stroke(rng.choice(["#eee2d24c", "#6f688c60", "#c7afc9b0"]))
        p.strokeWeight(0.38)
        p.bezier(xx, yy, xx - 5, yy - 3, xx + 1, yy - 6, xx + 3, yy - 7)
    colors = ["#baa3b5", "#7f7c9c", "#dab6b8", "#a480a3"]
    for n in range(16):
        xx = -47 + n * 6
        yy = 43 + math.sin(n * 1.5) * 20
        p.noFill()
        p.stroke(colors[n % 4])
        p.strokeWeight(0.65)
        p.bezier(xx, yy + 35, xx - 20, yy - 6, xx + 10, yy - 8, xx + 14, yy - 46)
        p.bezier(xx, yy + 20, xx + 28, yy + 4, xx + 13, yy - 21, xx + 10, yy - 35)
    for n in range(18):
        yy = 73 - n * 7
        line(p, (-40, yy), (5, yy - 18), "#d4b8c1", 0.45)
    p.pop()
    p.pop()


def cloud(p, x, y, s):
    p.push()
    p.translate(x, y)
    p.scale(s)
    p.noFill()
    p.stroke("#c8d4cf")
    p.strokeWeight(0.8)
    for n in range(3):
        y0 = n * 3
        p.bezier(0, y0, 20, y0 - 15, 39, y0 + 4, 57, y0 - 8)
        p.bezier(57, y0 - 8, 72, y0 - 28, 87, y0 + 12, 116, y0 - 5)
    p.pop()


def tree(p, x, y, h):
    line(p, (x, y), (x - 4, y - h), "#705444", 3)
    for n in range(11):
        yy = y - h * 0.15 - n * h * 0.07
        side = -1 if n % 2 else 1
        tip = x + side * (23 + 14 * math.sin(n * 0.8))
        line(p, (x - 4, yy + 20), (tip, yy - 15), "#715744", 1.2)
        for j in range(3):
            leaf(
                p, tip + side * j * 5, yy - j * 8, 23, side * (-70 - j * 10), "#426961"
            )
            if n % 3 == 0:
                flower(p, tip + side * (j * 7 + 8), yy - 8 - j * 10, 4.8, 5, "#d08b91")
    for n in range(6):
        line(p, (x, y), (x - 20 + n * 8, y + 10), "#77604b", 0.65)


def sprig(p, x, y, s, color="#4a6d5d"):
    line(p, (x, y), (x + s * 0.15, y - s), "#54745b", 0.6)
    for j in range(3):
        leaf(
            p, x + s * 0.08, y - s * (j + 1) / 4, s * 0.3, -155 if j % 2 else -25, color
        )
    flower(p, x + s * 0.15, y - s, s * 0.17, 5, "#d79e97")


def carpet(p, x, y, w, h, color):
    p.fill(color)
    p.stroke("#6a5039")
    p.strokeWeight(0.8)
    p.rect(x, y, w, h)
    p.noFill()
    p.stroke(GOLD)
    p.strokeWeight(1)
    p.rect(x + 4, y + 4, w - 8, h - 8)
    p.strokeWeight(0.45)
    p.rect(x + 8, y + 8, w - 16, h - 16)
    for xx in range(int(x + 14), int(x + w - 10), 11):
        for yy in range(int(y + 15), int(y + h - 10), 12):
            flower(p, xx, yy, 2.4, 4, "#cca96f")
    for xx in range(int(x + 2), int(x + w), 4):
        line(p, (xx, y + h), (xx + 1, y + h + 4), "#c0a16b", 0.4)


def person(p, x, y, s, robe, pose=0, royal=False):
    p.push()
    p.translate(x, y)
    p.scale(s)
    # Folded knees, coat, cuffs, fingers and a profile are separate filled contours.
    curve(
        p,
        (-30, 59),
        [
            (-45, 47, -42, 34, -19, 29),
            (-10, 30, 7, 34, 18, 34),
            (44, 33, 45, 57, 22, 62),
            (2, 59, -11, 68, -30, 59),
        ],
        robe,
        "#60484b",
        0.7,
    )
    curve(
        p,
        (-21, 30),
        [
            (-20, 8, -14, -8, -11, -17),
            (-1, -22, 10, -20, 15, -12),
            (19, 10, 26, 22, 18, 43),
            (1, 47, -9, 45, -21, 30),
        ],
        robe,
        "#5f4246",
        0.8,
    )
    line(p, (0, -13), (-7, 34), "#d2a170", 0.7)
    line(p, (7, -13), (9, 35), "#382b42", 0.5)
    for n in range(5):
        line(p, (-25 + n * 12, 49), (-17 + n * 11, 58), "#c39872", 0.5)
    curve(
        p,
        (-11, -18),
        [
            (-18, -29, -9, -46, 1, -44),
            (11, -44, 14, -30, 6, -20),
            (4, -15, -5, -14, -11, -18),
        ],
        "#e7bc9a",
        "#6d544d",
        0.65,
    )
    line(p, (2, -35), (5, -34), "#594448", 0.6)
    line(p, (4, -31), (8, -29), "#755749", 0.55)
    line(p, (3, -24), (-1, -22), "#9b6759", 0.5)
    # Turbans are stacked crescent bands; the crowned figure wears a gilded cap.
    if royal:
        polygon(
            p,
            [
                (-12, -42),
                (-14, -54),
                (-5, -50),
                (0, -60),
                (5, -49),
                (13, -54),
                (12, -41),
            ],
            "#d0aa56",
            "#694e3c",
            0.7,
        )
        for xx in (-8, 0, 8):
            disk(p, xx, -45, 3, "#283c7c", "#e4c781", 0.4)
        disk(p, 0, -55, 3, "#ae4249", "#e4c781", 0.4)
    else:
        for j in range(4):
            curve(
                p,
                (-13, -40 - j * 2),
                [
                    (-24, -49 - j * 2, 14, -58 + j, 17, -43 - j),
                    (4, -38 - j, -9, -39 - j, -13, -40 - j * 2),
                ],
                "#e4dac6",
                "#827567",
                0.55,
            )
        polygon(p, [(1, -50), (2, -61), (7, -63), (7, -46)], "#ece0c5", "#6d6259", 0.6)
    if pose == 1:
        curve(
            p,
            (-18, -7),
            [
                (-32, -2, -41, 16, -28, 23),
                (-18, 25, -8, 9, -7, 4),
                (-10, 3, -16, 1, -18, -7),
            ],
            robe,
            "#63484a",
            0.7,
        )
        polygon(
            p,
            [(-10, 7), (-8, -12), (-4, -17), (0, -14), (-2, 6)],
            "#e8bd9c",
            "#745a4d",
            0.6,
        )
        for j in range(3):
            line(p, (-7 + j * 2, -14), (-5 + j * 2, -9), "#9e7a61", 0.4)
    else:
        curve(
            p,
            (12, -8),
            [(24, -4, 36, 9, 29, 19), (22, 19, 14, 13, 7, 7), (8, 0, 10, -4, 12, -8)],
            robe,
            "#60464a",
            0.7,
        )
        polygon(
            p,
            [(27, 14), (39, 9), (44, 12), (40, 16), (28, 20)],
            "#e8bd9c",
            "#705244",
            0.5,
        )
        disk(p, 42, 9, 7, "#aa4a4e", "#735543", 0.6)
    for n in range(14):
        xx = -10 + (n % 3) * 9
        yy = -5 + (n // 3) * 8
        flower(p, xx, yy, 1.3, 4, "#d7b271")
    p.pop()


def deer(p, x, y, s, color="#ceae8d", horns=True):
    p.push()
    p.translate(x, y)
    p.scale(s)
    curve(
        p,
        (-33, 8),
        [
            (-42, -13, -6, -22, 20, -9),
            (22, -20, 22, -33, 32, -40),
            (39, -42, 42, -32, 35, -26),
            (29, -6, 40, 3, 24, 11),
            (8, 20, -20, 22, -33, 8),
        ],
        color,
        "#715a4d",
        0.7,
    )
    for off in (-24, -9, 17, 25):
        polygon(
            p,
            [(off, 10), (off + 4, 10), (off + 5, 38), (off - 1, 39)],
            color,
            "#715a4d",
            0.6,
        )
        line(p, (off - 1, 39), (off + 6, 39), "#443e36", 1.4)
    leaf(p, 32, -35, 13, -70, color)
    disk(p, 35, -32, 2, "#3d3831", "#3d3831", 0.3)
    line(p, (-35, 3), (-49, -13), "#745a4b", 1.5)
    if horns:
        for direction in (-1, 1):
            line(p, (30, -42), (30 + direction * 5, -59), "#705a46", 1)
            line(
                p, (30 + direction * 4, -54), (30 + direction * 12, -58), "#705a46", 0.8
            )
            line(
                p, (30 + direction * 3, -49), (30 + direction * 10, -51), "#705a46", 0.8
            )
    for n in range(8):
        disk(p, -24 + (n % 4) * 9, -6 + (n // 4) * 10, 2.7, "#f4ddaf", "#a18869", 0.2)
    p.pop()


def flames(p, x, y, s):
    p.push()
    p.translate(x, y)
    p.scale(s)
    polygon(
        p,
        [(-44, 11), (-15, -3), (35, 6), (47, 21), (-19, 26)],
        "#96816a",
        "#604d41",
        0.65,
    )
    for xx in (-28, -10, 9, 28):
        line(p, (xx - 11, 19), (xx + 24, 5), "#6b5140", 2.4)
    for j in range(8):
        xx = -33 + j * 9
        curve(
            p,
            (xx, 15),
            [
                (xx - 17, -5, xx + 5, -21, xx - 1, -43 - j % 3 * 13),
                (xx + 14, -23, xx + 24, -5, xx + 7, 18),
                (xx + 2, 20, xx - 3, 18, xx, 15),
            ],
            "#bd583c" if j % 2 else "#b04337",
            "#8f5b3c",
            0.5,
        )
        curve(
            p,
            (xx, 16),
            [
                (xx - 8, 0, xx + 9, -7, xx + 5, -30),
                (xx + 18, -8, xx + 12, 2, xx + 5, 17),
                (xx + 3, 17, xx + 1, 17, xx, 16),
            ],
            "#e1a352",
            "#e8bc68",
            0.35,
        )
    p.pop()


def painting(p):
    rng = random.Random(663)
    p.noStroke()
    p.fill("#37417a")
    p.rect(202, 348, 696, 734)
    for x, y, s in [(209, 374, 0.6), (713, 380, 0.9), (389, 444, 0.7), (778, 516, 0.6)]:
        cloud(p, x, y, s)
    curve(
        p,
        (202, 542),
        [
            (260, 465, 384, 560, 438, 509),
            (568, 443, 642, 545, 712, 503),
            (806, 438, 869, 524, 898, 551),
            (919, 689, 885, 992, 898, 1082),
            (677, 1084, 467, 1084, 202, 1082),
            (193, 875, 213, 710, 202, 542),
        ],
        "#96a995",
        "#716f67",
        0.7,
    )
    # Mineral formations extend past the rectangular miniature into its paper margins.
    for x, y, s, c in [
        (265, 516, 1.18, "#b48da6"),
        (370, 441, 1.18, "#c28e9e"),
        (430, 482, 0.9, "#a9b1b0"),
        (538, 439, 1.25, "#8495ad"),
        (684, 491, 1.06, "#aa91ae"),
        (805, 527, 1.18, "#c593a9"),
        (246, 846, 0.88, "#9b9aae"),
        (852, 856, 0.85, "#b68b9b"),
        (246, 1035, 0.65, "#b398aa"),
        (827, 1051, 0.78, "#b7a0ad"),
    ]:
        mountain(p, x, y, s, c)
    # Grass, rocks, blossoms and stipple all remain deterministic retained artwork.
    for n in range(850):
        x = rng.uniform(223, 877)
        y = rng.uniform(570, 1068)
        disk(
            p,
            x,
            y,
            rng.choice([0.6, 0.8, 1.2]),
            rng.choice(["#6c8974", "#bfc5a4", "#7d997c"]),
            "#7b937a",
            0.15,
        )
    for x, y, h in [
        (281, 754, 211),
        (770, 756, 237),
        (850, 1074, 200),
        (213, 995, 160),
    ]:
        tree(p, x, y, h)
    for n in range(52):
        sprig(p, rng.uniform(245, 854), rng.uniform(672, 1060), rng.uniform(11, 25))
    # Central dais and the surrounding courtiers follow the source's oval hierarchy.
    carpet(p, 476, 669, 157, 99, "#3c3264")
    person(p, 553, 684, 1.22, "#b75238", 0, True)
    carpet(p, 312, 778, 119, 78, "#6e4855")
    person(p, 370, 786, 1.0, "#326962", 1)
    carpet(p, 674, 769, 117, 77, "#a97855")
    person(p, 729, 779, 1.02, "#4a4370", 0)
    person(p, 288, 672, 0.87, "#9a4a47", 0)
    person(p, 294, 745, 0.84, "#454c8c", 0)
    person(p, 811, 687, 0.84, "#8d4b5f", 1)
    person(p, 805, 746, 0.86, "#b99a58", 0)
    person(p, 402, 894, 0.92, "#454382", 0)
    person(p, 738, 889, 0.94, "#b74f3c", 1)
    person(p, 679, 918, 0.89, "#484986", 0)
    flames(p, 548, 879, 1.15)
    # Long-necked vessels and shallow dishes echo the precise painted objects.
    for x, y in [(466, 828), (619, 830), (622, 942)]:
        curve(
            p,
            (x - 12, y + 13),
            [
                (x - 27, y + 7, x - 3, y - 1, x - 4, y - 13),
                (x - 4, y - 22, x - 7, y - 22, x - 5, y - 27),
                (x, y - 31, x + 3, y - 30, x + 3, y - 23),
                (x, y - 9, x + 17, y + 6, x + 11, y + 14),
                (x + 4, y + 18, x - 3, y + 18, x - 12, y + 13),
            ],
            "#b6a25c",
            "#79663d",
            0.65,
        )
        line(p, (x - 7, y + 11), (x + 9, y + 11), "#e2c58b", 0.5)
    for x, y in [(489, 794), (668, 861), (461, 924)]:
        p.fill("#d8bc81")
        p.stroke("#795c3f")
        p.strokeWeight(0.6)
        p.ellipse(x, y, 33, 9)
        for xx in (-10, 0, 10):
            disk(p, x + xx, y - 2, 6, "#a8443c", "#795c3f", 0.4)
    deer(p, 380, 1008, 0.98, "#ccb798", True)
    deer(p, 611, 1029, 0.97, "#d6c7ab", False)
    deer(p, 711, 986, 0.78, "#bea585", True)
    for x, y in [(455, 1020), (521, 978), (800, 984)]:
        sprig(p, x, y, 31)
    p.noFill()
    p.stroke("#433d50")
    p.strokeWeight(0.65)
    p.rect(202, 348, 696, 734)
    # The top of the illustration is interrupted by an original caption cartouche.
    p.fill(PAPER)
    p.stroke("#876740")
    p.strokeWeight(0.8)
    p.rect(682, 360, 202, 72)
    p.noFill()
    p.stroke("#bd9b59")
    p.strokeWeight(0.6)
    p.rect(687, 365, 192, 62)


def inscription_ruling(p):
    p.fill("#eadfbe")
    p.stroke("#8b7049")
    p.strokeWeight(0.8)
    p.rect(202, 1108, 696, 157)
    for x in (376, 550, 724):
        p.fill(LAPIS)
        p.stroke(GOLD)
        p.strokeWeight(0.55)
        p.rect(x - 5, 1108, 10, 157)
        for y in range(1119, 1260, 17):
            flower(p, x, y, 2.9, 4, "#e0c489")
    for x in range(218, 884, 174):
        line(p, (x, 1229), (x + 134, 1229), "#d0b882", 0.35)
        rosette(p, x + 64, 1244, 7, 6)
    for off, c in [(0, "#a88d52"), (5, "#c9aa65"), (10, "#586474")]:
        p.noFill()
        p.stroke(c)
        p.strokeWeight(0.6)
        p.rect(202 - off, 1108 - off, 696 + off * 2, 157 + off * 2)
    scroll(p, 256, 1292, 119, 37, "#b19a68")
    scroll(p, 726, 1292, 119, 37, "#b19a68")
    rosette(p, 550, 1314, 20, 8)


def ember_sparks(p):
    p.noStroke()
    for x, y, d in [
        (35, 58, 2.8),
        (52, 33, 2),
        (68, 54, 1.8),
        (82, 15, 1.8),
        (51, 5, 1.1),
    ]:
        p.fill("#e9c373")
        p.circle(x, y, d)
    for x, y in [(10, 56), (69, 30), (93, 47)]:
        line(p, (x - 3, y), (x + 3, y), "#efd793", 0.65)
        line(p, (x, y - 3), (x, y + 3), "#efd793", 0.65)


def label(
    value,
    x,
    y,
    w,
    h,
    size=18,
    family="Baskerville",
    color=INK,
    center=True,
    language="en",
    baseline=None,
):
    leaf_text = (
        text(value)
        .font(Type(language=language))
        .fontFamily(family)
        .fontSize(size)
        .ink(color)
    )
    if center:
        leaf_text = leaf_text.textAlign(TextAlignment.Center)
    if baseline is not None:
        leaf_text = leaf_text.textFirstBaseline(
            FrameOptions.FirstBaseline.Fixed, baseline
        )
    return at(leaf_text, x, y, w, h)


@sketch(size=(W, H), background="#17252b", capture_at=3.0)
class ShahnamehFolio:
    def setup(self, ctx):
        # Only the spark overlay animates; the folio's geometry and text remain retained.
        parts = [
            at(pen("folio.paper", paper, Cache.Picture), 0, 0, W, H),
            at(pen("folio.border", border, Cache.Picture), 0, 0, W, H),
            at(pen("folio.headings", headings, Cache.Picture), 0, 0, W, H),
            at(pen("folio.miniature", painting, Cache.Picture), 0, 0, W, H),
            at(pen("folio.ruling", inscription_ruling, Cache.Picture), 0, 0, W, H),
            label(
                "شاهنامه",
                302,
                197,
                496,
                80,
                52,
                "Noto Nastaliq Urdu",
                "#ecd7a5",
                language="fa",
                baseline=58,
            ),
            label("THE FIRST FIRE", 335, 289, 430, 32, 19, "Baskerville", "#6c533e"),
            label(
                "جشن آتش", 695, 363, 178, 55, 28, "Noto Nastaliq Urdu", language="fa"
            ),
        ]
        # The two opening couplets are printed in reading order from right to left.
        verses = [
            "به نام خداوندِ جان و خرد",
            "کز این برتر، اندیشه، بر نگذرد",
            "خداوندِ نام و خداوندِ جای",
            "خداوندِ روزی‌دِهِ رهنمای",
        ]
        for index, verse in enumerate(verses):
            parts.append(
                label(
                    verse,
                    730 - index * 174,
                    1120,
                    160,
                    108,
                    25,
                    "Noto Nastaliq Urdu",
                    language="fa",
                )
            )
        mixed = (
            rich()
            .add("شاهنامه", Type(language="fa", size=18))
            .add("  ·  Folio 22v  ·  ", Type(language="en", size=14))
            .add("۱۵۲۵", Type(language="fa", size=17))
        )
        parts.append(
            at(
                text(mixed)
                .fontFamily("Geeza Pro")
                .ink("#6d6049")
                .textAlign(TextAlignment.Center),
                269,
                1351,
                562,
                32,
            )
        )
        parts.append(
            label(
                "Original miniature · Opening couplets by Ferdowsi",
                153,
                1448,
                794,
                28,
                13,
                "Baskerville",
                "#65593e",
            )
        )
        parts.append(
            label(
                "STUDY  /  ILLUMINATED MANUSCRIPT",
                150,
                1507,
                800,
                24,
                12,
                "Baskerville",
                "#c2c2ab",
            )
        )
        printed_page = at(
            positioned(*parts).key("folio.printed-page").cache(Cache.Texture),
            0,
            0,
            W,
            H,
        )
        sparks = (
            at(
                pen("folio.ember-sparks", ember_sparks, Cache.Picture),
                495,
                751,
                110,
                80,
            )
            .opacity(animate(from_=0.22, to=0.9, duration=2.3, loop=-1, alternate=True))
            .translateY(animate(from_=0, to=-7, duration=2.3, loop=-1, alternate=True))
        )
        ctx.render(positioned(printed_page, sparks).width(W).height(H))
