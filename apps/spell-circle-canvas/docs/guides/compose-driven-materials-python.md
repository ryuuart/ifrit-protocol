# Compose elements as the material of a 3D body, in Python

A plaque stands in a lit scene. Its face is an ordinary Compose page — a
title, a rule, a caption — and the page is what the plaque is made of:
Compose paints the colour, the relief and the glow, and World lights it.
No geometry carries the lettering; the body is one flat quad. This page
walks that sketch up in six steps, every one of them run through
Grimoire as written. Save the whole file as `plaque.py` and open it with
`Grimoire plaque.py`, or take a still with `--frame plaque.png --gpu`.

Where Python stands today: a Python sketch bakes a 3D frame into an
image through the host's device and places it on its page; it has no
`Page` of its own yet, so each map Compose paints goes through an
offscreen `Graphics` buffer whose image becomes a texture. The steps
below say so where it matters, and the last section says what the C++
form does differently.

## Step 1 — the page

Write the plaque's face as you would any Compose tree. It is a function
returning an element, sized in page pixels.

```python
import math

from sigil import compose, material, motion, world
from sigil.draw import Graphics, Pen
from sigil.geometry import mesh
from sigil.material import hexColor
from sigil.material import surface as surfaces
from sigil.motion import ease
from sigil.sketch import Scene2DContext, sketch

WIDTH, HEIGHT = 960, 540          # the canvas
PAGE_W, PAGE_H = 768, 384         # the page the plaque wears, in page pixels
PLATE_W, PLATE_H = 320, 160       # the plaque's size in the scene
GROUND = "#0f1216"
STONE = "#d9d0b8"
INK = "#2b3a33"
COPPER = "#b16d46"


def page() -> compose.Element:
    return (
        compose.box(
            compose.text("PLAQUE", size=150, color=COPPER).fontWeight(700).letterSpacing(14),
            compose.box().width(540).height(6).borderRadius(3).fill(COPPER),
            compose.text("compose paints the maps · world lights them", size=26, color=INK),
        )
        .width(PAGE_W)
        .height(PAGE_H)
        .fill(STONE)
        .column()
        .gap(22)
        .justifyContent("center")
        .alignItems("center")
    )
```

Nothing here knows about 3D. The page is the base colour the plaque
will wear.

## Step 2 — the same page as a height

A lit surface reads relief from a normal map, and a normal map comes
from a height. Write the page a second time as a height: white stands
high, black lies low, grey in between. The lettering and the rule are
raised; the caption is a shallow ridge; a small blur gives the edges a
rolled shoulder instead of a cliff.

```python
def relief() -> compose.Element:
    return (
        compose.box(
            compose.text("PLAQUE", size=150, color="#ffffff").fontWeight(700).letterSpacing(14),
            compose.box().width(540).height(6).borderRadius(3).fill("#ffffff"),
            compose.text("compose paints the maps · world lights them", size=26, color="#777777"),
        )
        .width(PAGE_W)
        .height(PAGE_H)
        .fill("#000000")
        .column()
        .gap(22)
        .justifyContent("center")
        .alignItems("center")
        .filter(material.Filter.blur(1.2))
    )
```

The two trees share their layout, so the height lands exactly under the
colour. A third tree, `glow()`, is the same page again with only the
rule in white on black; it becomes the emission map in step 5.

## Step 3 — paint the maps once

A `Graphics` buffer is an offscreen canvas. Open it around a callback,
place the tree with `element`, and its `image()` is a Skia image. A
`material.Texture` wraps that image and `material.image` makes it a
material, which is what every map slot takes.

The normal map is derived from the height by
`surfaces.normalFromHeight`. That derivation is a Skia program, and a
body on the device cannot run it, so bake it too: fill a fourth buffer
with the derived material and read the image back. Every map is painted
once, in the first frame.

```python
@sketch(size=(WIDTH, HEIGHT), background=GROUND, capture_at=2.0)
class Plaque:
    needsDevice = True            # the bake draws on the host's device

    def setup(self, ctx: Scene2DContext) -> None:
        self.colour_buffer = Graphics(PAGE_W, PAGE_H)
        self.height_buffer = Graphics(PAGE_W, PAGE_H)
        self.normal_buffer = Graphics(PAGE_W, PAGE_H)
        self.glow_buffer = Graphics(PAGE_W, PAGE_H)
        self.plate = mesh.quad(PLATE_W, PLATE_H)
        self.lens = mesh.camera.Camera()
        self.lens.eye = (0, 150, 330)
        self.lens.target = (0, 0, 0)
        self.lens.fovYDeg = 36
        self.painted = False

    def paint_maps(self, pen: Pen) -> None:
        self.colour_buffer.draw(pen, lambda p: p.element(page(), 0, 0, PAGE_W, PAGE_H))
        self.height_buffer.draw(pen, lambda p: p.element(relief(), 0, 0, PAGE_W, PAGE_H))
        self.glow_buffer.draw(pen, lambda p: p.element(glow(), 0, 0, PAGE_W, PAGE_H))
        height = material.image(material.Texture(self.height_buffer.image()))
        normal = surfaces.normalFromHeight(
            height, surfaces.HeightNormalOptions(depth=-3.0, step=0.75, directX=True)
        )

        def bake_normal(p: Pen) -> None:
            p.noStroke()
            p.fill(normal)
            p.rect(0, 0, PAGE_W, PAGE_H)

        self.normal_buffer.draw(pen, bake_normal)
        self.painted = True
```

`depth` is how far one unit of height tilts the normal; negative because
the lettering is raised toward the viewer in this page's convention, and
`directX=True` says which way the map's green points, matched by
`normalDirectX=True` on the surface below. Change the two together.

## Step 4 — the surface, from the maps

A material is built from a base and told how it responds to light with
`.surface(...)`. The base is the colour page; the normal is the baked
map; the height map doubles as the metalness, so the raised copper
lettering is metal and the stone around it is not.

```python
    def surface(self, glow_strength: float) -> material.Material:
        colour = material.image(material.Texture(self.colour_buffer.image()))
        height = material.image(material.Texture(self.height_buffer.image()))
        normal = material.image(material.Texture(self.normal_buffer.image()))
        glow = material.image(material.Texture(self.glow_buffer.image()))
        return material.from_(colour).surface(
            metallic=height,
            roughness=0.4,
            normal=normal,
            normalDirectX=True,
            emission=COPPER,
            emissionStrength=glow_strength,
            emissionMap=glow,
        )
```

Any `.surface` channel takes a number or a material: `roughness=0.4` is
one value everywhere, `metallic=height` varies by the pixel. The same
slots reach a clear coat (`clearcoat`, `clearcoatRoughness`), a sheen
(`sheenColor`, `sheenRoughness`), a brushing (`anisotropy`,
`anisotropyDirection`), glass (`transmission`, `ior`, `thickness`) and
the rest of the lit surface's lobes; each is paid for only when stated.

## Step 5 — the scene, the lights, and a pulse

Every frame describes the scene from the clock: a key light that swings
over eight seconds and breathes over three, a hemisphere fill, the plaque
on its quad, a studio environment for the copper to reflect, and a
bloom pass that spreads the rule's glow. The glow's strength is the
pulse, passed into the surface each frame.

```python
STUDIO = world.kit.environmentMap(
    zenith=(0.55, 0.6, 0.7),
    nadir=(0.08, 0.07, 0.06),
    softBoxes=[
        world.kit.SoftBox(centre=(0.25, 0.3), radiance=(6, 5.6, 5), size=(0.18, 0.1)),
        world.kit.SoftBox(centre=(0.7, 0.35), radiance=(3, 3.4, 4), size=(0.1, 0.12)),
    ],
)
```

The environment stands above the class; the frame is described in the
class's `draw`:

```python
    def draw(self, pen: Pen, ctx: Scene2DContext) -> None:
        seconds = pen.millis() / 1000
        if not self.painted:
            self.paint_maps(pen)
        pulse = 0.5 + 0.5 * math.sin(seconds * 2 * math.pi / 3)
        swing = 0.5 + 0.5 * math.sin(seconds * 2 * math.pi / 8)
        key_angle = math.radians(-60 + 120 * swing)
        model = world.group(
            world.directionalLight(
                (math.sin(key_angle), -0.7, -0.5), intensity=2.2 + 0.8 * pulse, castShadow=True
            ).key("key"),
            world.hemisphereLight(color="#9fb3c8", groundColor="#2a241c", intensity=0.35),
            world.mesh(self.plate)
            .key("plaque")
            .rotateX(-12)
            .rotateY(motion.animate(from_=-90, to=0, duration=1.6, ease=ease.outCubic))
            .fill(self.surface(glow_strength=0.6 + 3.4 * pulse)),
        ).key("scene")
        frame = (
            world.Frame(model)
            .environment(STUDIO, intensity=1.2)
            .pass_(world.bloom(threshold=1.8, intensity=0.6, radius=28))
        )
        still = ctx.bakeScene3D(
            frame, (WIDTH, HEIGHT), camera=self.lens, background=hexColor(0x0F1216), at=seconds
        )
        if still is not None:
            pen.image(still, 0, 0)
```

Two kinds of motion sit side by side here, and it is worth seeing why.

**A value from the clock.** `pulse` and `swing` are numbers computed
from `seconds` and written into the description: the key's intensity,
its direction, the glow's strength. The scene is described fresh each
frame, so this is the simplest timeline there is — a function of time —
and it is exact at any moment, which is what a still at `--at` needs.

**An entrance the runtime owns.** `rotateY(motion.animate(from_=-90,
to=0, duration=1.6, ease=ease.outCubic))` is a tween on the node's lane.
It plays once from the moment the plaque mounts; the bake samples it at
the moment it is handed in `at`, so a still at 0.6 seconds shows the
plaque mid-swing and one at 2 seconds shows it landed. Describing the
tween again does not restart it: the lane keeps its own clock. Any
scalar lane — `rotateX`, `scale`, `translateY`, a light's `intensity` —
takes the same value.

The bloom threshold sits above the stone's brightness and below the
rule's emission, so only the glow blooms; lower it and the whole plaque
halos.

## Step 6 — run it, at a moment

```sh
Grimoire plaque.py                                   # live, swapped on every save
Grimoire plaque.py --frame plaque.png --gpu --at 0.6 # mid-swing
Grimoire plaque.py --frame plaque.png --gpu --at 2.0 # landed, glow near its peak
```

A Python bake needs the device, which the window brings up and a still
asks for with `--gpu`; `needsDevice = True` on the class is what tells
the host before the sketch opens.

## What to change next

- **Make it carved rather than raised.** Invert the height page — black
  lettering on white — and flip the sign of `depth`. The copper then
  lies in the cut.
- **Brush the metal.** Add `anisotropy=0.8` to the surface and paint a
  direction map the way the colour was painted; the highlight stretches
  along it.
- **Glaze it.** Add `clearcoat=1.0, clearcoatRoughness=0.08` and the
  soft boxes reflect in a coat over the stone.
- **Keyframe the entrance.** `motion.animate(keyframes=[...])` with
  `motion.Keyframe(to, duration, ease)` is the spelling for an overshoot
  and settle; on a World lane baked from Python it stands at its last
  value today, which is a queued finding, so the entrance above uses
  `from_` and `to`.

## The C++ form, for when the page binding lands

A C++ scene3d sketch does the same with one object: `ctx.page(size)`
gives a `sketch::Page`, `page.render(tree)` is a pass on the frame that
paints every map from one tree, and `page.surface()` is the material a
body wears — no buffers, no baked normal, and the page can change every
frame. `apps/grimoire/sketches/carved_marks/` and `painted_fields/` are
that form. Binding the page to Python is in the findings queue; until
then the four buffers above are the Python route, and they are the same
four maps.
