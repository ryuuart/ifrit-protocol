"""Study · Film — the illuminated Mother wall in Alien.

TAGS: Studies/Film, Interfaces/Film, Typography/Custom, Materials/CRT, Materials/Relief, Diagrams/Circuits, Animation/Signals

Molded panel and display conventions follow an inspected film frame.
The access narrative and all circuit topology are original study content.
"""

import json
from math import cos, pi, sin
from pathlib import Path

from sigil.compose import Cache, MotionPath, Overflow, box, heldPath, pen, text
from sigil.draw import RGB, SHAPE, Pen
from sigil.material import BloomOptions, Filter, Paint
from sigil.motion import animatable, bind, envelope
from sigil.sketch import SketchContext, sketch
from sigil.skia import PathBuilder

WIDTH, HEIGHT = (1800, 1125)
SX, SY, SW, SH = (544, 333, 712, 518)
GREEN, BRIGHT = ("#A5F4B8", "#C0FDD8")
BANKS = [
    (18, 18, 575, 250, 7, 3),
    (611, 18, 576, 250, 7, 3),
    (1205, 18, 577, 250, 7, 3),
    (18, 288, 464, 585, 6, 8),
    (1318, 288, 464, 585, 6, 8),
    (18, 1046, 1764, 130, 22, 2),
]
ROOT = Path(__file__).parent
GLYPHS = json.loads((ROOT / "data/glyphs.json").read_text())


def rgba(r, g, b, a):
    return (r, g, b, a)


def radial(stops):
    return Paint.radialGradient((0.5, 0.5), 0.72, stops)


def modules():
    result = []
    for bank, (x, y, w, h, columns, rows) in enumerate(BANKS):
        px, py = ((w - 42) / columns, (h - 42) / rows)
        for row in range(rows):
            for col in range(columns):
                result.append(
                    (
                        x + 21 + col * px,
                        y + 21 + row * py,
                        px - 7,
                        py - 8,
                        len(result),
                        bank,
                    )
                )
    return result


def label(value, x, y, size=10, ink="#20180F"):
    return (
        text(value, size=size, color=ink)
        .fontFamily("Helvetica Neue")
        .fontWeight(700)
        .at(x, y)
    )


def screw(p, x, y, diameter=7):
    p.noStroke()
    p.fill("#241B13")
    p.circle(x + 1, y + 1, diameter + 2)
    p.fill("#82715A")
    p.circle(x, y, diameter)
    p.fill("#4B3B29")
    p.circle(x, y, diameter - 3)
    p.stroke("#20190F")
    p.strokeWeight(1.3)
    p.line(x - 2, y + 1, x + 2, y - 1)


def wall_height(p: Pen):
    p.colorMode(RGB, 1)
    p.noStroke()
    p.fill(0.36)
    p.rect(0, 0, WIDTH, HEIGHT)
    for x, y, w, h, *_ in BANKS:
        for inset, height, radius in (
            (0, 0.06, 9),
            (3, 0.46, 8),
            (9, 0.57, 7),
            (18, 0.25, 4),
            (24, 0.36, 3),
        ):
            p.fill(height)
            p.rect(x + inset, y + inset, w - 2 * inset, h - 2 * inset, radius)
    for x, y, w, h, *_ in modules():
        for ix, iy, height, radius in (
            (3, 3, 0.17, 6),
            (4, 3, 0.43, 6),
            (9, 8, 0.52, 5),
            (14, 13, 0.62, 4),
            (19, 18, 0.45, 3),
        ):
            p.fill(height)
            p.rect(x + ix, y + iy, w - 2 * ix, h - 2 * iy - 5, radius)
    for x, y, w, h, height, radius in (
        (500, 288, 800, 586, 0.065, 33),
        (504, 291, 792, 580, 0.47, 30),
        (515, 298, 770, 565, 0.65, 27),
        (533, 315, 734, 540, 0.4, 23),
        (542, 329, 716, 524, 0.13, 20),
    ):
        p.fill(height)
        p.rect(x, y, w, h, radius)
    for x, y, w, h in (
        (18, 891, 389, 133),
        (422, 891, 956, 133),
        (1393, 891, 389, 133),
    ):
        for inset, height in ((0, 0.07), (4, 0.51), (11, 0.43)):
            p.fill(height)
            p.rect(x + inset, y + inset, w - 2 * inset, h - 2 * inset, 2)


def acrylic(p: Pen):
    p.noFill()
    p.stroke("#8F7855")
    p.strokeWeight(0.75)
    for x, y, w, h, *_ in BANKS:
        p.rect(x + 17, y + 18, w - 34, h - 37, 4)
        p.stroke("#3E3020")
        p.line(x + 18, y + h - 20, x + w - 17, y + h - 20)
        for xx in (x + 20, x + w - 20):
            for yy in (y + 22, y + h - 24):
                screw(p, xx, yy)
        p.noFill()
        p.stroke("#8F7855")
    for x, y, w, h, *_ in modules():
        p.noStroke()
        p.fill("#16100B")
        p.circle(x + w * 0.77, y + h - 7, 7)
        p.fill("#594329")
        p.circle(x + w * 0.77, y + h - 7, 4)
        p.stroke("#746044")
        p.strokeWeight(0.5)
        for i in range(5):
            p.line(x + 4, y + 11 + i * 3, x + 7, y + 11 + i * 3)
    p.stroke("#9F8962")
    p.strokeWeight(0.55)
    for i in range(110):
        x, y = (25 + i * 143.11 % 1748, 885 if i % 3 == 0 else 1028)
        p.line(x, y, x + 4 + i % 14, y - i % 3)
    p.noStroke()
    p.fill(
        Paint.linearGradient(
            (0, 0),
            (1, 1),
            [
                (0, (0.8, 0.64, 0.34, 0.085)),
                (0.35, (0, 0, 0, 0)),
                (1, (0.1, 0.07, 0.04, 0.13)),
            ],
        ),
        SHAPE,
    )
    p.rect(25, 27, 1750, 240)


def steady_lamps(p: Pen):
    p.noStroke()
    for x, y, w, h, serial, bank in modules():
        if (serial * 19 + bank * 7) % 11 > 6:
            continue
        x, y = (x + w * 0.77, y + h - 7)
        p.fill(
            radial(
                [
                    (0, (1, 0.65, 0.22, 0.65)),
                    (0.3, (1, 0.46, 0.1, 0.3)),
                    (1, (0, 0, 0, 0)),
                ]
            ),
            SHAPE,
        )
        p.circle(x, y, 72)
        p.fill("#DA8C30")
        p.circle(x, y, 7)
        p.fill("#FFDE89")
        p.circle(x - 0.6, y - 0.6, 4.6)
        p.fill("#FFF4CC")
        p.circle(x - 1, y - 1, 3.2)


def routes():
    result = []
    for sector in range(8):
        a = sector * pi / 4

        def point(x, y, angle=a):
            return (
                SW / 2 + x * cos(angle) - y * sin(angle),
                SH / 2 + x * sin(angle) + y * cos(angle),
            )

        for rail in range(14):
            depth = 32 + rail * (12.7 + sector % 3 * 0.23)
            width = min(depth * (0.37 + sector % 2 * 0.022), 84)
            inset = 6 if (rail + sector) % 3 == 0 else 2.2
            result.append(
                [
                    point(18, 4 + rail * 2),
                    point(depth - 8, 4 + rail * 2),
                    point(depth - 8, width - inset),
                    point(depth, width - inset),
                    point(depth, -width),
                    point(depth - 8, -width),
                    point(depth - 8, -10 - rail * 1.8),
                    point(25, -10 - rail * 1.8),
                ]
            )
        for chip in range(5):
            x, y = (86 + chip * 27, -22 - chip * 4)
            for turn in range(3):
                d = turn * 3.4
                result.append(
                    [
                        point(x - d, y - d),
                        point(x + 17 + d, y - d),
                        point(x + 17 + d, y + 13 + d),
                        point(x - d, y + 13 + d),
                        point(x - d, y + 3),
                        point(x + 9, y + 3),
                    ]
                )
        for chip in range(6):
            x, y = (62 + chip * 25, 12 + chip * 3.4)
            if (chip + sector) % 3 == 0:
                y += 9
            for turn in range(2):
                d = turn * 3.2
                result.append(
                    [
                        point(x - d, y - d),
                        point(x + 16 + d, y - d),
                        point(x + 16 + d, y + 12 + d),
                        point(x + 4, y + 12 + d),
                        point(x + 4, y + 4),
                        point(x + 12, y + 4),
                    ]
                )
        for finger in range(18):
            x = 60 + finger * 8.4
            result.append([point(x, -3), point(x, 5), point(x + 3, 5)])
    return result


ROUTES = routes()


def polyline(p, points):
    p.beginShape()
    for x, y in points:
        p.vertex(x, y)
    p.endShape()


def circuit_art(p: Pen):
    p.noFill()
    p.strokeWeight(1.05)
    for i, points in enumerate(ROUTES):
        p.stroke("#76D0A1" if i % 7 == 0 else "#398A58" if i % 3 == 0 else "#55B97E")
        polyline(p, points)
    p.stroke(GREEN)
    p.strokeWeight(1.6)
    for ring in range(4):
        r = 231 - ring * 3.8
        polyline(
            p,
            [
                (
                    SW / 2 + r * cos((v + 0.5) * pi / 4),
                    SH / 2 + r * sin((v + 0.5) * pi / 4),
                )
                for v in range(9)
            ],
        )
    p.strokeWeight(1)
    p.stroke("#74C991")
    for i in range(6):
        p.line(SW / 2 - 247, SH / 2 - 9 + i * 3, SW / 2 + 247, SH / 2 - 9 + i * 3)
    p.stroke(BRIGHT)
    p.rect(SW / 2 - 13, SH / 2 - 13, 26, 26, 2)
    p.noStroke()
    for sector in range(8):
        a = sector * pi / 4
        for i in range(28):
            x, y = (45 + i % 7 * 23, 20 + i // 7 * 8)
            xx, yy = (
                SW / 2 + x * cos(a) - y * sin(a),
                SH / 2 + x * sin(a) + y * cos(a),
            )
            p.fill("#76D0A1" if (i + sector) % 3 else "#A1EEC4")
            p.rect(xx, yy, 2.3, 3.6)


def glass(p: Pen):
    p.noStroke()
    p.fill(
        Paint.radialGradient(
            (0.45, 0.45),
            0.82,
            [(0, (0, 0, 0, 0)), (0.64, (0, 0, 0, 0)), (1, (0.01, 0.01, 0.008, 0.78))],
        ),
        SHAPE,
    )
    p.rect(0, 0, SW, SH, 22)
    p.fill(
        Paint.linearGradient(
            (0, 0),
            (0, 1),
            [
                (0, (0.65, 0.54, 0.34, 0.08)),
                (0.08, (0.13, 0.18, 0.11, 0.045)),
                (0.65, (0, 0, 0, 0)),
                (1, (0.53, 0.47, 0.27, 0.045)),
            ],
        ),
        SHAPE,
    )
    p.rect(5, 5, SW - 10, SH - 10, 24)
    p.noFill()
    p.stroke((0.5, 0.44, 0.28, 0.17))
    p.strokeWeight(1.3)
    p.rect(7, 7, SW - 14, SH - 14, 24)
    p.strokeWeight(0.4)
    p.stroke((0.4, 0.43, 0.32, 0.09))
    for i in range(35):
        x, y = (18 + i * 113.71 % (SW - 39), 20 + i * 179.91 % (SH - 42))
        p.line(x, y, x + i % 5 + 1, y + 1)


def glyph(character, size):
    runs = GLYPHS[character]

    def draw(p: Pen):
        p.noFill()
        p.stroke(GREEN)
        p.strokeWeight(size * 0.050)
        for run in runs:
            polyline(
                p, [((x + 65) * size / 1000, (760 - y) * size / 1000) for x, y in run]
            )

    return pen(f"mother.glyph.{character}.{size}", draw, Cache.Texture).rect(
        0, 0, size * 0.68, size
    )


def machine_line(value, x, y, size, clock, arrival):
    result = box().rect(x, y, len(value) * size * 0.68, size)
    for i, c in enumerate(value):
        if c == " ":
            continue
        result.children(
            [
                glyph(c, size)
                .at(i * size * 0.68, 0)
                .opacity(
                    bind(
                        clock,
                        from_=(arrival + i * 0.036, arrival + i * 0.036 + 0.022),
                        clampFrom=True,
                        to=(0, 1),
                    )
                )
            ]
        )
    return result


def program_filter(name, declared, values):
    result = Filter.program(declared + (ROOT / name).read_text())
    for uniform, value in values.items():
        result.set(uniform, value)
    return result


@sketch()
class NostromoMonitor:
    def setup(self, ctx: SketchContext):
        ctx.canvas(WIDTH, HEIGHT)
        ctx.background("#181008")
        ctx.captureAt(5.5)
        self.seconds, self.phase, self.power = (
            animatable(0.0),
            animatable(0.0),
            animatable(0.0),
        )
        self.circuit_visible, self.inquiry_visible = (animatable(0.0), animatable(0.0))
        wall = program_filter(
            "Wall.sksl",
            "uniform shader content; uniform float2 uSize; uniform float4 uIvory; uniform float4 uGrime; uniform float4 uLight; uniform float uDepth;\n",
            {
                "uSize": (WIDTH, HEIGHT),
                "uIvory": (0.72, 0.64, 0.49, 1),
                "uGrime": (0.11, 0.075, 0.042, 1),
                "uLight": (-0.5, -0.65, 0.72, 0),
                "uDepth": 29,
            },
        )
        tube = program_filter(
            "Tube.sksl",
            "uniform shader content; uniform float2 uSize; uniform float uSeconds; uniform float uCurvature; uniform float uRaster; uniform float uNoise;\n",
            {
                "uSize": (SW, SH),
                "uSeconds": 0.0,
                "uCurvature": 0.1,
                "uRaster": 0.2,
                "uNoise": 0.006,
            },
        )
        tube.bind("uSeconds", self.seconds)
        tube = Filter.bloom(
            BloomOptions(
                sigma=1.0,
                strength=0.2,
                spread=3.6,
                tail=0.13,
                threshold=0.1,
                knee=0.12,
                softness=0.2,
                whitening=0.06,
                dilation=0.06,
                deepening=0.85,
            )
        ).then(tube)
        transfers = (
            box().inset(0).children([pen("mother.acrylic", acrylic, Cache.Texture)])
        )
        legends = (
            "CORE CONTROL",
            "REFLECTOR",
            "LOCAL",
            "ALARM LIGHT",
            "EMERG CONTROL",
            "SUB CODE",
            "COMPARATOR",
            "MEMORY",
            "MAIN LOCK",
            "DISABLE MEMORY",
            "CODE SELECT",
            "READY",
        )
        for x, y, w, h, serial, bank in modules():
            red = serial % 5 == 0 or serial % 17 == 2
            code = f"{251 + serial * 731 % 7754:03d}" + ("M" if serial % 4 == 0 else "")
            transfers.children(
                [
                    label(
                        legends[serial % 12] if serial % 3 == 0 else code,
                        x + 4,
                        y + h * 0.4,
                        7.2 if serial % 3 == 0 else 10.2,
                        "#682B15" if red else "#20180F",
                    )
                ]
            )
        transfers.children(
            [
                label("MU/TH/UR 6000", 830, 301, 11, "#312419"),
                label("IMPORTANT", 848, 928, 13, "#662911"),
                label("DO NOT ISOLATE", 826, 952, 10, "#662911"),
                label("REMOTE ACCESS", 123, 937, 13),
                label("MEMORY RETAINED", 1488, 937, 13),
            ]
        )
        steady = (
            pen("mother.lamps.steady", steady_lamps, Cache.Texture)
            .rect(0, 0, WIDTH, HEIGHT)
            .filter(
                Filter.bloom(
                    BloomOptions(
                        sigma=2.4,
                        strength=0.32,
                        spread=4.4,
                        tail=0.3,
                        threshold=0.18,
                        knee=0.1,
                    )
                )
            )
        )
        live = box().inset(0)
        cells = modules()
        for i in range(18):
            x, y, w, h, serial, _bank = cells[(i * 23 + 5) % len(cells)]
            x, y = (x + w * 0.77, y + h - 7)
            pulse = bind(
                self.phase,
                from_=(i * 0.39, 3.7 + i * 0.39),
                envelope=envelope.cosine(),
                to=(0.18, 1),
            )
            live.children(
                [
                    box()
                    .rect(x - 18, y - 18, 36, 36)
                    .borderRadius(18)
                    .fill(
                        radial(
                            [
                                (0, (1, 0.7, 0.25, 0.52)),
                                (0.24, (1, 0.38, 0.05, 0.23)),
                                (1, (0, 0, 0, 0)),
                            ]
                        )
                    )
                    .opacity(pulse),
                    box()
                    .rect(x - 2.5, y - 2.5, 5, 5)
                    .borderRadius(3)
                    .fill("#FFE6A2")
                    .opacity(pulse),
                ]
            )
        circuits = (
            box()
            .inset(0)
            .opacity(self.circuit_visible)
            .children(
                [pen("mother.circuit", circuit_art, Cache.Texture).rect(0, 0, SW, SH)]
            )
        )
        for i in range(24):
            points = ROUTES[(i * 17 + 9) % len(ROUTES)]
            path = PathBuilder().moveTo(*points[0])
            for x, y in points[1:]:
                path.lineTo(x, y)
            circuits.children(
                [
                    box()
                    .width(4)
                    .height(2)
                    .fill(BRIGHT)
                    .travel(
                        MotionPath(
                            heldPath(path.detach()),
                            t=bind(
                                self.phase,
                                from_=(i * 0.11, 2.7 + i * 0.11),
                                to=(0, 1),
                                wrap=1,
                            ),
                            lookAhead=0.015,
                        )
                    )
                ]
            )
        inquiry = box().inset(0).opacity(self.inquiry_visible)
        for value, arrival, y in (
            ("MOTHER / LOCAL ACCESS", 9.7, 74),
            ("CREW INQUIRY 03", 11.0, 135),
            ("WHERE IS THE SIGNAL?", 12.5, 198),
            ("COMPARATOR ACTIVE", 14.2, 259),
            ("SOURCE OUTSIDE CHART", 16.1, 320),
            ("REQUEST HELD", 18.6, 386),
            ("AWAITING AUTHORITY", 20.2, 443),
        ):
            inquiry.children([machine_line(value, 57, y, 41, self.phase, arrival)])
        inquiry.children(
            [
                box()
                .rect(57, 484, 15, 3)
                .fill(BRIGHT)
                .opacity(
                    bind(self.phase, from_=(21.1, 21.8), envelope=envelope.square(0.6))
                )
            ]
        )
        display = (
            box()
            .rect(SX, SY, SW, SH)
            .borderRadius(22)
            .overflow(Overflow.Clip)
            .children(
                [
                    box().inset(0).fill("#020504"),
                    box()
                    .inset(0)
                    .opacity(self.power)
                    .children([circuits, inquiry])
                    .filter(tube),
                    pen("mother.glass.reflections", glass, Cache.Texture).rect(
                        0, 0, SW, SH
                    ),
                ]
            )
        )
        spill = (
            box()
            .rect(435, 238, 930, 690)
            .borderRadius(50)
            .fill(
                radial(
                    [
                        (0, (0.14, 0.38, 0.25, 0.1)),
                        (0.45, (0.12, 0.39, 0.23, 0.08)),
                        (1, (0, 0, 0, 0)),
                    ]
                )
            )
            .opacity(self.power)
        )
        lens = (
            box()
            .inset(0)
            .fill(
                Paint.radialGradient(
                    (0.5, 0.47),
                    0.77,
                    [
                        (0, (0, 0, 0, 0)),
                        (0.7, (0.025, 0.013, 0, 0.05)),
                        (1, (0.022, 0.013, 0.005, 0.44)),
                    ],
                )
            )
        )
        ctx.render(
            box()
            .inset(0)
            .children(
                [
                    pen("mother.wall.height", wall_height, Cache.Texture)
                    .rect(0, 0, WIDTH, HEIGHT)
                    .filter(Filter.blur(0.65).then(wall)),
                    transfers,
                    steady,
                    live,
                    spill,
                    display,
                    lens,
                ]
            )
        )

    def update(self, elapsed, ctx):
        t = elapsed % 30

        def ramp(a, b):
            return max(0.0, min(1.0, (t - a) / (b - a)))

        self.seconds.set(elapsed)
        self.phase.set(t)
        self.power.set(ramp(0.3, 1.5) * (1 - ramp(27.3, 29.4)))
        self.circuit_visible.set(1 - ramp(8.0, 9.1) + ramp(25.1, 26.2))
        self.inquiry_visible.set(ramp(9.2, 9.5) * (1 - ramp(24.2, 25.0)))
