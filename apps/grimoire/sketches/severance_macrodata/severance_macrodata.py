"""Study · Television — a Macrodata Refinement workstation from Severance.

TAGS: Studies/Television, Interfaces/Television, Typography/Monospace, Animation/Signals, Materials/CRT, Materials/Plastic

The screen hierarchy follows an inspected production-screen photograph.
The numeric file, collection event and temper balances are authored data.
"""

from pathlib import Path
from random import Random

from sigil.compose import Cache, MotionPath, Overflow, box, heldPath, pen, stroke, text
from sigil.draw import CLOSE, SHAPE, Pen
from sigil.material import (
    BevelOptions,
    BlendMode,
    BloomOptions,
    Filter,
    Lighting,
    Material,
    Paint,
    noise,
    studio,
)
from sigil.motion import animatable, bind, envelope
from sigil.sketch import SketchContext, sketch
from sigil.skia import PathBuilder

WIDTH, HEIGHT = 1800, 1280
SX, SY, SW, SH = 240, 155, 1104, 710
INK, BRIGHT, DIM, GROUND = "#66E6FF", "#C3F7FF", "#2B829F", "#042C49"
ROOT = Path(__file__).parent
SELECTION = {(8, 4), (9, 4), (10, 4), (8, 5), (9, 5), (10, 5), (9, 6)}


def label(value, x, y, size=18, ink=INK, family="Menlo", weight=600):
    return (
        text(value, size=size, color=ink).fontFamily(family).fontWeight(weight).at(x, y)
    )


def outline(x, y, w, h, weight=2, ink=INK):
    return box().rect(x, y, w, h).stroke(stroke(weight, ink))


def plastic(stops, size, highlight, shadow, roughness=0.75):
    return (
        Material(Paint.linearGradient((0, 0), (0, 1), stops))
        .layer(
            noise(14.0, octaves=2, seed=2107, grain=True), BlendMode.SoftLight, 0.045
        )
        .surface(roughness=roughness, clearcoat=0.12)
        .effects(
            Filter.bevel(
                BevelOptions(
                    size=size,
                    depth=0.72,
                    angleDegrees=128,
                    highlight=highlight,
                    shadow=shadow,
                )
            )
        )
    )


def support(p: Pen):
    p.noStroke()
    p.fill(
        Paint.linearGradient(
            (0, 0), (0, 1), [(0, "#D2DCDA"), (0.7, "#EEF0E4"), (1, "#CDD5CA")]
        ),
        SHAPE,
    )
    p.rect(0, 0, WIDTH, HEIGHT)
    p.fill("#DDE3DC")
    p.rect(0, 240, WIDTH, 220)
    p.fill(
        Paint.linearGradient((0, 0), (0, 1), [(0, "#F4F2E6"), (1, "#CDD0C4")]), SHAPE
    )
    p.rect(0, 884, WIDTH, 396)
    p.fill("#CCD3C8")
    p.rect(0, 885, WIDTH, 4)
    p.fill("#BFC6BD")
    p.rect(0, 1259, WIDTH, 21)
    p.stroke("#AEB9AD")
    p.strokeWeight(1)
    p.line(0, 1260, WIDTH, 1260)
    p.noStroke()
    p.fill(
        Paint.radialGradient(
            (0.5, 0.5),
            0.72,
            [
                (0, (0.025, 0.06, 0.06, 0.36)),
                (0.62, (0.025, 0.06, 0.06, 0.17)),
                (1, (0, 0, 0, 0)),
            ],
        ),
        SHAPE,
    )
    p.ellipse(900, 998, 1480, 204)
    p.fill("#243539")
    p.rect(780, 871, 228, 116, 20)
    p.fill(
        Paint.linearGradient(
            (0, 0), (0, 1), [(0, "#1B2D38"), (0.7, "#0C1F28"), (1, "#253E47")]
        ),
        SHAPE,
    )
    p.rect(749, 929, 296, 65, 11)
    p.fill("#52686A")
    p.rect(754, 931, 286, 3, 1)


def cabinet_marks(p: Pen):
    p.noFill()
    p.stroke("#34526A")
    p.strokeWeight(1.1)
    p.rect(164, 84, 1475, 818, 29)
    p.stroke("#05121D")
    p.line(182, 904, 1621, 904)
    for x, y in ((176, 110), (1626, 110), (176, 880), (1626, 880)):
        p.noStroke()
        p.fill("#06111A")
        p.circle(x, y, 10)
        p.fill("#143449")
        p.circle(x - 0.6, y - 0.6, 6)
        p.stroke("#527088")
        p.strokeWeight(1)
        p.line(x - 2, y, x + 2, y)
    # The reference's front fastener and lower-right control stay separate
    # from the CRT molding; no invented service widgets occupy the bezel.
    for x, y, d in ((920, 116, 32), (1567, 812, 38)):
        p.noStroke()
        p.fill("#010A12")
        p.circle(x, y, d + 3)
        p.fill(
            Paint.radialGradient(
                (0.35, 0.27), 0.85, [(0, "#314659"), (0.35, "#132432"), (1, "#010912")]
            ),
            SHAPE,
        )
        p.circle(x, y, d)
        p.noFill()
        p.stroke("#233F55")
        p.strokeWeight(1)
        p.circle(x, y, d - 6)
    p.stroke("#081D29")
    p.strokeWeight(13)
    p.line(1697, 438, 1715, 436)
    p.line(1715, 436, 1718, 528)
    p.line(1718, 528, 1697, 530)
    p.stroke("#658289")
    p.strokeWeight(2)
    p.line(1717, 450, 1719, 518)
    p.strokeWeight(0.55)
    p.stroke((0.38, 0.51, 0.61, 0.25))
    for i in range(90):
        x = 186 + (i * 103.137) % 1427
        y = 91 + i % 7 if i % 2 else 890 + i % 8
        p.line(x, y, x + 2 + i % 5, y - 0.5)
    p.noStroke()
    p.fill("#244154")
    for row in range(16):
        p.rect(1488, 178 + row * 20, 61, 2, 1)
    p.fill("#152C3C")
    p.rect(1431, 859, 183, 19, 2)
    p.stroke("#42677A")
    p.strokeWeight(0.6)
    p.line(1439, 862, 1603, 862)


def keyboard_art(p: Pen):
    p.noStroke()
    p.fill(
        Paint.linearGradient(
            (0, 0),
            (0, 1),
            [(0, "#46616C"), (0.12, "#D3DCCF"), (0.86, "#BCCFC8"), (1, "#748D90")],
        ),
        SHAPE,
    )
    p.rect(292, 1008, 1208, 233, 15)
    p.fill("#203D50")
    p.rect(309, 1023, 1174, 190, 9)
    p.fill(
        Paint.linearGradient(
            (0, 0), (0, 1), [(0, "#3F657A"), (0.3, "#254E65"), (1, "#153345")]
        ),
        SHAPE,
    )
    p.rect(315, 1025, 1162, 181, 8)
    p.fill("#08202D")
    p.rect(342, 1039, 818, 153, 6)
    layouts = [(16, 0), (15, 10), (14, 24), (13, 40)]
    for row, (count, offset) in enumerate(layouts):
        for col in range(count):
            x, y = 351 + offset + col * 49, 1046 + row * 34
            p.fill("#041924")
            p.rect(x, y + 3, 43, 30, 4)
            p.fill("#17445D")
            p.rect(x, y, 43, 28, 4)
            p.fill(
                Paint.linearGradient(
                    (0, 0),
                    (0, 1),
                    [
                        (0, "#81B9CE"),
                        (0.12, "#5898B5"),
                        (0.82, "#387FA0"),
                        (1, "#28617F"),
                    ],
                ),
                SHAPE,
            )
            p.rect(x + 3, y + 1, 37, 23, 4)
            p.noFill()
            p.stroke((0.67, 0.87, 0.93, 0.32))
            p.strokeWeight(0.5)
            p.rect(x + 5, y + 2, 33, 20, 3)
            p.noStroke()
    p.fill("#082335")
    p.rect(539, 1185, 281, 14, 3)
    p.fill(
        Paint.linearGradient((0, 0), (0, 1), [(0, "#73B4CE"), (1, "#276487")]), SHAPE
    )
    p.rect(542, 1183, 275, 12, 3)
    p.fill("#173748")
    p.circle(1315, 1118, 143)
    p.fill("#82A9B9")
    p.circle(1315, 1115, 128)
    p.fill("#06121E")
    p.circle(1315, 1115, 115)
    p.fill(
        Paint.radialGradient(
            (0.30, 0.24),
            0.91,
            [(0, "#577380"), (0.18, "#213E4E"), (0.52, "#081624"), (1, "#010A12")],
        ),
        SHAPE,
    )
    p.circle(1315, 1112, 103)
    p.fill(
        Paint.radialGradient(
            (0.5, 0.5), 0.74, [(0, (0.81, 0.94, 1, 0.35)), (1, (0.8, 0.95, 1, 0))]
        ),
        SHAPE,
    )
    p.ellipse(1292, 1086, 34, 13)
    for x, y in ((1250, 1201), (1380, 1201)):
        p.fill("#24475F")
        p.rect(x, y, 44, 13, 3)
        p.fill("#6697AF")
        p.rect(x + 2, y - 2, 40, 11, 2)
    p.noFill()
    p.stroke("#7798A2")
    p.strokeWeight(0.7)
    p.rect(296, 1011, 1200, 221, 13)
    p.stroke("#2F525A")
    p.line(309, 1222, 1479, 1222)


def keyboard_type():
    result = box().inset(0)
    rows = ("1234567890-+[]{}", "QWERTYUIOP[];:/", "ASDFGHJKL;:'?", "ZXCVBNM,./><!")
    for row, (value, offset) in enumerate(zip(rows, (0, 10, 24, 40))):
        for col, c in enumerate(value):
            result.children(
                [
                    label(
                        c,
                        365 + offset + col * 49,
                        1048 + row * 34,
                        12,
                        "#C5EAF4",
                        "Helvetica Neue",
                        600,
                    )
                ]
            )
    result.children(
        [
            label("LUMON", 319, 1215, 9, "#536F75", "Helvetica Neue", 700),
            label("ENTER", 1259, 1199, 7, "#D9EFF2", "Helvetica Neue"),
            label("RETURN", 1384, 1199, 6, "#D9EFF2", "Helvetica Neue"),
        ]
    )
    return result


def screen_rules(p: Pen):
    p.noFill()
    p.stroke(INK)
    p.strokeWeight(2)
    p.rect(35, 27, 877, 40)
    p.line(23, 90, 1089, 90)
    p.line(23, 94, 1089, 94)
    p.line(23, 589, 1089, 589)
    p.line(23, 593, 1089, 593)
    for i in range(45):
        p.line(30 + i * 24, 583, 30 + i * 24, 589)
    p.strokeWeight(1)
    for i in range(40):
        p.line(258 + i * 11.4, 31, 258 + i * 11.4, 64)
    p.noStroke()
    p.fill(GROUND)
    p.rect(718, 29, 194, 36)


def globe(p: Pen):
    p.noFill()
    p.stroke(INK)
    p.strokeWeight(1.5)
    p.ellipse(1022, 45, 119, 70)
    p.ellipse(1022, 45, 78, 70)
    p.ellipse(1022, 45, 35, 70)
    p.ellipse(1022, 45, 119, 31)
    p.line(963, 45, 1081, 45)
    p.line(975, 25, 1068, 25)
    p.line(975, 65, 1068, 65)


def glass(p: Pen):
    p.noStroke()
    p.fill(
        Paint.radialGradient(
            (0.47, 0.48),
            0.83,
            [(0, (0, 0, 0, 0)), (0.61, (0, 0, 0, 0)), (1, (0.005, 0.015, 0.04, 0.28))],
        ),
        SHAPE,
    )
    p.rect(0, 0, SW, SH, 54)
    p.fill(
        Paint.linearGradient(
            (0, 0),
            (1, 1),
            [
                (0, (0.64, 0.87, 1, 0.07)),
                (0.19, (0.29, 0.55, 0.82, 0.01)),
                (0.8, (0.07, 0.20, 0.36, 0.005)),
                (1, (0.58, 0.80, 0.88, 0.07)),
            ],
        ),
        SHAPE,
    )
    p.rect(7, 9, SW - 14, SH - 18, 52)
    p.fill(
        Paint.linearGradient(
            (0, 0), (1, 0.15), [(0, (0.68, 0.85, 0.88, 0.09)), (1, (0.5, 0.79, 0.9, 0))]
        ),
        SHAPE,
    )
    p.beginShape()
    p.vertex(16, 16)
    p.vertex(620, 12)
    p.vertex(522, 19)
    p.vertex(19, 31)
    p.endShape(CLOSE)
    p.noFill()
    p.stroke((0.35, 0.59, 0.78, 0.26))
    p.strokeWeight(1.2)
    p.rect(7, 8, SW - 14, SH - 16, 48)
    p.stroke((0.41, 0.70, 0.81, 0.1))
    p.strokeWeight(0.45)
    for i in range(48):
        x, y = 18 + (i * 183.53) % (SW - 39), 18 + (i * 149.33) % (SH - 39)
        p.line(x, y, x + 1 + i % 4, y + 0.3)


def cursor(p: Pen):
    p.stroke(BRIGHT)
    p.strokeWeight(1.4)
    p.fill(GROUND)
    p.triangle(0, 0, 1, 23, 6, 15)
    p.line(6, 15, 12, 23)


@sketch(size=(WIDTH, HEIGHT), background="#D5DED5", capture_at=4.6)
class MacrodataRefinement:
    def setup(self, ctx: SketchContext):
        self.seconds = animatable(0.0)
        self.phase = animatable(0.0)
        self.selected = animatable(0.0)
        self.grid_before = animatable(1.0)
        self.grid_after = animatable(0.0)
        self.packet_visible = animatable(0.0)
        self.packet_progress = animatable(0.0)
        self.drawer = animatable(0.0)
        self.committed = animatable(0.0)
        self.classifying = animatable(0.0)
        self.screen_power = animatable(1.0)
        self.pointer_visible = animatable(1.0)
        body = plastic(
            [(0, "#E4E3D4"), (0.07, "#CBCEC2"), (0.6, "#DFE4D7"), (1, "#8DA3A1")],
            18,
            (0.95, 0.96, 0.88, 0.24),
            (0.08, 0.16, 0.16, 0.28),
        )
        blue = plastic(
            [(0, "#183E56"), (0.11, "#0D2C44"), (0.55, "#0A263E"), (1, "#183A4D")],
            15,
            (0.28, 0.48, 0.60, 0.36),
            (0.01, 0.04, 0.07, 0.55),
        )
        molding = plastic(
            [(0, "#28546D"), (0.12, "#0D2C46"), (0.69, "#0B3654"), (1, "#1D5970")],
            24,
            "#54879E",
            "#01111E",
        )
        hardware = (
            box()
            .inset(0)
            .lighting(
                Lighting(
                    studio(
                        direction=128,
                        elevation=65,
                        color="#EDF7FF",
                        intensity=0.68,
                        ambient=0.74,
                    )
                )
            )
            .children(
                [
                    pen("mdr.desk.support", support, Cache.Texture),
                    box().rect(115, 49, 1570, 894).borderRadius(43).fill(body),
                    box().rect(147, 77, 1512, 831).borderRadius(35).fill(blue),
                    box()
                    .rect(SX - 26, SY - 37, SW + 54, SH + 66)
                    .borderRadius(78)
                    .fill(molding),
                    pen("mdr.cabinet.marks", cabinet_marks, Cache.Texture),
                    pen("mdr.keyboard", keyboard_art, Cache.Texture),
                    keyboard_type(),
                ]
            )
        )
        header = (
            box()
            .inset(0)
            .children(
                [
                    pen("mdr.screen.rules", screen_rules, Cache.Texture),
                    pen("mdr.globe", globe, Cache.Texture),
                    label("Cold Harbor", 47, 33, 24, BRIGHT, "Helvetica Neue", 700),
                    label(
                        "67% Complete", 730, 34, 23, BRIGHT, "Helvetica Neue", 700
                    ).opacity(bind(self.committed, reverse=True)),
                    label(
                        "68% Complete", 730, 34, 23, BRIGHT, "Helvetica Neue", 700
                    ).opacity(self.committed),
                    box().rect(978, 32, 88, 25).fill(GROUND),
                    label("LUMON", 978, 32, 25, BRIGHT, "Helvetica Neue", 700),
                    label("0x177056  :  0x09832E", 389, 682, 18).opacity(
                        bind(self.committed, reverse=True)
                    ),
                    label("0x177056  :  0x09836F", 389, 682, 18).opacity(
                        self.committed
                    ),
                ]
            )
        )
        rng = Random(2037)
        field = box().rect(28, 113, 1062, 471).overflow(Overflow.Clip)
        packet = box().rect(0, 0, 151, 133).opacity(self.packet_visible)
        selected_values = []
        for row in range(11):
            for col in range(22):
                x, y = 13 + col * 48, row * 43
                value = rng.randrange(10)
                period = rng.uniform(5.8, 13.8)
                phase = rng.random() * 7
                node = label(str(value), x, y, 17 + rng.choice((0, 0, 1, -1))).key(
                    f"mdr.digit.{row}.{col}"
                )
                node.translateX(
                    bind(
                        self.seconds,
                        from_=(phase, phase + period),
                        envelope=envelope.cosine(),
                        to=(-2.0, 2.0),
                    )
                )
                node.translateY(
                    bind(
                        self.seconds,
                        from_=(phase + 2, phase + period + 2),
                        envelope=envelope.cosine(),
                        to=(-2.5, 2.5),
                    )
                )
                if (col, row) in SELECTION:
                    node.scale(bind(self.selected, to=(1.0, 2.2))).ink(BRIGHT).opacity(
                        self.grid_before
                    )
                    replacement = (
                        label(str((value + 3) % 10), x, y, 17)
                        .key(f"mdr.refill.{row}.{col}")
                        .opacity(self.grid_after)
                    )
                    field.children([replacement])
                    selected_values.append((col, row, value))
                field.children([node])
        for col, row, value in selected_values:
            packet.children(
                [label(str(value), (col - 8) * 48, (row - 4) * 43, 37.4, BRIGHT)]
            )
        path = (
            PathBuilder()
            .moveTo(493, 342)
            .cubicTo(567, 393, 615, 523, 557, 632)
            .detach()
        )
        packet.travel(MotionPath(heldPath(path), t=self.packet_progress))
        packet.scale(bind(self.packet_progress, to=(1.0, 0.22)))
        bins = box().inset(0)
        for index, value in enumerate((77, 73, 59, 52, 75)):
            x = 68 + index * 198
            bins.children(
                [
                    outline(x, 611, 178, 39),
                    label(f"0{index + 1}", x + 69, 616, 24),
                    outline(x, 658, 178, 27),
                ]
            )
            if index == 2:
                bins.children(
                    [
                        box()
                        .rect(x + 3, 661, 172, 21)
                        .fill(INK)
                        .transformOrigin("0%", "50%")
                        .scaleX(bind(self.committed, to=(0.59, 0.64))),
                        label("59%", x + 9, 660, 19, GROUND).opacity(
                            bind(self.committed, reverse=True)
                        ),
                        label("64%", x + 9, 660, 19, GROUND).opacity(self.committed),
                        box()
                        .rect(x, 608, 178, 3)
                        .fill(BRIGHT)
                        .opacity(self.classifying),
                    ]
                )
            else:
                bins.children(
                    [
                        box().rect(x + 3, 661, 172 * value / 100, 21).fill(INK),
                        label(f"{value}%", x + 9, 660, 19, GROUND),
                    ]
                )
        drawer = (
            box()
            .rect(454, 387, 212, 214)
            .fill(GROUND)
            .stroke(stroke(2, INK))
            .opacity(self.drawer)
        )
        drawer.children([label("03", 89, 8, 25), box().rect(10, 42, 192, 2).fill(INK)])
        for i, (name, before, after) in enumerate(
            (
                ("WO", 0.59, 0.72),
                ("FC", 0.58, 0.62),
                ("DR", 0.63, 0.76),
                ("MA", 0.51, 0.60),
            )
        ):
            y = 61 + i * 32
            drawer.children(
                [
                    label(name, 13, y, 16),
                    outline(56, y, 140, 21, 1),
                    box()
                    .rect(59, y + 3, 134, 15)
                    .fill(INK)
                    .transformOrigin("0%", "50%")
                    .scaleX(bind(self.classifying, to=(before, after))),
                ]
            )
        drawer.children([label("TEMPER BALANCE", 27, 191, 14, DIM)])
        arrow = pen("mdr.cursor", cursor, Cache.Texture).rect(508, 347, 17, 27)
        arrow.translateX(bind(self.phase, from_=(0, 5.5), clampFrom=True, to=(-190, 0)))
        arrow.translateY(bind(self.phase, from_=(0, 5.5), clampFrom=True, to=(-58, 0)))
        arrow.opacity(self.pointer_visible)
        tube = Filter.program(
            "uniform shader content; uniform float2 uSize; uniform float uSeconds;\n"
            + (ROOT / "Tube.sksl").read_text()
        )
        tube.set("uSize", (SW, SH))
        tube.bind("uSeconds", self.seconds)
        optics = Filter.bloom(
            BloomOptions(
                sigma=0.86,
                strength=0.38,
                spread=3.6,
                tail=0.19,
                threshold=0.2,
                knee=0.12,
                softness=0.2,
                whitening=0.11,
                dilation=0.12,
                deepening=0.68,
            )
        ).then(tube)
        screen = (
            box()
            .rect(SX, SY, SW, SH)
            .borderRadius(54)
            .overflow(Overflow.Clip)
            .children(
                [
                    box().inset(0).fill(GROUND),
                    box()
                    .inset(0)
                    .opacity(self.screen_power)
                    .children([header, field, bins, arrow, drawer, packet])
                    .filter(optics),
                    pen("mdr.glass.reflection", glass, Cache.Texture).rect(
                        0, 0, SW, SH
                    ),
                ]
            )
        )
        spill = (
            box()
            .rect(SX - 62, SY - 45, SW + 124, SH + 106)
            .borderRadius(87)
            .fill(
                Paint.radialGradient(
                    (0.5, 0.5),
                    0.75,
                    [
                        (0, (0.05, 0.53, 0.88, 0.045)),
                        (0.6, (0.03, 0.47, 0.80, 0.037)),
                        (1, (0, 0, 0, 0)),
                    ],
                )
            )
        )
        ctx.render(
            box()
            .inset(0)
            .children(
                [
                    hardware,
                    spill,
                    screen,
                    label(
                        "LUMON INDUSTRIES",
                        1373,
                        867,
                        10,
                        "#3F667B",
                        "Helvetica Neue",
                        700,
                    ),
                    label(
                        "MACRODATA REFINEMENT",
                        1380,
                        883,
                        8,
                        "#35546A",
                        "Helvetica Neue",
                        600,
                    ),
                ]
            )
        )

    def update(self, elapsed, ctx: SketchContext):
        t = elapsed % 26

        def ramp(a, b):
            return max(0.0, min(1.0, (t - a) / (b - a)))

        self.seconds.set(elapsed)
        self.phase.set(t)
        self.selected.set(ramp(2.0, 3.6))
        self.grid_before.set(1 - ramp(6.25, 6.6))
        self.grid_after.set(ramp(15.4, 16.8))
        self.pointer_visible.set(1 - ramp(6.3, 6.7) + ramp(15.4, 16.8))
        self.packet_visible.set(ramp(6.25, 6.6) * (1 - ramp(8.5, 9.0)))
        self.packet_progress.set(ramp(6.5, 8.7))
        self.drawer.set(ramp(8.4, 9.0) * (1 - ramp(13.3, 14.0)))
        self.classifying.set(ramp(9.4, 11.9))
        self.committed.set(ramp(11.9, 12.15))
        self.screen_power.set(1 - 0.18 * ramp(24.8, 25.7))
