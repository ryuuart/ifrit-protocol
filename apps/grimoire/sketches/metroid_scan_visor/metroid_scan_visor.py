"""Optical biological research inside a retained, native scan visor.

TAGS: Studies/Games, Drawing/Organic, Drawing/Geometry, Layout/3D
TAGS: Materials/Shaders, Motion/Bindings, Typography/Interface, Runtime/Python
"""

import math
import random
from pathlib import Path

from sigil.compose import Cache, Overflow, box, pen, positioned, text
from sigil.compose.kit import at
from sigil.draw import CLOSE, DEGREES, SHAPE
from sigil.material import Paint, shader
from sigil.motion import animate
from sigil.sketch import sketch

W, H = 1440, 960
CYAN = "#d1eeec"
MUTED = "#648f99"
AMBER = "#efb360"
SOURCE = Path(__file__).parent


def line(p, a, b, color="#75999d", weight=1):
    p.noFill()
    p.stroke(color)
    p.strokeWeight(weight)
    p.line(*a, *b)


def poly(p, points, color, edge="#577b81", weight=1):
    p.fill(color)
    p.stroke(edge)
    p.strokeWeight(weight)
    p.beginShape()
    for point in points:
        p.vertex(*point)
    p.endShape(CLOSE)


def ramp(colors, a=(0, 0), b=(0, 1)):
    return Paint.linearGradient(a, b, colors)


def label(s, x, y, w, size=12, color=CYAN, mono=False):
    face = "Menlo" if mono else "DIN Alternate"
    return at(text(s).fontFamily(face).fontSize(size).ink(color), x, y, w, size * 1.55)


def panel(p, w, h):
    for margin, color, edge, weight in [
        (0, "#07151ceb", "#426c78", 5),
        (3, "#071118e8", "#a8c3cdaa", 1.5),
        (7, "#061018e6", "#507783", 1),
        (11, "#071017e5", "#172e39", 1),
    ]:
        q = margin
        poly(
            p,
            [
                (q + 13, q),
                (w - q - 16, q),
                (w - q, q + 15),
                (w - q, h - q - 16),
                (w - q - 16, h - q),
                (q + 15, h - q),
                (q, h - q - 16),
                (q, q + 15),
            ],
            color,
            edge,
            weight,
        )
    p.noFill()
    for row in range(16, int(h - 14), 3):
        line(p, (13, row), (w - 13, row), "#92c6d20b", 0.65)
    line(p, (21, 2), (w - 26, 2), "#d6f0ec70", 1.2)
    line(p, (2, 22), (2, h - 28), "#d6f0ec38", 1)


def tank(p):
    p.angleMode(DEGREES)
    p.fill(
        Paint.radialGradient(
            (0.47, 0.54),
            0.65,
            [(0, "#38adb139"), (0.55, "#3ca3b52a"), (1, "#29545b13")],
        ),
        SHAPE,
    )
    p.stroke("#6ec8d22b")
    p.strokeWeight(1)
    p.beginShape()
    p.vertex(490, 160)
    p.bezierVertex(550, 104, 882, 103, 948, 161)
    p.vertex(960, 645)
    p.bezierVertex(862, 702, 576, 702, 478, 645)
    p.endShape(CLOSE)
    p.noFill()
    for n in range(8):
        y = 170 + n * 65
        p.stroke("#72b7bd2b")
        p.strokeWeight(1.5)
        p.bezier(490, y, 588, y - 31, 854, y - 31, 949, y)
    for y in (151, 645):
        p.fill(ramp(["#0e2027", "#55707a", "#122730"]), SHAPE)
        p.stroke("#7596a060")
        p.strokeWeight(1)
        p.beginShape()
        p.vertex(474, y)
        p.bezierVertex(555, y - 29, 869, y - 31, 965, y)
        p.vertex(966, y + 21)
        p.bezierVertex(860, y - 4, 560, y - 1, 473, y + 23)
        p.endShape(CLOSE)
        line(p, (564, y + 3), (880, y + 3), "#b5fff8aa", 2.4)
    for side in (-1, 1):
        x = 719 + side * 222
        p.noFill()
        p.stroke("#050d15")
        p.strokeWeight(8)
        p.bezier(x, 164, x - side * 9, 293, x - side * 11, 508, x - side * 18, 646)
        p.stroke("#57798277")
        p.strokeWeight(1.4)
        p.bezier(
            x + 3,
            164,
            x - side * 9 + 3,
            293,
            x - side * 11 + 3,
            508,
            x - side * 18 + 3,
            646,
        )
    rng = random.Random(190)
    for n in range(100):
        x = rng.uniform(500, 945)
        y = rng.uniform(175, 640)
        p.fill("#afd5cb21" if n % 7 else "#f1fee65e")
        p.noStroke()
        p.circle(x, y, rng.choice([0.7, 1, 1.7, 2.1]))


def machinery(p):
    p.angleMode(DEGREES)
    rng = random.Random(3009)
    for x, y, r in [(1112, 191, 92), (1311, 278, 66)]:
        p.fill(
            Paint.radialGradient(
                (0.28, 0.22),
                0.84,
                [
                    (0, "#3a5159"),
                    (0.35, "#152933"),
                    (0.65, "#0a141b"),
                    (0.89, "#43535a"),
                    (1, "#061018"),
                ],
            ),
            SHAPE,
        )
        p.stroke("#60767c65")
        p.strokeWeight(2)
        p.circle(x, y, r * 2)
        p.fill("#02070c")
        p.stroke("#324b5577")
        p.circle(x, y, r * 1.47)
        for n in range(43):
            a = n * math.tau / 43
            pts = []
            for radius, angle in [
                (r * 0.21, a),
                (r * 0.61, a + 0.08),
                (r * 0.65, a + 0.14),
                (r * 0.24, a + 0.10),
            ]:
                pts.append((x + math.cos(angle) * radius, y + math.sin(angle) * radius))
            poly(p, pts, "#172e36", "#526e764a", 0.55)
        p.fill(
            Paint.radialGradient((0.29, 0.24), 0.73, ["#3c525d", "#0e1d28", "#020a12"]),
            SHAPE,
        )
        p.stroke("#6a899356")
        p.strokeWeight(1)
        p.circle(x, y, r * 0.57)
        for n in range(12):
            a = n * math.tau / 12
            p.fill("#070e18")
            p.stroke("#6e869244")
            p.strokeWeight(0.6)
            p.circle(x + math.cos(a) * r * 0.90, y + math.sin(a) * r * 0.9, 4)
    for side in (-1, 1):
        p.push()
        p.translate(720, 0)
        p.scale(side, 1)
        for j in range(4):
            p.noFill()
            for width, c in [
                (22, "#050a12"),
                (17, "#243742"),
                (11, "#0b1a24"),
                (1.4, "#63778144"),
            ]:
                p.stroke(c)
                p.strokeWeight(width)
                p.bezier(
                    371 + j * 20,
                    98,
                    492 + j * 13,
                    171,
                    412 + j * 24,
                    402,
                    487 + j * 31,
                    617,
                )
            for k in range(8):
                y = 161 + k * 47
                x = 407 + j * 19 + math.sin(k * 0.48) * 19
                line(p, (x - 8, y), (x + 9, y + 5), "#647b825c", 1.2)
        for k in range(8):
            y = 188 + k * 60
            poly(
                p,
                [(461, y), (536, y - 20), (536, y - 4), (464, y + 16)],
                "#11232c",
                "#48616d42",
                0.7,
            )
            for j in range(4):
                line(
                    p,
                    (476 + j * 13, y + 3 - j * 3),
                    (480 + j * 13, y - 9 - j * 3),
                    "#dca85369",
                    3.1,
                )
        p.pop()
    for n in range(420):
        x = rng.uniform(360, 1080)
        y = rng.uniform(105, 920)
        line(p, (x, y), (x + 1.4, y + 0.12), "#7693a009", 0.5)


def hardware(p):
    p.angleMode(DEGREES)
    rng = random.Random(331)
    for side in (-1, 1):
        p.push()
        p.translate(720, 0)
        p.scale(side, 1)
        for pts, colors, edge in [
            (
                [
                    (0, 0),
                    (720, 0),
                    (720, 58),
                    (625, 68),
                    (492, 57),
                    (369, 86),
                    (196, 91),
                    (164, 71),
                    (0, 91),
                ],
                ["#09151b", "#25373e", "#0a1922"],
                "#536975",
            ),
            (
                [
                    (0, 7),
                    (651, 5),
                    (582, 29),
                    (395, 42),
                    (322, 25),
                    (185, 32),
                    (175, 67),
                    (0, 71),
                ],
                ["#233139", "#111b25", "#030b11"],
                "#233c46",
            ),
            (
                [
                    (0, 68),
                    (177, 54),
                    (190, 70),
                    (370, 61),
                    (492, 39),
                    (622, 50),
                    (606, 75),
                    (345, 113),
                    (190, 104),
                    (178, 90),
                    (0, 96),
                ],
                ["#4d656e", "#1b303a", "#08131a"],
                "#526d79",
            ),
            (
                [
                    (720, 960),
                    (720, 900),
                    (601, 897),
                    (530, 919),
                    (400, 899),
                    (319, 914),
                    (277, 947),
                    (0, 935),
                    (0, 960),
                ],
                ["#0a1820", "#30474f", "#111f2a"],
                "#52727c",
            ),
        ]:
            p.fill(ramp(colors), SHAPE)
            p.stroke(edge)
            p.strokeWeight(1.2)
            p.beginShape()
            for xy in pts:
                p.vertex(*xy)
            p.endShape(CLOSE)
        p.noFill()
        p.stroke("#d1f4f246")
        p.strokeWeight(0.9)
        p.bezier(0, 99, 224, 113, 444, 86, 674, 61)
        for n in range(28):
            x = 230 + n * 11
            line(p, (x, 7), (x - 5, 32), "#040a10", 4)
            line(p, (x + 2, 8), (x - 3, 30), "#3f545c88", 0.65)
        for x, y in [
            (168, 45),
            (204, 78),
            (383, 40),
            (546, 56),
            (651, 28),
            (451, 930),
            (628, 919),
        ]:
            p.fill("#040a10")
            p.stroke("#52707a")
            p.strokeWeight(0.8)
            p.circle(x, y, 7)
            line(p, (x - 1.5, y), (x + 1.5, y), "#71949a", 0.6)
        for n in range(240):
            x = rng.uniform(12, 690)
            y = rng.uniform(3, 55)
            line(p, (x, y), (x + rng.uniform(1.0, 5), y + 0.2), "#8499a312", 0.55)
        for n in range(18):
            x = 341 + n * 18
            line(p, (x, 935 + math.sin(n * 0.5) * 8), (x, 954), "#030a12", 5)
            line(
                p, (x + 2, 938 + math.sin(n * 0.5) * 8), (x + 2, 952), "#6e8e992a", 0.8
            )
        for lower in (False, True):
            p.push()
            p.translate(0, 747 if lower else 286)
            p.scale(1, -1 if lower else 1)
            points = [(386, 31), (386, 12), (366, -8), (337, -6), (328, -1)]
            for width, color in [
                (10, "#6a929e50"),
                (5, "#101e2bb0"),
                (1.6, "#bdd9eacc"),
            ]:
                p.noFill()
                p.stroke(color)
                p.strokeWeight(width)
                p.beginShape()
                for xy in points:
                    p.vertex(*xy)
                p.endShape()
            p.noFill()
            p.stroke("#87947b66")
            p.strokeWeight(0.8)
            p.line(381, 49, 381, 160)
            p.bezier(375, 57, 374, 100, 373, 139, 380, 160)
            p.pop()
        p.pop()
    for n in range(4):
        p.fill("#bdffff")
        p.stroke("#6da8bc")
        p.strokeWeight(3)
        p.rect(618 + n * 54, 66, 39, 6, 1)
    p.noFill()
    p.stroke("#b7f9fc24")
    p.strokeWeight(16)
    p.line(602, 68, 825, 68)


def consoles(p):
    p.angleMode(DEGREES)
    rng = random.Random(661)
    for index, x in enumerate([22, 152, 320, 963, 1138, 1305]):
        y = 727 + (index % 3) * 18
        p.push()
        p.translate(x, y)
        p.rotate(-5 if x < 720 else 5)
        p.fill(ramp(["#364c55", "#08151f"]), SHAPE)
        p.stroke("#738f9948")
        p.strokeWeight(1)
        p.rect(0, 0, 113, 78, 2)
        p.fill("#09323e")
        p.stroke("#74aebc81")
        p.rect(8, 7, 97, 60, 1)
        for cell in range(5):
            q = cell % 3
            r = cell // 3
            p.fill("#40798144")
            p.stroke("#7cc2c481")
            p.strokeWeight(0.6)
            p.rect(13 + q * 29, 12 + r * 25, 24, 19)
            for row in range(4):
                line(
                    p,
                    (16 + q * 29, 16 + r * 25 + row * 3),
                    (32 + q * 29 + rng.randrange(0, 8), 16 + r * 25 + row * 3),
                    "#a5ead58c",
                    0.45,
                )
        p.fill("#4e8f9080")
        p.noStroke()
        p.rect(15, 64, 43, 2)
        p.fill("#091018")
        p.rect(42, 79, 22, 16)
        p.pop()
    for side in (-1, 1):
        p.push()
        p.translate(720, 0)
        p.scale(side, 1)
        p.fill(ramp(["#283d49", "#060c17"]), SHAPE)
        p.stroke("#426575")
        p.strokeWeight(1)
        p.beginShape()
        p.vertex(720, 824)
        p.bezierVertex(488, 791, 378, 826, 304, 883)
        p.vertex(299, 942)
        p.bezierVertex(463, 880, 566, 864, 720, 907)
        p.endShape(CLOSE)
        p.noFill()
        p.stroke("#79c3c82b")
        p.strokeWeight(1)
        p.bezier(316, 885, 438, 817, 563, 828, 718, 851)
        for n in range(13):
            x = 354 + n * 27
            line(
                p,
                (x, 858 + math.sin(n * 0.32) * 26),
                (x + 4, 894 + math.sin(n * 0.32) * 20),
                "#081019",
                3,
            )
        p.pop()


def bracket(p, w, h):
    for x in (0, w):
        for y in (0, h):
            p.push()
            p.translate(x, y)
            p.scale(-1 if x else 1, -1 if y else 1)
            for width, c in [(7, "#5b94a65e"), (2.1, "#c0eef0b8"), (0.7, "#f0fffacf")]:
                p.noFill()
                p.stroke(c)
                p.strokeWeight(width)
                p.beginShape()
                p.vertex(0, 24)
                p.vertex(0, 10)
                p.vertex(10, 0)
                p.vertex(28, 0)
                p.endShape()
            p.pop()


def leaders(p):
    for pts in [
        [(108, 168), (71, 131), (26, 131)],
        [(226, 247), (261, 266), (285, 266)],
        [(160, 398), (117, 429), (39, 429)],
    ]:
        p.noFill()
        p.stroke(AMBER)
        p.strokeWeight(0.7)
        p.beginShape()
        for xy in pts:
            p.vertex(*xy)
        p.endShape()
        p.circle(*pts[0], 5)
    for n in range(19):
        line(
            p,
            (274, 98 + n * 18),
            (281 if n % 4 == 0 else 278, 98 + n * 18),
            "#64929b6c",
            0.6,
        )
    for n in range(3):
        p.noFill()
        p.stroke("#4f879328")
        p.strokeWeight(0.6)
        p.ellipse(154, 263, 215 + n * 21, 286 + n * 21)


def spectrum(p):
    p.noFill()
    for y in range(0, 91, 15):
        line(p, (0, y), (258, y), "#5c8b9c25", 0.6)
    for x in range(0, 259, 26):
        line(p, (x, 0), (x, 90), "#5c8b9c25", 0.6)
    for lane, c in enumerate([AMBER, "#81d4dca6", "#79adb45a"]):
        p.stroke(c)
        p.strokeWeight(0.9)
        p.beginShape()
        for x in range(259):
            e = math.exp(-(((x - 125) / 54) ** 2))
            p.vertex(x, 49 + math.sin(x * (0.145 + lane * 0.048)) * e * (31 - lane * 7))
        p.endShape()


@sketch(size=(W, H), background="#020910", capture_at=4.2)
class MetroidScanVisor:
    def setup(self, ctx):
        hub = ctx.assets.hub()
        optical = ctx.local("specimen.sksl")
        creature = shader(hub, optical, {"bearing": -8.0, "diagnostic": 0.0})
        diagnostic = shader(hub, optical, {"bearing": -22.0, "diagnostic": 1.0})
        room = shader(hub, ctx.local("laboratory.sksl"))
        fluid = shader(hub, ctx.local("fluid.sksl"))
        parts = [
            at(box().fill(room).cache(Cache.Texture), 0, 0, W, H).translateX(
                animate(from_=-1.7, to=1.7, duration=9, alternate=True, loop=-1)
            ),
            at(pen("lab.machinery", machinery, Cache.Texture), 0, 0, W, H),
            at(pen("lab.tank", tank, Cache.Texture), 0, 0, W, H),
            at(box().fill(creature).cache(Cache.Texture), 490, 210, 460, 490)
            .translateY(animate(from_=-5, to=6, duration=8, alternate=True, loop=-1))
            .rotate(animate(from_=-1.3, to=1.3, duration=11, alternate=True, loop=-1)),
            at(box().fill(fluid), 493, 169, 455, 483),
            at(pen("lab.consoles", consoles, Cache.Texture), 0, 0, W, H),
        ]
        left = [
            at(
                pen("morphology.panel", lambda p: panel(p, 306, 652), Cache.Picture),
                0,
                0,
                306,
                652,
            ),
            label("BIOFORM DATABASE", 22, 22, 270, 13),
            label("MORPHOLOGY / 078.006", 22, 44, 268, 8, MUTED, True),
            at(box().fill(diagnostic).cache(Cache.Texture), 24, 93, 260, 378),
            at(pen("morphology.leaders", leaders, Cache.Picture), 0, 0, 306, 652),
            label("A  //  MEMBRANE", 20, 111, 168, 8, AMBER, True),
            label("B", 264, 273, 29, 8, AMBER, True),
            label("C // MANDIBLE", 18, 435, 189, 8, AMBER, True),
            label("SEMI-PERMEABLE ENVELOPE", 23, 482, 275, 9, CYAN, True),
            label("TETRANUCLEAR ENERGY CORE", 23, 505, 275, 9, AMBER, True),
            label("CHITINOUS ATTACHMENT", 23, 528, 275, 9, CYAN, True),
            label("ORIGIN", 23, 563, 117, 8, MUTED, True),
            label("SR388", 187, 562, 93, 10, AMBER, True),
            label("LIFE-CYCLE", 23, 589, 134, 8, MUTED, True),
            label("LARVAL", 187, 588, 93, 10, CYAN, True),
            label("CONTACT  /  HIGH RISK", 23, 620, 270, 9, "#db8974", True),
        ]
        parts.append(
            at(positioned(*left).cache(Cache.Texture), 48, 143, 306, 652)
            .rotateY(11)
            .rotate(-2)
        )
        right = [
            at(
                pen("analysis.panel", lambda p: panel(p, 306, 470), Cache.Picture),
                0,
                0,
                306,
                470,
            ),
            label("ANATOMICAL COMPARISON", 22, 21, 270, 12),
            label("OPTICAL  /  BETA  /  STRUCTURE", 22, 45, 270, 8, MUTED, True),
        ]
        for i, (bearing, colour) in enumerate([(-52, 0), (0, 1), (58, 1)]):
            mode = shader(
                hub, optical, {"bearing": float(bearing), "diagnostic": float(colour)}
            )
            right.append(
                at(box().fill(mode).cache(Cache.Texture), 12 + i * 95, 91, 99, 150)
            )
            right.append(
                label(
                    ["OBLIQUE", "FRONTAL", "PROFILE"][i],
                    20 + i * 95,
                    250,
                    90,
                    7,
                    MUTED,
                    True,
                )
            )
        right += [
            label("ENERGY TRANSFER", 23, 285, 276, 9, CYAN, True),
            label("98.4", 22, 306, 186, 42),
            label("% MATCH", 198, 337, 90, 9, AMBER, True),
            at(pen("spectrum", spectrum, Cache.Picture), 24, 360, 258, 90),
        ]
        parts.append(
            at(positioned(*right).cache(Cache.Texture), 1085, 306, 306, 470)
            .rotateY(-11)
            .rotate(2)
        )
        data = [
            at(
                pen("acquired.panel", lambda p: panel(p, 673, 178), Cache.Picture),
                0,
                0,
                673,
                178,
            ),
            label("Morphology: Metroid", 24, 14, 620, 27),
            label("Research data acquired.", 25, 49, 620, 13, AMBER),
            label(
                "A parasitic life-form that absorbs the life energy of other\norganisms. A translucent membrane encloses four energy\nnuclei. Extreme cold interrupts the organism’s metabolism.",
                25,
                74,
                623,
                16,
            ),
            label(
                "CREATURES  >>  TALLON IV  >>  RESEARCH", 25, 150, 533, 8, MUTED, True
            ),
            label("[ A ]", 592, 145, 56, 15),
        ]
        parts.append(
            at(positioned(*data).cache(Cache.Texture), 388, 743, 673, 178).opacity(
                animate(from_=0, to=1, duration=0.45, delay=3.3)
            )
        )
        parts += [
            at(pen("visor.hardware", hardware, Cache.Texture), 0, 0, W, H),
            label("92", 572, 123, 97, 47),
            label("ENERGY", 573, 113, 121, 8, MUTED, True),
            at(box().fill("#9ce7e0"), 671, 154, 205, 5),
            at(box().fill("#2e656f"), 876, 154, 24, 5),
            label("SCAN VISOR", 71, 119, 275, 12, CYAN),
            label("PHENDRANA / LAB AETHER", 71, 96, 300, 7, MUTED, True),
            label("LOGBOOK", 1100, 823, 281, 11),
            label("078 / 120", 1100, 845, 264, 26),
            label("RESEARCH COMPLETE  65%", 1100, 884, 265, 8, MUTED, True),
            label("HOLD [ ZL ] TO ANALYZE", 1113, 209, 255, 9, CYAN, True),
            label("BIO-SIGNATURE  //  LOCKED", 529, 215, 420, 9, CYAN, True),
            label("SCAN COMPLETE.", 565, 270, 369, 31).opacity(
                animate(from_=0, to=1, duration=0.35, delay=3.3)
            ),
            at(
                pen("optic.lock", lambda p: bracket(p, 397, 384), Cache.Texture),
                519,
                313,
                397,
                384,
            ),
            label("SCANNING", 647, 704, 209, 13).opacity(
                animate(from_=1, to=0, duration=0.3, delay=3.3)
            ),
            label("RESEARCH ACQUIRED", 599, 704, 311, 11, AMBER).opacity(
                animate(from_=0, to=1, duration=0.3, delay=3.3)
            ),
            at(box().fill("#45808099"), 539, 727, 359, 3),
            at(box().fill("#c2eeee"), 539, 727, 359, 3)
            .transformOrigin("0%", "50%")
            .scaleX(animate(from_=0, to=1, duration=3.3)),
            label("08.4 m", 931, 683, 104, 9, CYAN, True),
            label("STUDY / METROID PRIME", 64, 844, 270, 8, MUTED, True),
            label("ORIGINAL BIOLOGICAL RESEARCH", 64, 863, 290, 7, MUTED, True),
        ]
        for tankno in range(5):
            parts.append(
                at(
                    box().fill("#b9f2e9" if tankno < 4 else "#385f6a"),
                    677 + tankno * 18,
                    135,
                    12,
                    6,
                )
            )
        sweep = at(
            box()
            .fill(
                Paint.linearGradient(
                    (0, 0), (0, 1), ["#b5fff200", "#b5fff224", "#d9fff978"]
                )
            )
            .cache(Cache.Texture),
            0,
            0,
            395,
            21,
        )
        sweep.translateY(animate(from_=-21, to=384, duration=3.3)).opacity(
            animate(from_=1, to=0, duration=0.25, delay=3.3)
        )
        parts.append(at(positioned(sweep).overflow(Overflow.Clip), 520, 314, 395, 384))
        target = at(box().fill("#ea9f5234"), 705, 439, 21, 21)
        parts.append(
            target.opacity(
                animate(from_=0.4, to=1, duration=1.5, alternate=True, loop=-1)
            )
        )
        ctx.render(positioned(*parts).width(W).height(H).perspective(1800).preserve3d())
