"""Carved and plated: limestone lettering cut glyph by glyph into slate, brushed copper traces with plated vias, and a pulse of light running one trace — one Compose page on one quad, lit in a 3D scene."""

# TAGS: World/Scenes, Compose/Materials, Typography/Relief

import math
import random

from sigil import compose, material, media, motion, world
from sigil.draw import Pen
from sigil.geometry import mesh, path
from sigil.material import hexColor
from sigil.motion import ease
from sigil.sketch import CanvasSpecification, Scene2DContext, sketch

WIDTH, HEIGHT = 1280, 720  # the canvas
PAGE_WIDTH, PAGE_HEIGHT = 1024, 576  # the page the slab wears, in page pixels
SLAB_WIDTH, SLAB_HEIGHT = 480, 270  # the slab's size in the scene
GROUND = "#0c0f12"
LIMESTONE = "#ddd3bd"
COPPER = "#c47a48"
PLATING = "#d8b25a"
GLOW = "#ffb04a"
TRACE_WIDTH = 9
PULSE_PERIOD = 2.4  # seconds for the pulse to run its trace end to end
PULSE_LENGTH = 0.09  # the pulse's length, as a share of its trace


def grain_tile(size: int, cell: int, seed: int) -> media.Image:
    """A grey value noise that wraps at its own edges, two octaves of it:
    a lattice of seeded levels every `cell` pixels and every half of that,
    each eased between its corners. Painted once as pixels and repeated,
    it is a grain the page samples rather than evaluates at every pixel."""
    chance = random.Random(seed)
    octaves = []
    for step in (cell, cell // 2):
        count = size // step
        octaves.append(
            (
                step,
                count,
                [[chance.random() for _ in range(count)] for _ in range(count)],
            )
        )
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            level, weight = 0.0, 0.0
            for amplitude, (step, count, lattice) in zip((0.65, 0.35), octaves):
                column, across = divmod(x, step)
                row, down = divmod(y, step)
                u = across / step
                v = down / step
                u, v = u * u * (3 - 2 * u), v * v * (3 - 2 * v)
                left, right = column % count, (column + 1) % count
                top, bottom = row % count, (row + 1) % count
                upper = (
                    lattice[top][left] + (lattice[top][right] - lattice[top][left]) * u
                )
                lower = (
                    lattice[bottom][left]
                    + (lattice[bottom][right] - lattice[bottom][left]) * u
                )
                level += amplitude * (upper + (lower - upper) * v)
                weight += amplitude
            grey = round(255 * level / weight)
            pixels += bytes((grey, grey, grey, 255))
    return media.fromRgba(bytes(pixels), size, size)


def grain(seed: int) -> material.Material:
    """The grain tile as a material repeated across whatever it fills."""
    return material.image(
        grain_tile(128, 16, seed), material.ImageOptions(repeat=material.Repeat.Repeat)
    )


# The slab is dark slate with a fine grain, honed rather than polished.
SLATE = (
    material.from_("#24282c")
    .layer(grain(21), blend=material.BlendMode.Multiply, opacity=0.4)
    .surface(roughness=0.78)
)

# The lettering is pale limestone inlaid in the slate and cut into it:
# under PaintBox.Glyph each glyph is the unit the relief reads, so every
# letter is cut on its own shoulder, and a negative depth impresses it.
LIMESTONE_CUT = (
    material.from_(LIMESTONE)
    .layer(grain(4), blend=material.BlendMode.Multiply, opacity=0.25)
    .surface(roughness=0.92)
    .relief(shoulder=6, depth=-3.0)
)

# Copper, brushed along the unit it is painted on: a ribbon's unit runs
# along its stroke, so the brushing follows every bend of a trace.
COPPER_TRACE = (
    material.from_(COPPER).surface(metallic=1.0, roughness=0.24).brushed(strength=0.9)
)

# The vias are plated: a paler, yellower metal, brushed round each ring.
PLATED = (
    material.from_(PLATING).surface(metallic=1.0, roughness=0.3).brushed(strength=0.7)
)

# A via's hole goes through the slab: dark and dull.
DRILLED = material.from_("#08090a").surface(roughness=1.0)

# The pulse is the page's own light, well past white, so the bloom gathers
# round it and nothing else.
PULSE = material.from_(GLOW).surface(roughness=0.4, emission=GLOW, emissionStrength=9.0)

# The circuit, in page pixels: each trace a run of straight legs with
# forty-five degree bends, ending on a via at either end.
TRACES = (
    [(90, 250), (300, 250), (350, 300), (610, 300), (660, 250), (930, 250)],
    [(90, 330), (250, 330), (300, 380), (560, 380), (610, 430), (930, 430)],
    [(130, 500), (330, 500), (380, 450), (470, 450)],
    [(560, 520), (760, 520), (810, 470), (930, 470)],
    [(470, 450), (520, 410), (520, 380)],
)
PULSE_TRACE = 0  # the trace the pulse runs


def spine(points: list[tuple[float, float]]) -> compose.Shape:
    """A trace's centre line: straight legs through the points."""
    return compose.heldPath(path.through(points, smooth=path.Smooth.None_))


def trace(points: list[tuple[float, float]]) -> compose.Element:
    """A brushed copper ribbon along the trace, rounded at every bend."""
    ribbon = compose.brush.ribbon(TRACE_WIDTH, COPPER_TRACE)
    ribbon.join = path.Join.Round
    return compose.box().absolute().inset(0).shape(spine(points)).stroke(ribbon)


def via(centre: tuple[float, float]) -> compose.Element:
    """A plated via: a ring swept round a round box over its drilled hole."""
    ring = compose.brush.ribbon(7, PLATED)
    return (
        compose.box()
        .absolute()
        .centerAt(centre)
        .width(22)
        .height(22)
        .borderRadius(11)
        .fill(DRILLED)
        .stroke(ring)
    )


def pulse(points: list[tuple[float, float]]) -> compose.Element:
    """A short glowing ribbon riding the trace end to end, again and again.

    The window it fills is two live values the page reads at the frame's
    time, so the face is described once and only the window moves."""
    lead = motion.animate(
        from_=0.0,
        to=1.0 + PULSE_LENGTH,
        duration=PULSE_PERIOD,
        ease=ease.linear,
        loop=-1,
    )
    tail = motion.animate(
        from_=-PULSE_LENGTH,
        to=1.0,
        duration=PULSE_PERIOD,
        ease=ease.linear,
        loop=-1,
    )
    glow = compose.brush.ribbon(TRACE_WIDTH + 3, PULSE)
    glow.join = path.Join.Round
    return (
        compose.box()
        .absolute()
        .inset(0)
        .shape(spine(points))
        .stroke(compose.spans.range(tail, lead), glow)
    )


def slab_face() -> compose.Element:
    """The slab: a carved title over a small circuit of copper and plating."""
    title = (
        compose.box(
            compose.text("LINEWORK", size=112, color=LIMESTONE)
            .fontWeight(800)
            .letterSpacing(22)
            .ink(LIMESTONE_CUT, compose.PaintBox.Glyph)
        )
        .absolute()
        .rect(0, 40, PAGE_WIDTH, 140)
        .row()
        .justifyContent("center")
        .alignItems("center")
    )
    ends = {point for points in TRACES for point in (points[0], points[-1])}
    return (
        compose.box(
            title,
            *[trace(points) for points in TRACES],
            pulse(TRACES[PULSE_TRACE]),
            *[via(centre) for centre in sorted(ends)],
        )
        .width(PAGE_WIDTH)
        .height(PAGE_HEIGHT)
        .fill(SLATE)
    )


def raking(
    azimuth: float, elevation: float, distance: float
) -> tuple[float, float, float]:
    """A point in the slab's own space seen from its centre at an azimuth
    across its face and an elevation above it, both in degrees."""
    across, above = math.radians(azimuth), math.radians(elevation)
    return (
        distance * math.cos(above) * math.cos(across),
        distance * math.cos(above) * math.sin(across),
        distance * math.sin(above),
    )


# A dark studio: what the metal mirrors is mostly shadow, with one long
# soft box behind the camera where the slab's reflections point.
STUDIO = world.kit.environmentMap(
    zenith=(0.05, 0.055, 0.07),
    nadir=(0.015, 0.012, 0.01),
    softBoxes=[
        world.kit.SoftBox(centre=(0.0, 0.4), radiance=(7, 6.6, 6), size=(0.03, 0.014)),
        world.kit.SoftBox(centre=(0.9, 0.3), radiance=(3, 3.4, 4.2), size=(0.04, 0.05)),
    ],
)


@sketch(size=(WIDTH, HEIGHT), background=GROUND, capture_at=2.0)
class CarvedLinework:
    # A sheet that bakes a 3D frame draws it on the host's device, so the
    # host brings the device up before this sketch opens.
    needsDevice = True

    def setup(self, ctx: Scene2DContext) -> None:
        # Everything a frame shows is read at the bake's moment, so a
        # capture's pre-roll steps need not paint: the still is the frame
        # at its own time either way.
        ctx.canvas(
            CanvasSpecification(
                size=(WIDTH, HEIGHT),
                background=GROUND,
                captureSeconds=2.0,
                paintDiscardedFrames=False,
            )
        )
        # The face is described once and held; the pulse moves by its own
        # live window, read at each bake's moment.
        self.page = ctx.page((PAGE_WIDTH, PAGE_HEIGHT))
        self.face = slab_face()
        self.slab = mesh.quad(SLAB_WIDTH, SLAB_HEIGHT)
        # The key's rail: a low arc round the slab in its own space, so the
        # light rakes across the cuts and along the brushing in turn.
        self.key_rail = mesh.curve.Spline3(
            [raking(azimuth, 16, 420) for azimuth in (15, 50, 90, 130, 165)],
            type=mesh.curve.Spline3.Type.CatmullRom,
        )
        self.key_rail_length = self.key_rail.length()
        self.lens = mesh.camera.Camera()
        self.lens.eye = (0, 90, 520)
        self.lens.target = (0, -6, 0)
        self.lens.fovYDeg = 34

    def draw(self, pen: Pen, ctx: Scene2DContext) -> None:
        seconds = pen.millis() / 1000
        # The key rides its rail from one end to the other and back.
        swing = 0.5 - 0.5 * math.cos(seconds * 2 * math.pi / 8)
        panel_width, panel_height, panel_distance = 380.0, 90.0, 380.0
        model = world.group(
            world.rectLight(
                (0, -0.4, -1),
                width=panel_width,
                height=panel_height,
                color="#dfe8ff",
                # A panel's intensity is the radiance of its face, so the
                # strength it delivers grows with its area.
                intensity=1.0 * panel_distance**2 / (panel_width * panel_height),
            )
            .key("soft")
            .at((180, -40, panel_distance))
            .lookAt("slab"),
            world.hemisphereLight(
                color="#a7b6c8", groundColor="#2a241c", intensity=0.12
            ),
            world.group(
                world.mesh(self.slab).key("slab").fill(self.page.surface()),
                world.spotLight(
                    (0, 0, -1),
                    color="#fff1dc",
                    intensity=world.intensityAt(5.0, 420.0),
                    outerAngle=45,
                    castShadow=True,
                )
                .key("key")
                .along(self.key_rail, swing * self.key_rail_length)
                .lookAt("slab"),
            )
            .key("specimen")
            .rotateX(
                motion.animate(from_=-80, to=-16, duration=1.4, ease=ease.outCubic)
            ),
        ).key("scene")
        frame = (
            world.Frame(model)
            .environment(STUDIO, intensity=1.3)
            .pass_(self.page.render(self.face))
            .pass_(world.bloom(threshold=3.0, intensity=0.9, radius=30))
        )
        still = ctx.bakeScene3D(
            frame,
            (WIDTH, HEIGHT),
            camera=self.lens,
            background=hexColor(0x0C0F12),
            at=seconds,
        )
        if still is not None:
            pen.image(still, 0, 0)
