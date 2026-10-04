"""An original illuminated folio after the Shah Tahmasp manuscript tradition.

TAGS: Studies/Manuscripts, Studies/Cultural, Typography/RTL, Runtime/Python
TAGS: Drawing/Geometry, Drawing/Organic, Materials/Gilding, Motion/Bindings
"""

import math
import random

from sigil.compose import Cache, pen, positioned, text
from sigil.compose.kit import at
from sigil.draw import CLOSE, DEGREES, SHAPE, brush
from sigil.material import Paint, mixLinear
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
    p.angleMode(DEGREES)
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
    p.angleMode(DEGREES)
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
    p.angleMode(DEGREES)
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
    rng = random.Random(int(x * 13 + y * 29))
    tilt = rng.uniform(-8, 9)
    p.rotate(tilt)
    tip = rng.uniform(-22, 24)

    def outline():
        curve(
            p,
            (-59, 87),
            [
                (-88, 81, -80, 59, -63, 43),
                (-87, 26, -58, 9, -47, -7),
                (-68, -22, -33, -34, -30, -52),
                (-46, -72, tip - 9, -79, tip, -104),
                (tip + 30, -113, tip + 31, -72, 40, -61),
                (67, -73, 50, -34, 59, -18),
                (85, -15, 65, 12, 72, 32),
                (94, 49, 78, 73, 59, 83),
                (61, 99, 23, 108, -5, 104),
                (-20, 103, -46, 106, -59, 87),
            ],
            color,
            "#71606c",
            0.58,
        )

    outline()
    p.push()
    p.clip(outline)
    brush.wash(
        p,
        brush.Wash(
            color="#342648", opacity=0.13, layers=4, border=0.2, texture=0.8, bleed=0.35
        ),
        [(-75, 108), (-34, -66), (30, -96), (86, 74), (39, 112)],
    )
    for ridge in range(4):
        base = -49 + ridge * 27 + rng.uniform(-7, 7)
        knots = [
            (base, 109),
            (base + 16, 66),
            (base - 8, 37),
            (base + 17, 6),
            (base + 7, -29),
            (base + 24, -78),
        ]
        inkline(p, knots, "#44325350", 2.2, 0.6)
        inkline(p, [(xx + 2, yy - 2) for xx, yy in knots], "#f1d3c8a0", 0.7, 0.8)
    for shelf in range(12):
        cx = rng.uniform(-32, 25)
        cy = -83 + shelf * 15.3 + rng.uniform(-8, 8)
        reach = rng.uniform(37, 65)
        rise = rng.uniform(12, 25)
        pigment = mixLinear(color, "#633d73", rng.uniform(0.05, 0.22))
        curve(
            p,
            (cx - reach, cy + rise * 0.55),
            [
                (
                    cx - reach * 1.1,
                    cy - 4,
                    cx - reach * 0.50,
                    cy - rise * 0.1,
                    cx - reach * 0.31,
                    cy - rise,
                ),
                (
                    cx - reach * 0.25,
                    cy - rise * 1.13,
                    cx + reach * 0.08,
                    cy - rise * 0.76,
                    cx + reach * 0.33,
                    cy - rise * 1.22,
                ),
                (
                    cx + reach * 0.29,
                    cy - rise * 0.54,
                    cx + reach * 0.7,
                    cy - rise * 0.51,
                    cx + reach,
                    cy - rise * 0.2,
                ),
                (
                    cx + reach * 0.84,
                    cy + rise * 0.61,
                    cx + reach * 0.39,
                    cy + rise * 0.58,
                    cx + reach * 0.1,
                    cy + rise * 0.30,
                ),
                (
                    cx - reach * 0.25,
                    cy + rise * 0.75,
                    cx - reach * 0.75,
                    cy + rise * 0.8,
                    cx - reach,
                    cy + rise * 0.55,
                ),
            ],
            pigment,
            "#624969b0",
            0.6,
        )
        for band in range(5):
            d = band * 1.8
            knots = [
                (cx - reach * 0.94, cy + rise * 0.35 - d),
                (cx - reach * 0.72, cy - d),
                (cx - reach * 0.32, cy - rise * 0.62 - d),
                (cx - reach * 0.12, cy - rise * 0.45 - d),
                (cx + reach * 0.33, cy - rise * 0.91 - d),
                (cx + reach * 0.67, cy - rise * 0.24 - d),
            ]
            inkline(p, knots, "#e9c5bc90" if band % 2 else "#7250768a", 0.5, 0.8)
        for fissure in range(3):
            xx = cx - reach * 0.5 + fissure * reach * 0.42
            inkline(
                p,
                [
                    (xx, cy + rise * 0.35),
                    (xx + 8, cy + rise * 0.12),
                    (xx + 9, cy - rise * 0.25),
                    (xx + 18, cy - rise * 0.55),
                ],
                "#694d728a",
                0.5,
                0.65,
            )
    for ledge in range(16):
        xx = rng.uniform(-61, 59)
        yy = rng.uniform(-70, 86)
        reach = rng.uniform(9, 28)
        for ring in range(4):
            w = reach - ring * 1.9
            inkline(
                p,
                [
                    (xx - w * 0.9, yy + ring * 2),
                    (xx - w * 0.8, yy - w * 0.24 + ring * 2),
                    (xx + w * 0.3, yy - w * 0.50 + ring * 2),
                    (xx + w * 0.9, yy - w * 0.8 + ring * 2),
                ],
                "#6651748a" if ring % 2 else "#e8cec19e",
                0.45,
                0.7,
            )
    for n in range(330):
        xx = rng.uniform(-84, 86)
        yy = rng.uniform(-102, 106)
        pigment = rng.choice(["#fcddc066", "#65577466", "#8c75904e"])
        p.noFill()
        p.stroke(pigment)
        p.strokeWeight(0.22)
        p.bezier(xx, yy, xx - 1.5, yy - 2, xx + 2, yy - 2.7, xx + 3, yy - 4)
    for n in range(9):
        xx = -45 + n * 12 + rng.uniform(-5, 5)
        yy = 61 + rng.uniform(-38, 25)
        inkline(
            p,
            [
                (xx - 12, yy + 8),
                (xx - 5, yy - 6),
                (xx + 10, yy - 16),
                (xx + 6, yy - 28),
            ],
            "#5d486b",
            0.62,
            0.75,
        )
        inkline(
            p,
            [(xx - 8, yy + 6), (xx - 3, yy - 5), (xx + 13, yy - 16)],
            "#e0c3b1",
            0.5,
            0.65,
        )
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


def arabesque(p, x, y, w, h, seed=0):
    rng = random.Random(seed)
    for j in range(2):
        p.noFill()
        p.stroke("#e1c480" if j == 0 else "#91713f")
        p.strokeWeight(0.55 if j == 0 else 0.28)
        p.bezier(
            x,
            y + h * 0.72 + j * 0.6,
            x + w * 0.28,
            y - h * 0.19,
            x + w * 0.75,
            y + h * 1.05,
            x + w,
            y + h * 0.28 + j * 0.6,
        )
        p.bezier(
            x,
            y + h * 0.27 + j * 0.5,
            x + w * 0.40,
            y + h * 1.0,
            x + w * 0.72,
            y - h * 0.14,
            x + w,
            y + h * 0.72 + j * 0.5,
        )
    for j in range(7):
        t = (j + 0.5) / 7
        xx = x + w * t
        yy = y + h * (0.53 + 0.23 * math.sin(t * math.tau))
        leaf(p, xx, yy, min(w, h) * 0.26, -65 if j % 2 else 160, "#d2ac59")
        leaf(p, xx, yy, min(w, h) * 0.19, 75 if j % 2 else -145, "#b69a61")
        if j % 2 == 0:
            flower(p, xx, yy, 2.3, 5, "#d7c194")
    for j in range(30):
        p.noStroke()
        p.fill("#d2aa596a")
        p.circle(x + rng.random() * w, y + rng.random() * h, 0.45)


def border_motif(p, x, y, w, h, phase):
    p.push()
    p.translate(x + w * 0.5, y + h * 0.5)
    if phase % 3 == 0:
        flower(p, 0, 0, 3.5, 8, "#c3c4ad")
        for side in (-1, 1):
            leaf(p, side * 5, 3, 8, 25 if side > 0 else 155, "#e4c16c")
            leaf(p, side * 6, -2, 7, -40 if side > 0 else -140, "#ba9c57")
            inkline(
                p,
                [(side * 8, -5), (side * 12, -7), (side * 14, -3)],
                "#ede1b5",
                0.4,
                0.65,
            )
    elif phase % 3 == 1:
        for j in range(5):
            leaf(p, 0, 4, 10, -155 + j * 33, "#e4c482" if j % 2 else "#b5944e")
        curve(
            p,
            (-4, 3),
            [(-7, -2, -6, -6, 0, -9), (6, -6, 7, -2, 4, 3), (2, 6, -2, 6, -4, 3)],
            "#384a7a",
            "#e5c584",
            0.45,
        )
        flower(p, 0, -2, 2.2, 5, "#ccb99e")
    else:
        curve(
            p,
            (-4, 5),
            [
                (-10, 1, -7, -5, -1, -5),
                (4, -5, 7, -8, 5, -11),
                (12, -6, 10, 1, 4, 5),
                (2, 8, -2, 8, -4, 5),
            ],
            "#756d7c",
            "#e2bf77",
            0.55,
        )
        p.noFill()
        p.stroke("#eee2b3")
        p.strokeWeight(0.35)
        p.bezier(-3, 3, -7, -1, 1, -5, 4, -8)
        disk(p, 1, 1, 2.3, "#c2ac73", "#eddaa0", 0.3)
    p.pop()


def illumination(p):
    p.angleMode(DEGREES)
    for xx in range(173, 911, 42):
        arabesque(p, xx, 129, 40, 25, xx)
        border_motif(p, xx, 129, 40, 25, xx // 42)
        arabesque(p, xx, 1387, 40, 24, xx + 41)
        border_motif(p, xx, 1387, 40, 24, xx // 42 + 1)
    for yy in range(176, 1369, 42):
        p.push()
        p.translate(157, yy)
        p.rotate(90)
        arabesque(p, 0, 0, 40, 25, yy)
        border_motif(p, 0, 0, 40, 25, yy // 42)
        p.pop()
        p.push()
        p.translate(943, yy + 40)
        p.rotate(-90)
        arabesque(p, 0, 0, 40, 25, yy + 4)
        border_motif(p, 0, 0, 40, 25, yy // 42 + 1)
        p.pop()
    for yy in (216, 252):
        for xx in range(257, 832, 42):
            arabesque(p, xx, yy, 38, 13, xx + yy)
    for side in (-1, 1):
        p.push()
        p.translate(550, 0)
        p.scale(side, 1)
        for n in range(4):
            xx = 208 + n * 30
            p.noFill()
            p.stroke("#b8a473")
            p.strokeWeight(0.5)
            p.bezier(xx, 1308, xx - 19, 1292, xx - 7, 1274, xx + 5, 1283)
            p.bezier(xx + 5, 1283, xx + 18, 1289, xx - 8, 1301, xx, 1308)
            flower(p, xx + 5, 1283, 3.3, 7, "#b1b7a0")
            leaf(p, xx - 7, 1299, 11, -37, "#b3a075")
        p.pop()
    for x, y in [(189, 325), (911, 325), (189, 1097), (911, 1097)]:
        rosette(p, x, y, 11, 8)
    for x in range(185, 913, 9):
        line(p, (x, 161), (x + 3, 168), "#e6c887", 0.35)
        line(p, (x, 1373), (x + 3, 1380), "#e6c887", 0.35)


def tree(p, x, y, h):
    rng = random.Random(int(x * 53 + y))
    curve(
        p,
        (x - 6, y),
        [
            (x - 14, y - h * 0.33, x + 11, y - h * 0.57, x - 3, y - h),
            (x + 5, y - h * 0.58, x - 6, y - h * 0.28, x + 5, y),
            (x + 1, y + 3, x - 2, y + 3, x - 6, y),
        ],
        "#715649",
        "#5c4a3e",
        0.55,
    )
    inkline(
        p,
        [
            (x - 3, y - 4),
            (x - 7, y - h * 0.29),
            (x + 3, y - h * 0.57),
            (x - 4, y - h * 0.91),
        ],
        "#b49872",
        0.65,
    )
    for b in range(9):
        side = -1 if b % 2 else 1
        yy = y - h * 0.12 - b * h * 0.084
        endx = x + side * rng.uniform(22, 47)
        endy = yy - rng.uniform(18, 36)
        inkline(
            p,
            [
                (x, yy + 13),
                (x + side * 10, yy),
                (endx - side * 7, endy + 5),
                (endx, endy),
            ],
            "#5d4d3b",
            1.65,
            0.9,
        )
        for branch in range(4):
            xx = endx + side * branch * 8
            yyy = endy - branch * 5
            inkline(
                p, [(endx, endy), (xx, yyy), (xx + side * 4, yyy - 11)], "#6a5a3f", 0.75
            )
            for j in range(3):
                size = rng.uniform(7, 14)
                leaf(
                    p,
                    xx + side * j * 3,
                    yyy - j * 5,
                    size,
                    -155 + j * 20 if side < 0 else -45 + j * 18,
                    rng.choice(["#3b685d", "#476f60", "#47615c", "#658168"]),
                )
            if b % 2 == 0:
                flower(p, xx + side * 4, yyy - 10, rng.uniform(2.1, 3.8), 5, "#bd6f7b")
                flower(p, xx - side * 2, yyy - 16, 2.3, 5, "#e0b3a9")
    for n in range(7):
        inkline(
            p,
            [(x - 4, y - 7), (x - 6 + n * 1.5, y + 2), (x - 22 + n * 7, y + 10)],
            "#84745c",
            0.45,
        )


def cypress(p, x, y, h):
    rng = random.Random(int(x + y * 3))
    width = h * 0.16
    curve(
        p,
        (x - width, y),
        [
            (
                x - width * 1.1,
                y - h * 0.30,
                x - width * 0.60,
                y - h * 0.65,
                x - 2,
                y - h,
            ),
            (
                x + width * 0.50,
                y - h * 0.68,
                x + width * 1.1,
                y - h * 0.25,
                x + width,
                y,
            ),
            (x + width * 0.3, y + 5, x - width * 0.3, y + 5, x - width, y),
        ],
        "#3b5b51",
        "#405848",
        0.6,
    )
    inkline(
        p,
        [(x, y), (x + 2, y - h * 0.36), (x - 1, y - h * 0.75), (x - 2, y - h)],
        "#28483f",
        1.15,
        0.75,
    )
    for n in range(32):
        t = (n + 0.4) / 33
        yy = y - h * t
        spread = width * (1 - t) ** 0.68
        for side in (-1, 1):
            xx = x + side * spread * rng.uniform(0.62, 0.98)
            inkline(
                p,
                [(x, yy + 7), (x + side * spread * 0.48, yy - 3), (xx, yy - 12)],
                "#233f3b",
                0.6,
                0.8,
            )
            for j in range(5):
                q = (j + 0.3) / 5
                lx = x + side * spread * q
                ly = yy - 7 * q
                length = rng.uniform(6, 12) * (1 - t * 0.55)
                angle = (
                    -145 + rng.uniform(-12, 12)
                    if side < 0
                    else -34 + rng.uniform(-12, 12)
                )
                leaf(
                    p,
                    lx,
                    ly,
                    length,
                    angle,
                    rng.choice(["#537768", "#5c7c66", "#41675b", "#2c5048"]),
                )
    for n in range(180):
        t = rng.random()
        yy = y - h * t
        xx = x + rng.uniform(-1, 1) * width * (1 - t) ** 0.68
        inkline(
            p,
            [(xx, yy), (xx + rng.uniform(-2, 2), yy - 3), (xx + 1, yy - 5)],
            "#8aa18166",
            0.28,
            0.7,
        )


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


def inkline(p, points, color="#443a35", width=0.6, pressure=0.8):
    tool = brush.marker(color, width)
    # workaround: sprite nib triangles ignore antialiasing at fine widths.
    tool.tip = brush.Tip.Fibres
    tool.bristles = 1
    tool.density = 1
    tool.sharpness = 1
    tool.opacity = 0.94
    tool.spacing = 0.05
    tool.scatter = 0
    tool.sizeJitter = 0.025
    tool.opacityJitter = 0.015
    tool.noise = 0.10
    brush.spline(
        p,
        tool,
        [
            brush.Sample(
                pt,
                pressure
                * (0.55 + 0.45 * math.sin(i * math.pi / max(1, len(points) - 1))),
            )
            for i, pt in enumerate(points)
        ],
        curvature=1.0,
    )


def cloth(p, start, spans, color, seed, folds=7):
    def outline():
        curve(p, start, spans, color, "#443941", 0.7)

    outline()
    p.push()
    p.clip(outline)
    rng = random.Random(seed)
    pts = [start] + [(span[i], span[i + 1]) for span in spans for i in (0, 2, 4)]
    left = min(pt[0] for pt in pts)
    right = max(pt[0] for pt in pts)
    top = min(pt[1] for pt in pts)
    bottom = max(pt[1] for pt in pts)
    w = right - left
    h = bottom - top
    crease = left + w * (0.35 + (seed % 3) * 0.13)
    brush.wash(
        p,
        brush.Wash(
            color="#34203d",
            opacity=0.14,
            layers=4,
            border=0.12,
            texture=0.9,
            bleed=0.16,
        ),
        [
            (left, top),
            (left + w * 0.36, top + h * 0.17),
            (right, bottom),
            (left, bottom),
        ],
    )
    brush.wash(
        p,
        brush.Wash(
            color="#ead2a4",
            opacity=0.09,
            layers=3,
            border=0.02,
            texture=0.65,
            bleed=0.09,
        ),
        [
            (crease, top),
            (right, top + h * 0.25),
            (right - w * 0.17, bottom),
            (crease - w * 0.25, bottom),
        ],
    )
    for n in range(folds):
        q = (n + 0.5) / folds
        knots = [
            (crease + (q - 0.5) * w * 0.19, top + h * 0.16),
            (crease + (q - 0.5) * w * 0.31, top + h * 0.34),
            (left + w * q + (q - 0.5) * w * 0.10, top + h * 0.64),
            (left + w * q, bottom - h * 0.04),
        ]
        inkline(p, knots, "#32223c78", 1.4, 0.65)
        inkline(p, [(xx + 0.95, yy - 0.3) for xx, yy in knots], "#f1d4a290", 0.55, 0.85)
    for n in range(3):
        yy = top + h * (0.60 + n * 0.12)
        inkline(
            p,
            [
                (left + w * 0.13, yy),
                (crease, yy + h * 0.07),
                (right - w * 0.13, yy - h * 0.07),
            ],
            "#3a2b455a",
            0.72,
            0.65,
        )
    for n in range(50):
        x = rng.uniform(left, right)
        y = rng.uniform(top, bottom)
        if seed % 3 == 0:
            flower(p, x, y, 1.15, 5, "#d7b56c")
            leaf(p, x + 2, y + 1, 3.5, 45, "#ccb27677")
        elif seed % 3 == 1:
            leaf(p, x, y, 3, -43, "#d8b579a0")
            leaf(p, x + 1, y + 1, 2.5, 145, "#e6c98e88")
            disk(p, x + 2, y - 0.5, 0.9, "#d8b36b", "#d8b36b", 0.15)
        else:
            curve(
                p,
                (x, y),
                [
                    (x + 1, y - 2, x + 3, y - 1, x + 3, y + 1),
                    (x + 1, y + 3, x - 1, y + 1, x, y),
                ],
                "#d9bb8488",
                "#e7c99188",
                0.24,
            )
    for n in range(160):
        x = rng.uniform(left, right)
        y = rng.uniform(top, bottom)
        line(p, (x, y), (x + 0.6, y + 0.1), rng.choice(["#2a244720", "#f5dcad3d"]), 0.2)
    p.pop()


def hand(p, x, y, angle=0):
    p.push()
    p.translate(x, y)
    p.rotate(angle)
    curve(
        p,
        (-2, 5),
        [
            (1, 6, 7, 3, 10, -1),
            (14, -4, 14, -7, 11, -6),
            (8, -4, 7, -2, 4, -2),
            (8, -8, 6, -10, 4, -7),
            (1, -6, 0, -2, -2, 0),
            (-4, 1, -4, 4, -2, 5),
        ],
        "#e9c8a8",
        "#785b4b",
        0.45,
    )
    for j in range(3):
        inkline(
            p,
            [(3 + j * 2, -2 - j), (6 + j * 2, -4 - j), (9 + j, -5 - j)],
            "#92705a",
            0.32,
            0.65,
        )
    p.pop()


def face(p, x, y, tilt=0, variant=0):
    p.push()
    p.translate(x, y)
    p.rotate(tilt)
    p.scale(0.69 + (variant % 3) * 0.028, 0.91 + (variant % 2) * 0.04)
    nose = 12 + (variant % 4) * 0.75
    jaw = 6 + (variant % 3) * 1.4
    skin = [
        "#e4c29b",
        "#dfb58f",
        "#edd0a8",
        "#d7ad86",
        "#dfbe98",
        "#e9cba6",
        "#d4b28e",
    ][variant % 7]
    curve(
        p,
        (-11, -18),
        [
            (-3, -24, 9, -22, 12, -12),
            (12, -7, 11, -3, nose, 0),
            (nose + 1, 2, 10, 2, 11, 4),
            (11, 7, jaw, 12, 1, 14),
            (-6, 12, -10, 7, -12, 1),
            (-14, -6, -15, -13, -11, -18),
        ],
        skin,
        "#654b40",
        0.55,
    )
    brush.wash(
        p,
        brush.Wash(
            color="#ae7961",
            opacity=0.09,
            layers=2,
            border=0.01,
            texture=0.5,
            bleed=0.05,
        ),
        [(-11, -17), (-2, -20), (2, 13), (-10, 7)],
    )
    brow_y = -7 - variant % 2
    inkline(
        p,
        [(-6, brow_y), (-1, brow_y - 1.1), (3, brow_y), (6, brow_y + 1)],
        "#4b3530",
        0.58,
    )
    curve(
        p,
        (-5, -4),
        [(-2, -6, 2, -5.7, 5, -3.6), (1, -2.1, -2, -2.3, -5, -4)],
        "#f1d9b5",
        "#765747",
        0.3,
    )
    disk(p, 1.5, -4.1, 1.05, "#312824", "#312824", 0.15)
    line(p, (5, -2), (6, 1), "#b38568", 0.35)
    inkline(p, [(nose - 4, -1), (nose - 5, 2), (nose - 2, 2)], "#775247", 0.4)
    inkline(p, [(5, 6), (8, 5.5), (10, 6.5)], "#955f51", 0.45)
    line(p, (6, 8), (9, 8), "#b7866c", 0.3)
    curve(
        p,
        (-9, -1),
        [(-14, -5, -16, 2, -12, 6), (-9, 7, -8, 3, -9, -1)],
        skin,
        "#87624e",
        0.4,
    )
    inkline(p, [(-12, 1), (-13, 3), (-11, 4)], "#ad7e62", 0.35)
    if variant in (3, 5, 6):
        beard = ["#554038", "#433830", "#a39a88"][[3, 5, 6].index(variant)]
        length = 16 + (variant % 3) * 4
        curve(
            p,
            (-8, 5),
            [
                (-2, 13, 7, 13, 10, 7),
                (9, length, 1, length + 4, -5, 13),
                (-8, 10, -9, 8, -8, 5),
            ],
            beard,
            "#4f3b32",
            0.4,
        )
        for n in range(15):
            inkline(
                p,
                [(-7 + n * 1.1, 10), (0 + n * 0.35, length + 1), (2 + n * 0.3, length)],
                "#ccb69b68",
                0.27,
                0.65,
            )
        inkline(p, [(4, 4), (7, 3), (11, 4)], "#44342f", 0.6, 0.8)
    elif variant in (1, 2):
        inkline(p, [(4, 4), (7, 3), (11, 4)], "#553d32", 0.65, 0.8)
        inkline(p, [(5, 4), (4, 5), (2, 5)], "#553d32", 0.42, 0.8)
    if variant in (5, 6):
        inkline(p, [(-5, -10), (-1, -11), (4, -10)], "#96735d", 0.28, 0.6)
        inkline(p, [(-1, 2), (0, 6), (-2, 9)], "#a98268", 0.28, 0.6)
    p.pop()


def turban(p, x, y, royal=False):
    p.push()
    p.translate(x, y)
    p.scale(0.87, 0.85)
    if royal:
        curve(
            p,
            (-15, 2),
            [
                (-16, -8, -12, -13, -8, -13),
                (-4, -29, 3, -30, 6, -13),
                (14, -18, 17, -6, 16, 2),
                (5, 7, -4, 7, -15, 2),
            ],
            "#c29b44",
            "#674d2f",
            0.6,
        )
        for n in range(10):
            inkline(
                p,
                [(-13 + n * 2.9, 1), (-10 + n * 2.4, -5), (-7 + n * 1.7, -11)],
                "#f3d88a",
                0.45,
            )
        for x0 in [-10, -3, 4, 11]:
            disk(p, x0, -1, 3.3, "#325c84", "#edd292", 0.3)
        disk(p, 1, -17, 4.6, "#9f3037", "#ecd391", 0.4)
        p.noFill()
        p.stroke("#c7ad69")
        p.strokeWeight(0.5)
        p.bezier(4, -27, 14, -40, 22, -32, 17, -20)
    else:
        curve(
            p,
            (-16, 2),
            [
                (-27, -5, -15, -22, 2, -20),
                (15, -21, 22, -10, 18, -1),
                (7, 7, -5, 7, -16, 2),
            ],
            "#e8dfc5",
            "#73675a",
            0.55,
        )
        for n in range(8):
            inkline(
                p,
                [(-18 + n * 3, -1), (-10 + n * 2.6, -9), (4 + n * 1.6, -15)],
                "#a99a7e",
                0.45,
            )
        curve(
            p,
            (-14, -6),
            [(-2, 2, 7, 3, 17, -3), (18, 1, 3, 9, -11, 2), (-17, 0, -18, -3, -14, -6)],
            "#e8dec5",
            "#9a8c74",
            0.5,
        )
        polygon(p, [(0, -15), (2, -32), (7, -33), (9, -14)], "#e7dbc0", "#766755", 0.4)
        inkline(p, [(3, -29), (4, -21), (6, -15)], "#b0a185", 0.4)
        curve(
            p,
            (-19, -1),
            [(-23, 7, -20, 17, -23, 29), (-17, 20, -15, 6, -13, 0)],
            "#e0d0b1",
            "#897b64",
            0.4,
        )
    p.pop()


def person(p, x, y, s, robe, pose=0, royal=False):
    p.push()
    p.translate(x, y)
    p.scale(s)
    p.randomSeed(int(x * 11 + y * 17))
    flip = -1 if pose in (1, 3, 5) else 1
    p.scale(flip, 1)
    bodyseed = int(x * 3 + y * 7)
    if pose == 2:
        polygon(
            p, [(-16, 78), (-17, 110), (-5, 112), (0, 73)], "#b79172", "#574844", 0.5
        )
        polygon(
            p, [(10, 74), (12, 109), (25, 110), (21, 75)], "#b79172", "#574844", 0.5
        )
        curve(
            p,
            (-19, 109),
            [(-26, 115, -5, 120, -4, 113), (-7, 108, -13, 108, -19, 109)],
            "#3b3b3b",
            "#342a2a",
            0.6,
        )
        curve(
            p,
            (14, 108),
            [(13, 116, 34, 116, 31, 111), (27, 108, 20, 107, 14, 108)],
            "#3b3b3b",
            "#342a2a",
            0.6,
        )
        cloth(
            p,
            (-22, -5),
            [
                (-29, 24, -24, 62, -29, 88),
                (-2, 95, 19, 91, 31, 86),
                (23, 53, 27, 12, 13, -5),
                (3, -13, -10, -11, -22, -5),
            ],
            robe,
            bodyseed,
        )
    elif pose == 3:
        cloth(
            p,
            (-13, 3),
            [
                (-23, 28, -32, 50, -39, 62),
                (-39, 76, -16, 82, 16, 76),
                (36, 69, 37, 60, 20, 52),
                (9, 35, 9, 10, -1, 0),
                (-4, -4, -10, -2, -13, 3),
            ],
            robe,
            bodyseed,
        )
        cloth(
            p,
            (-14, 49),
            [
                (-3, 45, 27, 46, 35, 59),
                (52, 75, 29, 84, 1, 83),
                (-31, 81, -50, 72, -44, 62),
                (-41, 55, -28, 51, -14, 49),
            ],
            robe,
            bodyseed + 1,
        )
    else:
        cloth(
            p,
            (-25, 41),
            [
                (-41, 42, -50, 67, -36, 78),
                (-14, 95, 3, 89, 15, 87),
                (31, 90, 52, 75, 43, 63),
                (29, 41, 10, 45, -25, 41),
            ],
            robe,
            bodyseed,
        )
        cloth(
            p,
            (-19, -7),
            [
                (-24, 9, -26, 27, -23, 44),
                (-11, 56, 14, 59, 27, 44),
                (23, 29, 27, 7, 13, -9),
                (5, -16, -10, -17, -19, -7),
            ],
            robe,
            bodyseed + 4,
        )
        inkline(p, [(-37, 69), (-19, 74), (1, 74), (19, 69), (36, 71)], "#e2bc84", 0.55)
        inkline(p, [(-29, 78), (-5, 82), (13, 82)], "#352b4244", 1.3)
    # The inner coat and folded gold sash follow the torso rather than a grid.
    curve(
        p,
        (-3, -11),
        [(-8, 0, -9, 23, -7, 44), (0, 48, 6, 47, 9, 44), (11, 21, 8, -2, 5, -13)],
        "#bd956d",
        "#56443b",
        0.5,
    )
    for n in range(8):
        disk(p, 1, -5 + n * 6, 1.9, "#dfbc71", "#584432", 0.3)
    curve(
        p,
        (-22, 37),
        [(-9, 35, 9, 36, 25, 35), (25, 41, 8, 43, -22, 43)],
        "#d2ba83",
        "#6c5b41",
        0.45,
    )
    for n in range(13):
        line(p, (-19 + n * 3.3, 38), (-16 + n * 3.3, 41), "#977947", 0.38)
    if pose == 1:
        cloth(
            p,
            (-19, -7),
            [
                (-30, -3, -38, 15, -34, 21),
                (-24, 33, -14, 15, -8, 7),
                (-10, 1, -15, -5, -19, -7),
            ],
            robe,
            bodyseed + 11,
        )
        hand(p, -17, 4, -88)
        cloth(
            p,
            (17, -3),
            [(34, 5, 27, 23, 9, 31), (-1, 30, -3, 26, 3, 21), (11, 16, 18, 8, 17, -3)],
            robe,
            bodyseed + 9,
        )
        hand(p, 0, 24, 20)
    elif pose == 2:
        cloth(
            p,
            (-16, -5),
            [
                (-35, 6, -34, 26, -19, 25),
                (-10, 23, -4, 14, -1, 11),
                (-4, 2, -9, -1, -16, -5),
            ],
            robe,
            bodyseed + 8,
        )
        hand(p, -3, 15, 10)
        cloth(
            p,
            (12, -5),
            [(26, 0, 37, 17, 28, 24), (14, 28, 9, 17, 6, 12), (6, 4, 8, 0, 12, -5)],
            robe,
            bodyseed + 9,
        )
        hand(p, 27, 21, -10)
        p.fill("#c6b478")
        p.stroke("#6d5b3e")
        p.strokeWeight(0.6)
        p.ellipse(15, 15, 48, 9)
        for j in range(4):
            disk(p, 2 + j * 8, 12, 6, "#ab4c3e", "#684d38", 0.3)
    elif pose == 4:
        cloth(
            p,
            (-20, -6),
            [
                (-34, 4, -31, 27, -17, 32),
                (-4, 35, 5, 18, 3, 10),
                (-1, 0, -15, -6, -20, -6),
            ],
            robe,
            bodyseed + 3,
        )
        cloth(
            p,
            (17, -5),
            [(33, 2, 34, 21, 20, 30), (13, 34, 9, 25, 5, 22), (13, 14, 17, 9, 17, -5)],
            robe,
            bodyseed + 8,
        )
        p.push()
        p.translate(16, 41)
        p.rotate(-44)
        curve(
            p,
            (-18, -2),
            [
                (-24, 17, -7, 32, 8, 23),
                (22, 13, 22, 1, 13, -5),
                (6, -10, -9, -11, -18, -2),
            ],
            "#bc914f",
            "#59472f",
            0.6,
        )
        polygon(p, [(-3, -5), (-4, -57), (4, -60), (5, -4)], "#9a773f", "#524531", 0.6)
        for j in range(4):
            line(p, (-2 + j * 1.5, -57), (-2 + j * 1.7, 21), "#e1c48c", 0.35)
        p.fill("#674b2d")
        p.noStroke()
        p.circle(0, 9, 8)
        p.pop()
        hand(p, 23, 22, -42)
        hand(p, 9, 40, 45)
    elif pose == 3:
        cloth(
            p,
            (11, -2),
            [(25, 5, 35, 28, 23, 35), (16, 36, 9, 20, 5, 12), (4, 4, 7, 0, 11, -2)],
            robe,
            bodyseed + 8,
        )
        hand(p, 24, 32, 22)
        polygon(p, [(19, 40), (42, 36), (55, 50), (32, 54)], "#ead5a6", "#8b7050", 0.55)
        for j in range(4):
            inkline(p, [(30, 41 + j * 2.1), (45, 39 + j * 2.1)], "#76624e", 0.35)
        hand(p, 4, 38, 60)
    else:
        cloth(
            p,
            (-18, -8),
            [
                (-31, -1, -40, 13, -30, 22),
                (-17, 34, -8, 16, -5, 8),
                (-7, 1, -13, -6, -18, -8),
            ],
            robe,
            bodyseed + 3,
        )
        hand(p, -9, 13, -30)
        cloth(
            p,
            (17, -6),
            [(30, -3, 40, 9, 32, 20), (24, 29, 9, 13, 6, 7), (8, 1, 13, -5, 17, -6)],
            robe,
            bodyseed + 7,
        )
        hand(p, 32, 17, 15)
        if royal:
            p.fill("#b8463f")
            p.stroke("#d1b071")
            p.strokeWeight(0.5)
            p.ellipse(43, 12, 13, 6)
            line(p, (42, 15), (43, 20), "#ae8651", 0.7)
    # A slight head inclination changes the social direction of each figure.
    tilt = {0: 0, 1: -16, 2: -5, 3: 21, 4: 11, 5: -9, 6: 8}.get(pose, 0)
    face(p, 0, -24, tilt, (pose + (int(x) % 3)) % 7)
    turban(p, 0, -42, royal)
    if royal:
        inkline(p, [(-17, -2), (-19, 16), (-13, 33)], "#e6c17d", 1.5)
        inkline(p, [(13, -4), (18, 15), (21, 30)], "#e6c17d", 1.5)
    p.pop()


def deer(p, x, y, s, color="#ceae8d", horns=True):
    p.push()
    p.translate(x, y)
    p.scale(s)
    p.randomSeed(int(x * 17 + y))

    def body():
        curve(
            p,
            (-39, 4),
            [
                (-40, -14, -16, -19, 6, -11),
                (19, -9, 24, -21, 27, -37),
                (29, -48, 42, -51, 44, -41),
                (44, -36, 48, -34, 53, -32),
                (56, -28, 46, -27, 39, -30),
                (36, -14, 43, -3, 28, 11),
                (15, 21, -25, 21, -39, 4),
            ],
            color,
            "#685344",
            0.6,
        )

    # Staggered near and far limbs have flexed knees and dark hooves.
    for off, bend, shade in [
        (-25, -9, "#967e63"),
        (17, 7, "#a58c6e"),
        (-32, 2, color),
        (23, -6, color),
    ]:
        curve(
            p,
            (off, 9),
            [
                (off + 5, 17, off + bend, 28, off + bend + 2, 33),
                (off + bend + 6, 36, off + 5, 43, off + 1, 47),
                (off - 2, 48, off - 4, 45, off - 3, 43),
                (off + 2, 35, off + bend - 2, 31, off + bend - 3, 27),
                (off - 6, 20, off - 4, 16, off, 9),
            ],
            shade,
            "#715849",
            0.45,
        )
        polygon(
            p,
            [(off - 5, 43), (off + 4, 43), (off + 3, 48), (off - 6, 48)],
            "#484137",
            "#705948",
            0.3,
        )
    body()
    p.push()
    p.clip(body)
    brush.wash(
        p,
        brush.Wash(
            color="#705642", opacity=0.12, layers=3, border=0.03, texture=0.7, bleed=0.1
        ),
        [(-42, 1), (7, 0), (37, -25), (39, 20), (-41, 22)],
    )
    rng = random.Random(int(x + y))
    for n in range(100):
        xx = rng.uniform(-39, 44)
        yy = rng.uniform(-46, 18)
        inkline(
            p, [(xx, yy), (xx + 2, yy + 1), (xx + 3, yy + 3)], "#765d4959", 0.28, 0.6
        )
    for n in range(15):
        disk(p, -28 + (n % 5) * 10, -8 + (n // 5) * 8, 2.5, "#f3dfb8", "#bcaa88", 0.2)
    p.pop()
    inkline(p, [(-35, 4), (-51, -1), (-58, -12)], "#6b5946", 1.0, 0.9)
    leaf(p, 40, -43, 12, -63, color)
    leaf(p, 34, -41, 10, -112, color)
    p.fill("#514137")
    p.noStroke()
    p.circle(52, -31, 3.5)
    curve(
        p,
        (36, -38),
        [(39, -40, 42, -38, 42, -36), (40, -35, 37, -35, 36, -38)],
        "#ebe0bd",
        "#6b5545",
        0.35,
    )
    disk(p, 39, -37, 1.5, "#3b352b", "#3b352b", 0.2)
    inkline(p, [(43, -34), (48, -33), (52, -34)], "#7e604b", 0.3)
    if horns:
        for direction in (-1, 1):
            points = [(34, -45), (34 + direction * 4, -58), (32 + direction * 7, -70)]
            inkline(p, points, "#705b45", 1.0, 0.8)
            inkline(
                p,
                [
                    (34 + direction * 4, -57),
                    (36 + direction * 13, -59),
                    (38 + direction * 15, -65),
                ],
                "#705b45",
                0.7,
                0.8,
            )
            inkline(
                p,
                [(33 + direction * 6, -64), (31 + direction * 11, -68)],
                "#705b45",
                0.65,
                0.8,
            )
    p.pop()


def flames(p, x, y, s):
    p.push()
    p.translate(x, y)
    p.scale(s)
    p.randomSeed(9801)
    rng = random.Random(763)
    for n in range(11):
        xx = rng.uniform(-40, 35)
        yy = rng.uniform(8, 22)
        p.push()
        p.translate(xx, yy)
        p.rotate(rng.uniform(-27, 25))
        curve(
            p,
            (-20, -3),
            [(-22, -7, 14, -7, 19, -3), (21, 2, -16, 5, -20, 1)],
            "#7b6554",
            "#564b44",
            0.45,
        )
        for j in range(3):
            inkline(
                p,
                [(-17, -2 + j * 1.3), (-4, -3 + j * 1.3), (16, -2 + j * 1.3)],
                "#aa9471",
                0.35,
            )
        p.pop()
    for n in range(23):
        xx = rng.uniform(-40, 35)
        ht = rng.uniform(29, 73)
        curl = rng.uniform(-24, 24)
        r = rng.uniform(4.5, 8)
        # The tip turns back into the plume, producing an S rather than a straight wedge.
        curve(
            p,
            (xx - r, 15),
            [
                (xx - r * 2, -2, xx + curl - r * 2, -ht * 0.30, xx + curl, -ht * 0.54),
                (
                    xx + curl + r * 1.9,
                    -ht * 0.81,
                    xx + curl - r * 0.7,
                    -ht * 0.94,
                    xx + curl - r * 0.6,
                    -ht,
                ),
                (
                    xx + curl + r * 3,
                    -ht * 0.88,
                    xx + curl + r * 2,
                    -ht * 0.62,
                    xx + curl + r * 0.6,
                    -ht * 0.44,
                ),
                (xx + r * 1.2, -ht * 0.17, xx + r * 2.1, 5, xx + r, 15),
                (xx + r * 0.1, 19, xx - r * 0.5, 18, xx - r, 15),
            ],
            rng.choice(["#bb4436", "#a94333", "#c8703d", "#b95537"]),
            "#87533d",
            0.43,
        )
        curve(
            p,
            (xx - r * 0.22, 14),
            [
                (
                    xx - r * 0.7,
                    0,
                    xx + curl - r,
                    -ht * 0.21,
                    xx + curl - r * 0.3,
                    -ht * 0.38,
                ),
                (
                    xx + curl + r,
                    -ht * 0.45,
                    xx + curl + r * 0.25,
                    -ht * 0.59,
                    xx + curl - r * 0.2,
                    -ht * 0.66,
                ),
                (
                    xx + curl + r * 1.2,
                    -ht * 0.57,
                    xx + curl + r * 1.3,
                    -ht * 0.39,
                    xx + curl + r * 0.5,
                    -ht * 0.28,
                ),
                (xx + r * 0.7, -ht * 0.1, xx + r, 6, xx + r * 0.5, 14),
            ],
            "#d7a854",
            "#e6c179",
            0.25,
        )
        inkline(
            p,
            [
                (xx, 13),
                (xx - 2, -2),
                (xx + curl - r * 0.3, -ht * 0.26),
                (xx + curl + r * 0.2, -ht * 0.40),
            ],
            "#f0d39b",
            0.5,
            0.8,
        )
    for n in range(9):
        xx = rng.uniform(-32, 31)
        yy = rng.uniform(-74, -48)
        curve(
            p,
            (xx, yy),
            [
                (xx - 2, yy - 5, xx + 7, yy - 7, xx + 3, yy - 13),
                (xx + 10, yy - 6, xx + 5, yy - 3, xx + 4, yy + 2),
                (xx + 1, yy + 4, xx, yy + 2, xx, yy),
            ],
            "#c58a4d",
            "#ae7544",
            0.3,
        )
    for n in range(15):
        xx = rng.uniform(-26, 28)
        yy = rng.uniform(-87, -54)
        p.noStroke()
        p.fill("#c3915965")
        p.circle(xx, yy, 0.8)
    p.pop()


def painting(p):
    p.angleMode(DEGREES)
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
    for n in range(110):
        sprig(p, rng.uniform(245, 854), rng.uniform(672, 1060), rng.uniform(11, 25))
    cypress(p, 332, 701, 161)
    cypress(p, 771, 633, 155)
    # Central dais and the surrounding courtiers follow the source's oval hierarchy.
    carpet(p, 476, 669, 157, 99, "#3c3264")
    person(p, 553, 684, 1.22, "#b75238", 0, True)
    carpet(p, 312, 778, 119, 78, "#6e4855")
    person(p, 370, 786, 1.0, "#326962", 1)
    carpet(p, 674, 769, 117, 77, "#a97855")
    person(p, 729, 779, 1.02, "#4a4370", 3)
    person(p, 279, 659, 0.77, "#9a4a47", 2)
    person(p, 306, 745, 0.84, "#454c8c", 5)
    person(p, 802, 655, 0.76, "#8d4b5f", 2)
    person(p, 813, 756, 0.86, "#b99a58", 1)
    person(p, 362, 898, 0.94, "#454382", 4)
    person(p, 745, 898, 0.94, "#b74f3c", 1)
    person(p, 688, 932, 0.89, "#484986", 6)
    person(p, 471, 983, 0.62, "#617b67", 3)
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
    p.angleMode(DEGREES)
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
    for off, c in [(0, "#a88d52"), (5, "#c9aa65"), (10, "#586474")]:
        p.noFill()
        p.stroke(c)
        p.strokeWeight(0.6)
        p.rect(202 - off, 1108 - off, 696 + off * 2, 157 + off * 2)
    scroll(p, 256, 1292, 119, 37, "#b19a68")
    scroll(p, 726, 1292, 119, 37, "#b19a68")
    rosette(p, 550, 1314, 20, 8)


def ember_sparks(p):
    p.angleMode(DEGREES)
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


@sketch(size=(W * 1.5, H * 1.5), background="#17252b", capture_at=3.0)
class ShahnamehFolio:
    def setup(self, ctx):
        # Only the spark overlay animates; the folio's geometry and text remain retained.
        parts = [
            at(pen("folio.paper", paper, Cache.Picture), 0, 0, W, H),
            at(pen("folio.border", border, Cache.Picture), 0, 0, W, H),
            at(pen("folio.headings", headings, Cache.Picture), 0, 0, W, H),
            at(pen("folio.illumination", illumination, Cache.Picture), 0, 0, W, H),
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
        # Verses read across the four columns from right to left, then down.
        verses = [
            "به نام خداوندِ جان و خرد",
            "کز این برتر، اندیشه، بر نگذرد",
            "خداوندِ نام و خداوندِ جای",
            "خداوندِ روزی‌دِهِ رهنمای",
            "خداوندِ کیوان و گَردان‌سپهر",
            "فروزندهٔ ماه و ناهید و مِهر",
            "ز نام و نشان و گمان، برتر است",
            "نگارندهٔ بَرشده‌پیکر است",
            "به بینندگان، آفریننده را",
            "نبینی، مرنجان دو بیننده را",
            "نیابد بِدو، نیز، اندیشه، راه",
            "که او برتر از نام و از جایگاه",
            "سخن، هر چه زین گوهران، بُگذرد",
            "نیابد، بِدو راه، جان و خرد",
            "خرد، گر سخن، برگزیند همی",
            "همان را گزیند که بیند همی",
        ]
        max_width = max(
            ctx.measure(
                text(verse)
                .font(Type(language="fa"))
                .fontFamily("Noto Nastaliq Urdu")
                .fontSize(17.8)
            ).width()
            for verse in verses
        )
        verse_size = 17.8 * min(1.0, 151.0 / max_width)
        for index, verse in enumerate(verses):
            parts.append(
                label(
                    verse,
                    730 - (index % 4) * 174,
                    1107 + (index // 4) * 35,
                    160,
                    40,
                    verse_size,
                    "Noto Nastaliq Urdu",
                    language="fa",
                    baseline=29,
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
                "Original miniature · Opening verses by Ferdowsi",
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
        ctx.render(
            positioned(printed_page, sparks)
            .width(W)
            .height(H)
            .transformOrigin("0%", "0%")
            .scale(1.5)
        )
