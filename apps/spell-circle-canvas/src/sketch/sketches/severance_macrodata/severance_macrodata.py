"""Study · Television — the Macrodata Refinement terminal in Severance.

TAGS: Studies/Television, Interfaces/Television, Typography/Monospace, Animation/Signals, Materials/CRT

The numeric field, selected cluster and classification events are authored
study data. The terminal hierarchy follows a photographed production screen.
"""

from random import Random

from sigil.compose import Cache, Overflow, box, pen, stroke, text
from sigil.draw import Pen
from sigil.material import BloomOptions, Filter, shader
from sigil.motion import animatable, bind, envelope
from sigil.sketch import SketchContext, sketch

WIDTH, HEIGHT = 1440, 1000
SCREEN_X, SCREEN_Y = 164, 144
SCREEN_W, SCREEN_H = 1112, 710
INK = "#65E4FC"
BRIGHT = "#BBF5FF"
DIM = "#3693AC"
GROUND = "#062335"


def label(value, x, y, size=18, ink=INK):
    return (
        text(value, size=size, color=ink).fontFamily("Menlo").fontWeight(600).at(x, y)
    )


def outline(x, y, width, height, weight=2, ink=INK):
    return box().rect(x, y, width, height).stroke(stroke(weight, ink))


def cabinet(p: Pen) -> None:
    p.noStroke()
    p.fill("#DBE7E6")
    p.rect(0, 0, WIDTH, HEIGHT)
    p.fill("#BDC9C7")
    p.rect(0, 830, WIDTH, 170)
    p.fill("#9DAEAC")
    p.rect(90, 89, 1276, 805, 38)
    p.fill("#405354")
    p.rect(91, 63, 1260, 812, 37)
    p.fill("#283D42")
    p.rect(94, 60, 1250, 802, 32)
    p.fill("#192C34")
    p.rect(110, 79, 1215, 764, 22)
    p.fill("#101F28")
    p.rect(135, 111, 1163, 732, 32)
    p.fill("#104254")
    p.rect(151, 131, 1136, 732, 55)
    p.fill(GROUND)
    p.rect(SCREEN_X, SCREEN_Y, SCREEN_W, SCREEN_H, 38)
    p.stroke("#54666A")
    p.strokeWeight(1)
    p.noFill()
    p.rect(94, 60, 1250, 802, 32)
    p.stroke("#0F1F28")
    p.line(114, 841, 1320, 841)
    p.noStroke()
    p.fill("#172C32")
    p.circle(720, 98, 30)
    p.fill("#263D42")
    p.circle(720, 98, 20)
    p.fill("#10242B")
    p.circle(720, 98, 14)
    p.fill("#152932")
    p.circle(1298, 791, 29)
    p.fill("#38545A")
    p.circle(1298, 791, 18)
    p.fill("#08212C")
    p.circle(1298, 791, 10)
    for side in (106, 1324):
        for y in (137, 776):
            p.fill("#12242B")
            p.circle(side, y, 11)
            p.stroke("#617271")
            p.strokeWeight(1)
            p.line(side - 3, y, side + 3, y)
            p.noStroke()
    p.fill("#AFBCB7")
    p.rect(210, 879, 1020, 95, 11)
    p.fill("#899B94")
    p.rect(222, 889, 996, 72, 6)
    for row in range(3):
        for column in range(24):
            x, y = 235 + column * 40, 895 + row * 21
            p.fill("#536861")
            p.rect(x, y + 2, 35, 16, 2)
            p.fill("#CFD5C9")
            p.rect(x, y, 35, 14, 2)
    p.stroke("#667877")
    p.strokeWeight(1)
    for i in range(22):
        p.line(62, 160 + i * 26, 81, 160 + i * 26)
        p.line(1359, 160 + i * 26, 1378, 160 + i * 26)
    p.stroke("#698083")
    for i in range(43):
        x = 161 + (i * 113.17) % 1060
        y = 119 if i % 2 else 849
        p.line(x, y, x + 2 + i % 9, y - (i % 3))


def globe(p: Pen) -> None:
    p.noFill()
    p.stroke(INK)
    p.strokeWeight(1.3)
    p.ellipse(1022, 40, 105, 60)
    p.ellipse(1022, 40, 72, 60)
    p.ellipse(1022, 40, 31, 60)
    p.ellipse(1022, 40, 105, 28)
    p.line(970, 40, 1074, 40)
    p.line(979, 23, 1065, 23)
    p.line(979, 57, 1065, 57)


def screen_rules(p: Pen) -> None:
    p.noFill()
    p.stroke(INK)
    p.strokeWeight(2)
    p.rect(35, 28, 877, 39)
    p.line(23, 90, 1089, 90)
    p.line(23, 94, 1089, 94)
    p.line(23, 589, 1089, 589)
    p.line(23, 593, 1089, 593)
    for i in range(45):
        x = 30 + i * 24
        p.line(x, 583, x, 589)
    p.strokeWeight(1)
    for i in range(40):
        x = 259 + i * 11.4
        p.line(x, 31, x, 64)
    p.noStroke()
    p.fill("#052336")
    p.rect(718, 30, 193, 35)


def file_header():
    return (
        box()
        .inset(0)
        .children(
            pen("severance.header.rules", screen_rules, Cache.Texture),
            pen("severance.lumon.globe", globe, Cache.Texture),
            label("Cold Harbor", 47, 33, 23, BRIGHT)
            .fontFamily("Arial")
            .fontWeight(700),
            label("67% Complete", 729, 34, 22, BRIGHT)
            .fontFamily("Arial")
            .fontWeight(700),
            box().rect(980, 28, 85, 25).fill(GROUND),
            label("LUMON", 980, 27, 25, BRIGHT).fontFamily("Arial").fontWeight(700),
            label("0x177056  :  0x09832E", 389, 682, 18),
        )
    )


def bins():
    complete = (77, 73, 59, 52, 75)
    result = box().inset(0)
    for index, value in enumerate(complete):
        x = 68 + index * 198
        result.children(
            outline(x, 611, 178, 39),
            label(f"0{index + 1}", x + 69, 616, 24),
            outline(x, 658, 178, 27),
            box().rect(x + 3, 661, (172 * value / 100), 21).fill(INK),
            label(f"{value}%", x + 9, 660, 19, GROUND),
        )
    return result


def cursor(p: Pen) -> None:
    p.stroke(BRIGHT)
    p.strokeWeight(1.5)
    p.fill(GROUND)
    p.triangle(0, 0, 1, 23, 6, 15)
    p.line(6, 15, 12, 23)


def temper_drawer(seconds):
    opened = bind(
        seconds, from_=(0, 18), envelope=envelope.trapezoid(0.60, 0.63, 0.84, 0.88)
    )
    result = box().rect(465, 390, 208, 212).fill(GROUND).stroke(stroke(2, INK))
    result.children(label("03", 87, 8, 25), box().rect(10, 43, 187, 2).fill(INK))
    for i, (name, value) in enumerate(
        (("WO", 0.72), ("FC", 0.49), ("DR", 0.84), ("MA", 0.61))
    ):
        y = 61 + i * 32
        result.children(
            label(name, 13, y, 16),
            outline(55, y, 137, 21, 1),
            box().rect(58, y + 3, 131 * value, 15).fill(INK),
        )
    result.children(label("TEMPER BALANCE", 25, 191, 14, DIM))
    return result.opacity(opened)


RASTER = """
half4 main(float2 p) {
    float row = 0.5 + 0.5 * cos(p.y * 3.14159265);
    float2 uv = p / uResolution;
    float corner = pow(length((uv - 0.5) * float2(1.0, 1.2)), 3.0);
    float a = 0.027 * row + 0.18 * corner;
    return half4(0, 0.005 * a, 0.012 * a, a);
}
"""


@sketch(size=(WIDTH, HEIGHT), background="#DBE7E6", capture_at=4.6)
class MacrodataRefinement:
    def setup(self, ctx: SketchContext) -> None:
        self.seconds = animatable(0)
        rng = Random(2037)
        field = box().rect(28, 113, 1062, 471).overflow(Overflow.Clip)
        select = bind(
            self.seconds,
            from_=(0, 18),
            envelope=envelope.trapezoid(0.12, 0.17, 0.32, 0.38),
        )
        selection = {(8, 4), (9, 4), (10, 4), (8, 5), (9, 5), (10, 5), (9, 6)}
        self.digits = []
        for row in range(11):
            for column in range(22):
                x = 13 + column * 48
                y = row * 43
                digit = rng.randrange(10)
                self.digits.append(digit)
                size = 17 + rng.choice((0, 0, 1, -1))
                phase = rng.random() * 6
                period = rng.uniform(4.8, 13.8)
                node = label(str(digit), x, y, size, INK).key(f"digit.{row}.{column}")
                node.translateX(
                    bind(
                        self.seconds,
                        from_=(phase, phase + period),
                        envelope=envelope.cosine(),
                        to=(-1.5, 1.5),
                    )
                )
                node.translateY(
                    bind(
                        self.seconds,
                        from_=(phase + 2, phase + period + 2),
                        envelope=envelope.cosine(),
                        to=(-1.8, 1.8),
                    )
                )
                if (column, row) in selection:
                    node.scale(bind(select, to=(1.0, 2.1))).ink(BRIGHT)
                field.children(node)
        selection_box = outline(399, 280, 147, 105, 1.1, DIM).opacity(
            bind(select, to=(0.0, 0.75))
        )
        pointer = pen("severance.pointer", cursor, Cache.Texture).rect(507, 345, 16, 26)
        pointer.translateX(
            bind(self.seconds, from_=(0, 18), envelope=envelope.cosine(), to=(-14, 26))
        )
        pointer.translateY(
            bind(self.seconds, from_=(0, 9), envelope=envelope.cosine(), to=(-10, 12))
        )
        # A second retained cluster moves independently of the original field.
        # This deliberately tests many cached glyph nodes beneath a live parent.
        packet = box().rect(425, 333, 119, 88)
        for i, value in enumerate((7, 8, 2, 0, 1, 4)):
            packet.children(label(str(value), (i % 3) * 32, (i // 3) * 31, 25, BRIGHT))
        packet.translateX(
            bind(
                self.seconds,
                from_=(0, 18),
                envelope=envelope.trapezoid(0.38, 0.51, 0.53, 0.54),
                to=(0, 82),
            )
        )
        packet.translateY(
            bind(
                self.seconds,
                from_=(0, 18),
                envelope=envelope.trapezoid(0.38, 0.51, 0.53, 0.54),
                to=(0, 298),
            )
        )
        packet.opacity(
            bind(
                self.seconds,
                from_=(0, 18),
                envelope=envelope.trapezoid(0.37, 0.39, 0.51, 0.54),
            )
        )
        screen = (
            box()
            .rect(SCREEN_X, SCREEN_Y, SCREEN_W, SCREEN_H)
            .borderRadius(36)
            .overflow(Overflow.Clip)
        )
        screen.fill(GROUND).children(
            file_header(),
            field,
            selection_box,
            bins(),
            pointer,
            packet,
            temper_drawer(self.seconds),
            box().inset(0).fill(shader(RASTER)),
        )
        screen.filter(
            Filter.bloom(
                BloomOptions(
                    sigma=0.8,
                    strength=0.24,
                    spread=3.0,
                    tail=0.14,
                    threshold=0.24,
                    knee=0.12,
                    softness=0.2,
                    whitening=0.08,
                    dilation=0.2,
                    deepening=0.7,
                )
            )
        )
        ctx.render(
            box()
            .inset(0)
            .children(
                pen("severance.cabinet", cabinet, Cache.Texture),
                screen,
                label("LUMON INDUSTRIES", 185, 866, 11, "#425D60"),
                label("MACRODATA REFINEMENT", 1071, 866, 11, "#425D60"),
                label("CTRL", 239, 938, 8, "#61716A"),
                label("ENTER", 1138, 938, 8, "#61716A"),
            )
        )

    def update(self, elapsed, ctx: SketchContext) -> None:
        self.seconds.set(elapsed)
