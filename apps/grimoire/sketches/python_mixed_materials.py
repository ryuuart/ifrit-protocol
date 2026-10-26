"""A specimen sheet of mixed materials: one Compose page, many lit materials
side by side, worn by one quad.

Eight swatches (emissive, velvet, thin film, clear glass, glazed tile,
satin, matte, frosted glass) under a title cut into slate, with brushed
copper traces between them ending on plated vias, and a pulse of light
running the bus in a live window while the face is described once.

EDIT THESE FIRST: the materials by name, PULSE_PERIOD, PULSE_LENGTH.
"""

# TAGS: World/Scenes, Compose/Materials, Material/Surfaces, Typography/Relief

import math

from sigil import compose, material, motion, world
from sigil.geometry import mesh, path
from sigil.material import surface as surfaces
from sigil.motion import ease
from sigil.sketch import Scene3DContext, sketch

WIDTH, HEIGHT = 1280, 720  # the canvas
PAGE_WIDTH, PAGE_HEIGHT = 1024, 576  # the page the sheet wears, in page pixels
SHEET_WIDTH, SHEET_HEIGHT = 480, 270  # the sheet's size in the scene
GROUND = "#0d1014"
LIMESTONE = "#ddd5c2"
LABEL = "#b9b3a6"
COPPER = "#c27a4c"
PLATING = "#d8b25a"
GLOW = "#ffb347"
PULSE_PERIOD = 2.4  # seconds for the pulse to run the bus end to end
PULSE_LENGTH = 0.09  # the pulse's length, as a share of the bus

# The grid: four columns of swatches over two rows, with a gutter between
# the rows wide enough for the copper bus to run through.
COLUMN_LEFT = (40, 283, 526, 769)
ROW_TOP = (146, 382)
SWATCH_WIDTH, SWATCH_HEIGHT = 215, 160
GUTTER = 355  # the horizontal gutter's centre line
BACKING_SIZE = (256, 394)  # the page behind the glass, in its own pixels
MARK_CENTRE = (COLUMN_LEFT[0] + SWATCH_WIDTH / 2, ROW_TOP[0] + SWATCH_HEIGHT / 2)
MARK_RADIUS = 40

# The sheet is dark slate with a fine grain, rough and dull.
SLATE = (
    material.from_("#26292d")
    .layer(
        material.noise(0.09, octaves=3, seed=11, grain=True),
        blend=material.BlendMode.Multiply,
        opacity=0.3,
    )
    .surface(roughness=0.8)
)

# The title is pale limestone inlaid in the slate and cut: under
# PaintBox.Glyph each glyph is the unit the relief reads, so every letter is
# cut on its own shoulder.
LIMESTONE_STONE = (
    material.from_(LIMESTONE)
    .layer(
        material.noise(0.06, octaves=3, seed=4, grain=True),
        blend=material.BlendMode.Multiply,
        opacity=0.2,
    )
    .surface(roughness=0.9)
)
CARVED = LIMESTONE_STONE.relief(shoulder=7, depth=-3.0)

# Copper, brushed along the unit it is painted on.
COPPER_TRACE = (
    material.from_(COPPER).surface(metallic=1.0, roughness=0.22).brushed(strength=0.9)
)

# The vias are plated: a paler, yellower metal, brushed round each ring,
# over a hole drilled dark and dull through the slate.
PLATED = (
    material.from_(PLATING).surface(metallic=1.0, roughness=0.3).brushed(strength=0.7)
)
DRILLED = material.from_("#08090a").surface(roughness=1.0)

# The pulse is the page's own light, well past white, so the bloom gathers
# round it as it does round the mark.
PULSE = material.from_(GLOW).surface(roughness=0.4, emission=GLOW, emissionStrength=9.0)

# A glazed tile: a teal body under a glossy clear coat. A turbulent noise
# falls to nothing along its creases; stretching the bottom of its range to
# the whole of it leaves those creases as fine dark lines, the crazing. The
# lines darken the body a little and the coat's own normal dips into them.
CRAZING = material.remap(
    material.noise(0.05, octaves=2, seed=7, turbulence=True),
    from_=(0.0, 0.04),
    to=(0.0, 1.0),
)
GLAZE = (
    material.from_("#2c6f73")
    .layer(CRAZING, blend=material.BlendMode.Multiply, opacity=0.35)
    .surface(
        roughness=0.5,
        clearcoat=1.0,
        clearcoatRoughness=0.06,
        clearcoatNormal=surfaces.normalFromHeight(
            CRAZING, surfaces.HeightNormalOptions(depth=0.08)
        ),
    )
)

# Velvet: a deep red that answers light with a soft pink sheen; its pile is
# crushed, so the sheen catches where the nap leans toward the light.
VELVET = material.from_("#4a0c1a").surface(
    roughness=1.0,
    sheenColor="#ffd6e0",
    sheenRoughness=0.5,
    normal=surfaces.normalFromHeight(
        material.noise(0.03, octaves=3, seed=5, grain=True),
        surfaces.HeightNormalOptions(depth=16.0),
    ),
)

# A thin film over pale steel: its thickness varies across the chip between
# the minimum and the full thickness, so the interference colour drifts with it.
FILM = material.from_("#a4a9b2").surface(
    metallic=1.0,
    roughness=0.1,
    iridescence=1.0,
    iridescenceThickness=820.0,
    iridescenceThicknessMinimum=240.0,
    iridescenceThicknessMap=material.noise(0.02, octaves=2, seed=3, grain=True),
)

# Satin and matte, for contrast: the same blue, one smooth and one chalky.
SATIN = material.from_("#2d4f8a").surface(roughness=0.22)
MATTE = material.from_("#2d4f8a").surface(roughness=1.0)

# The emissive mark and the basalt cell it sits in. The mark's emission is
# well past white: the page's emissive map keeps it, so the mark is the
# brightest thing in the frame and the bloom gathers round it alone.
BASALT = material.from_("#121417").surface(roughness=0.3)
MARK = material.from_(GLOW).surface(roughness=0.4, emission=GLOW, emissionStrength=7.0)

# Glass: the sheet transmits here, bent by its index through the thickness
# of the volume, onto whatever stands behind the sheet. A page carries one
# volume, so both panes share the index. The clear pane's face is gently
# uneven, so what is behind it wavers; the frosted one is rough, so it blurs.
CLEAR_GLASS = material.from_("#e8f2f4").surface(
    roughness=0.02,
    transmission=1.0,
    ior=1.45,
    thickness=12.0,
    normal=surfaces.normalFromHeight(
        material.noise(0.02, octaves=2, seed=9, grain=True),
        surfaces.HeightNormalOptions(depth=14.0),
    ),
)
FROSTED_GLASS = material.from_("#e8f2f4").surface(
    roughness=0.3, transmission=1.0, ior=1.45, thickness=12.0
)


def backing_face() -> compose.Element:
    """What stands behind the glass: bold bars of their own light, unlit."""
    colours = ("#ffb000", "#00b4d8", "#ff3d6e", "#7cff6b")
    bars = [
        compose.box()
        .width(32)
        .height(BACKING_SIZE[1])
        .fill(material.from_(colours[index % 4]).surface(unlit=True))
        for index in range(BACKING_SIZE[0] // 32)
    ]
    return (
        compose.box(*bars)
        .row()
        .width(BACKING_SIZE[0])
        .height(BACKING_SIZE[1])
        .fill(material.from_("#101010").surface(unlit=True))
    )


def caption(label: str, column: int, row: int) -> compose.Element:
    """A flat line of type under a swatch: it states no surface, so it is ink the light falls on."""
    return (
        compose.text(label, size=17, color=LABEL)
        .fontWeight(600)
        .letterSpacing(2)
        .absolute()
        .rect(COLUMN_LEFT[column], ROW_TOP[row] + SWATCH_HEIGHT + 6, SWATCH_WIDTH, 22)
    )


def swatch(fill: material.Material, column: int, row: int) -> compose.Element:
    """One big cell of the grid, filled with one material."""
    return (
        compose.box()
        .absolute()
        .rect(COLUMN_LEFT[column], ROW_TOP[row], SWATCH_WIDTH, SWATCH_HEIGHT)
        .borderRadius(6)
        .fill(fill)
    )


def trace(points: list[tuple[float, float]], width: float) -> compose.Element:
    """A copper ribbon swept along a smooth spine through the points, a little
    wider where it meets its vias at either end."""
    spine = compose.heldPath(path.curveThrough(points))
    swell = [(0.0, width * 1.25), (0.04, width), (0.96, width), (1.0, width * 1.25)]
    return (
        compose.box()
        .absolute()
        .inset(0)
        .shape(spine)
        .stroke(compose.brush.ribbon(swell, COPPER_TRACE))
    )


def bus_line(offset: float) -> list[tuple[float, float]]:
    """The points one bus trace runs through, an offset from the gutter's centre line."""
    left, right = 24, PAGE_WIDTH - 24
    return [
        (left, GUTTER + offset - 3),
        (300, GUTTER + offset + 4),
        (540, GUTTER + offset - 5),
        (right, GUTTER + offset + 2),
    ]


def bus(offsets: tuple[float, ...], width: float) -> list[compose.Element]:
    """Parallel copper traces along the gutter, one per offset from its centre line."""
    return [trace(bus_line(offset), width) for offset in offsets]


def via(centre: tuple[float, float]) -> compose.Element:
    """A plated via a trace ends on: a ring swept round a round box over its
    drilled hole."""
    return (
        compose.box()
        .absolute()
        .centerAt(centre)
        .width(24)
        .height(24)
        .borderRadius(12)
        .fill(DRILLED)
        .stroke(compose.brush.ribbon(7, PLATED))
    )


def pulse(points: list[tuple[float, float]], width: float) -> compose.Element:
    """A short glowing ribbon riding a trace end to end, again and again.

    The stretch it fills is a window of two live values the page reads at
    the frame's moment, so the face is described once and only the window
    moves."""
    lead = motion.animate(
        from_=0.0, to=1.0 + PULSE_LENGTH, duration=PULSE_PERIOD, ease=ease.linear, loop=-1
    )
    tail = motion.animate(
        from_=-PULSE_LENGTH, to=1.0, duration=PULSE_PERIOD, ease=ease.linear, loop=-1
    )
    return (
        compose.box()
        .absolute()
        .inset(0)
        .shape(compose.heldPath(path.curveThrough(points)))
        .stroke(compose.spans.range(tail, lead), compose.brush.ribbon(width + 3, PULSE))
    )


def sheet_face() -> compose.Element:
    """The specimen sheet: a carved title, eight swatches, copper traces between them."""
    title = (
        compose.box(
            compose.text("MATERIA", size=104, color=LIMESTONE)
            .fontWeight(800)
            .letterSpacing(26)
            .ink(CARVED, compose.PaintBox.Glyph)
        )
        .absolute()
        .rect(0, 8, PAGE_WIDTH, 130)
        .row()
        .justifyContent("center")
        .alignItems("center")
    )
    mark = (
        compose.box()
        .absolute()
        .centerAt(MARK_CENTRE)
        .width(2 * MARK_RADIUS)
        .height(2 * MARK_RADIUS)
        .borderRadius(MARK_RADIUS)
        .fill(MARK)
    )
    traces = [
        *bus((-13, 0, 13), 10),
        trace([(269, 156), (264, 250), (271, GUTTER - 13)], 10),
        trace([(512, GUTTER + 13), (507, 450), (514, 534)], 10),
        pulse(bus_line(0), 10),
        via((269, 156)),
        via((514, 534)),
    ]
    return (
        compose.box(
            title,
            *traces,
            swatch(BASALT, 0, 0),
            mark,
            swatch(VELVET, 1, 0),
            swatch(FILM, 2, 0),
            swatch(CLEAR_GLASS, 3, 0),
            swatch(GLAZE, 0, 1),
            swatch(SATIN, 1, 1),
            swatch(MATTE, 2, 1),
            swatch(FROSTED_GLASS, 3, 1),
            caption("EMISSIVE", 0, 0),
            caption("VELVET", 1, 0),
            caption("THIN FILM", 2, 0),
            caption("CLEAR GLASS", 3, 0),
            caption("GLAZED TILE", 0, 1),
            caption("SATIN", 1, 1),
            caption("MATTE", 2, 1),
            caption("FROSTED GLASS", 3, 1),
        )
        .width(PAGE_WIDTH)
        .height(PAGE_HEIGHT)
        .fill(SLATE)
    )


def on_sheet(x: float, y: float, lift: float) -> tuple[float, float, float]:
    """A page pixel as a point in the sheet's own space, lifted off its face."""
    return (
        (x / PAGE_WIDTH - 0.5) * SHEET_WIDTH,
        (0.5 - y / PAGE_HEIGHT) * SHEET_HEIGHT,
        lift,
    )


def raking(
    azimuth: float, elevation: float, distance: float
) -> tuple[float, float, float]:
    """A point in the sheet's own space seen from its centre at an azimuth
    across its face and an elevation above it, both in degrees."""
    across, above = math.radians(azimuth), math.radians(elevation)
    return (
        distance * math.cos(above) * math.cos(across),
        distance * math.cos(above) * math.sin(across),
        distance * math.sin(above),
    )


# A dark studio: what a metal mirrors is mostly shadow, with one long soft
# box behind the camera where the sheet's reflections point and a smaller
# one off to the side.
STUDIO = world.kit.environmentMap(
    zenith=(0.05, 0.055, 0.07),
    nadir=(0.015, 0.012, 0.01),
    softBoxes=[
        world.kit.SoftBox(centre=(0.0, 0.4), radiance=(7, 6.6, 6), size=(0.025, 0.012)),
        world.kit.SoftBox(centre=(0.9, 0.3), radiance=(3, 3.4, 4.2), size=(0.04, 0.05)),
    ],
)


@sketch(size=(WIDTH, HEIGHT), background=GROUND, captureAt=2.0)
class MixedMaterials:
    def setup(self, ctx: Scene3DContext) -> None:
        # The faces are built once: each page paints the first time and
        # keeps it. A face that changed every frame would repaint every map
        # of its page every frame, so everything that moves is in the scene,
        # the mark's pulse included: the mark holds its stated strength and
        # the bloom round it breathes.
        self.page = ctx.page((PAGE_WIDTH, PAGE_HEIGHT))
        self.face = sheet_face()
        self.backing_page = ctx.page(BACKING_SIZE)
        self.backing_face = backing_face()
        self.backing = mesh.quad(130, 200)
        self.sheet = mesh.quad(SHEET_WIDTH, SHEET_HEIGHT)
        # The key's rail: an arc round the sheet in the sheet's own space,
        # 18 degrees above its face, from its right edge over the top to its
        # left edge.
        self.key_rail = mesh.curve.Spline3(
            [raking(azimuth, 18, 420) for azimuth in (20, 55, 90, 125, 160)],
            type=mesh.curve.Spline3.Type.CatmullRom,
        )
        self.key_rail_length = self.key_rail.length()
        ctx.camera(
            mesh.camera.Camera(
                eye=(0, 90, 520), target=(0, -6, 0), verticalFieldOfViewDegrees=34
            )
        )

    def describe(self, seconds: float) -> world.Frame:
        pulse = 0.5 + 0.5 * math.sin(seconds * 2 * math.pi / 1.6)
        # The key rides its rail from the right end to the left and back.
        swing = 0.5 - 0.5 * math.sin(seconds * 2 * math.pi / 7)
        panel_width, panel_height, panel_distance = 400.0, 90.0, 380.0
        drift = math.sin(seconds * 2 * math.pi / 7 + 0.9)
        model = world.group(
            world.rectLight(
                (0, -0.4, -1),
                width=panel_width,
                height=panel_height,
                color="#dfe8ff",
                # A panel's intensity is the radiance of its face, so the
                # strength it delivers grows with its area.
                intensity=1.2 * panel_distance**2 / (panel_width * panel_height),
            )
            .key("soft")
            .at((-260 * drift, -60, panel_distance))
            .lookAt("sheet"),
            world.hemisphereLight(
                color="#a7b6c8", groundColor="#2a241c", intensity=0.15
            ),
            world.group(
                world.mesh(self.sheet).key("sheet").fill(self.page.surface()),
                world.spotLight(
                    (0, 0, -1),
                    color="#fff1dc",
                    intensity=world.intensityAt(5.0, 420.0),
                    outerAngle=45,
                    castShadow=True,
                )
                .key("key")
                .along(self.key_rail, swing * self.key_rail_length)
                .lookAt("sheet"),
                # Behind the sheet, behind the glass column: a second page
                # of bold bars for the glass to bend.
                world.mesh(self.backing)
                .key("backing")
                .at(on_sheet(COLUMN_LEFT[3] + SWATCH_WIDTH / 2, GUTTER - 12, -12))
                .fill(self.backing_page.surface()),
            )
            .key("specimen")
            .rotateX(
                motion.animate(from_=-80, to=-14, duration=1.4, ease=ease.outCubic)
            ),
        ).key("scene")
        return (
            world.Frame(model)
            .environment(STUDIO, intensity=1.4)
            .pass_(self.backing_page.render(self.backing_face))
            .pass_(self.page.render(self.face))
            .pass_(world.bloom(threshold=1.5, intensity=0.3 + 1.1 * pulse, radius=36))
        )
