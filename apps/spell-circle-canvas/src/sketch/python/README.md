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

The public direct binding surface is `sigil.native`: its `compose`,
`draw` and `motion` namespaces expose the bound native types and verbs.
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

## Build and run

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

The built package and extension are in `build/python`. Use the Python
interpreter selected by CMake, since an extension belongs to that
interpreter's ABI:

```sh
PYTHONPATH=build/python python3 - <<'PY'
from sigil.sketch import render_file

render_file(
    "src/sketch/sketches/python_dashboard.py",
    "/tmp/python-dashboard.png",
    at=2.0,
)
PY
```

The renderer accepts strings and `pathlib.Path` objects. Omitting `at`
uses the capture moment declared by the sketch. The headless host steps
the scene clock from zero, so native entrances and pen history are
present in a capture.

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
| `fill`, `ink` | a hex color string or a normalized RGB/RGBA tuple |
| `absolute` | boolean |
| `align_items`, `justify` | a supported alignment name |
| `key` | a stable string |
| `font_size`, `font_weight` | numbers |
| `opacity`, `rotate`, `scale`, `scale_x`, `scale_y`, `translate_x`, `translate_y` | numbers or native motion declarations |

Text takes `text(value, size=16, color="#ffffff", **properties)`. Font
selection follows the host's native font context. Explicit retained
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

## Scope

Imports follow library ownership: composition comes from `sigil.compose`,
motion from `sigil.motion`, drawing from `sigil.draw`, and sketch
declarations and rendering from `sigil.sketch`. Direct bindings preserve
those library namespaces under `sigil.native`.

The initial motion surface is `entrance(start, stop, duration=...,
delay=...)` and `transition(target, duration=..., delay=...)`, with times
in seconds. The declarations carry their values into native composition;
authors do not keep raw output pointers alive.

The pen exposes basic geometry, paths, colors, text, transforms and the
seeded random and noise functions. The experiment does not yet bind the
complete typography, geometry, material, image, world or brush APIs.
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
