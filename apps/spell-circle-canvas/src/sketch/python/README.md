# Python sketches

Python is an optional authoring language for the native canvas session.
A saved `.py` file is imported into a fresh sketch instance without a C++
compile or link. Composition, layout, text, motion and drawing still run
through the same native libraries as a C++ canvas sketch.

This is an experiment with two authoring paths: ordinary Python functions
that produce retained native elements, and an immediate drawing method
that receives the native pen. The package also imports in a matching
standalone Python interpreter, including headless file rendering.

## Direct bindings and convenient authorship

The public direct binding surface is `sigil.native`: its library namespaces
expose the bound native types and verbs, including `compose`, `draw`,
`material`, `geometry`, `image`, `weave`, `core`, `motion` and `skia`.
The separate authoring layer in `sigil.compose` adds keyword properties
and child normalization. It produces those same native elements, so an
author can mix both styles within one component:

```python
from sigil.native import compose as raw
from sigil.compose import row, text

mark = raw.box().width(12).height(12).fill("#8bd0bd")
caption = text("Observing", size=14)
panel = row(mark, caption, gap=8, align_items="center")
```

There is one element type and no separate Python node model to keep in
sync. A direct fluent call can continue a convenience-built element, and
a convenience builder can accept a directly bound element. `_sigil` is
the extension's implementation module; application code uses the public
package paths.

Both Python surfaces deliberately cover a curated subset of C++. A
native capability becoming available does not require immediately binding
it or designing a convenience wrapper. Coverage can lag while the C++
libraries develop. The bindings and authoring layer ship from the same
build, with tests checking shared types, mixed construction and lifecycle
contracts. Expanding the surface means deciding and testing each new
Python contract, rather than maintaining a second renderer.

## Install and render

The distribution is named `sigil-sketch`; its import package is `sigil`.
Install a wheel matching the interpreter and platform shown in its
filename. For a CPython 3.14 wheel:

```sh
uv venv .venv --python 3.14
uv pip install --python .venv/bin/python dist/sigil_sketch-*.whl
.venv/bin/sigil render sketch.py --output preview.png --at 2
```

The wheel includes example sketches. List them with `sigil examples`,
then render one directly:

```sh
.venv/bin/sigil render --example python_orbits --output orbits.png
```

The optional `studies` extra adds NumPy for the vectorized examples. The
base package has no third-party Python runtime dependencies.

The installed command renders directly through the native canvas session.
It needs no Sketchbook window, display server, `PYTHONPATH`, or source
checkout. `python -m sigil render ...` is the same command. The Python API
is also available from the installed environment:

```python
from sigil.sketch import render_file

render_file("sketch.py", "preview.png", at=2.0)
```

The renderer accepts strings and `pathlib.Path` objects. Omitting `at`
uses the capture moment declared by the sketch. The headless host steps
the scene clock from zero, so native entrances and pen history are
present in a capture. The CLI creates output directories as needed and
returns a nonzero status when import, setup or rendering fails.

## Build a wheel

The application root contains the package's `pyproject.toml`. Its PEP 517
backend uses scikit-build-core to configure CMake, build `sigil_python`,
and install only the Python component. On macOS, delocate then copies
required non-system dynamic libraries into the wheel and rewrites their
load paths. The wheel's platform tag reflects those libraries' minimum
macOS versions. The extension uses its importing Python interpreter;
it does not bundle or link another libpython. Licensed optional SDKs are
disabled for wheel builds.

A source build needs the native dependencies and toolchain configured for
the application. A wheel installation needs the matching Python and
operating system, with its native libraries already included. Wheels are
specific to the Python ABI and target architecture; the initial packaging
workflow bundles macOS runtimes.

After normal build setup, reuse the configured native build directory:

```sh
uv build --wheel --out-dir dist -Cbuild-dir=build
```

Use the same interpreter and CMake generator that configured that build,
or select a separate build directory and supply the native toolchain and
dependency paths with `-Ccmake.define.NAME=VALUE`. The wheel build does not
build the sketch catalogue or application bundles. A source distribution
is available through `uv build --sdist`; building it still requires the
native dependencies. No distribution is published by these commands.

Verify a built wheel with a fresh uv environment outside the checkout:

```sh
python3 src/sketch/python/packaging/check_wheel.py dist/sigil_sketch-*.whl
```

The check installs offline, removes Python path overrides, and compares
images rendered by the installed CLI and by Python's isolated mode.

## Develop with Sketchbook

After the application's normal build setup, enable the optional feature
from `apps/spell-circle-canvas`:

```sh
cmake -S . -B build \
  -DSIGIL_SKETCH_PYTHON=ON -DVCPKG_MANIFEST_FEATURES=python
cmake --build build --config Release --target Sketchbook sigil_python
```

Open either example by path:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/python_orbits.py

build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/python_dashboard.py
```

Save the file while the window is open to replace its session. Each
successful reload starts a fresh instance and restarts its scene clock.
Syntax, import and setup failures leave the existing session displayed.
An entry can import helpers beside itself with relative imports such as
`from .palette import COLORS`; a reload imports those local modules into
the new generation too. Local packages are watched as well. Installed
modules remain process-scoped. An exception during update or drawing
stops that session until the next edit; its canvas may include drawing
performed before the exception.

The developer build also places the package and extension in
`build/python`. That directory is for tests and in-tree development;
the wheel is the installation artifact.

## A drawing

```python
from math import cos, sin
from sigil.sketch import sketch


@sketch(size=(640, 420), background="#121720", capture_at=2.0)
class Orbit:
    def draw(self, pen):
        t = pen.millis() / 1000
        pen.background("#121720")
        pen.noStroke()
        pen.fill("#f1bb7b")
        pen.circle(320 + 140 * cos(t), 210 + 110 * sin(t), 24)
```

The decorator installs a full-canvas graphics element for `draw(self,
pen)`. The pen keeps its p5 method names and style state; its matrix is
reset for each frame. A translucent `background` accumulates trails on
the element's retained canvas. Time comes from the scene clock through
`millis()`, `deltaTime` and `frameCount`.

An optional `setup(self, ctx)` can initialize ordinary Python state. It
runs after the decorator configures the canvas and installs the drawing
element. If setup explicitly calls `ctx.render(...)`, that tree replaces
the default drawing element. An optional `update(self, elapsed, ctx)`
reads elapsed scene seconds before the next frame. These methods use the
signatures shown here.

The host owns scheduling and teardown. Keep model data freely on the
sketch, and use a pen during the drawing callback that supplied it.
Access through a pen after its callback, or a context after its session
ends, raises an exception. Context and pen operations run on the
session's owning thread.

## A retained composition

Components are ordinary Python functions returning native elements:

```python
from sigil.compose import box, column, row, text
from sigil.motion import entrance
from sigil.sketch import sketch


def metric(label, value, accent):
    return column(
        text(label, size=13, color="#92a4b6"),
        text(value, size=42, color=accent),
        gap=10, padding=24, fill="#1b2735", corners=16, grow=1,
        opacity=entrance(0, 1, duration=0.6),
    )


@sketch(size=(800, 320), background="#101923", capture_at=1.0)
class Metrics:
    def setup(self, ctx):
        readings = [("RESONANCE", "0.86", "#e5b677"),
                    ("COHERENCE", "94%", "#8bc9bc")]
        ctx.render(column(
            text("Field observations", size=28),
            row((metric(*reading) for reading in readings), gap=16),
            padding=32, gap=24, absolute=True, inset=0,
        ))
```

The description is submitted once. Its entrance motion is evaluated
natively on later frames; no Python update method is needed. A changing
model can submit another tree with `ctx.render(...)`. Keys match elements
across those descriptions, and native transition declarations animate a
property when its described target changes.

`box`, `row` and `column` accept native elements, strings, `None`, and
nested ordered iterables as children. Strings become text, `None` is
omitted, and lists and generators flatten in order. Call component
functions explicitly: the result is already a native element, with no
Python node tree or second renderer to synchronize.

Keyword properties spell the common native setters in Python form:

| Properties | Values |
| --- | --- |
| `width`, `height` | canvas units or a percentage string |
| `gap`, `left`, `top`, `inset`, `grow` | numbers |
| `padding` | one number or a tuple of two or four numbers |
| `corners` | one number or four corner radii |
| `fill`, `ink` | a hex color string or a normalized RGB/RGBA tuple; `fill` also takes a native material paint |
| `absolute` | boolean |
| `align_items`, `justify` | a supported alignment name |
| `key` | a stable string |
| `font_size`, `font_weight` | numbers |
| `opacity`, `rotate`, `scale`, `scale_x`, `scale_y`, `translate_x`, `translate_y` | numbers or native motion declarations |

Text takes `text(value, size=None, color=None, **properties)`; omitted size
and color inherit from its container. Font selection follows the host's
native font context. Explicit retained
drawings use `graphics(program, key="identity", **properties)`.
The graphics key identifies its paint program as well as its node:
reusing it across descriptions promises an equivalent program. A drawing
that changes can read model state on its sketch instance; a replacement
closure that captures different values needs a different graphics key.
Properties outside the supported subset raise a `TypeError`; misspelled
names suggest a close supported spelling.

The builders return the actual native `Element` type. Native fluent
methods are available on that object, and `copy()` creates another native
description value. Python assignment aliases the same wrapper: use a
fresh component call or `copy()` before changing a reused description.
Descriptions already submitted to the native composer retain its
copy-on-write behavior.

## Drawing beyond the basic pen

The pen exposes the geometry, shape contours, curves, clipping, dash,
color models, text, transforms, images, input state and loop controls used
by the Draw collections. `PARITY.md` maps those sketches to their native
requirements and distinguishes audited surfaces from rendered Python ports.
Seeded pen streams and `sigil.core.chance` keep deterministic models in the
same native random vocabulary.

Use `pen.canvas().drawPath(...)` or `drawPoints(...)` to submit native
batches. `sigil.skia` supplies paths, a path builder, path operations,
points, rectangles, matrices, images and paints. The canvas expires with
the drawing callback. `pen.fillPaint()` and `strokePaint()` return an owned
paint copy, or `None` when that style is disabled. A four-number rectangle
tuple means `(x, y, width, height)`.

`Graphics(width, height)` from `sigil.draw` is an offscreen canvas. Call
`buffer.draw(pen, function)` to open and close it around a callback;
`buffer.begin(pen)` and `buffer.end()` are the explicit form. An unclosed
buffer closes when the host pen's callback ends, including on exceptions.
Its pixels survive resizing, and `pen.image(buffer, ...)` draws them.

Material paints come from `sigil.material.skia` as an attribute namespace:

```python
from sigil.material import field, skia

paper = skia.Paint.recipe(field.grain(0.02, seed=23))
glass = skia.Paint.sksl(shader_source, {"uStrength": 42})
glass.slot("uSource", paper)
# Inside draw(self, pen): pen.fill(glass, CANVAS)
```

The paint factories include solid colors, gradients, images, native field
recipes, shader programs and layered blends. Compile a shader once in setup
and change a copy's uniforms when re-describing it. Native `uTime`,
`uResolution` and `uContentScale` retain their frame meanings. A child paint
fills a shader input through `slot`; it is the native material graph.

`sigil.image.from_rgba(buffer, width, height)` copies a contiguous RGBA byte
buffer into an immutable native image. It accepts `bytearray`, `memoryview`
and compatible NumPy arrays. This is the bridge for Python simulations:
compute with an array library, then hand the finished pixels to the pen in
one call. `image.decode`/`encode` operate on bytes, and `load`/`save` provide
ordinary Python file access. Native `weave.Type`, relative lengths and
typefaces can be passed to `pen.textFont` or an element's `font` method.

## Natural media

`from sigil.draw import brush` exposes the native brush library: tools,
pressure envelopes, stylus dynamics, samplers, fields, hatches, washes,
masses, plots, the brush engine, and brush decoding/encoding from bytes.
Algorithms such as dab spacing, field tracing and pigment deposition run
in that library. Python custom tips and fields use the same callback
lifetime checks as the pen.

Brush records accept keyword arguments and expose mutable fields. An
embedded record such as `tool.pressure` is part of its parent, so changing
`tool.pressure.start` changes that tool. Optional records such as a shape,
grain or dynamics response are returned as independent copies: edit the
copy and assign it back, or construct a replacement. Engine and catalogue
selection values are copies as well. These semantics let a Python value
survive replacing or clearing its former native owner.

```python
from sigil.draw import brush

tool = brush.Tool()
tool.width = 18
tool.pressure.start = 0.1
tool.pressure.end = 0.05
# Inside draw(self, pen): brush.line(pen, tool, (40, 80), (280, 160))
```

## Meshes and the example gallery

`from sigil.geometry import mesh` exposes the native mesh currency, stock
solids, extrusion, lathing and parametric grids. `mesh.camera` owns cameras
and model transforms; `mesh.render` owns lights, surface styles and mesh
drawing. A Python function can define the parametric surface for
`mesh.grid`; native geometry computes its tessellation and normals. Mesh
array properties are copied Python values: assign the changed array back
to update the native mesh.

The mesh painter runs on its native CPU executor and works without a
display or GPU. This surface does not expose SigilWorld's device scene,
frame graph, compute operators or full PBR material system.

The wheel includes these examples; `sigil examples` lists their names.
Render one with `sigil render --example NAME -o preview.png`.

| Example | What it exercises |
| --- | --- |
| `python_mesh_observatory` | Parametric 3D knot, lathed vessel, regular solid, camera, native lighting |
| `python_liquid_glass` | Shader-driven refraction, nested shader inputs, moving field uniforms, Bezier filaments |
| `python_botanical_study` | Layered natural media, native hatching and dry pigment |
| `python_liquid_layers` | Pressure-shaped ribbons, wet fibres, pigment blooms and pattern materials |
| `python_observable_flowfield` | Persistent particles and native batched line drawing |
| `python_observable_l_system` | Python string rewriting, turtle stack and native geometry |
| `python_observable_reynolds` | Stateful flocking, seeded initialization and transforms |
| `python_observable_reaction_diffusion` | NumPy Gray–Scott simulation and bulk pixel transfer; needs the `studies` extra |
| `python_dashboard` | Functions and iterables constructing retained native elements |
| `python_orbits` | An animated immediate drawing with retained trails |

## Scope

Imports follow library ownership: composition comes from `sigil.compose`,
motion from `sigil.motion`, drawing from `sigil.draw`, and sketch
declarations and rendering from `sigil.sketch`. Direct bindings preserve
those library namespaces under `sigil.native`.

The initial motion surface is `entrance(start, stop, duration=...,
delay=...)` and `transition(target, duration=..., delay=...)`, with times
in seconds. The declarations carry their values into native composition;
authors do not keep raw output pointers alive.

The experiment does not bind every API of every native library. Complete
typography, world rendering, media pipelines and networking remain separate
coverage decisions. The native brush `weightedChoice` template and generic
byte-source loading are not exposed; Python can choose values and supply
the brush decoder with bytes.
Repeated drawing calls still cross into native code individually; a
larger geometry operation should use a native batch API when one is
available. Python supports experimentation and higher-level components
without restricting the full C++ API.

The examples are loaded by file path. They are not compiled C++ registry
entries. Reload replaces Python instance state; it does not migrate an
existing simulation or model into the new class.

The authoring contract tests run against the built extension:

```sh
PYTHONPATH=build/python python3 -m unittest discover \
  -s src/sketch/python/test -p 'test_*.py'
```
