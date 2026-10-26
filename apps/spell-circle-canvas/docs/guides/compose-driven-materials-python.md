# Compose elements as the material of a 3D body, in Python

A plaque stands in a lit scene. Its face is an ordinary Compose page — a
title, a rule, a caption — and the page is what the plaque is made of:
each fill on the page says what it is made of, the page paints every
map those materials name, and World lights it. No geometry carries the
lettering; the body is one flat quad. This page walks that sketch up in
five steps, every one of them run through Grimoire as written. The
whole file is `apps/grimoire/sketches/python_plaque.py`; open it with
`Grimoire python_plaque.py`, or take a still with `--frame plaque.png
--gpu`.

## Step 1 — what the page is made of

A material says what it is: a base, layers over it, and `.surface(...)`
for how it answers light. Three of them here: a rough stone with a grain
in it, polished copper, and copper that glows.

```python
import math

from sigil import compose, material, motion, world
from sigil.geometry import mesh
from sigil.motion import ease
from sigil.sketch import Scene3DContext, sketch

WIDTH, HEIGHT = 960, 540  # the canvas
PAGE_WIDTH, PAGE_HEIGHT = 768, 384  # the page the plaque wears, in page pixels
PLATE_WIDTH, PLATE_HEIGHT = 320, 160  # the plaque's size in the scene
GROUND = "#0f1216"
STONE = "#d9d0b8"
INK = "#2b3a33"
COPPER = "#b16d46"

STONE_FACE = (
    material.from_(STONE)
    .layer(
        material.noise(0.02, octaves=3, seed=4, grain=True),
        blend=material.BlendMode.Multiply,
        opacity=0.16,
    )
    .surface(roughness=0.85)
)
COPPER_METAL = material.from_(COPPER).surface(metallic=1.0, roughness=0.32)
CAPTION_INK = material.from_(INK).surface(roughness=0.7)
GLOWING_RULE = material.from_(COPPER).surface(
    metallic=1.0, roughness=0.3, emission=COPPER, emissionStrength=12.0
)
```

`.surface` takes a number or a material for each channel, and the same
slots reach a clear coat, a sheen, a brushing, glass and the rest of the
lit surface's lobes; each is paid for only when stated.

## Step 2 — the page

Write the face as you would any Compose tree. The materials go on the
fills and inks. The title is raised: its glyph outlines are decorated
with `compose.relief`, which writes the shoulder and the depth into the
page's height, and the copper is what the relief is made of.

```python
def plaque_face() -> compose.Element:
    title = (
        compose.text("PLAQUE", size=150, color=COPPER)
        .fontWeight(700)
        .letterSpacing(14)
        .decorationOutline(compose.Boundary.Glyphs)
        .foreground(compose.relief(COPPER_METAL, shoulder=5, depth=1.2))
    )
    return (
        compose.box(
            title,
            compose.box().width(540).height(6).borderRadius(3).fill(GLOWING_RULE),
            compose.text(
                "compose paints the maps · world lights them", size=26, color=INK
            ).ink(CAPTION_INK),
        )
        .width(PAGE_WIDTH)
        .height(PAGE_HEIGHT)
        .fill(STONE_FACE)
        .column()
        .gap(22)
        .justifyContent("center")
        .alignItems("center")
    )
```

Nothing here knows about 3D, and nothing paints a height or a normal by
hand: the page derives every map from the materials its fills state.

## Step 3 — the page as a surface

The plaque is a scene3d sketch: its `describe(seconds)` returns a
`world.Frame`, and the annotation `-> world.Frame` is what tells the host
to draw it on the device. `setup` is handed a `Scene3DContext`.
`ctx.page(size)` makes the page; the session keeps it. `page.render(tree)`
is a pass on the frame `describe` returns, and `page.surface()` is the
material the quad wears. `ctx.camera(lens)` is the lens the frame is seen
through.

```python
STUDIO = world.kit.environmentMap(
    zenith=(0.55, 0.6, 0.7),
    nadir=(0.08, 0.07, 0.06),
    softBoxes=[
        world.kit.SoftBox(centre=(0.25, 0.3), radiance=(6, 5.6, 5), size=(0.18, 0.1)),
        world.kit.SoftBox(centre=(0.7, 0.35), radiance=(3, 3.4, 4), size=(0.1, 0.12)),
    ],
)


@sketch(size=(WIDTH, HEIGHT), background=GROUND, captureAt=2.0)
class Plaque:
    def setup(self, ctx: Scene3DContext) -> None:
        self.page = ctx.page((PAGE_WIDTH, PAGE_HEIGHT))
        self.face = plaque_face()
        self.plate = mesh.quad(PLATE_WIDTH, PLATE_HEIGHT)
        ctx.camera(
            mesh.camera.Camera(
                eye=(0, 150, 330), target=(0, 0, 0), verticalFieldOfViewDegrees=36
            )
        )
```

The face is built once, so the page paints once and keeps what it
painted while the face is unchanged. A face built fresh each frame is
repainted each frame, which is what a page with moving content wants.

## Step 4 — the scene, with two kinds of time

Every frame describes the scene from the clock: a key light that swings
over eight seconds and breathes over three, a hemisphere fill, the
plaque on its quad wearing the page, the studio environment for the
copper to reflect, the page's pass, and a bloom pass.

```python
    def describe(self, seconds: float) -> world.Frame:
        pulse = 0.5 + 0.5 * math.sin(seconds * 2 * math.pi / 3)  # a three-second breath
        key_angle = math.radians(
            -60 + 120 * (0.5 + 0.5 * math.sin(seconds * 2 * math.pi / 8))
        )
        model = world.group(
            world.directionalLight(
                (math.sin(key_angle), -0.7, -0.5),
                intensity=2.2 + 0.8 * pulse,
                castShadow=True,
            ).key("key"),
            world.hemisphereLight(
                color="#9fb3c8", groundColor="#2a241c", intensity=0.35
            ),
            world.mesh(self.plate)
            .key("plaque")
            .rotateX(-12)
            .rotateY(motion.animate(from_=-90, to=0, duration=1.6, ease=ease.outCubic))
            .fill(self.page.surface()),
        ).key("scene")
        return (
            world.Frame(model)
            .environment(STUDIO, intensity=1.2)
            .pass_(self.page.render(self.face))
            .pass_(world.bloom(threshold=1.8, intensity=0.6, radius=28))
        )
```

**A value from the clock.** `pulse` and `key_angle` are numbers computed
from `seconds` and written into the description: the key's intensity and
direction. The scene is described fresh each frame, so this is the
simplest timeline there is, a function of time, exact at any moment,
which is what a still at `--at` needs.

**An entrance the runtime owns.** `rotateY(motion.animate(from_=-90,
to=0, duration=1.6, ease=ease.outCubic))` is a tween on the node's lane.
It plays once from the moment the plaque mounts and the frame samples it
at the scene time, so a still at 0.6 seconds is mid-swing and one at 2
seconds has landed. Describing the tween again does not restart it.
Any scalar lane — `rotateX`, `scale`, `translateY`, a light's
`intensity` — takes the same value.

## Step 5 — run it, at a moment

```sh
Grimoire python_plaque.py                                      # live
Grimoire python_plaque.py --frame plaque.png --gpu --at 0.6    # mid-swing
Grimoire python_plaque.py --frame plaque.png --gpu --at 2.0    # landed
```

A scene3d sketch needs the device, which the window brings up and a still
asks for with `--gpu`; the host reads the kind from the `-> world.Frame`
on `describe` when it lists the sketch, before it opens.

## What to change next

- **Carve rather than raise.** Give `compose.relief` a negative `depth`
  and the copper lies in the cut.
- **Glaze the stone.** Add `clearcoat=1.0, clearcoatRoughness=0.08` to
  `STONE_FACE`'s surface and the soft boxes reflect in a coat over it.
- **Move the page.** Build the face inside `describe` from `seconds` and the
  page repaints each frame; a sliding caption or a reading that ticks
  over is then part of the material.
- **Brush the copper.** `anisotropy=0.8` on `COPPER_METAL` stretches its
  highlight; the direction is the page's horizontal until the brushing
  can follow the fill's own shape, which is queued.

## Two things worth knowing

A page's emission map keeps its light past white: its white stands for
the brightest emission the page states, so the rule at strength twelve
reaches the bloom's threshold and halos, while a page that states
nothing past white is unchanged. And `compose.relief` over the glyph
outline is the same statement a material makes with `.relief(shoulder=,
depth=)`; the decoration is the spelling for a text leaf's outline, the
material's is the spelling for any fill, and both cut the page's height
per the unit the fill names.
