"""Study · Literature — a reader's view into the Library of Babel.

TAGS: Studies/Cultural, Studies/Literature, Typography/Paragraphs, Drawing/Generative, Diagrams/Architecture, Materials/Physical, Runtime/Python

Four book walls, two open sides and a central well form each hexagon.
The perspective, joinery, connecting route and reader's ledger are authored.
"""

from math import cos, exp, hypot, pi, sin
from random import Random

from sigil.compose import Cache, box, text
from sigil.compose import pen as canvas
from sigil.draw import CLOSE, Pen
from sigil.material import shader
from sigil.motion import animatable, bind
from sigil.sketch import SketchContext, sketch
from sigil.weave import FrameOptions, Leading, Type

SIZE = (1600, 1100)
GROUND = "#171b19"
TYPE = "Baskerville, Georgia, serif"
LABEL = "Avenir Next, Helvetica Neue, sans-serif"
MONO = "Menlo, monospace"
CAMERA = (0.4, 4.0, -1)
PITCH, FOCAL, HORIZON, CENTER = 0.115, 800, 381, 880
ROOM_RADIUS, WELL_RADIUS, ROOM_STEP, LEVEL_STEP = 12, 5.25, 24.15, 5.5
HEX = [(-12, 0), (-6, 10.3923), (6, 10.3923), (12, 0), (6, -10.3923), (-6, -10.3923)]
WALLS = (0, 2, 3, 5)
BACK = (23, 27, 25)
SPINES = ("MIXA", "LVM", "QVRA", "MAV", "AXM", "VAR", "TXX")
LEATHER = (
    (105, 61, 42),
    (77, 78, 50),
    (128, 90, 49),
    (107, 47, 33),
    (141, 113, 72),
    (76, 61, 52),
    (61, 70, 65),
)


def lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def project(q):
    x, y, z = (q[i] - CAMERA[i] for i in range(3))
    depth = cos(PITCH) * z - sin(PITCH) * y
    vertical = cos(PITCH) * y + sin(PITCH) * z
    if depth < 0.8:
        return None
    return (CENTER + FOCAL * x / depth, HORIZON - FOCAL * vertical / depth, depth)


def polygon(p, points, fill, edge=None, weight=0.8):
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


def rgba(rgb, alpha=255):
    return tuple(max(0, min(255, v)) / 255 for v in rgb) + (alpha / 255,)


class Interior:
    """A painter's projection of bounded native polygons, not a bitmap scene."""

    def __init__(self):
        self.faces = []
        self.lamps = []
        self.build()

    def light(self, q, rgb, normal=1.0):
        x, y, z = q
        level = round(y / LEVEL_STEP)
        center = 18 + ROOM_STEP * round((z - 18) / ROOM_STEP)
        strength = 0.22
        for lx, lz in ((-3.8, center - 2.8), (3.8, center + 2.8)):
            d2 = (x - lx) ** 2 + (y - level * LEVEL_STEP - 3.6) ** 2 + (z - lz) ** 2
            strength += 18.0 / (7 + d2)
        strength = min(1.52, strength) * normal
        fog = 1 - exp(-max(0, z - 24) / 135)
        return rgba(
            tuple((c * strength) * (1 - fog) + BACK[i] * fog for i, c in enumerate(rgb))
        )

    def face(self, points, rgb, normal=1.0, bias=0):
        vertices = [project(q) for q in points]
        if any(q is None for q in vertices):
            return
        xy = [(q[0], q[1]) for q in vertices]
        if (
            max(q[0] for q in xy) < -20
            or min(q[0] for q in xy) > 1620
            or max(q[1] for q in xy) < -20
            or min(q[1] for q in xy) > 1010
        ):
            return
        center = tuple(sum(q[k] for q in points) / len(points) for k in range(3))
        depth = sum(q[2] for q in vertices) / len(vertices) + bias
        self.faces.append((depth, 0, xy, self.light(center, rgb, normal), 0))

    def line(self, a, b, rgb, width=1.0, normal=1.0, bias=-0.04):
        aa, bb = project(a), project(b)
        if aa is None or bb is None:
            return
        if (
            max(aa[0], bb[0]) < -20
            or min(aa[0], bb[0]) > 1620
            or max(aa[1], bb[1]) < -20
            or min(aa[1], bb[1]) > 1010
        ):
            return
        depth = (aa[2] + bb[2]) * 0.5 + bias
        weight = max(0.28, min(3.8, FOCAL / depth * width))
        self.faces.append(
            (
                depth,
                1,
                [(aa[0], aa[1]), (bb[0], bb[1])],
                self.light(lerp(a, b, 0.5), rgb, normal),
                weight,
            )
        )

    def cuboid(self, low, high, rgb):
        x, y, z = low
        xx, yy, zz = high
        self.face([(x, y, z), (xx, y, z), (xx, yy, z), (x, yy, z)], rgb, 0.79)
        self.face([(x, yy, z), (xx, yy, z), (xx, yy, zz), (x, yy, zz)], rgb, 1.12)
        self.face([(x, y, z), (x, yy, z), (x, yy, zz), (x, y, zz)], rgb, 0.90)
        self.face([(xx, y, z), (xx, yy, z), (xx, yy, zz), (xx, y, zz)], rgb, 0.95)
        # Long lintels expose a timber face: its grain is longitudinal,
        # with a bevel highlight and a shaded lower molding edge.
        if xx - x > 4 and z < 42:
            self.line(
                (x, y + 0.025, z - 0.013),
                (xx, y + 0.025, z - 0.013),
                (48, 36, 24),
                0.025,
            )
            self.line(
                (x, yy - 0.016, z - 0.018),
                (xx, yy - 0.016, z - 0.018),
                (189, 136, 72),
                0.017,
            )
            for strand in range(9):
                h = y + (yy - y) * (strand + 1) / 11
                for segment in range(20):
                    a = x + (xx - x) * segment / 20
                    b = x + (xx - x) * (segment + 1) / 20
                    ya = h + sin(a * 1.1 + strand * 0.47) * 0.006
                    yb = h + sin(b * 1.1 + strand * 0.47) * 0.006
                    self.line(
                        (a, ya, z - 0.02),
                        (b, yb, z - 0.02),
                        (95 + strand * 3, 65 + strand * 2, 37),
                        0.006,
                    )

    def wall(self, a, b, y, seed, far):
        rng = Random(seed)
        nx, nz = -(a[0] + b[0]), -(a[2] + b[2] - 2 * (18 + ROOM_STEP * far))
        nn = hypot(nx, nz)
        nx, nz = nx / nn, nz / nn

        def q(t, h, inward=0):
            return (
                a[0] + (b[0] - a[0]) * t + nx * inward,
                y + h,
                a[2] + (b[2] - a[2]) * t + nz * inward,
            )

        for panel in range(64):
            t0, t1 = panel / 64, (panel + 1) / 64
            self.face([q(t0, 0), q(t1, 0), q(t1, 4.84), q(t0, 4.84)], (58, 44, 31), 0.8)
        # Width diversity shares one shelf extent and preserves 32 books.
        for row in range(5):
            lo = 0.35 + row * 0.83
            for segment in range(64):
                t0, t1 = segment / 64, (segment + 1) / 64
                self.face(
                    [
                        q(t0, lo - 0.13, 0.25),
                        q(t1, lo - 0.13, 0.25),
                        q(t1, lo + 0.005, 0.25),
                        q(t0, lo + 0.005, 0.25),
                    ],
                    (97, 67, 39),
                )
            self.line(
                q(0, lo + 0.025, 0.30),
                q(1, lo + 0.025, 0.30),
                (183, 133, 71),
                0.019,
                1.15,
            )
            widths = [rng.uniform(0.64, 1.5) for _ in range(32)]
            total, acc = sum(widths), 0
            for book, width in enumerate(widths):
                t0, t1 = (
                    0.028 + 0.944 * acc / total,
                    0.028 + 0.944 * (acc + width * 0.94) / total,
                )
                acc += width
                hi, inward = lo + rng.uniform(0.59, 0.76), rng.uniform(0.12, 0.24)
                color = LEATHER[rng.randrange(len(LEATHER))]
                self.face(
                    [
                        q(t0, lo, inward),
                        q(t1, lo, inward),
                        q(t1, hi - 0.025, inward),
                        q(t1 - 0.0015, hi, inward),
                        q(t0 + 0.0015, hi, inward),
                        q(t0, hi - 0.025, inward),
                    ],
                    color,
                    1.0,
                    -0.015,
                )
                self.line(
                    q(t0 + 0.001, lo + 0.03, inward + 0.008),
                    q(t0 + 0.001, hi - 0.025, inward + 0.008),
                    tuple(c * 1.42 for c in color),
                    0.010,
                )
                self.line(
                    q(t1, lo, inward + 0.008),
                    q(t1, hi - 0.02, inward + 0.008),
                    (37, 28, 23),
                    0.014,
                )
                if far == 0 and row in (1, 3) and book % 7 == 0:
                    origin = project(
                        q(t0 + (t1 - t0) * 0.32, hi - 0.12, inward + 0.025)
                    )
                    down = project(q(t0 + (t1 - t0) * 0.32, lo + 0.20, inward + 0.025))
                    across = project(
                        q(t1 - (t1 - t0) * 0.20, hi - 0.12, inward + 0.025)
                    )
                    if origin and down and across:
                        self.faces.append(
                            (
                                origin[2] - 0.035,
                                3,
                                [origin[:2], down[:2], across[:2]],
                                self.light(q(t0, hi), (201, 166, 92), 1.18),
                                SPINES[(book + row) % len(SPINES)],
                            )
                        )
                if far < 3:
                    for band in (0.10, 0.18, 0.47, 0.55):
                        if band < hi - lo:
                            self.line(
                                q(t0, lo + band, inward + 0.013),
                                q(t1, lo + band, inward + 0.013),
                                (156, 120, 64),
                                0.012,
                            )
                    if book % 3 == 0:
                        for mark in range(rng.randrange(2, 5)):
                            h = lo + 0.27 + mark * 0.039
                            self.line(
                                q(t0 + (t1 - t0) * 0.25, h, inward + 0.02),
                                q(t1 - (t1 - t0) * 0.21, h, inward + 0.02),
                                (179, 145, 85),
                                0.010,
                            )
        for t in (0, 0.5, 1):
            self.face(
                [
                    q(t - 0.017, 0, 0.38),
                    q(t + 0.017, 0, 0.38),
                    q(t + 0.017, 4.87, 0.38),
                    q(t - 0.017, 4.87, 0.38),
                ],
                (108, 69, 37),
                0.94,
                -0.08,
            )
            self.line(
                q(t - 0.010, 0.1, 0.401),
                q(t - 0.010, 4.75, 0.401),
                (179, 129, 71),
                0.021,
            )
            self.line(
                q(t + 0.014, 0.1, 0.411), q(t + 0.014, 4.75, 0.411), (56, 39, 25), 0.027
            )
            if far == 0:
                for grain in range(7):
                    tbase = t - 0.011 + grain * 0.0031
                    for segment in range(16):
                        h0, h1 = 0.12 + segment * 0.282, 0.12 + (segment + 1) * 0.282
                        tt0 = tbase + sin(h0 * 3 + grain) * 0.00065
                        tt1 = tbase + sin(h1 * 3 + grain) * 0.00065
                        self.line(
                            q(tt0, h0, 0.417),
                            q(tt1, h1, 0.417),
                            (132 + grain * 4, 92 + grain * 2, 49),
                            0.005,
                        )
            for h in (0.1, 4.58, 4.75):
                self.line(
                    q(t - 0.02, h, 0.42), q(t + 0.02, h, 0.42), (202, 151, 81), 0.035
                )
        for h in (0.12, 4.80, 4.91):
            for segment in range(48):
                t0, t1 = segment / 48, (segment + 1) / 48
                self.face(
                    [
                        q(t0, h, 0.40),
                        q(t1, h, 0.40),
                        q(t1, h + 0.075, 0.40),
                        q(t0, h + 0.075, 0.40),
                    ],
                    (146, 101, 51),
                    1.03,
                    -0.09,
                )

    def floor(self, outer, inner, y, seed, far):
        rng = Random(seed)
        for k in range(6):
            a, b, ia, ib = outer[k], outer[(k + 1) % 6], inner[k], inner[(k + 1) % 6]
            for strip in range(8):
                r0, r1 = strip / 8, (strip + 1) / 8
                for board in range(12):
                    t0, t1 = board / 12, (board + 1) / 12
                    q00 = lerp(lerp(ia, a, r0), lerp(ib, b, r0), t0)
                    q01 = lerp(lerp(ia, a, r0), lerp(ib, b, r0), t1)
                    q10 = lerp(lerp(ia, a, r1), lerp(ib, b, r1), t0)
                    q11 = lerp(lerp(ia, a, r1), lerp(ib, b, r1), t1)
                    tone = 0.94 + (board % 4) * 0.025
                    self.face(
                        [q00, q01, q11, q10],
                        tuple(c * tone for c in (130, 102, 65)),
                        1.18,
                    )
            self.face(
                [ia, ib, (ib[0], y - 0.28, ib[2]), (ia[0], y - 0.28, ia[2])],
                (93, 59, 33),
                0.8,
                -0.08,
            )
            for fraction in (0.22, 0.47, 0.72, 0.95):
                aa, bb = lerp(ia, a, fraction), lerp(ib, b, fraction)
                self.line(aa, bb, (67, 49, 31), 0.024)
                self.line(
                    (aa[0], aa[1] + 0.003, aa[2] + 0.018),
                    (bb[0], bb[1] + 0.003, bb[2] + 0.018),
                    (176, 131, 75),
                    0.009,
                )
            for t in range(1, 9):
                aa, bb = lerp(ia, ib, t / 9), lerp(a, b, t / 9)
                self.line(aa, bb, (84, 63, 40), 0.012)
                if far < 2:
                    for g in range(3):
                        start, stop = (
                            lerp(aa, bb, rng.uniform(0.09, 0.58)),
                            lerp(aa, bb, rng.uniform(0.61, 0.96)),
                        )
                        self.line(
                            (start[0] + g * 0.04, y + 0.004, start[2]),
                            (stop[0] + g * 0.04, y + 0.004, stop[2]),
                            (114, 87, 50),
                            0.006,
                        )
            # Grain follows each projected timber plane and bends around
            # a knot. Its direction is inherited from the plank joints.
            if far == 0:
                for grain in range(31):
                    tbase = (grain + 0.6) / 32
                    for segment in range(15):
                        u0, u1 = segment / 15, (segment + 1) / 15
                        t0 = (
                            tbase
                            + sin(u0 * 15 + grain * 0.67) * 0.0018
                            + sin(u0 * 42 + grain) * 0.0005
                        )
                        t1 = (
                            tbase
                            + sin(u1 * 15 + grain * 0.67) * 0.0018
                            + sin(u1 * 42 + grain) * 0.0005
                        )
                        q0 = lerp(lerp(ia, a, u0), lerp(ib, b, u0), t0)
                        q1 = lerp(lerp(ia, a, u1), lerp(ib, b, u1), t1)
                        self.line(
                            (q0[0], y + 0.009, q0[2]),
                            (q1[0], y + 0.009, q1[2]),
                            (95 + (grain % 5) * 4, 70 + (grain % 5) * 3, 40),
                            0.008,
                        )
                if k in (3, 4):
                    for ring in range(6):
                        for sample in range(24):
                            a0, a1 = sample * pi / 12, (sample + 1) * pi / 12

                            def knot(angle, ring=ring, ia=ia, a=a, ib=ib, b=b, y=y):
                                u = 0.54 + (0.026 + ring * 0.011) * cos(angle)
                                t = 0.41 + (0.008 + ring * 0.003) * sin(angle)
                                q = lerp(lerp(ia, a, u), lerp(ib, b, u), t)
                                return (q[0], y + 0.011, q[2])

                            self.line(knot(a0), knot(a1), (82, 57, 32), 0.009)
            # A narrow contact shadow stays on the floor beside the rail.
            shadow_a, shadow_b = lerp(ia, a, 0.023), lerp(ib, b, 0.023)
            self.face([ia, ib, shadow_b, shadow_a], (39, 34, 25), 0.82, -0.08)
            for t in range(11):
                q = lerp(ia, ib, t / 10)
                self.line(q, (q[0], y + 0.64, q[2]), (139, 104, 61), 0.035, 0.85, -0.15)
                self.cuboid(
                    (q[0] - 0.035, y, q[2] - 0.035),
                    (q[0] + 0.035, y + 0.07, q[2] + 0.035),
                    (94, 71, 43),
                )
                if t % 2 == 0:
                    self.line(
                        (q[0], y + 0.15, q[2]),
                        (q[0], y + 0.47, q[2]),
                        (190, 149, 81),
                        0.014,
                        1.1,
                        -0.18,
                    )
            for h in (0.14, 0.64, 0.70):
                self.line(
                    (ia[0], y + h, ia[2]),
                    (ib[0], y + h, ib[2]),
                    (161, 117, 62),
                    0.043 if h > 0.5 else 0.020,
                    1.12,
                    -0.2,
                )

    def reader(self, x, y, z, facing=1):
        self.face(
            [
                (x - 0.19, y + 0.013, z - 0.10),
                (x + 0.24, y + 0.013, z - 0.10),
                (x + 0.79, y + 0.013, z + 0.46),
                (x + 0.28, y + 0.013, z + 0.55),
            ],
            (35, 32, 24),
            0.7,
            -0.07,
        )
        self.cuboid(
            (x - 0.13, y + 0.45, z), (x + 0.13, y + 1.29, z + 0.18), (44, 47, 42)
        )
        self.cuboid(
            (x - 0.105, y + 1.36, z + 0.03),
            (x + 0.105, y + 1.63, z + 0.17),
            (132, 107, 74),
        )
        for dx in (-0.09, 0.09):
            self.line(
                (x + dx, y, z + 0.08), (x + dx, y + 0.59, z + 0.08), (39, 38, 31), 0.085
            )
        self.line(
            (x, y + 1.18, z),
            (x + facing * 0.34, y + 0.94, z - 0.08),
            (59, 61, 52),
            0.070,
        )
        self.face(
            [
                (x + facing * 0.24, y + 0.92, z - 0.14),
                (x + facing * 0.49, y + 0.96, z - 0.14),
                (x + facing * 0.49, y + 1.15, z - 0.14),
                (x + facing * 0.24, y + 1.11, z - 0.14),
            ],
            (171, 137, 83),
            1.3,
            -0.1,
        )

    def stair(self):
        cx, cz = -5.15, 6.65
        for i in range(179):
            angle, y = i * 0.19, -19.0 + i * 0.154
            a0, a1 = angle, angle + 0.165

            def q(r, a, h=y):
                return (cx + r * cos(a), h, cz + r * sin(a))

            self.face(
                [q(0.16, a0), q(1.34, a0), q(1.34, a1), q(0.16, a1)],
                (142, 106, 58),
                0.99,
                -0.025,
            )
            self.line(q(0.18, a0), q(1.34, a0), (199, 149, 73), 0.019)
            self.line(q(1.30, a0), q(1.30, a0, y + 0.82), (104, 97, 68), 0.023)
            self.line(
                q(1.30, a0, y + 0.82), q(1.30, a1, y + 0.974), (185, 151, 88), 0.030
            )
        self.line((cx, -20, cz), (cx, 12.4, cz), (74, 67, 46), 0.11)

    def build(self):
        for far in range(6):
            cz = 18 + far * ROOM_STEP
            for level in range(-5, 5):
                y = level * LEVEL_STEP
                outer = [(x, y, z + cz) for x, z in HEX]
                inner = [
                    (
                        x * WELL_RADIUS / ROOM_RADIUS,
                        y,
                        z * WELL_RADIUS / ROOM_RADIUS + cz,
                    )
                    for x, z in HEX
                ]
                self.floor(outer, inner, y, far * 59 + level, far)
                for k in WALLS:
                    self.wall(
                        outer[k],
                        outer[(k + 1) % 6],
                        y,
                        501 + far * 719 + level * 41 + k,
                        far,
                    )
                for z in (cz - 10.4, cz + 10.4):
                    for x in (-5.65, 5.30):
                        self.cuboid(
                            (x, y, z - 0.15),
                            (x + 0.35, y + 5.15, z + 0.25),
                            (107, 75, 44),
                        )
                    self.cuboid(
                        (-5.7, y + 4.88, z - 0.15),
                        (5.65, y + 5.18, z + 0.25),
                        (128, 87, 45),
                    )
                if far < 5:
                    start, stop = cz + 10.39, cz + ROOM_STEP - 10.39
                    self.face(
                        [
                            (-2.15, y, start),
                            (2.15, y, start),
                            (2.15, y, stop),
                            (-2.15, y, stop),
                        ],
                        (134, 105, 67),
                        1.12,
                    )
                    for x in (-2.2, 2.2):
                        self.line(
                            (x, y + 0.62, start),
                            (x, y + 0.62, stop),
                            (165, 122, 67),
                            0.052,
                        )
                        for t in range(6):
                            z = start + (stop - start) * t / 5
                            self.line((x, y, z), (x, y + 0.62, z), (122, 96, 60), 0.038)
                    if level == 0:
                        self.line(
                            (0.55, y + 0.012, start),
                            (0.55, y + 0.012, stop),
                            (191, 113, 65),
                            0.042,
                        )
                for x, z in ((-3.8, cz - 2.8), (3.8, cz + 2.8)):
                    q = project((x, y + 3.6, z))
                    if q is not None and -20 < q[1] < 1000:
                        self.lamps.append(
                            (q[0], q[1], FOCAL / q[2] * 0.19, q[2], level)
                        )
                        self.faces.append(
                            (
                                q[2] - 0.12,
                                2,
                                [(q[0], q[1])],
                                (1, 1, 1, 1),
                                FOCAL / q[2] * 0.19,
                            )
                        )
                    self.line((x, y + 3.85, z), (x, y + 5.10, z), (102, 105, 72), 0.020)
                    self.cuboid(
                        (x - 0.05, y + 3.72, z - 0.06),
                        (x + 0.05, y + 3.86, z + 0.06),
                        (169, 134, 65),
                    )
        self.stair()
        self.reader(6.3, 0, 11.8)
        self.reader(-6.8, 5.5, 20.8, -1)
        self.reader(2.9, 0, 50.1)
        self.reader(-3.2, -5.5, 37.1)
        self.faces.sort(key=lambda q: q[0], reverse=True)

    def draw(self, p: Pen):
        p.push()
        p.clip(lambda: p.rect(0, 0, 1600, 996))
        for _, kind, points, color, weight in self.faces:
            if kind == 0:
                polygon(p, points, color)
            elif kind == 1:
                p.stroke(color)
                p.strokeWeight(weight)
                p.line(*points[0], *points[1])
            elif kind == 3:
                origin, down, across = points
                p.push()
                p.applyMatrix(
                    (down[0] - origin[0]) / 20,
                    (down[1] - origin[1]) / 20,
                    (across[0] - origin[0]) / 5,
                    (across[1] - origin[1]) / 5,
                    *origin,
                )
                p.noStroke()
                p.fill(color)
                p.textFont("Baskerville", 4.2)
                p.text(weight, 0, 0)
                p.pop()
            else:
                x, y = points[0]
                radius = weight
                p.noStroke()
                for ring in range(9, 0, -1):
                    p.fill(216, 162, 76, max(1, 11 - ring))
                    p.circle(x, y, radius * (1.1 + ring * 0.92) * 2)
                p.fill("#D5B77A")
                p.circle(x, y, radius * 2)
                p.fill("#EEDAAF")
                p.circle(x - radius * 0.19, y - radius * 0.23, radius * 1.4)
                p.fill("#FFF1C8")
                p.circle(x - radius * 0.3, y - radius * 0.35, radius * 0.61)
        p.pop()


ATMOSPHERE = """
half4 main(float2 p) {
    float2 uv = p / float2(1600,1100);
    float shade = 0.015 + 0.45*pow(abs(uv.x-0.55)*1.5,2.0) + 0.13*pow(abs(uv.y-0.42),2.0);
    float aperture = smoothstep(0.10,0.67,length((p-float2(839,370))/float2(720,590)));
    float pulse = 0.012*sin(seconds*1.1) + 0.006*sin(seconds*2.67);
    float a=clamp(shade + aperture*0.10 + pulse,0.0,0.53);
    return half4(half3(0.025,0.033,0.026)*half(a),half(a));
}
"""
BACKGROUND = """
half4 main(float2 p) {
    float focus = exp(-length((p-float2(845,390))/float2(620,520)));
    float tone = 0.035 + 0.16*focus;
    return half4(tone*1.04,tone*1.01,tone*0.86,1);
}
"""


def desk(p):
    polygon(p, [(0, 976), (1600, 956), (1600, 1100), (0, 1100)], "#30261B")
    rng = Random(749)
    for row in range(68):
        y = 980 + row * 2.0
        p.stroke(93 + row % 8, 62, 36, 65)
        p.strokeWeight(0.45)
        p.bezier(0, y, 540, y - 12 + sin(row * 0.3) * 5, 1080, y + 7, 1600, y - 20)
    polygon(p, [(60, 917), (745, 914), (813, 933), (829, 1100), (22, 1100)], "#131510")
    polygon(
        p, [(819, 933), (884, 914), (1528, 910), (1585, 1100), (828, 1100)], "#16150E"
    )
    for i in range(7):
        color = rgba((179 + i * 6, 165 + i * 5, 128 + i * 4))
        polygon(
            p,
            [
                (67 - i, 912 - i * 2),
                (752, 914 - i * 2),
                (814, 933 - i),
                (822, 1100),
                (39 - i * 2, 1100),
            ],
            color,
        )
        polygon(
            p,
            [
                (827, 934 - i),
                (878, 914 - i * 2),
                (1527 + i, 908 - i * 2),
                (1570 + i * 2, 1100),
                (829, 1100),
            ],
            color,
        )
    for i in range(22):
        p.stroke(73, 51, 28, max(1, 26 - i))
        p.strokeWeight(1.5)
        p.bezier(
            811 - i * 1.6,
            924,
            824 - i * 0.85,
            952,
            819 - i * 0.40,
            1010,
            823 - i * 0.3,
            1100,
        )
        p.bezier(
            833 + i * 1.6,
            924,
            821 + i * 0.85,
            952,
            829 + i * 0.40,
            1010,
            829 + i * 0.3,
            1100,
        )
    for _ in range(950):
        x, y = rng.uniform(74, 1540), rng.uniform(941, 1100)
        if 797 < x < 850:
            continue
        p.stroke(125, 98, 54, rng.randrange(5, 13))
        p.strokeWeight(0.3)
        p.line(x, y, x + rng.uniform(0.8, 4.0), y - rng.uniform(0, 0.5))
    p.stroke("#6C5A3C")
    p.strokeWeight(0.6)
    p.line(109, 981, 764, 979)
    p.line(891, 979, 1497, 974)
    for i in range(9):
        x, y = 1057 + i * 46, 1042
        points = [
            (x + 24 * cos(pi / 6 + k * pi / 3), y + 24 * sin(pi / 6 + k * pi / 3))
            for k in range(6)
        ]
        polygon(p, points, None, "#91754C", 0.7)
        p.noStroke()
        p.fill("#9D4D2D")
        p.circle(x, y, 3)
        if i < 8:
            p.stroke("#9D4D2D")
            p.strokeWeight(1.2)
            p.line(x, y, x + 46, 1042)


def label(
    value,
    x,
    y,
    size=14,
    width=650,
    height=50,
    family=LABEL,
    color="#D8C5A0",
    tracking=0,
    leading=None,
):
    node = (
        text(value)
        .fontFamily(family)
        .font(Type(size=size))
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


@sketch(size=SIZE, background=GROUND, capture_at=4.0)
class BorgesLibrary:
    def setup(self, ctx: SketchContext):
        self.seconds = animatable(0)
        self.interior = Interior()
        route_dot = (
            box()
            .absolute()
            .left(1057)
            .translateX(bind(bind(self.seconds, from_=(0, 24), wrap=1), to=(0, 368)))
            .top(1039)
            .width(6)
            .height(6)
            .fill("#9D4D2D")
        )
        architecture = (
            canvas("borges.interior", self.interior.draw, Cache.Texture)
            .absolute()
            .inset(0)
        )
        foreground = canvas("borges.ledger", desk, Cache.Texture).absolute().inset(0)
        words = [
            label(
                "ESPACIOS IMAGINADOS   /   JORGE LUIS BORGES", 62, 48, 11, tracking=2.1
            ),
            label("La Biblioteca", 58, 83, 55, 635, 80, TYPE, "#E5D4AC", -0.7),
            label("de Babel", 61, 140, 42, 495, 64, TYPE, "#BE9E65"),
            label(
                "Una galería es una medida.\nLa búsqueda no tiene término.",
                65,
                213,
                17,
                340,
                57,
                TYPE,
                "#C4B590",
                leading=24,
            ),
            label(
                "POZOS, PASILLOS, ANAQUELES / HACIA EL CATÁLOGO",
                62,
                865,
                9,
                810,
                tracking=1.7,
            ),
            label(
                "Uncounted rooms. A finite alphabet. Each book waits for its reader.",
                62,
                883,
                13,
                970,
                25,
                TYPE,
            ),
            label("CUADERNO DEL VIAJERO", 109, 940, 13, 647, 31, TYPE, "#713D29", 2.0),
            label(
                "20 ANAQUELES   /   640 VOLÚMENES",
                109,
                990,
                14,
                647,
                28,
                TYPE,
                "#504A36",
                0.8,
            ),
            label(
                "Cuatro paredes · cinco estantes · treinta y dos libros por estante",
                109,
                1018,
                15,
                671,
                28,
                TYPE,
                "#544B37",
            ),
            label(
                "Dos lámparas. Un pozo central. Una escalera entre pisos.",
                109,
                1045,
                15,
                665,
                27,
                TYPE,
                "#544B37",
            ),
            label(
                "A book has 410 pages; each page, 40 lines and about 80 characters.",
                109,
                1073,
                12,
                675,
                24,
                TYPE,
                "#746344",
            ),
            label(
                "ITINERARIO / SEARCH FOR A CATALOGUE",
                893,
                938,
                12,
                610,
                33,
                TYPE,
                "#713D29",
                1.2,
            ),
            label(
                "La forma se repite. El sentido no se deja localizar.",
                893,
                988,
                16,
                626,
                29,
                TYPE,
                "#554D38",
            ),
            label("01", 967, 1035, 13, 75, 29, MONO, "#9D4D2D"),
            label("∞", 1491, 1034, 23, 45, 34, TYPE, "#9D4D2D"),
            label(
                "25 SÍMBOLOS / COMBINACIONES FINITAS / RECORRIDO SIN TÉRMINO",
                894,
                1080,
                8,
                629,
                18,
                LABEL,
                "#746344",
                1.1,
            ),
        ]
        ctx.render(
            box()
            .width(1600)
            .height(1100)
            .children(
                box().absolute().inset(0).fill(shader(BACKGROUND)),
                architecture,
                box()
                .absolute()
                .inset(0)
                .fill(
                    shader(ATMOSPHERE, {"seconds": 0.0}).bind("seconds", self.seconds)
                ),
                foreground,
                route_dot,
                *words,
            )
        )

    def update(self, elapsed, ctx: SketchContext):
        self.seconds.set(elapsed)
