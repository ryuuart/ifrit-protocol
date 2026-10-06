"""Two braziers burning a generated flame flipbook, flowing coals and embers, baked on the device."""

# TAGS: World/Scenes, Materials/Lighting, Effects

import itertools
import math

from sigil import material, media, world
from sigil.draw import LEFT, TOP, Pen
from sigil.geometry import mesh
from sigil.material import Color
from sigil.material import surface as surfaces
from sigil.sketch import Scene2DContext, sketch

TAU = math.tau
# The loop: the flame's 64 frames at 32 a second, and every flicker a
# whole number of cycles inside it.
COLUMNS = 8
ROWS = 8
FRAMES = COLUMNS * ROWS
FRAMES_PER_SECOND = 32.0
LOOP_SECONDS = FRAMES / FRAMES_PER_SECOND
# One cell of the sheet, in pixels.
CELL = 64
# Where the bowls' rims stand, and how far apart the two braziers are.
RIM = 92.0
APART = 175.0
WIDTH, HEIGHT = 960, 600
MUTED = "#93a4aa"


def smooth(edge0: float, edge1: float, x: float) -> float:
    t = min(max((x - edge0) / (edge1 - edge0), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def byte(value: float) -> int:
    return round(min(max(value, 0.0), 1.0) * 255)


HEAT_STOPS = (
    (0.00, 0.00, 0.00, 0.00),
    (0.22, 0.45, 0.04, 0.00),
    (0.50, 0.98, 0.30, 0.03),
    (0.76, 1.00, 0.70, 0.22),
    (1.00, 1.00, 0.96, 0.82),
)


def heat_colour(heat: float) -> tuple[float, float, float]:
    """How hot a flame is at heat in 0..1, as a colour: dark red at the
    edge, through orange and yellow, to a pale core."""
    for previous, stop in itertools.pairwise(HEAT_STOPS):
        if heat <= stop[0]:
            t = (heat - previous[0]) / (stop[0] - previous[0])
            return (
                previous[1] + (stop[1] - previous[1]) * t,
                previous[2] + (stop[2] - previous[2]) * t,
                previous[3] + (stop[3] - previous[3]) * t,
            )
    return (1.0, 0.96, 0.82)


def flame_at(u: float, v: float, phase: float) -> float:
    """One pixel of the flame at u across and v down its cell, at phase of
    the loop: a tongue that narrows to a flickering tip, swaying and torn
    by ripples that rise with the loop. Every ripple turns a whole number
    of times a loop, so the last frame leads back into the first."""
    height = 1 - v
    turn = phase * TAU
    sway = (
        0.055 * math.sin(turn + height * 5.0) * height
        + 0.025 * math.sin(2 * turn + height * 13.0) * height
    )
    tear = (
        0.55 * math.sin(2 * turn - height * 10.0 + u * 6.0)
        + 0.30 * math.sin(3 * turn - height * 17.0 + u * 13.0 + 1.3)
        + 0.15 * math.sin(5 * turn - height * 29.0 - u * 7.0 + 2.1)
    )
    tip = 0.80 + 0.07 * math.sin(2 * turn + 0.7) + 0.05 * math.sin(3 * turn + 2.0)
    width = 0.27 * max(0.0, 1 - height / tip) ** 0.6 * smooth(0.0, 0.16, height)
    across = abs(u - 0.5 - sway) / max(width * (1 + 0.22 * tear), 1e-3)
    body = smooth(1.0, 0.25, across)
    rise = smooth(tip, tip * 0.5, height + 0.07 * tear)
    foot = smooth(0.0, 0.06, height)
    return body * rise * foot


def flame_sheet() -> material.Texture:
    """The flame sheet: 64 frames of one flame in an 8 x 8 grid, read row
    by row from the top left, on black."""
    width, height = COLUMNS * CELL, ROWS * CELL
    pixels = bytearray(width * height * 4)
    for frame in range(FRAMES):
        left = (frame % COLUMNS) * CELL
        top = (frame // COLUMNS) * CELL
        phase = frame / FRAMES
        for y in range(CELL):
            v = (y + 0.5) / CELL
            row = ((top + y) * width + left) * 4
            for x in range(CELL):
                strength = flame_at((x + 0.5) / CELL, v, phase)
                at = row + x * 4
                pixels[at + 3] = 255
                if strength <= 0:
                    continue
                # The core runs hotter low in the flame than at its tip.
                heat = strength * (0.8 + 0.2 * smooth(0.6, 0.1, 1 - v))
                red, green, blue = heat_colour(heat)
                shown = strength**0.85
                pixels[at] = byte(red * shown)
                pixels[at + 1] = byte(green * shown)
                pixels[at + 2] = byte(blue * shown)
    return material.Texture(media.fromRgba(bytes(pixels), width, height)).tile(
        material.Repeat.Pad
    )


def noise(x: float, y: float, period: int) -> float:
    """A smooth value noise that repeats every period cells."""

    def hashed(i: int, j: int) -> float:
        i %= period
        j %= period
        value = (i * 374761393 + j * 668265263) & 0xFFFFFFFF
        value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
        return ((value ^ (value >> 16)) & 0xFFFF) / 65535.0

    i, j = math.floor(x), math.floor(y)
    fx, fy = smooth(0, 1, x - i), smooth(0, 1, y - j)
    a, b = hashed(i, j), hashed(i + 1, j)
    c, d = hashed(i, j + 1), hashed(i + 1, j + 1)
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy


def packed_mask() -> material.Texture:
    """The packed mask: red a soft disc, green a two-octave noise that
    repeats across the image, blue a ring. It is data, read exactly."""
    size = 128
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            u, v = (x + 0.5) / size, (y + 0.5) / size
            radius = math.hypot(u - 0.5, v - 0.5) * 2
            grain = 0.65 * noise(u * 8, v * 8, 8) + 0.35 * noise(u * 16, v * 16, 16)
            pixels += bytes(
                (
                    byte(smooth(1.0, 0.2, radius)),
                    byte(smooth(0.35, 0.85, grain)),
                    byte(smooth(0.25, 0.0, abs(radius - 0.7))),
                    255,
                )
            )
    return (
        material.Texture(media.fromRgba(bytes(pixels), size, size))
        .colorSpace(media.ColorSpace.Data)
        .tile(material.Repeat.Repeat)
    )


def swirl() -> material.Texture:
    """The flow: a slow swirl about the middle of the image, drawn outward a
    little, red along u and green along v around one half."""
    size = 64
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            u = (x + 0.5) / size - 0.5
            v = (y + 0.5) / size - 0.5
            along = -v * 1.4 + u * 0.4
            down = u * 1.4 + v * 0.4
            pixels += bytes((byte(0.5 + 0.5 * along), byte(0.5 + 0.5 * down), 0, 255))
    return (
        material.Texture(media.fromRgba(bytes(pixels), size, size))
        .colorSpace(media.ColorSpace.Data)
        .tile(material.Repeat.Repeat)
    )


def flame(
    sheet: material.Texture, blend_frames: bool, start_frame: float
) -> material.Material:
    """A flame: the sheet as the light a card gives off, added over the
    scene, fading out where it dips into the bowl."""
    played = material.flipbook(
        sheet,
        material.FlipbookOptions(
            columns=COLUMNS,
            rows=ROWS,
            framesPerSecond=FRAMES_PER_SECOND,
            blendFrames=blend_frames,
            startFrame=start_frame,
        ),
    )
    return material.from_(Color(0, 0, 0, 1)).surface(
        metallic=1.0,
        roughness=1.0,
        emission=Color(1, 1, 1, 1),
        emissionStrength=3.2,
        emissionMap=played,
        blend=material.BlendMode.PlusLighter,
        depthFade=22.0,
    )


def ember(sheet: material.Texture) -> material.Material:
    """An ember: the flame sheet as a spark of light, each instance showing
    the cell its own "cell" scalar names."""
    played = material.flipbook(
        sheet,
        material.FlipbookOptions(
            columns=COLUMNS, rows=ROWS, framesPerSecond=0, frameAttribute="cell"
        ),
    )
    return material.from_(Color(0, 0, 0, 1)).surface(
        metallic=1.0,
        roughness=1.0,
        emission=Color(1.0, 0.78, 0.52, 1),
        emissionStrength=3.0,
        emissionMap=played,
        blend=material.BlendMode.PlusLighter,
    )


def scatter(n: int, salt: float) -> float:
    """A number in 0..1 that n and salt decide and nothing else."""
    x = math.sin(n * 12.9898 + salt * 78.233) * 43758.547
    return x - math.floor(x)


def rising_embers(seconds: float, salt: float) -> mesh.Instances:
    """The embers over one fire: sparks rising off the coals and drifting,
    each a pure function of the time and its own seed."""
    count, climb = 40, 250.0
    out = mesh.Instances()
    out.stamp = mesh.quad(10, 10)
    positions: list[tuple[float, float, float]] = []
    scales: list[tuple[float, float, float]] = []
    colors: list[tuple[float, float, float, float]] = []
    cells: list[float] = []
    for n in range(count):
        own = scatter(n, salt)
        life = LOOP_SECONDS if scatter(n, salt + 1) < 0.5 else LOOP_SECONDS / 2
        rise = seconds / life + own
        age = rise - math.floor(rise)
        bearing = scatter(n, salt + 2) * TAU
        reach = 10.0 + 26.0 * scatter(n, salt + 3)
        sway = 9.0 * math.sin(age * TAU * 1.5 + own * TAU) * age
        positions.append(
            (
                math.cos(bearing) * reach * (0.4 + age) + sway,
                RIM - 6 + age * climb,
                math.sin(bearing) * reach * (0.4 + age),
            )
        )
        size = 1.0 - 0.6 * age
        scales.append((size, size, size))
        # Brightest just off the coals, gone by the top of the rise.
        colors.append((1, 1, 1, smooth(0.0, 0.08, age) * (1.0 - age) ** 1.6))
        frame = own * FRAMES + seconds * FRAMES_PER_SECOND
        cells.append(frame - math.floor(frame / FRAMES) * FRAMES)
    out.positions = positions
    out.scales = scales
    out.colors = colors
    out.setAttribute("cell", cells)
    return out


def coals(mask: material.Texture, flow: material.Texture) -> material.Material:
    """The coals: dark where cold, glowing where the packed mask's noise,
    streaming slowly along the swirl, says they are hot."""
    grain = material.channel(material.image(mask), material.ColorChannel.Green)
    return material.from_(Color(0.07, 0.05, 0.045, 1)).surface(
        roughness=0.9,
        emission=Color(1.0, 0.36, 0.08, 1),
        emissionStrength=4.0,
        emissionMap=material.flowmap(
            grain, material.FlowmapOptions(flow=flow, strength=0.18, speed=0.5)
        ),
    )


def brazier(iron: material.Material) -> world.Group:
    """The bowl and its three legs, in dark iron."""
    bowl = mesh.revolve(
        [
            (0, RIM - 40),
            (34, RIM - 38),
            (58, RIM - 22),
            (70, RIM - 2),
            (74, RIM),
            (66, RIM + 1),
            (56, RIM - 16),
            (32, RIM - 30),
            (0, RIM - 32),
        ],
        segments=64,
    )
    parts = [world.mesh(bowl).key("bowl").fill(iron)]
    for leg in range(3):
        turn = leg * TAU / 3 + 0.5
        parts.append(
            world.mesh(mesh.box((-3.5, 0, -3.5), (3.5, RIM - 34, 3.5)))
            .key(f"leg-{leg}")
            .at((34 * math.cos(turn), 0, 34 * math.sin(turn)))
            .fill(iron)
        )
    return world.group(parts).key("brazier")


def flicker(seconds: float, offset: float) -> float:
    """How hard the fire under a light burns: a breath and two quicker
    flutters, each a whole number of cycles in the loop."""
    t = seconds / LOOP_SECONDS * TAU
    return (
        1.0
        + 0.16 * math.sin(3 * t + offset)
        + 0.09 * math.sin(7 * t + 1.7 + offset)
        + 0.05 * math.sin(13 * t + 0.4 + offset * 2)
    )


@sketch(size=(WIDTH, HEIGHT), background="#030304", capture_at=0.6)
class PythonFlipbookFire:
    def setup(self, ctx: Scene2DContext) -> None:
        self.sheet = flame_sheet()
        self.iron = surfaces.program(
            surfaces.SurfaceParameters(
                baseColor=Color(0.20, 0.18, 0.16, 1), metallic=1.0, roughness=0.42
            )
        )
        self.floor = surfaces.program(
            surfaces.SurfaceParameters(
                baseColor=Color(0.16, 0.14, 0.13, 1), roughness=0.85
            )
        )
        self.embers = coals(packed_mask(), swirl())
        self.sparks = ember(self.sheet)
        self.coal_bed = mesh.revolve(
            [(0, 0), (52, 0), (50, 4), (34, 12), (0, 16)], segments=64
        )
        self.lens = mesh.camera.Camera()
        self.lens.eye = (0, 175, 560)
        self.lens.target = (0, 120, 0)
        self.lens.fovYDeg = 40

    def fire(
        self, key: str, x: float, blend_frames: bool, start_frame: float, seconds: float
    ) -> world.Group:
        """One brazier at x: its iron, its coals, a flame card turned to the
        camera about the up axis, embers turned to the camera, and a light
        above it that burns with the flame."""
        burning = flicker(seconds, start_frame * 0.37)
        return (
            world.group(
                brazier(self.iron),
                world.mesh(self.coal_bed)
                .key("coals")
                .at((0, RIM - 20, 0))
                .fill(self.embers),
                world.mesh(mesh.quad(150, 230))
                .key("flame")
                .at((0, RIM + 88, 0))
                .facing(world.Facing.CameraUpright)
                .fill(flame(self.sheet, blend_frames, start_frame)),
                world.mesh(rising_embers(seconds, start_frame + x * 0.01))
                .key("embers")
                .facing(world.Facing.Camera)
                .fill(self.sparks),
                world.pointLight(
                    color=Color(1.0, 0.62, 0.30, 1),
                    intensity=world.intensityAt(1.6 * burning, 220.0),
                )
                .key("light")
                .at((0, RIM + 70, 0)),
            )
            .key(key)
            .at((x, 0, 0))
        )

    def draw(self, pen: Pen, ctx: Scene2DContext) -> None:
        seconds = pen.millis() / 1000
        model = world.group(
            world.camera(self.lens, exposure=1.4, toneCurve=world.ToneCurve.Neutral),
            world.hemisphereLight(
                color=Color(0.10, 0.12, 0.20, 1),
                groundColor=Color(0.05, 0.03, 0.02, 1),
                intensity=0.25,
            ),
            world.mesh(mesh.quad(1600, 1600))
            .key("floor")
            .rotateX(-90)
            .fill(self.floor),
            self.fire("blended", -APART, True, 0, seconds),
            self.fire("stepped", APART, False, 23, seconds),
        ).key("set")
        frame = world.Frame(model).pass_(
            world.bloom(threshold=1.0, intensity=0.7, radius=48)
        )
        # A scene3d frame is drawn on a device and nowhere else: the bake
        # answers None where the host brought none up, and the sheet says so.
        still = ctx.bakeScene3D(
            frame,
            (WIDTH, HEIGHT),
            camera=self.lens,
            background=Color(0.012, 0.012, 0.016, 1),
            at=seconds,
        )
        if still is not None:
            pen.image(still, 0, 0)
        else:
            pen.background("#030304")
            pen.noStroke()
            pen.fill(MUTED)
            pen.textAlign(LEFT, TOP)
            pen.textSize(15)
            pen.text("This sheet bakes its set on a device, and none is here.", 48, 280)
