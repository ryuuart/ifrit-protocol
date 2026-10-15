# Many materials on one page, in Python

A specimen sheet stands in a lit scene: a limestone title cut into dark
slate, eight big swatches under it — a glowing mark, velvet, a thin film,
clear glass, a crackled glaze, satin, matte and frosted glass — and a bus
of brushed copper traces running in the gutter between the rows. The
whole sheet is one Compose page worn by one flat quad. Every swatch is an
ordinary fill whose material says what it is made of; the page paints the
maps those materials name and World lights them, so each swatch answers
the same lights in its own way. This page builds the sheet up material by
material, every step run through Grimoire as written, and every block is
the sketch's own code. The whole file is
`apps/grimoire/sketches/python_mixed_materials.py`; open it with
`Grimoire python_mixed_materials.py`, or take a still with `--frame
sheet.png --gpu --at 2.0`.

It follows [Compose elements as the material of a 3D body](guide:compose-driven-materials-python),
which introduces the page, the pass and the quad; read that first if
`ctx.page` is new to you.

## Step 1 — the slate, and a limestone title cut into it

The sheet is dark slate with a fine grain, rough and dull. The title is
pale limestone, and the relief is stated on the limestone itself,
impressed rather than raised.

```python
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
GLOW = "#ffb347"

# The grid: four columns of swatches over two rows, with a gutter between
# the rows wide enough for the copper bus to run through.
COLUMN_LEFT = (40, 283, 526, 769)
ROW_TOP = (146, 382)
SWATCH_WIDTH, SWATCH_HEIGHT = 215, 160
GUTTER = 355  # the horizontal gutter's centre line
BACKING_SIZE = (256, 394)  # the page behind the glass, in its own pixels
MARK_CENTRE = (COLUMN_LEFT[0] + SWATCH_WIDTH / 2, ROW_TOP[0] + SWATCH_HEIGHT / 2)
MARK_RADIUS = 40
```

```python
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
```

The title is a line of type inked with that material, one unit per
glyph:

```python
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
```

`.relief(...)` reads the unit the material is painted into, and
`compose.PaintBox.Glyph` makes that unit each glyph: every letter is cut
on its own shoulder. In the stills the pale letters are the first thing
on the sheet, each with a dark wall along one side of its cut and a lit
wall along the other. On limestone the same cut is stone on stone and
barely shows; it is the pale inlay on dark slate, and the low key in
step 9, that make it read at a glance.

**What you used.** `material.from_`, `.layer(..., blend=, opacity=)`,
`material.noise(..., grain=True)` (a grey grain; without `grain` the
noise is coloured), `.surface(roughness=)`, `Material.relief(shoulder=,
depth=)` with a negative depth to impress, and `ink(material,
compose.PaintBox.Glyph)`.

## Step 2 — the grid of swatches

The swatches are the subject, so they are big: four columns of 215 by
160 over two rows, placed absolutely like cells on a specimen sheet.
Each is one box with one fill; each caption is plain type.

```python
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
```

A caption states no surface, so it is a stock surface: its colour in the
base colour map, taking the scene's light like the slate around it.

**What you used.** `.absolute()`, `.rect(x, y, width, height)`,
`.borderRadius`, `.fill(material)`.

## Step 3 — satin and matte, the control pair

The two swatches that differ in one number. The same blue, one smooth
and one chalky:

```python
# Satin and matte, for contrast: the same blue, one smooth and one chalky.
SATIN = material.from_("#2d4f8a").surface(roughness=0.22)
MATTE = material.from_("#2d4f8a").surface(roughness=1.0)
```

At 2 seconds the soft panel of step 9 lies across the satin and it reads
pale and silvery, while the matte beside it stays a deep, even blue; at
5 seconds the panel's edge shows in the satin as a lighter band across
its top.

## Step 4 — velvet

Velvet is a dark base with a sheen: a soft reflection that cloth shows
where light grazes it. On a flat page the sheen needs something to graze,
so the pile is crushed — a normal from a grey noise's height.

```python
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
```

Under the raking key the swatch is a dusty rose over a crushed pile;
with `sheenColor` set to black the same swatch is a plain deep red.

**What you used.** `sheenColor`, `sheenRoughness`, `normal=`, and
`surfaces.normalFromHeight(height, surfaces.HeightNormalOptions(depth=))`.

## Step 5 — a thin film

```python
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
```

The chip shows oil-slick bands of magenta, green and blue. Over a dark
base the same film is muted; the bright metal under it is what gives the
colours something to reflect.

**What you used.** `iridescence`, `iridescenceThickness`,
`iridescenceThicknessMinimum`, `iridescenceThicknessMap`.

## Step 6 — a crackled glaze

```python
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
```

At 2 seconds the coat carries the panel's reflection with a crisp
vertical edge across the left of the tile, the cracks breaking it; the
rest of the tile is the darker teal body. Keep the coat normal's `depth`
small: the remapped lines are steep, and at a depth of 1.5 the coat is so
broken that no reflection survives in it.

**What you used.** `material.remap(source, from_=, to=)`,
`noise(..., turbulence=True)`, `clearcoat`, `clearcoatRoughness`,
`clearcoatNormal`.

## Step 7 — glass, and something behind it to see

A swatch that states `transmission` is glass on the body that wears the
page: there the sheet lets through whatever stands behind it, bent by
the index through the thickness of the volume, and everywhere else it is
the sheet. A page carries one volume — the index of the first
transmitting material it paints is the whole page's — so both panes share
`ior=1.45`. The clear pane's face is given a gentle unevenness so what
is behind it visibly bends; the frosted one is rough.

```python
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
```

Glass needs something behind it. A second page of bold bars, each its own
light, stands on a small quad just behind the glass column:

```python
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
```

In the stills the clear pane shows the bars sharp, with their edges
wavering where the pane's face is uneven; the frosted pane shows the same
bars as soft blurred stripes. Around the panes the slate hides the
backing completely.

**What you used.** `transmission`, `ior`, `thickness`, `roughness` on a
transmitting surface, `surface(unlit=True)` for content that is its own
light, and a second `ctx.page`.

## Step 8 — a mark that glows past white, and a copper bus

The mark is a disc whose emission is well past white, in a basalt cell.
The page's emissive map keeps that: its white stands for the brightest
emission the page states, so the mark reaches the bloom's threshold on
its own and nothing else on the sheet does.

```python
# The emissive mark and the basalt cell it sits in. The mark's emission is
# well past white: the page's emissive map keeps it, so the mark is the
# brightest thing in the frame and the bloom gathers round it alone.
BASALT = material.from_("#121417").surface(roughness=0.3)
MARK = material.from_(GLOW).surface(roughness=0.4, emission=GLOW, emissionStrength=7.0)
```

```python
    mark = (
        compose.box()
        .absolute()
        .centerAt(MARK_CENTRE)
        .width(2 * MARK_RADIUS)
        .height(2 * MARK_RADIUS)
        .borderRadius(MARK_RADIUS)
        .fill(MARK)
    )
```

The traces are ribbons: `compose.heldPath` holds a smooth spine as a box's
shape, and `compose.brush.ribbon` sweeps a band of a width law along it,
painted in the full material. The width law here is a list of (along,
width) stops, a quarter wider over the first and last few hundredths of
the spine where a trace meets its pad. The copper is brushed along the
unit it is painted into, which for a ribbon is the spine's own tangent.
Three ribbons side by side make a bus along the gutter.

```python
# Copper, brushed along the unit it is painted on.
COPPER_TRACE = (
    material.from_(COPPER).surface(metallic=1.0, roughness=0.22).brushed(strength=0.9)
)
```

```python
def trace(points: list[tuple[float, float]], width: float) -> compose.Element:
    """A copper ribbon swept along a smooth spine through the points, a little
    wider where it meets its pads at either end."""
    spine = compose.heldPath(path.curveThrough(points))
    swell = [(0.0, width * 1.25), (0.04, width), (0.96, width), (1.0, width * 1.25)]
    return (
        compose.box()
        .absolute()
        .inset(0)
        .shape(spine)
        .stroke(compose.brush.ribbon(swell, COPPER_TRACE))
    )


def bus(offsets: tuple[float, ...], width: float) -> list[compose.Element]:
    """Parallel copper traces along the gutter, one per offset from its centre line."""
    left, right = 24, PAGE_WIDTH - 24
    return [
        trace(
            [
                (left, GUTTER + offset - 3),
                (300, GUTTER + offset + 4),
                (540, GUTTER + offset - 5),
                (right, GUTTER + offset + 2),
            ],
            width,
        )
        for offset in offsets
    ]


def pad(x: float, y: float) -> compose.Element:
    """A round copper pad a trace ends on."""
    return (
        compose.box()
        .absolute()
        .rect(x - 12, y - 12, 24, 24)
        .borderRadius(12)
        .fill(COPPER_TRACE)
    )
```

The page itself is every piece in one box filled with the slate:

```python
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
        pad(269, 156),
        pad(514, 534),
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
```

**What you used.** `emission`, `emissionStrength` past one, `.centerAt`,
`path.curveThrough`, `compose.heldPath`, `compose.brush.ribbon` with a
list of width stops, `.stroke(ribbon)`, and
`Material.brushed(strength=, rotation=)`.

## Step 9 — the scene: a dark studio, a raking key, a soft panel, a pulse

A metal is what it mirrors. Under a bright, even environment flat copper
mirrors the same light everywhere and reads as an orange band; in a dark
studio with one small soft box where the sheet's reflections point, the
bus is bright copper only where that box lands and dark copper
elsewhere.

```python
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
```

The sheet is a scene3d sketch: `describe(seconds)` returns a
`world.Frame`, which the host draws on its device, and `setup` is handed a
`Scene3DContext` to ask for the pages and the lens. The faces are built
once in `setup`, so each page paints once and keeps what it painted.
Everything that moves is in the scene.

```python
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
        ctx.camera(mesh.camera.Camera(eye=(0, 90, 520), target=(0, -6, 0), fovYDeg=34))

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
```

**The key rides a rail.** `setup` builds the rail once: a Catmull-Rom
`mesh.curve.Spline3` through five points `raking` places in the sheet's
own space, 18 degrees above the face, from its right edge over the top to
its left. The key is a child of the specimen group, so the rail tilts in
with the sheet, and `.along(rail, distance)` stands it at a distance the
clock writes, swinging from end to end; `.lookAt` keeps it aimed at the
sheet. Light that low throws the cut of every letter into a lit wall and a
dark one.

**The panel.** A rect light's intensity is the radiance of its face, so a
larger panel delivers more at the same intensity: here the strength
wanted at the sheet is scaled by the distance squared over the area. Its
reflection lies across the glaze and the satin at 2 seconds.

**The copper.** At 2 seconds the bus is bright along most of its length
and darkens to brown copper toward its right end. The brushing decides
where the soft box's reflection lands along the ribbons: at 0.5 seconds,
with the sheet still tilting, the brushed bus is bright at its left and
dark at its right, and with `strength=0.0` the same bus is the other way
round; at 5 seconds the brushed highlight runs further along the bus
than the unbrushed one.

**The pulse is the bloom's.** The mark holds its stated strength and the
bloom gathers round it; the bloom's intensity breathes from the clock. At
2 seconds the mark has a soft amber halo; at 2.8 seconds it is a crisp
disc with almost none. The entrance is the specimen group's `rotateX`
tween, mid-tilt at 0.5 seconds and landed by 2.

**What you used.** `world.kit.environmentMap` with `world.kit.SoftBox`,
`world.spotLight`, `world.rectLight`, `mesh.curve.Spline3`,
`.along(rail, distance)`, `.at`, `.lookAt`, a nested
`world.group` so the key and the backing ride with the sheet, two pages'
passes on one frame, `world.bloom`, and `motion.animate` on a group's
lane.

## Run it, at a moment

```sh
Grimoire python_mixed_materials.py                                      # live
Grimoire python_mixed_materials.py --frame sheet.png --gpu --at 0.5     # tilting in
Grimoire python_mixed_materials.py --frame sheet.png --gpu --at 2.0     # landed
Grimoire python_mixed_materials.py --frame sheet.png --gpu --at 5.0     # the key from the left
```

## What to change next

- **Raise instead of cut.** A positive `depth` on `CARVED` stands the
  letters proud of the slate, lit along their upper edges.
- **Brush across.** `.brushed(strength=0.9, rotation=90)` turns the
  brushing across each ribbon: at 2 seconds the bus's bright stretch
  draws in toward its middle and both of its ends go dark.
- **Lacquer the satin.** Add `clearcoat=1.0, clearcoatRoughness=0.05` to
  `SATIN` and the panel's edges appear in it as crisp lines over the soft
  highlight.

## Fine print

- A page's range is learned from what it states: the emissive map's white
  is the brightest emission stated anywhere on the page. A pulse that
  re-described the mark's strength each frame would repaint every map of
  the page each frame — about a minute for a still here against three
  seconds for the held face — so the pulse lives in the bloom.
- A page carries one volume: the index, attenuation and dispersion of the
  first transmitting material it paints are the whole page's.
- `material.noise` is coloured unless `grain=True`; read it as a height
  only through its luminance, or ask for grain.
- The crazing is noise creases, not polygonal cells; there is no
  cellular noise to make a true craze from.
