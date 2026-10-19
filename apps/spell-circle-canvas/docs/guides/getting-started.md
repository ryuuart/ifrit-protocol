# Getting started: one sketch, from a box to a lit scene

Sigil is a suite of libraries for describing pictures: a page of boxes
and text, a pen drawing into a canvas, or a 3D scene under lights. You
write a **sketch** — one file that declares what the picture is — and
**Grimoire**, the stock host, opens it, draws it, and swaps in every save.
This page follows one sketch through seven steps, in C++ and in Python
side by side, and says at each step which library you just used and
where its own document is.

A sketch is one of two kinds. A **scene2d** sketch describes a flat
page: a tree of elements, or a pen program, or both. A **scene3d** sketch
describes a scene in space, which a device draws. The steps below build
a scene2d sketch first and turn it into a scene3d one at the end.

## Before the first step

Build the tree once, from `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target Grimoire
```

For Python, make a project that installs `sigil-sketch` editable from the
checkout; the editor then completes and checks every name below:

```sh
uv init --package my_sketches && cd my_sketches
uv python pin 3.14
uv add --editable /path/to/ifrit-protocol/apps/python/sigil
```

Grimoire runs a Python sketch with the bindings compiled into it, so a
`.py` file needs none of this to open; the project is for the editor and
for rendering without Grimoire.

## Step 1 — a page with a title

The smallest sketch: a canvas, one box filling it, and a line of text.
Save it anywhere as `title_card.cpp` or `title_card.py`.

```cpp
#include <sigilcompose/core/Core.h>
#include <sigilsketch/scene2d/Scene2D.h>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {
constexpr material::Color kGround = material::hexColor(0x11151a);
constexpr material::Color kInk = material::hexColor(0xe8eef2);
}  // namespace

struct TitleCard {
  void setup(sketch::Scene2DContext& ctx) {
    ctx.canvas({.size = {640, 360}, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .cover()
        .padding(40)
        .ink(kInk)
        .font({.size = 32})
        .children({text("A title card")});
  }
};

SIGIL_SKETCH(TitleCard, "Guide", "a page with a title")
```

```python
from sigil import compose
from sigil.sketch import Scene2DContext, sketch

GROUND = "#11151a"
INK = "#e8eef2"


@sketch(size=(640, 360), background=GROUND, captureAt=0)
class TitleCard:
    def setup(self, ctx: Scene2DContext) -> None:
        ctx.render(self.redraw())

    def redraw(self) -> compose.Element:
        return (
            compose.box(compose.text("A title card", size=32, color=INK))
            .cover()
            .padding(40)
        )
```

Open it live, or take one still:

```sh
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire title_card.cpp
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire title_card.cpp --frame card.png
```

```sh
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire title_card.py
sigil render title_card.py --output card.png     # from the Python project, no Grimoire
```

Edit and save while the window is open: the C++ file recompiles and the
window swaps to the new build; the Python file is re-imported. Either
way the sketch starts again as a new instance with a fresh clock.

**What you used.** `setup` runs once when the session opens and declares
the canvas and the tree. `box()` and `text()` are **elements**, free
functions that start a node; `.cover()`, `.padding()`, `.ink()` and
`.font()` are **verbs**, members that hand the node back so they chain.
The tree is **SigilCompose**; `ink` and `font` cascade, so the text
inherits its colour and size from the box. The sketch body, the context
and the `SIGIL_SKETCH` line are **SigilSketch**. A colour is a
`material::Color` from **SigilMaterial**, which in Python is also a CSS
string.

## Step 2 — a row of chips from data

A page is built from your data by ordinary functions. Add a row of chips
under the title, one per entry, tinted along a ramp.

```cpp
#include <sigilmaterial/color/Color.h>

namespace {
constexpr material::Color kCool = material::hexColor(0x2f6f8f);
constexpr material::Color kHot = material::hexColor(0xe2714b);
constexpr int kChips = 7;

Element chip(int index) {
  const float along = (float)index / (float)(kChips - 1);
  return box().width(54).height(54).borderRadius({10}).fill(
      material::lerpOklab(kCool, kHot, along));
}
}  // namespace

Element TitleCard::describe() const {
  return box()
      .cover()
      .padding(40)
      .column()
      .gap(24)
      .ink(kInk)
      .font({.size = 32})
      .children({text("A title card"),
                 box().row().gap(12).children(each(kChips, chip)),
                 text("seven chips along a ramp").font({.size = 14})});
}
```

```python
from sigil import material

COOL = "#2f6f8f"
HOT = "#e2714b"
CHIPS = 7


def chip(index: int) -> compose.Element:
    tint = material.lerpOklab(COOL, HOT, index / (CHIPS - 1))
    return compose.box().width(54).height(54).borderRadius(10).fill(tint)


class TitleCard:
    def redraw(self) -> compose.Element:
        chips = compose.box(*[chip(i) for i in range(CHIPS)]).row().gap(12)
        return (
            compose.box(
                compose.text("A title card", size=32, color=INK),
                chips,
                compose.text("seven chips along a ramp", size=14, color=INK),
            )
            .cover()
            .padding(40)
            .column()
            .gap(24)
        )
```

**What you used.** `.row()` and `.column()` set the flex direction and
`.gap()` the space between children. `each(count, make)` is the list a
loop makes, and sits inside the braced children block beside single
elements; in Python the children are positional, so a list is unpacked
with `*`. `material::lerpOklab` mixes two colours in a perceptual space,
so the ramp passes through no muddy band. Nothing here is a component
class: a function from data to an `Element` is the whole of a component.

## Step 3 — motion

Two kinds of movement, and they differ in who drives them. An
**entrance** plays once when a node first mounts: the runtime owns it.
A **live value** is written by you, from a timer, and read by the tree
at paint.

```cpp
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Time.h>

namespace motion = sigil::motion;
using namespace std::chrono_literals;

struct TitleCard {
  motion::Animatable<Fill> accent = motion::animatable(Fill::color(kCool));

  void setup(sketch::Scene2DContext& ctx) {
    ctx.canvas({.size = {640, 360}, .background = kGround, .captureSeconds = 1.5});
    ctx.engine.timer([this, &engine = ctx.engine] {
      const float phase = motion::phase(engine.elapsed(), 2s);
      accent = Fill::color(material::lerpOklab(kCool, kHot, phase));
    });
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .cover()
        .padding(40)
        .column()
        .gap(24)
        .ink(kInk)
        .font({.size = 32})
        .children({text("A title card")
                       .opacity(motion::animate({.from = 0.0f, .to = 1.0f, .duration = 600ms})),
                   box().row().gap(12).children(each(kChips, chip)),
                   box().width(220).height(6).borderRadius({3}).fill(accent)});
  }
};
```

```python
from sigil import motion


class TitleCard:
    def setup(self, ctx: Scene2DContext) -> None:
        self.accent = motion.animatable(compose.Fill.color(COOL))
        ctx.engine.timer(self.pulse)
        ctx.render(self.redraw())

    def pulse(self, delta: float, elapsed: float) -> None:
        phase = (elapsed % 2.0) / 2.0
        self.accent.set(material.lerpOklab(COOL, HOT, phase))

    def redraw(self) -> compose.Element:
        title = compose.text("A title card", size=32, color=INK).opacity(
            motion.animate(from_=0.0, to=1.0, duration=0.6)
        )
        bar = compose.box().width(220).height(6).borderRadius(3).fill(self.accent)
        chips = compose.box(*[chip(i) for i in range(CHIPS)]).row().gap(12)
        return compose.box(title, chips, bar).cover().padding(40).column().gap(24)
```

**What you used.** `motion::animate({.from, .to, .duration})` describes a
tween and any property that takes an `Animatable` accepts it; the same
verb with only `.to` eases to a new value whenever you describe one.
`motion::animatable(value)` makes a cell you write; the node holds the
cell and reads it every frame, so the tree is described once and never
rebuilt for the pulse. `ctx.engine` is the session's clock,
**SigilMotion**'s `Engine`; its timer hands a callback the frame's delta
and its own elapsed seconds, as many of the two as the callback names.
Start timers in `setup` and keep state in members, because a reload
makes a new instance. A still of this sketch
is taken at `captureSeconds`, when the entrance has landed.

## Step 4 — a pen, for the parts that are drawn rather than described

Some marks are easier to draw than to declare. A `graphics` node holds
an immediate pen program inside the retained tree; the pen keeps its
canvas between frames, so a translucent background leaves a trail.

```cpp
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>

#include <cmath>

namespace draw = sigil::draw;

Element orbit() {
  return graphics("card.orbit", [](draw::Pen& pen) {
    if (pen.frameCount == 1) pen.noStroke();
    pen.background(17, 21, 26, 30);
    pen.fill(226, 113, 75);
    pen.circle(110 + 90 * std::cos(pen.millis() / 900), 60, 18);
  }).width(220).height(120);
}
```

```python
import math

from sigil.draw import Pen


@sketch(size=(640, 360), background=GROUND, captureAt=1.5)
class TitleCard:
    def draw(self, pen: Pen, ctx: Scene2DContext) -> None:
        if pen.frameCount == 1:
            pen.noStroke()
        pen.background(17, 21, 26, 30)
        pen.fill(226, 113, 75)
        pen.circle(110 + 90 * math.cos(pen.millis() / 900), 60, 18)
```

Put `orbit()` in the children block beside the chips. In Python a sketch
class may carry both a `setup` that renders a tree and a `draw` that
takes the pen; the pen draws over the whole canvas, and `pen.element`
places a composed tree where the pen stands.

**What you used.** The pen is **SigilDraw**: `background`, `fill`,
`circle`, `millis`, `frameCount`, and the rest of a Processing-shaped
vocabulary. Persistent style is set on the first frame. The pen reads the
host's pointer and keys each frame; there is no event callback to wire.

## Step 5 — the same card as a scene in space

Now the chips become bodies on a plinth under lights. A **scene3d**
sketch describes a `world::Frame` as a function of the scene time, and
a device draws it. This is a second body contract: `describe(now)`
returning the frame, in place of `setup` rendering a tree. In Python the
same decorator reads it off the class: a `describe(self, seconds)`
annotated `-> world.Frame` makes the sketch scene3d, and its `setup` is
handed a `Scene3DContext`.

```cpp
#include <sigilgeometry/kit/Solids.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

namespace gm = sigil::geometry::mesh;
namespace world = sigil::world;

struct TitleScene {
  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(ctx, {.size = {640, 360},
                             .captureAt = 1.5,
                             .background = kGround});
  }

  world::Frame describe(motion::Duration now) {
    const float turn = (float)now.count() * 14.0f;
    return world::group(
               {world::directionalLight({-0.45f, -0.8f, -0.4f},
                                        {.intensity = 2.4f, .castShadow = true})
                    .key("sun"),
                world::ambientLight({.intensity = 0.07f}).key("fill"),
                world::kit::turntable({.radius = 420, .height = 170}, now),
                world::mesh(gm::box({-200, -9, -120}, {200, 9, 120}))
                    .key("plinth")
                    .at({0, -74, 0})
                    .fill(material::Color{0.16f, 0.21f, 0.26f, 1}),
                world::group(world::each(kChips,
                                  [&](std::size_t i) {
                                    const float along = (float)i / (kChips - 1);
                                    return world::mesh(gm::torus(22, 8))
                                        .key("chip" + std::to_string(i))
                                        .at({-180 + 60.0f * i, 0, 0})
                                        .rotateY(turn)
                                        .fill(material::lerpOklab(kCool, kHot, along));
                                  }))
                    .key("chips")})
        .key("card");
  }
};

SIGIL_SKETCH(TitleScene, "Guide", "the title card as a scene in space")
```

```python
from sigil import world
from sigil.geometry import mesh
from sigil.material import surface as surfaces
from sigil.sketch import Scene3DContext


def tinted(color) -> material.Material:
    return surfaces.program(surfaces.SurfaceParameters(baseColor=color, roughness=0.5))


@sketch(size=(640, 360), background=GROUND, captureAt=1.5)
class TitleScene:
    def setup(self, ctx: Scene3DContext) -> None:
        self.plinth = mesh.box((-200, -9, -120), (200, 9, 120))
        self.chip = mesh.torus(22, 8)
        ctx.camera(mesh.camera.Camera(eye=(0, 170, 420), target=(0, 0, 0)))

    def describe(self, seconds: float) -> world.Frame:
        chips = [
            world.mesh(self.chip)
            .key(f"chip{i}")
            .at((-180 + 60 * i, 0, 0))
            .rotateY(seconds * 14)
            .fill(tinted(material.lerpOklab(COOL, HOT, i / (CHIPS - 1))))
            for i in range(CHIPS)
        ]
        model = world.group(
            world.directionalLight((-0.45, -0.8, -0.4), intensity=2.4, castShadow=True),
            world.ambientLight(intensity=0.07),
            world.mesh(self.plinth).key("plinth").at((0, -74, 0)).fill(tinted("#29363f")),
            world.group(chips).key("chips"),
        ).key("card")
        return world.Frame(model)
```

Open the C++ file as before; a scene3d sketch needs the device, which
the window brings up on its own and a still asks for with `--gpu`:

```sh
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire title_scene.cpp --frame scene.png --gpu
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire title_scene.py --frame scene.png --gpu
```

**What you used.** The scene vocabulary is **SigilWorld**: `group` is the
one node with children and takes them as its argument; `mesh` is the one
node that draws; the lights act on every body wherever they stand in the
tree; a `Frame` holds what is not a node. Every node places itself with
`.at()`, `.rotateY()` and `.scale()`, answers to a `.key()`, and a
scalar lane such as `rotateY` takes an `Animatable`, so the motion
vocabulary of step 3 applies unchanged. The meshes are **SigilGeometry**:
`box`, `torus`, `quad`, `revolve`, `extrude` and the rest of the solids
kit. The camera on a rail is the World kit's `turntable`; the Python
column states a fixed lens, since a tree with no camera is seen through
the one `ctx.camera` hands the host. In C++ a plain colour on `fill` is lit at
the stock surface response; in Python a mesh's `fill` takes a
`Material`, which the next step makes.

## Step 6 — a material and a post pass

A body wears a **material**. The lit surface takes the metallic-roughness
parameters 3D graphics already uses, and a frame can carry passes that
work on the picture after the bodies are drawn.

```cpp
const material::Material brass = material::surface::program(
    {.baseColor = {0.85f, 0.65f, 0.35f, 1}, .metallic = 1.0f, .roughness = 0.3f});
const material::Material glaze = material::surface::program(
    {.baseColor = {0.53f, 0.73f, 0.68f, 1}, .roughness = 0.45f});

// in describe(): .fill(i % 2 ? brass : glaze) on each chip, and
return world::Frame(scene).pass(
    world::bloom({.threshold = 1.0f, .intensity = 0.4f, .radius = 32}));
```

```python
from sigil.material import surface as surfaces

brass = surfaces.program(
    surfaces.SurfaceParameters(baseColor="#d9a659", metallic=1.0, roughness=0.3)
)
glaze = surfaces.program(surfaces.SurfaceParameters(baseColor="#87b9ad", roughness=0.45))

# in describe(): .fill(brass if i % 2 else glaze) on each chip, and
return world.Frame(model).pass_(world.bloom(threshold=1.0, intensity=0.4, radius=32))
```

**What you used.** `material::surface::program` is the lit surface of
**SigilMaterial**, and its parameters reach into clear coat, sheen,
transmission and the other lobes when you set them; `unlit` is the
surface that is its own light. A frame with no geometry pass of its own
is drawn through the stock one, so naming only a bloom is enough. A
frame's environment, its passes and which image it presents are the
frame's verbs, and the lights, shadows and collections that narrow what
a light reaches are the World README's subject.

## Step 7 — stills, sweeps and where this runs

Every sketch is also a plate. `--frame` writes one still at the moment
the sketch declared, or at `--at`; `--headless` sweeps a catalogue of
them; `--window-bench` says the rate a sketch reaches the screen at; a
workspace is a folder of sketches outside this repository that Grimoire
opens as one.

```sh
Grimoire title_card.cpp --frame card.png --at 2
Grimoire --workspace ~/sketches
python3 scripts/sigil.py workspace new ~/sketches/first
```

```sh
sigil render title_card.py --output card.png --at 2
sigil open title_card.py
```

## Where each thing is documented

Every library's `README.md` beside its code is the canon for that
library, written for someone with no prior context.

| You used | Library | Read |
| --- | --- | --- |
| elements, verbs, layout, the cascade | SigilCompose | `src/common/compose/README.md` |
| the pen | SigilDraw | `src/common/draw/README.md` |
| colours, materials, the lit surface | SigilMaterial | `src/common/material/README.md` |
| tweens, live values, the clock | SigilMotion | `src/common/motion/README.md` |
| nodes, lights, frames, passes | SigilWorld | `src/common/world/README.md` |
| meshes and the solids kit | SigilGeometry | `src/common/geometry/README.md` |
| the sketch body, the context, hosts | SigilSketch | `src/sketch/README.md` |
| the window, every command line | Grimoire | `apps/grimoire/RUNNING.md` |
| the Python package, the wheel, parity | sigil | `apps/python/sigil/README.md` |

The [value index](/values/index.html) answers what makes a value a
signature asks for, the [element](/reference/SigilCompose/elements/index.html)
and [verb](/reference/SigilCompose/verbs/index.html) indexes list what
there is to say to a node, and the [glossary](glossary:) holds the house
words.

## The fine print

- **State lives in members.** A reload makes a new instance with a fresh
  clock; `Scene2DContext` is valid only during its callback, and a
  callback that needs the composer or the engine captures those
  references explicitly.
- **Describe when data changes, not every frame.** The tree is retained.
  A live value or a tween moves a property without re-describing; a
  changed structure is re-described, and keyed nodes keep their identity
  across descriptions.
- **A scene3d sketch needs the device.** The window brings it up; a
  still asks with `--gpu`. A scene2d body that bakes a 3D frame or paints
  meshes on the device says `static constexpr bool needsDevice = true;`
  in C++ and `needsDevice = True` on the class in Python, so a host
  brings the device up before the sketch opens; a Python scene3d sketch
  needs no such line, since its kind is read off `describe`.
- **Names are the domain's own.** Compose spells CSS (`justifyContent`,
  `borderRadius`), World spells USD, Blender and three.js
  (`directionalLight`, `lightLink`, `castShadow`), and the pen spells
  Processing. Namespaces are `sigil::compose`, `sigil::draw`,
  `sigil::material`, `sigil::motion`, `sigil::world`,
  `sigil::geometry::mesh` and `sigil::sketch`; the Python modules are
  `sigil.compose`, `sigil.draw`, `sigil.material`, `sigil.motion`,
  `sigil.world`, `sigil.geometry.mesh` and `sigil.sketch`.
- **Python is the same API with three spellings.** Children are
  positional, enumerations are strings, and values have literal forms: a
  colour is a CSS string, a dimension a number or `"50%"`. A Python
  process draws no 3D scene itself; it bakes one through the host's
  device.
