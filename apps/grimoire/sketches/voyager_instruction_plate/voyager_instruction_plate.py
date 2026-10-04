"""Study · Voyager — an engraved object and a working decoding folio.

TAGS: Studies/Science, Studies/Cultural Media, Geometry/Diagrams, Materials/Metal, Typography/Documents, Animation/Signals

The vector transcription supplies a height field. Reflection and incision
shading read that field; the folio performs a clock-to-picture sequence.
"""

from math import cos, pi, sin
from pathlib import Path
from random import Random

from sigil.compose import Cache, box, image, pen
from sigil.draw import CENTER, CLOSE, LEFT, Pen
from sigil.material import Filter, ShadowOptions, shader
from sigil.media import load
from sigil.sketch import SketchContext, sketch
from sigil.weave import typeface

WIDTH, HEIGHT = 2100, 1460
DISC_X, DISC_Y, DISC_SIZE = 85, 220, 1150
PAGE_X, PAGE_Y, PAGE_W, PAGE_H = 1280, 132, 682, 1208
INK, QUIET, RED, BLUE = "#292C2C", "#737267", "#9B4C34", "#386875"
CYCLE = 24.0
LOCAL = Path(__file__).parent


def label(
    p: Pen,
    words,
    x,
    y,
    size=15,
    color=INK,
    center=False,
    family="Baskerville",
    italic=False,
) -> None:
    p.noStroke()
    p.fill(color)
    p.textFont(typeface(family, italic=italic))
    p.textSize(size)
    p.textAlign(CENTER if center else LEFT)
    p.text(words, x, y)


def line(p: Pen, x1, y1, x2, y2, ink=QUIET, weight=0.75) -> None:
    p.noFill()
    p.stroke(ink)
    p.strokeWeight(weight)
    p.line(x1, y1, x2, y2)


def poly(p: Pen, points, color=None, weight=1, closed=False) -> None:
    p.fill(color) if color is not None else p.noFill()
    p.strokeWeight(weight)
    p.beginShape()
    for x, y in points:
        p.vertex(x, y)
    p.endShape(CLOSE) if closed else p.endShape()


def arrow(p: Pen, x1, y1, x2, y2, ink=INK, size=6, weight=1) -> None:
    from math import atan2

    a = atan2(y2 - y1, x2 - x1)
    line(p, x1, y1, x2, y2, ink, weight)
    for side in (-0.5, 0.5):
        p.line(x2, y2, x2 - cos(a + side) * size, y2 - sin(a + side) * size)


def hydrogen(p: Pen, x, y, radius=27, ink=INK) -> None:
    p.noFill()
    p.stroke(ink)
    p.strokeWeight(1.15)
    for i in (0, 1):
        cx = x + i * radius * 3.0
        p.circle(cx, y, radius * 2)
        p.circle(cx, y, 3.3)
        arrow(p, cx, y - 10, cx, y - 22, ink, 4)
        arrow(p, cx, y + (10 if i else 22), cx, y + (22 if i else 10), ink, 4)
        p.line(cx, y - radius - 8, cx, y - radius + 3)
    arrow(p, x + radius + 7, y, x + radius * 2 - 7, y, ink, 4)
    label(p, "1", x + radius * 1.5, y + 24, 12, ink, True, "Menlo")


def bits(p: Pen, value, x, y, spacing=5.0, ink=INK) -> None:
    p.stroke(ink)
    p.strokeWeight(0.85)
    for i, bit in enumerate(value):
        xx = x + i * spacing
        if bit == "1":
            p.line(xx, y - 4, xx, y + 4)
        else:
            p.line(xx - 1.3, y, xx + 1.3, y)


def cloth_details(p: Pen) -> None:
    rng = Random(1977)
    # Two crossing thread directions belong to the woven ground.
    for row in range(0, HEIGHT, 7):
        line(p, 0, row, WIDTH, row + 19, "#242B291D", 0.6)
    for col in range(0, WIDTH, 8):
        line(p, col, 0, col + 36, HEIGHT, "#04080732", 0.7)
    for _ in range(260):
        x, y = rng.uniform(0, WIDTH), rng.uniform(0, HEIGHT)
        line(p, x, y, x + rng.uniform(5, 24), y - 1, "#5E666514", 0.6)
    # Padded mounts establish a support beneath the cover.
    for x, y in ((290, 1210), (1030, 1210), (668, 315)):
        p.noStroke()
        p.fill("#060B0C")
        p.ellipse(x, y + 6, 130, 40)
        p.fill("#293333")
        p.ellipse(x, y, 112, 29)
        p.fill("#4D5752")
        p.ellipse(x, y - 3, 90, 17)


def setting(p: Pen) -> None:
    label(
        p,
        "ARCHIVE   /   INTERSTELLAR CORRESPONDENCE",
        90,
        80,
        13,
        "#A9AEA1",
        family="Menlo",
    )
    label(p, "A message without a shared language", 90, 130, 37, "#DED9C7")
    line(p, 90, 164, 1185, 164, "#8B948A55")
    label(p, "01", 1180, 128, 20, "#CECBB4", True, "Menlo")
    label(p, "VOYAGER", 354, 1381, 25, "#DED7BF", family="Helvetica Neue")
    label(p, "ENGRAVED COVER  ·  1977", 550, 1380, 13, "#AEA998", family="Menlo")
    label(p, "SOUND  →  TIME  →  GEOMETRY", 354, 1405, 12, "#888F83", family="Menlo")
    line(p, 91, 1420, 1962, 1420, "#747D7044")
    label(
        p,
        "STUDY OF AN OBJECT, ITS SYMBOLS AND THE ACT OF READING",
        91,
        1440,
        11,
        "#8B9485",
        family="Menlo",
    )


def folio(p: Pen) -> None:
    rng = Random(238)
    # Fibres cluster near the cut edge rather than covering the diagrams.
    for _ in range(1100):
        x, y = rng.uniform(10, PAGE_W - 10), rng.uniform(4, PAGE_H - 4)
        line(
            p,
            x,
            y,
            x + rng.uniform(0.4, 2.8),
            y + rng.uniform(-0.5, 0.6),
            "#69554110",
            0.35,
        )
    for inset, ink in ((2, "#9D8B6855"), (5, "#EEE5CD99")):
        p.noFill()
        p.stroke(ink)
        p.strokeWeight(0.6)
        p.rect(inset, inset, PAGE_W - inset * 2, PAGE_H - inset * 2)
    label(p, "FIELD NOTES", 50, 48, 12, QUIET, family="Menlo")
    label(p, "An unknown reader", 50, 102, 43)
    label(p, "Four instructions, one common clock.", 51, 139, 21, QUIET, italic=True)
    line(p, 50, 161, PAGE_W - 49, 161, "#867A61", 0.7)
    for y, number, heading in (
        (196, "I", "ESTABLISH A UNIT"),
        (416, "II", "FOLLOW THE GROOVE"),
        (639, "III", "LET TIME BECOME AN IMAGE"),
        (930, "IV", "FIND THE POINT OF ORIGIN"),
    ):
        label(p, number, 50, y, 17, RED)
        label(p, heading, 85, y, 14, INK, family="Menlo")
        line(p, 50, y + 13, PAGE_W - 49, y + 13, "#9F92736C")
    hydrogen(p, 83, 269, 31)
    label(p, "t₀ ≈ 0.70 × 10⁻⁹ s", 243, 262, 25)
    label(p, "Hyperfine transition of hydrogen", 245, 294, 15, QUIET)
    label(p, "The connecting mark says: one event, one unit.", 51, 337, 18)
    label(
        p, "Every binary duration on the cover uses this interval.", 51, 365, 17, QUIET
    )

    p.noFill()
    p.stroke(INK)
    p.strokeWeight(0.8)
    p.circle(111, 493, 89)
    p.circle(111, 493, 11)
    for rr in (29, 33, 36, 39):
        p.circle(111, 493, rr * 2)
    poly(p, [(155, 477), (167, 459), (173, 459), (174, 494), (167, 497), (155, 477)])
    label(p, "3.6 seconds / revolution", 244, 470, 23)
    bits(p, "100110010100011100010100000000001", 247, 502, spacing=6)
    label(p, "Read from the edge toward the centre.", 244, 529, 17, QUIET)
    label(p, "The side view gives the duration of a whole side.", 51, 572, 18)
    line(p, 61, 592, 588, 592, "#A5967466", 0.6)

    for x, word in ((64, "SIGNAL"), (357, "512 VERTICAL LINES")):
        label(p, word, x, 682, 11, QUIET, family="Menlo")
    signal = []
    for i in range(97):
        xx = 61 + i * 2.27
        local = (i % 31) / 31
        yy = 737 - 40 * max(0, sin(local * pi)) ** 1.5
        signal.append((xx, yy))
    p.stroke(BLUE)
    poly(p, signal, weight=1.25)
    arrow(p, 300, 720, 335, 720, QUIET, 7)
    for x1, y1, x2, y2 in (
        (356, 691, 614, 691),
        (356, 691, 356, 841),
        (614, 691, 614, 841),
        (356, 841, 614, 841),
    ):
        line(p, x1, y1, x2, y2, INK)
    p.stroke("#A29B85")
    p.strokeWeight(0.38)
    for i in range(128):
        p.line(359 + i * 1.986, 694, 359 + i * 1.986, 838)
    p.stroke(INK)
    p.strokeWeight(1.0)
    p.noFill()
    p.circle(485, 766, 110)
    label(p, "First image: a circle. Distortion reveals a wrong ratio.", 51, 877, 17)

    ox, oy = 105, 1030
    ends = [
        (109, 974),
        (123, 999),
        (167, 1002),
        (196, 1018),
        (171, 1047),
        (145, 1079),
        (118, 1100),
        (99, 1099),
        (75, 1080),
        (63, 1052),
        (43, 1037),
        (53, 1011),
        (66, 991),
        (86, 982),
    ]
    for x, y in ends:
        line(p, ox, oy, x, y, QUIET, 0.75)
        p.line(x - 2, y, x + 2, y)
    line(p, ox, oy, 216, oy, INK, 1)
    label(p, "14 pulsars", 260, 1007, 27)
    label(p, "Independent periods give a place and an epoch.", 261, 1039, 17, QUIET)
    label(p, "The long radius points toward the galactic centre.", 261, 1068, 16, QUIET)
    line(p, 50, 1119, PAGE_W - 49, 1119, "#8A7B5F")
    label(
        p,
        "NASA / JPL  ·  A READING OF THE ENGRAVED COVER",
        50,
        1148,
        10,
        QUIET,
        family="Menlo",
    )
    label(
        p,
        "Clock   /   Groove   /   Image   /   Location",
        50,
        1180,
        15,
        INK,
        italic=True,
    )


def smooth(x) -> float:
    x = min(1.0, max(0.0, x))
    return x * x * (3 - 2 * x)


def reading(p: Pen) -> None:
    t = (p.millis() / 1000) % CYCLE
    stage = min(3, int(t / 6))
    q = (t % 6) / 6
    alpha = smooth(q * 8) * smooth((1 - q) * 8)
    if alpha <= 0.002:
        return
    p.push()
    p.translate(PAGE_X, PAGE_Y)
    # The active rule marks the current instruction in the folio.
    active_y = (196, 416, 639, 930)[stage]
    line(p, 35, active_y - 16, 35, active_y + 105, RED, 2.2)
    if stage == 0:
        phase = (t * 0.6) % 1
        for x in (83, 176):
            p.noStroke()
            p.fill(RED)
            p.circle(x, 237 + 8 * sin(phase * pi * 2), 5)
        label(p, "1", 207, 271, 15, RED, family="Menlo")
    elif stage == 1:
        a = -pi / 2 + t / 3.6 * pi * 2
        radius = 40 - smooth(q) * 18
        p.noStroke()
        p.fill(RED)
        p.circle(111 + cos(a) * radius, 493 + sin(a) * radius, 6)
        label(p, "OUTSIDE  →  INSIDE", 245, 549, 11, RED, family="Menlo")
    elif stage == 2:
        column = int(smooth(q) * 512)
        # Logical columns reveal the calibration circle from left to right.
        p.stroke(BLUE)
        p.strokeWeight(0.38)
        for i in range(column):
            xx = 358 + i * 0.496
            dx = xx - 485
            if abs(dx) < 55:
                dy = (55 * 55 - dx * dx) ** 0.5
                p.line(xx, 766 - dy, xx, 766 + dy)
        line(p, 358 + column * 0.496, 694, 358 + column * 0.496, 838, RED, 1.2)
        label(p, f"{column:03d} / 512", 359, 861, 11, RED, family="Menlo")
    else:
        a = -pi / 2 + int(q * 14) * pi * 2 / 14
        line(p, 105, 1030, 105 + cos(a) * 55, 1030 + sin(a) * 55, RED, 1.4)
        p.noStroke()
        p.fill(RED)
        p.circle(105, 1030, 5)
    for i in range(4):
        p.noStroke()
        p.fill(RED if i == stage else "#AA9C7B")
        p.circle(585 + i * 14, 1175, 4 if i != stage else 6)
    p.pop()


@sketch(size=(WIDTH, HEIGHT), background="#0F1617", capture_at=14.8)
class VoyagerInstructionPlate:
    def setup(self, ctx: SketchContext) -> None:
        cover = load(LOCAL / "data" / "cover.svg", width=DISC_SIZE, height=DISC_SIZE)
        hub = ctx.assets.hub()
        optical = Filter.of(
            shader(
                hub,
                ctx.local("gold.sksl"),
                {"uSize": (float(DISC_SIZE), float(DISC_SIZE))},
                textures={"content": None},
            )
        )
        disc = (
            image(cover)
            .absolute()
            .left(DISC_X)
            .top(DISC_Y)
            .width(DISC_SIZE)
            .height(DISC_SIZE)
            .filter(optical)
            .cache(Cache.Texture)
            .key("voyager.incised.cover")
        )
        shadow = (
            image(cover)
            .absolute()
            .left(DISC_X)
            .top(DISC_Y)
            .width(DISC_SIZE)
            .height(DISC_SIZE)
            .filter(
                Filter.program(
                    "uniform shader content; half4 main(float2 p) { half4 c=content.eval(p); return half4(0,0,0,c.a); }"
                ).then(Filter.blur(20))
            )
            .translateY(21)
            .translateX(10)
            .cache(Cache.Texture)
        )
        page = (
            box()
            .absolute()
            .left(PAGE_X)
            .top(PAGE_Y)
            .width(PAGE_W)
            .height(PAGE_H)
            .fill(
                shader(hub, ctx.local("paper.sksl")).effects(
                    Filter.shadow("#000000A0", ShadowOptions(blur=20, offset=(7, 17)))
                )
            )
            .children(
                pen("voyager.folio", folio, cache=Cache.Picture).absolute().inset(0)
            )
            .cache(Cache.Texture)
        )
        ctx.render(
            box()
            .width(WIDTH)
            .height(HEIGHT)
            .children(
                box().absolute().inset(0).fill(shader(hub, ctx.local("cloth.sksl"))),
                pen("voyager.cloth", cloth_details, cache=Cache.Picture)
                .absolute()
                .inset(0),
                shadow,
                disc,
                page,
                pen("voyager.setting", setting, cache=Cache.Picture)
                .absolute()
                .inset(0),
                pen("voyager.reading", reading).absolute().inset(0),
            )
        )
