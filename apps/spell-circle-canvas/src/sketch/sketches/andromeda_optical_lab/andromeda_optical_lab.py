"""Study · Film — document overprints and photographed optical evidence.

TAGS: Studies/Film, Typography/Titles, Materials/Optics, Animation/Sequence, Runtime/Python

The composition studies classified title graphics and crystalline microscopy.
All records, instruments, observations and specimen geometry are invented.
"""

from math import sin
from pathlib import Path

from sigil.compose import Cache, Overflow, box, pen, text
from sigil.draw import CLOSE, Pen
from sigil.material import BloomOptions, Filter, Paint, shader
from sigil.motion import animatable
from sigil.sketch import SketchContext, sketch
from sigil.weave import TextAlignment

WIDTH, HEIGHT = 1880, 800
ROOT = Path(__file__).parent
WHITE, BLUE, RED, MAGENTA = "#DBDFDA", "#175D86", "#9B122A", "#8D3F76"


def type_(words, x, y, size=17, ink=WHITE, weight=400):
    return (
        text(words, size=size, color=ink)
        .fontFamily("Courier")
        .fontWeight(weight)
        .at(x, y)
    )


def rule(x, y, width, ink=WHITE, thickness=1):
    return box().rect(x, y, width, thickness).fill(ink)


def instrument(p: Pen):
    """A sectioned optical train with shared axis and repeated lens mounts."""
    p.noFill()
    p.stroke(BLUE)
    p.strokeWeight(1.25)
    p.line(70, 326, 1740, 326)
    p.line(85, 270, 230, 270)
    p.line(85, 382, 230, 382)
    p.rect(95, 285, 73, 82)
    for i in range(16):
        p.line(100 + i * 4, 286, 100 + i * 4, 367)
    for cx, radius, length in (
        (291, 70, 54),
        (469, 96, 99),
        (707, 64, 38),
        (1122, 103, 88),
        (1490, 126, 125),
    ):
        p.rect(cx - length / 2, 326 - radius - 25, length, 2 * radius + 50)
        p.rect(cx - length / 2 - 6, 326 - radius - 34, length + 12, 10)
        p.rect(cx - length / 2 - 6, 326 + radius + 24, length + 12, 10)
        p.bezier(
            cx,
            326 - radius,
            cx - radius * 0.36,
            326 - radius / 2,
            cx - radius * 0.36,
            326 + radius / 2,
            cx,
            326 + radius,
        )
        p.bezier(
            cx,
            326 - radius,
            cx + radius * 0.36,
            326 - radius / 2,
            cx + radius * 0.36,
            326 + radius / 2,
            cx,
            326 + radius,
        )
        p.line(cx, 530, cx, 568)
        p.line(cx - length / 2, 552, cx + length / 2, 552)
        for xx in (cx - length / 2, cx + length / 2):
            p.line(xx - 4, 556, xx + 4, 548)
        for xx in (cx - length / 2 + 8, cx + length / 2 - 8):
            for yy in (326 - radius - 17, 326 + radius + 17):
                p.circle(xx, yy, 5)
    for side in (-1, 1):
        points = [
            (169, 326 + side * 29),
            (291, 326 + side * 50),
            (469, 326 + side * 70),
            (707, 326),
            (1122, 326 + side * 82),
            (1490, 326 + side * 104),
            (1710, 326 + side * 69),
        ]
        p.beginShape()
        for xx, yy in points:
            p.vertex(xx, yy)
        p.endShape()
    p.strokeWeight(0.65)
    for y in (255, 275, 295, 315, 335, 355, 375, 395):
        p.line(1690, y, 1732, y + 16)
    for x in range(90, 1741, 24):
        p.line(x, 610, x, 619 if x % 3 else 630)
    p.line(90, 610, 1740, 610)
    p.line(882, 150, 882, 470)
    p.line(859, 150, 905, 150)
    p.line(859, 470, 905, 470)
    p.rect(862, 273, 40, 106)
    p.beginShape()
    for xy in ((823, 270), (945, 349), (823, 349)):
        p.vertex(*xy)
    p.endShape(CLOSE)
    for i in range(15):
        p.line(827, 279 + i * 4.3, 830 + i * 6.4, 344)


def chart(p: Pen):
    p.noFill()
    p.stroke("#40536B")
    p.strokeWeight(0.8)
    for x in range(0, 611, 61):
        p.line(x, 0, x, 174)
    for y in range(0, 175, 29):
        p.line(0, y, 610, y)
    for mode, color in ((0, "#C4C8C3"), (1, "#AE5992"), (2, "#AA3137")):
        p.stroke(color)
        p.strokeWeight(1.5)
        p.beginShape()
        for i in range(306):
            x = i * 2
            v = sin(i * 0.043 + mode * 0.6) * 0.17 + sin(i * 0.103) * 0.04
            peak = 0.65 * max(0, 1 - abs(i - (120 + mode * 16)) / (40 + mode * 12)) ** 2
            p.vertex(x, 130 - 105 * (v + peak) - mode * 18)
        p.endShape()


def reticle(p: Pen, w, h, magnification, full=False):
    p.noFill()
    p.stroke("#D1D8CBAA")
    p.strokeWeight(0.7)
    for side in (-1, 1):
        for x, y in ((w / 2, 25), (w / 2, h - 25)):
            p.line(x + side * 6, y, x + side * 22, y)
        p.line(20, h / 2 + side * 5, 20, h / 2 + side * 18)
        p.line(w - 20, h / 2 + side * 5, w - 20, h / 2 + side * 18)
    length = min(w, h) * magnification * 0.4 / 5.2
    p.stroke("#EDF1DE")
    p.strokeWeight(1.2)
    p.line(25, h - 28, 25 + length, h - 28)
    p.line(25, h - 33, 25, h - 23)
    p.line(25 + length, h - 33, 25 + length, h - 23)
    if full:
        for i in range(41):
            x = 25 + i * (w - 50) / 40
            p.strokeWeight(0.65)
            p.line(x, 0, x, 10 if i % 5 else 18)
        for i in range(25):
            y = 24 + i * (h - 48) / 24
            p.line(0, y, 8 if i % 4 else 15, y)


def film_filter(clock):
    # Stock grain supplies a material, while this transfer modulates the
    # already composed exposure and moves its emulsion at film-frame cadence.
    body = (ROOT / "film.sksl").read_text()
    return Filter.program(body, {"seconds": 0.0}).bind("seconds", clock)


@sketch()
class AndromedaOpticalLab:
    def setup(self, ctx: SketchContext):
        ctx.canvas(WIDTH, HEIGHT)
        ctx.background("#030406")
        ctx.captureAt(24.0)
        self.elapsed = 0.0
        self.seconds = animatable(0.0)
        self.exposure_seconds = animatable(0.0)
        self.focus = animatable(0.45)
        self.overview_mag = animatable(1.0)
        self.hero_mag = animatable(1.0)
        self.document_opacity = animatable(1.0)
        self.field_opacity = animatable(0.0)
        self.compare_opacity = animatable(0.0)
        self.report_opacity = animatable(0.0)
        self.title_opacity = animatable(0.0)
        self.stamp_opacity = animatable(0.0)
        self.index_opacity = animatable(0.0)
        self.title_shift = animatable(0.0)
        self.stage_opacity = [animatable(0.0) for _ in range(4)]
        self.shutter = animatable(0.0)

        document = box().inset(0).overflow(Overflow.Clip).opacity(self.document_opacity)
        overprints = [
            pen("optical.train", instrument, Cache.Texture).rect(0, 42, WIDTH, 630),
            type_("OBSERVATION / 023", 1260, 24, 18, BLUE),
            type_("SECTION THROUGH THE IMAGE PLANE", 105, 701, 16, BLUE),
            type_(
                "FIG. 04     LONGITUDINAL OPTICAL SECTION     1 : 1", 100, 746, 14, BLUE
            ),
        ]
        for i in range(14):
            overprints.append(
                type_("THE OPTICAL RECORD", 431, -67 + i * 57, 44, RED, 700)
            )
        overprints.extend(
            [
                box().rect(1107, 165, 643, 486).fill("#790D22"),
                type_("OBSERVATION", 1140, 185, 42, "#210709", 700),
                type_(
                    "IMAGING DIVISION\nRECORD 023 / COPY 04\n\nSUBJECT: OPTICAL STRUCTURE\n\nALL FIELDS REFER TO ONE OBJECT.\nA CHANGE OF VIEW MUST NOT\nBE MISTAKEN FOR A CHANGE\nOF SPECIMEN.\n\nATTACH CONTACT EXPOSURES\nTO THE OBSERVATION RECORD.",
                    1140,
                    266,
                    19,
                    "#210709",
                ),
                type_("UNRESOLVED", -177, 334, 226, "#24492B", 900)
                .fontFamily("Helvetica Neue")
                .translateX(self.title_shift)
                .opacity(self.stamp_opacity),
                type_("UNRESOLVED", -82, 282, 231, "#89132E", 900)
                .fontFamily("Helvetica Neue")
                .translateX(self.title_shift)
                .opacity(self.stamp_opacity),
                type_("RECORD OF EFFECTIVENESS", 700, 585, 45, MAGENTA, 700).opacity(
                    self.index_opacity
                ),
                rule(702, 634, 1150, MAGENTA, 3).opacity(self.index_opacity),
            ]
        )
        document.children(overprints)
        title = (
            box()
            .inset(0)
            .opacity(self.title_opacity)
            .children(
                [
                    type_("THE OPTICAL RECORD", 0, 328, 80, "#EFECE2", 700)
                    .width(WIDTH)
                    .textAlign(TextAlignment.Center),
                    type_("an observation in four parts", 0, 433, 23, "#D6D3C8")
                    .width(WIDTH)
                    .textAlign(TextAlignment.Center),
                ]
            )
        )

        self.fields = []

        def optical(key, x, y, width, height, mode, magnification, fixed_focus=None):
            uniforms = {
                "seconds": 0.0,
                "magnification": 1.0,
                "focus": 0.45,
                "modality": float(mode),
                "center": (0.0, 0.0),
            }
            paint = shader(ctx.assets.hub(), ctx.local("micrograph.sksl"), uniforms)
            depth = shader(
                ctx.assets.hub(),
                ctx.local("micrograph.sksl"),
                {**uniforms, "modality": 3.0},
            )
            for value in (paint, depth):
                value.bind("seconds", self.exposure_seconds)
                value.bind("magnification", magnification)
                if fixed_focus is None:
                    value.bind("focus", self.focus)
                else:
                    value.set("focus", fixed_focus)
            lens = Filter.blur(Paint(depth), 6.0).then(
                Filter.bloom(
                    BloomOptions(
                        sigma=1.15,
                        strength=0.13,
                        spread=3.2,
                        tail=0.12,
                        threshold=0.45,
                        knee=0.15,
                    )
                )
            )

            def rulers(p):
                reticle(p, width, height, magnification.value, key == "hero")

            viewport = (
                box()
                .rect(x, y, width, height)
                .overflow(Overflow.Clip)
                .children(
                    [
                        box().inset(0).fill(paint).filter(lens),
                        pen(key + ".rulers", rulers, Cache.None_).inset(0),
                        type_("0.4", 25, height - 54, 12, "#D2DCCF"),
                    ]
                )
            )
            self.fields.append((paint, depth, lens))
            return viewport

        overview = (
            box()
            .inset(0)
            .opacity(self.field_opacity)
            .children(
                [
                    optical("overview", 38, 106, 1804, 604, 0, self.overview_mag),
                    type_("EXPOSURE 01 / TRANSMITTED FIELD", 39, 32, 24),
                    type_("OPTICAL RECORD 023", 1527, 38, 18),
                    type_("FIELD POSITION  0.000 / 0.000", 39, 742, 16),
                    type_("ONE OBJECT / CONTINUOUS OBSERVATION", 1414, 742, 16),
                ]
            )
        )
        comparison = (
            box()
            .inset(0)
            .opacity(self.compare_opacity)
            .children(
                [
                    optical("hero", 38, 106, 1174, 604, 2, self.hero_mag),
                    optical("context", 1230, 106, 612, 286, 0, self.overview_mag, 0.45),
                    optical("relief", 1230, 424, 612, 286, 1, self.hero_mag),
                    type_("EXPOSURE 02 / MULTIPLE ILLUMINATION", 39, 32, 24),
                    type_("OPTICAL RECORD 023", 1527, 38, 18),
                    type_("TRANSMISSION / CONTEXT", 1230, 82, 14, "#A4B7A4"),
                    type_("RELIEF / COMMON IMAGE PLANE", 1230, 400, 14, "#B1ADAB"),
                    type_("FIG. 02   INTERFERENCE AT THE BOUNDARY", 39, 742, 17),
                    type_("SIMULTANEOUS FIELDS", 1230, 742, 17),
                    type_("023 / B", 1723, 742, 17),
                ]
            )
        )
        for i, caption in enumerate(
            (
                "ACQUIRE / HOLD THE FIELD",
                "FOCAL PLANE / TRAVERSE",
                "CHANGE THE LIGHT / RETAIN THE OBJECT",
                "EXPOSURE HELD / COMPARE THE EDGES",
            )
        ):
            caption_layer = (
                box()
                .rect(38, 716, 1174, 22)
                .opacity(self.stage_opacity[i])
                .children([type_(caption, 0, 2, 14)])
            )
            comparison.children([caption_layer])
            overview.children([caption_layer.copy()])
        report = (
            box()
            .inset(0)
            .fill("#040609")
            .opacity(self.report_opacity)
            .children(
                [
                    type_("OBSERVATION 023", 66, 46, 24, WHITE, 700),
                    type_("INDEX OF AGREEMENT", 65, 92, 57, MAGENTA, 700),
                    rule(66, 165, 1727, MAGENTA, 2),
                    type_(
                        "01    The field is continuous.\n\n02    Magnification preserves position.\n\n03    The boundary survives a change of light.\n\n04    Apparent depth follows the focal plane.",
                        68,
                        207,
                        25,
                    ),
                    type_("DETERMINATION", 68, 487, 18, "#939EAD"),
                    type_(
                        "STRUCTURE CHANGES.\nOR THE RECORD DOES.",
                        65,
                        525,
                        46,
                        WHITE,
                        700,
                    ),
                    optical("report", 1154, 202, 642, 286, 2, self.hero_mag),
                    pen("optical.response", chart, Cache.Texture).rect(
                        1170, 536, 610, 174
                    ),
                    type_(
                        "EXPOSURE RESPONSE / RELATIVE UNITS", 1170, 509, 14, "#A7ACB3"
                    ),
                    type_("RELIEF", 1789, 649, 14, "#DADEDA"),
                    type_("VIOLET", 1789, 625, 14, "#D994C0"),
                    type_("FIELD", 1789, 606, 14, "#D7898D"),
                    type_("IMMOBILITY IS NOT AGREEMENT.", 68, 723, 17, MAGENTA),
                    type_("END OF RECORD     /     023", 1490, 747, 15),
                ]
            )
        )
        ctx.render(
            box()
            .inset(0)
            .overflow(Overflow.Clip)
            .children(
                [
                    document,
                    title,
                    overview,
                    comparison,
                    report,
                    box().inset(0).fill("#000000").opacity(self.shutter),
                ]
            )
            .filter(film_filter(self.seconds))
        )

    def update(self, elapsed, ctx):
        t = elapsed % 38
        self.elapsed = t

        def smooth(a, b):
            value = max(0.0, min(1.0, (t - a) / (b - a)))
            return value * value * (3 - 2 * value)

        self.seconds.set(t)
        self.exposure_seconds.set(min(t, 26.0))
        self.document_opacity.set(1 - smooth(6.5, 7.3))
        self.title_opacity.set(smooth(1.6, 2.8) * (1 - smooth(5.3, 6.1)))
        self.stamp_opacity.set(smooth(4.8, 5.4))
        self.index_opacity.set(smooth(5.4, 6.0))
        self.title_shift.set(-44 + 88 * smooth(0, 7))
        self.field_opacity.set(smooth(6.7, 7.6) * (1 - smooth(17.5, 18.1)))
        self.compare_opacity.set(smooth(17.6, 18.2) * (1 - smooth(30.5, 31.1)))
        self.report_opacity.set(smooth(30.7, 31.5) * (1 - smooth(36.6, 37.5)))
        self.focus.set(0.24 + 0.55 * smooth(9, 15) - 0.36 * smooth(19, 26))
        self.overview_mag.set(1.0 + 0.4 * smooth(14, 16))
        self.hero_mag.set(1.65 + 0.35 * smooth(20, 23))
        self.shutter.set(max(0.0, 1 - t / 0.45) + smooth(37.5, 37.95))
        for i, (a, b) in enumerate(((7, 10), (10, 18), (18, 26), (26, 31))):
            self.stage_opacity[i].set(smooth(a, a + 0.3) * (1 - smooth(b - 0.3, b)))
