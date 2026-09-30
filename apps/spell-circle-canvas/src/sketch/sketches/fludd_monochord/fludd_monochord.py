"""Study · Fludd — separated engraving ink and a living harmonic folio.

TAGS: Studies/Manuscripts, Studies/Science, Geometry/Diagrams, Typography/Curved, Drawing/Engraving, Materials/Paper, Motion/Waves

The historical engraving retains its own linework. The native facing folio
connects interval geometry to the nodes of a plucked string.
"""

from math import exp, pi, sin
from pathlib import Path

from sigil.compose import Cache, FontStyle, TextPath, box, pen, text
from sigil.draw import CENTER, LEFT, Pen
from sigil.geometry.shapes import arc
from sigil.material import Filter, ShadowOptions, shader
from sigil.media import load
from sigil.sketch import SketchContext, sketch
from sigil.weave import typeface

WIDTH, HEIGHT = 2310, 2190
PX, PY, PW, PH = 130, 83, 1300, 1980
OX, OY, SCALE = 61, 115, 1180 / 803
NOTE_X, NOTE_Y, NOTE_W, NOTE_H = 1510, 237, 648, 1530
INK, DIM, RED = "#292920", "#646052", "#984D32"
LOCAL = Path(__file__).parent


def label(
    p: Pen,
    words,
    x,
    y,
    size=19,
    ink=INK,
    italic=False,
    center=False,
    family="Baskerville",
):
    p.noStroke()
    p.fill(ink)
    p.textFont(typeface(family, italic=italic))
    p.textSize(size)
    p.textAlign(CENTER if center else LEFT)
    p.text(words, x, y)


def line(p: Pen, x1, y1, x2, y2, weight=0.7, ink=DIM):
    p.noFill()
    p.stroke(ink)
    p.strokeWeight(weight)
    p.line(x1, y1, x2, y2)


def string_motion(p: Pen):
    t = p.millis() / 1000
    mode = 1 + int(t / 12) % 3
    amplitude = 2.4 * exp(-(t % 12) * 0.40)
    p.noFill()
    p.stroke(INK)
    p.strokeWeight(0.72)
    p.beginShape()
    for i in range(240):
        q = i / 239
        x = OX + SCALE * (423 - 31 * q)
        y = OY + SCALE * (230 + 884 * q)
        wave = amplitude * (
            sin(mode * pi * q) * sin(t * 31 * mode)
            + 0.19 * sin(3 * mode * pi * q) * sin(t * 93 * mode)
        )
        p.vertex(x + wave, y)
    p.endShape()
    for i in range(1, mode):
        q = i / mode
        x, y = OX + SCALE * (423 - 31 * q), OY + SCALE * (230 + 884 * q)
        p.stroke("#984D3280")
        p.strokeWeight(0.85)
        p.circle(x, y, 9)
    label(
        p,
        "A living string, read beside the engraved world",
        91,
        1916,
        18,
        DIM,
        italic=True,
    )
    label(p, f"PARTIAL {mode}   /   {mode} : 1", 91, 1944, 13, DIM, family="Menlo")


def folio(p: Pen):
    label(p, "MUSICA MUNDANA  /  READING LEAF", 47, 53, 12, DIM, family="Menlo")
    label(p, "The world as a string", 47, 117, 38)
    label(p, "A tuning hand outside the world.", 49, 155, 21, DIM, italic=True)
    line(p, 47, 183, NOTE_W - 47, 183)
    for n, heading, y in (
        ("I", "ONE STRING, SEVERAL WORLDS", 222),
        ("II", "PROPORTION BECOMES INTERVAL", 626),
        ("III", "A PLUCK, THEN QUIET", 1000),
    ):
        label(p, n, 48, y, 17, RED)
        label(p, heading, 91, y, 14, INK, family="Menlo")
        line(p, 47, y + 21, NOTE_W - 47, y + 21, 0.6, "#9C907B")
    for row, (words, meaning) in enumerate(
        (
            ("Empyrean", "The tuning figure lies beyond the cosmos."),
            ("Æthereal", "Planetary signs occupy the middle register."),
            ("Elemental", "Fire, air, water and earth descend below."),
        )
    ):
        y = 279 + row * 86
        label(p, words, 49, y, 25, italic=True)
        label(p, meaning, 50, y + 30, 18, DIM)
    label(p, "The stellar belt divides the luminous and material", 49, 539, 18, DIM)
    label(p, "registers. Curved inscriptions bind distance to ratio.", 49, 565, 18, DIM)
    for x, ratio, name, fraction in (
        (138, "2 : 1", "Diapason", 0.5),
        (328, "3 : 2", "Diapente", 2 / 3),
        (517, "4 : 3", "Diatessaron", 0.75),
    ):
        y = 743
        p.noFill()
        p.stroke("#736D5A")
        p.strokeWeight(0.8)
        p.circle(x, y, 140)
        line(p, x - 65, y + 10, x + 65, y + 10, 1.1)
        line(p, x - 65, y + 10, x - 65 + 130 * fraction, y + 10, 2.4, RED)
        p.noStroke()
        p.fill(RED)
        p.circle(x - 65 + 130 * fraction, y + 10, 5)
        label(p, ratio, x, y + 51, 21, INK, center=True)
        label(p, name, x, y + 113, 24, INK, italic=True, center=True)
    label(p, "The geometry above uses sounding-length ratios.", 49, 930, 18, DIM)
    label(p, "Frequency runs inversely to the vibrating length.", 49, 958, 18, DIM)
    label(p, "NATIVE HARMONIC STUDY", 49, 1091, 12, DIM, family="Menlo")
    for mode, y in ((1, 1148), (2, 1227), (3, 1306)):
        line(p, 72, y, NOTE_W - 65, y, 0.6, "#8D877480")
        label(p, str(mode), 49, y + 6, 15, DIM, family="Menlo")
    line(p, 47, 1421, NOTE_W - 47, 1421, 0.6)
    label(
        p,
        "Engraving  /  Curved type  /  Harmonic motion",
        49,
        1457,
        16,
        DIM,
        italic=True,
    )
    label(
        p, "ROBERT FLUDD · UTRIUSQUE COSMI HISTORIA", 49, 1490, 10, DIM, family="Menlo"
    )


def harmonic_motion(p: Pen):
    t = p.millis() / 1000
    current = 1 + int(t / 12) % 3
    phase = t % 12
    for mode, y in ((1, 1148), (2, 1227), (3, 1306)):
        active = mode == current
        a = 26 * exp(-phase * 0.4) if active else 9
        p.noFill()
        p.stroke(RED if active else "#64786F85")
        p.strokeWeight(1.7 if active else 1.0)
        p.beginShape()
        for i in range(160):
            q = i / 159
            oscillation = sin(t * 31 * mode) if active else 0.65
            p.vertex(72 + (NOTE_W - 137) * q, y - a * sin(mode * pi * q) * oscillation)
        p.endShape()
        for n in range(mode + 1):
            x = 72 + (NOTE_W - 137) * n / mode
            p.noStroke()
            p.fill(INK if active else "#9B9789")
            p.circle(x, y, 4.5)
    label(
        p,
        f"Partial {current} · its nodes remain still as the string moves",
        49,
        1391,
        18,
        DIM,
        italic=True,
    )


def curved_labels():
    return [
        text(words, size=18, color=DIM)
        .fontFamily("Baskerville")
        .fontStyle(FontStyle.Italic)
        .absolute()
        .left(x - 81)
        .top(660)
        .width(162)
        .height(162)
        .textOnPath(
            TextPath(
                path=arc(199, 142),
                at=0.5,
                align=TextPath.Align.Center,
                exactTangent=True,
                offset=2,
            )
        )
        for x, words in (
            (138, "Proportio dupla"),
            (328, "Sesquialtera"),
            (517, "Sesquitertia"),
        )
    ]


@sketch(size=(WIDTH, HEIGHT), background="#282B28", capture_at=0.36)
class FluddMonochord:
    def setup(self, ctx: SketchContext):
        hub = ctx.assets.hub()
        paper = shader(hub, ctx.local("paper.sksl")).effects(
            Filter.shadow("#030605C0", ShadowOptions(blur=24, offset=(11, 17)))
        )
        plate = load(LOCAL / "data" / "monochord.jpeg")
        engraving = (
            box()
            .absolute()
            .left(OX)
            .top(OY)
            .width(1180)
            .height(1200 * SCALE)
            .fill(
                shader(
                    hub,
                    ctx.local("figure.sksl"),
                    {"uCrop": (0.0, 0.0, 803.0, 1200.0)},
                    textures={"plate": plate},
                )
            )
        )
        press = Filter.of(
            shader(hub, ctx.local("print.sksl"), textures={"content": None})
        )
        leaf = (
            box()
            .absolute()
            .left(PX)
            .top(PY)
            .width(PW)
            .height(PH)
            .fill(paper)
            .children(
                box()
                .absolute()
                .inset(0)
                .children(engraving)
                .filter(press)
                .cache(Cache.Texture),
                pen("fludd.string", string_motion).absolute().inset(0),
            )
        )
        notes = (
            box()
            .absolute()
            .left(NOTE_X)
            .top(NOTE_Y)
            .width(NOTE_W)
            .height(NOTE_H)
            .fill(paper)
            .children(
                pen("fludd.folio", folio, cache=Cache.Picture).absolute().inset(0),
                *curved_labels(),
                pen("fludd.harmonics", harmonic_motion).absolute().inset(0),
            )
        )
        ctx.render(
            box()
            .width(WIDTH)
            .height(HEIGHT)
            .children(
                box().absolute().inset(0).fill(shader(hub, ctx.local("table.sksl"))),
                box()
                .absolute()
                .left(PX + 8)
                .top(PY + 9)
                .width(PW + 3)
                .height(PH + 3)
                .fill("#C9C5B0"),
                leaf,
                notes,
                text("ROBERT FLVDD  /  THE WORLD AS A STRING", size=12, color="#B7B7A5")
                .fontFamily("Menlo")
                .absolute()
                .left(132)
                .top(2124),
            )
        )
