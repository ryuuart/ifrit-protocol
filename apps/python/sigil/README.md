# Python sketches

Python is an alternative authoring language for the native canvas session,
included in every Sketchbook build.
A saved `.py` file is imported into a fresh sketch instance without a C++
compile or link. Composition, layout, text, motion and drawing still run
through the same native libraries as a C++ canvas sketch.

The Python frontend has two authoring paths: ordinary Python functions
that produce retained native elements, and an immediate drawing method
that receives the native pen. The package also imports in a matching
standalone Python interpreter, including headless file rendering.

## Direct bindings and convenient authorship

There is one importable package, `sigil`, and its library modules are the
direct binding surface: `compose`, `draw`, `material`, `geometry`, `image`,
`weave`, `core`, `motion`, `sketch`, `data`, `io`, `skia` and `world`.
Each one carries the bound native types and verbs, plus the few things
Python adds by hand — `row()` and `column()` as shortcuts for a box's flow
direction, `image.load()` and `image.save()`, the `@sketch` decorator.
Properties use native fluent methods; container arguments supply composed
elements:

```python
from sigil import compose as raw
from sigil.compose import row, text

mark = raw.box().width(12).height(12).fill("#8bd0bd")
caption = text("Observing", size=14)
panel = row(mark, caption).gap(8).alignItems("center")
```

There is one element type and no separate Python node model to keep in
sync. `box`, `text`, `frame`, `image`, `picture`, `pathFigure`, `slot`, `pen`,
`graphics`, `memo`, `layout`, `stack` and `positioned` are direct bound
factories; `PARITY.md` names the native overloads and factories not yet bound.
`_sigil` is the compiled extension. It ships as a bare shared object with
no declarations beside it, so a type checker cannot resolve a name through
it and an editor offers each name once, under the module an author imports
it from. A bound class reports that module too: `material.Color` reads as
`sigil.material.Color` in a repr and in `pydoc`.

Both Python surfaces target the capabilities needed to reproduce the native
sketch catalog. Coverage is still incomplete; `PARITY.md` lists the outstanding
work. The bindings and authoring layer ship from the same build, with tests
checking shared types, mixed construction, typing and lifecycle contracts.

## Type information

The wheel includes type declarations for the direct bindings and authoring
helpers, with a `py.typed` marker for editors. They are not files in this
project: `typing/generate.py` writes them from the extension it was built
with, into the package in the build tree, and the wheel and the editable
install take them from there. `typing/surface.py` is the one table saying
which module of the extension surfaces where, and which of its names an
author spells differently; the same table writes the package's own modules,
so the surface an editor reads and the surface an import finds cannot
disagree. Builders return native
`Element` values; material factories return native paints or materials.
Supported input forms use unions and overloads. Fluent
methods retain their native signatures,
and memo builders preserve the type of their model. Decorating a sketch retains
its class type.

Bound callables name their inputs in both runtime help and editor signatures:
for example, `pen.line(x1, y1, x2, y2)` and
`element.size(width, height)`. These names also support keyword calls.
Positional APIs, such as `.children(*children)`, declare that distinction in
their overloads.

Annotate the parameters of your own callbacks so an editor knows which
native service you receive:

```python
from sigil.compose import Element, text
from sigil.draw import Pen
from sigil.sketch import SketchContext, sketch


def caption(label: str) -> Element:
    return text(label, size=18, color="#e8eef2")


@sketch(size=(640, 420))
class Hello:
    def setup(self, ctx: SketchContext) -> None:
        ctx.background("#18252e")

    def draw(self, pen: Pen) -> None:
        pen.circle(320, 210, 80)
```

Python still runs without annotations. Static checking is an authoring aid;
runtime conversion and lifetime checks remain in force. Python cannot infer
an unannotated method parameter merely because its class has a decorator.
The bundled starter sketches show both Draw and Compose with explicit types.

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
uses the capture moment declared by the sketch, or 1.5 seconds if none is
declared. Sketchbook and standalone Python use the native host's capture
preparation: whole steps at 60 FPS followed by the fractional remainder.
Zero runs one update without advancing time. The headless host steps the
scene clock from zero, so native entrances and pen history are present
in a capture. Fractional canvas dimensions round up to whole pixels.
The CLI creates output directories as needed and
returns a nonzero status when import, setup or rendering fails.

## Build a wheel

The package project lives at `apps/python/sigil/`, a member of the Python
workspace at `apps/python/`. Its PEP 517 backend uses scikit-build-core to
configure CMake, build `sigil_python`,
and install only the Python component. On macOS, delocate then copies
required non-system dynamic libraries into the wheel and rewrites their
load paths. The wheel's platform tag reflects those libraries' minimum
macOS versions. The extension uses its importing Python interpreter;
it does not bundle or link another libpython. Licensed optional SDKs are
disabled for wheel builds. In a checkout, the backend selects the native tree
at `apps/spell-circle-canvas/`; a source distribution carries that tree within
its own archive so it does not need a sibling checkout.

`SigilPython`, under `src/common/python/` in the native tree, owns reusable
bindings over the drawing, composition, motion, data and resource libraries.
It does not depend on the sketch runtime. The `SigilSketchPython` leaf adapter
under `src/sketch/python/` adds embedding, native sketch sessions and SketchKit;
the `SigilSketch` core does not link Python. A native live host opts in by
supplying its Python loader
through `Host::Options::pythonLoader`. Sketchbook and the standalone
renderer always supply that function. Both use the same module registration
and canvas-session implementation. Sketchbook initializes the interpreter
when it loads a Python sketch or checks a declared Python package requirement.

A source build needs the native dependencies and toolchain configured for
the application, plus Python 3.12 or newer with development headers and
an embedding library. The dependency manifest supplies pybind11. A wheel
installation needs the matching Python and
operating system, with its native libraries already included. Wheels are
specific to the Python ABI and target architecture; the initial packaging
workflow bundles macOS runtimes.

The Python build does not read the application's CMake presets. Supply its
native dependency paths explicitly and use a separate package build directory.
For a macOS arm64 checkout with dependencies already installed in the native
application's `build/vcpkg_installed`, run from `apps/python`:

```sh
export VCPKG_ROOT=/absolute/path/to/vcpkg
export SIGIL_QT_ROOT=/absolute/path/to/Qt/6.11.1/macos
uv build --package sigil-sketch --wheel --python 3.14 \
  -Cbuild-dir="$PWD/../spell-circle-canvas/build-python-package" \
  -Ccmake.define.CMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -Ccmake.define.VCPKG_INSTALLED_DIR="$PWD/../spell-circle-canvas/build/vcpkg_installed" \
  -Ccmake.define.VCPKG_MANIFEST_INSTALL=OFF \
  -Ccmake.define.VCPKG_TARGET_TRIPLET=arm64-osx \
  -Ccmake.define.Qt6_DIR="$SIGIL_QT_ROOT/lib/cmake/Qt6"
```

Choose the Python version, Qt installation and vcpkg triplet for your target.
When reusing a package build directory, keep its CMake generator and Python
interpreter consistent; `--python /absolute/path/to/python` selects a specific
interpreter. Use a fresh directory when changing either. Do not point an
isolated package build at the application's active `build` directory.
From `apps/python/sigil/`, `uv build` selects the same package; adjust the
relative dependency paths or supply absolute paths. An extracted source archive
uses the same dependency flags with external installed dependency directories.

The wheel build does not build the sketch catalogue or application bundles.
`uv build --package sigil-sketch --sdist` from the workspace builds only the source archive;
building its wheel still requires the native dependencies. These commands
publish no distributions. The `ifrit-protocol-apps` peer project in
`apps/python/spellcircle/` owns the lightweight SpellCircle package and has no
dependency on the native extension.

An editable install uses the same native dependency settings. Its `sigil`
Python modules are read directly from this project, while the compiled
extension, native declarations and bundled sketches are installed into the
environment. Editing Python code takes effect in the next process; rebuilding
the editable install is required after changing native bindings.

Verify a built wheel with a fresh uv environment outside the checkout:

```sh
python3 packaging/check_wheel.py dist/sigil_sketch-*.whl \
  --checker ../.venv/bin/basedpyright
```

The check installs offline, removes Python path overrides, compares
images rendered by the installed CLI and by Python's isolated mode, and
checks positive and invalid authoring examples against the installed types.
The `typing` development dependency group supplies the checker and pinned
stub generator; these tools are not runtime dependencies.

The declarations an editor reads are a build product: `typing/generate.py`
writes them into the package in the build tree when the extension links,
and the wheel and the editable install take them from there. They are
never a file in this project. The package's own modules are, and a plain
run of the same script refreshes those — do that after a binding change,
and `--check` reports the modules an extension no longer agrees with
while writing nothing.

The script imports the extension, so it needs an interpreter that matches
the built module's Python version, carries the pinned stub generator from
the `typing` group, and HAS NO SIGIL PACKAGE OF ITS OWN: an installed
copy, an editable one above all, is found ahead of `PYTHONPATH`, so an
interpreter that has one would describe a binding that is not the one
just built, and the script refuses it rather than writing the wrong
declarations. The build looks for one at `build/typing-tools`, then
beside this workspace, then wherever CMake found Python, and leaves the
declarations and the checks over them out when none of them qualifies. A
wheel build needs none of this, because its isolated build environment
has the generator and no package of its own. Nothing creates the first
candidate; make it from `apps/spell-circle-canvas`:

```sh
uv venv --python <the extension's Python version> build/typing-tools
uv pip install --python build/typing-tools pybind11-stubgen==2.5.5
```

Refresh the package modules from `apps/python`, against the rebuilt
extension:

```sh
PYTHONPATH=../spell-circle-canvas/build/python \
  ../spell-circle-canvas/build/typing-tools/bin/python sigil/typing/generate.py
PYTHONPATH=../spell-circle-canvas/build/python \
  ../spell-circle-canvas/build/typing-tools/bin/python sigil/typing/generate.py --check
```

Generation uses the compiled binding signatures plus explicit refinements
for conversion boundaries such as colours, callbacks and property records.
New C++ APIs are not automatically bound. Bound API changes update generated
declarations; a changed conversion contract may also need its refinement
updated. CTest checks native export coverage and static authoring when the
checker is available in the configured development environment. Declaration
drift is checked when the configured interpreter or one of those local
environments has the pinned stub generator and matches the extension's
Python version.

## Develop with Sketchbook

Use Sketchbook's **Open → Open Sketch…** or **Open → Open Workspace…**
picker to open sources outside the bundled catalogue. The app remembers
recent files, workspace folders and each workspace's selected sketch; a
normal launch shows Welcome with recent locations and Browse Examples.
Close Workspace returns there without forgetting recent locations. A folder is the workspace,
so there is no separate workspace document to create.

**The entry is a file, chosen explicitly.** Open Sketch loads that file.
Open Workspace restores its last selected entry, opens its only sketch, or
shows a chooser when several sketches exist. The Library starts in the
Workspace collection and shows each entry's relative path. The canvas names
the file it is running; Details exposes the full path and can reveal it in
the file manager. A workspace without sketches explains how to add one.

Every `.py` file is a Python module. A module declaring a `@sketch` class
is discoverable as a sketch. A file called `__init__.py` initializes an
imported package; its name alone does not make it a sketch. The same holds
for `__main__.py` and a file named after its enclosing folder. Helper modules
and assets are loaded by the entry. Workspace discovery reads source without
importing these files, and leaves nested Python projects for their own windows.
Directly opened legacy files may still export one class with `setup(ctx)`;
use the decorator to make an entry discoverable in a workspace.

For a project with a source-layout package:

```text
spell_circle/
  pyproject.toml              Python version and dependencies
  spell_circle.py             @sketch class: the entry file
  src/spell_circle/
    __init__.py              package initialization
    components.py            reusable components imported by the entry
  assets/                    images, fonts and data
```

Open the outer `spell_circle` directory as the workspace. `pyproject.toml`
does not select an entry, and `[project.scripts]` defines terminal commands
that Sketchbook does not invoke. Several `.py` entries can share the same
project and package. Adding an entry requires reopening the workspace;
editing an already open entry reloads its running sketch.

For a Python project, Sketchbook finds its nearest `pyproject.toml` or
`.venv`. It uses uv to synchronize a project's dependencies, or reuses an
existing plain virtual environment. Preparation reports progress and errors
in the opening window. Standalone sketches use the host's default Python.
The project does not need to install `sigil-sketch` to draw in Sketchbook:
the native application supplies its matching bindings and authoring code.
An incompatible Python interpreter is rejected before the sketch runs.

Each project opens in another window of the same C++ application, with one
environment for that process. Reopen the project after changing installed
dependencies; saving sketch code or local helpers still hot reloads the
current session. A workspace scan leaves nested Python projects for their
own windows and skips environment, cache and build directories.

Bundled Python sketches appear in the Python collection and in the subject
tree beside C++ sketches. Their module docstring supplies the description;
`# TAGS:` lines supply subject paths. A root `.py` file or a directory
entry `<name>/<name>.py` declaring a `@sketch` class joins the registry on
the next build. Helpers without a declaration stay out of the catalogue. Registration
reads metadata without executing the sketch; every session imports current
source, including thumbnail and headless sessions.

The bundled catalogue shares the uv project and lockfile under
`apps/spell-circle-canvas/src/sketch/sketches/`. Browse Examples prepares its
interpreter and declared dependencies, including NumPy, automatically before
opening the catalogue. Direct file opens and headless rendering use that same
project. Independent sketches still use their nearest project or virtual
environment. The standalone wheel keeps NumPy in its optional `studies` extra.

A sketch with optional installed modules can declare a literal tuple such
as `REQUIRES = ("numpy",)`. The browser keeps the entry visible and marks
it unavailable when a module cannot be found. Headless selections skip
unavailable entries, and `sigil render` names a declared module this
interpreter does not have and exits nonzero rather than starting the host.
This declaration checks availability; it does not
install packages or choose an environment.

The application's normal build setup includes Python support. From
`apps/spell-circle-canvas`, build the host and standalone extension:

```sh
cmake --build build --config Release --target Sketchbook sigil_python
```

Open either example by path:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/python_orbits.py

build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/python_dashboard.py
```

For terminal launching and standalone Python rendering, install the
matching wheel into a uv project. `sigil open` remains an optional way to
launch the same native application:

```sh
uv init my-sketches
cd my-sketches
uv python pin 3.14
uv add /path/to/sigil_sketch-0.1.0a9-cp314-cp314-macosx_26_0_arm64.whl
uv add numpy
uv run sigil open sketch.py --sketchbook /path/to/Sketchbook
```

Use the actual wheel filename and the Python version it targets. Sketchbook
is built or installed separately; the wheel provides the bindings,
headless renderer and launcher. `--sketchbook` names the executable inside
the application bundle on macOS. It can be omitted when `SIGIL_SKETCHBOOK`
names that executable, `Sketchbook` is on `PATH`, or the native build is
under a parent of the working directory or sketch. The launcher also checks
the macOS Applications folders.

The launcher checks the host's Python version, ABI and architecture before
opening the window. The native host then uses the active environment's
site packages, including its `.pth` files and editable installs. A process
uses one Python environment; restart it after changing dependencies. The
host supplies its own matching `sigil` package, so its bindings and
authoring layer stay together. Python sketches and C++ sketches share the
same catalogue, controls, rendering and live host.

Native command arguments can follow `--`, including headless capture:

```sh
uv run sigil open sketch.py -- --frame preview.png
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

The native developer build also places the package and extension in
`apps/spell-circle-canvas/build/python`. That directory is for tests and development;
the wheel is the installation artifact.

## A drawing

```python
from math import cos, sin
from sigil.draw import Pen
from sigil.sketch import sketch


@sketch(size=(640, 420), background="#121720", capture_at=2.0)
class Orbit:
    def draw(self, pen: Pen) -> None:
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
reads elapsed scene seconds before the next frame. Methods may omit trailing arguments: `setup()` or `setup(ctx)`,
`update()`, `update(elapsed)` or `update(elapsed, ctx)`, and `draw()`,
`draw(pen)` or `draw(pen, ctx)`. The adapter resolves each signature when
the instance or drawing program is created. A drawing with two arguments
receives the same checked session context as setup.

The host owns scheduling and teardown. Keep model data freely on the
sketch, and use a pen during the drawing callback that supplied it.
Access through a pen after its callback, or a context after its session
ends, raises an exception. Context and pen operations run on the
session's owning thread.

## Session services

The context exposes checked views of the native services:

- `ctx.composer` renders trees and named slots, reads keyed bounds and hit
  tests, reports settling and cache statistics, and purges caches.
- `ctx.ticker` schedules callbacks with `add`, fixed steps with `addFixed`,
  and a bound chain with `derive`. A regular callback may take no arguments,
  `dt`, or `dt, elapsed`; returning `False` removes it and returning `None`
  keeps it. Fixed callbacks take no arguments. Their catch-up limit and
  optional interpolation output are the native scheduler's.
- `ctx.assets.image(uri)` reads an owned native image asset; `frameAt`
  supplies its image. `json` and `table` return owned data snapshots, and
  `database` returns a native query view. `ctx.assets.hub()` provides
  mounts, URI resolution, text, bytes, resource metadata, selection,
  live feeds and byte output.
  `ctx.local(name)` creates a URI for a file beside the sketch.
- `ctx.measure(element, maxSize)` and `ctx.snapshot(element, maxSize)` use
  the session's font context. `ctx.measured(value, pinned=0)` supplies the
  pinned value in a deterministic session.

`width`, `height`, `size`, `elapsed` and `deterministic` are current session
readings. The context and all service views reject use after teardown or
on a different thread. Keeping a view in Python therefore keeps a checked
handle, not a stack-allocated native context. Native images and motion
outputs are owned values and can outlive the session that produced them.

## Live data and output

`sigil.io` exposes the native SigilIO types directly. A sketch uses its
existing resource hub; transports are
already registered, and the host advances recorded feeds before `update`.
Receive and decode messages on the sketch thread, without authoring a worker
thread or calling back into Python from a transport:

```python
from sigil.data import decodeJson, encodeJson
from sigil.sketch import SketchContext


def setup(self, ctx: SketchContext) -> None:
    self.input = ctx.assets.hub().feed("udp://:27021")
    if not self.input.opened():
        raise RuntimeError(self.input.error())
    self.level = 0.0


def update(self, elapsed: float, ctx: SketchContext) -> None:
    while (arrival := self.input.receive()) is not None:
        document = decodeJson(arrival.bytes.decode("utf-8"))
        if document is not None:
            self.level = document["level"].number(self.level)
            reply = encodeJson({"level": self.level}).encode("utf-8")
            self.input.sendTo(arrival.from_, reply)
```

`receive()` takes the next queued arrival and returns `None` immediately when
the queue is empty. `newest()` reads the latest arrival without draining the
queue, and `latest()` reads just its bytes. Arrivals own their bytes and carry
`generation`, `at` and `from_`; they remain readable after the feed closes.
`FeedPolicy(capacity=...)` bounds the queue, and `dropped()` reports overflow.
Check `opened()`, `closed()` and `error()` when presenting connection status.

Feeds opened through a sketch context belong to that session. Successful
reloads retain connections that the replacement sketch still uses and release
those it drops; a failed replacement leaves the current session's feeds intact.
Escaped hub and feed handles reject calls after their session ends. Standalone
feeds have ordinary owned lifetimes and can be closed explicitly with `close`.
Reacquire desired feeds in each `setup`; a redeclaration releases omitted feeds.
As in C++, a URI names the same feed while it is owned, even after `close`.
Closing is terminal for that feed: end its owning session, or release all
standalone references, before opening that URI anew. Queue policy is selected
when a feed is first created.

A listening UDP feed replies to the sender named by an arrival with `sendTo`.
A peer feed such as `hub.feed("udp://127.0.0.1:27021")` uses `send` to reach
its configured destination. A WebSocket listener broadcasts with `send` or
addresses one peer with `sendTo`. These calls return whether the transport
accepted the send; acceptance is not a delivery acknowledgement. Payloads are
bytes or Python buffer objects. SigilIO transports bytes; the sketch chooses
JSON, CSV, FlatBuffers or another data format.

File output goes through the same URI mounts as input:

```python
from pathlib import Path
from sigil.data import encodeJson

hub = ctx.assets.hub()
hub.mount("out://", Path.cwd() / "output")
payload = encodeJson({"level": self.level}).encode("utf-8")
if not hub.write("out://readings.json", payload):
    raise OSError("Could not write readings.json")
```

`write` creates parent directories and invalidates the resource's cached reads.
It is a local byte sink, not an HTTP upload or a database transaction.
Export in response to an explicit action or output configuration; ordinary
rendering and catalogue thumbnails should not write application data.

Standalone Python uses the same bindings without a Sketchbook process:

```python
from sigil.io import Hub, registerUdp

hub = Hub()
registerUdp(hub)
peer = hub.feed("udp://127.0.0.1:27021")
try:
    if not peer.send(b'{"sequence": 1, "pressure": 0.6, "flow": 0.4}'):
        raise RuntimeError(peer.error() or "Could not send readings")
finally:
    peer.close()
```

Register only the transports needed, or call `registerTransports(hub)` for
the complete native set. A standalone program calls `hub.dispatch(seconds)`
when replaying recordings. Sketches leave that call to their host.
`feed.record(path)` writes arrivals for later playback; an empty path stops
recording. Mount a feed URI onto that file before opening it to replay the
same input through the same authoring code. `RecordingWriter`, `readRecording`
and `Feed.replay` also expose the recording format for generated fixtures.
Use a recording or explicit sample data when `ctx.deterministic` is true.

The bundled `python_live_signals.py` sketch shows received pressure and flow,
acknowledges accepted packets and distinguishes live input from sample data.
Its packaged companion sends through a standalone SigilIO hub and can export
received acknowledgements when requested:

```sh
uv run python -m sigil.examples.tools.send_live_signals
uv run python -m sigil.examples.tools.send_live_signals --export output/readings.json
```

### Publish the rendered canvas

Python sketches use Sketchbook's existing frame output. In a matching Python
environment, open the sketch with a named publication:

```sh
uv run sigil open sketch.py --publish "Live Sketch"
```

On macOS, a Syphon client such as Receiver subscribes to `Live Sketch` and
receives the canvas directly from Sketchbook's rendered GPU texture.
`--publish` without a name publishes under the sketch file's stem. Ctrl-P toggles output
in the window, and the status line shows the publication name. Drawing with
either Python or C++ uses this same path.

Frame publication belongs to the host that owns the GPU. SigilIO handles
resource and data bytes; it does not turn image bytes into a Syphon stream.
Sketchbook must be running on its Metal/Graphite backend to publish. The
standalone `sigil render` command remains a headless PNG renderer and does
not start a persistent GPU publisher.

## A retained composition

Components are ordinary Python functions returning native elements:

```python
from sigil.compose import box, column, row, text
from sigil.motion import entrance
from sigil.sketch import sketch


def metric(label, value, accent):
    return (
        column()
        .gap(10)
        .padding(24)
        .fill("#1b2735")
        .borderRadius(16)
        .flexGrow(1)
        .opacity(entrance(0, 1, duration=0.6))
        .children(
            text(label, size=13, color="#92a4b6"),
            text(value, size=42, color=accent),
        )
    )


@sketch(size=(800, 320), background="#101923", capture_at=1.0)
class Metrics:
    def setup(self, ctx):
        readings = [("RESONANCE", "0.86", "#e5b677"), ("COHERENCE", "94%", "#8bc9bc")]
        ctx.render(
            (
                column()
                .padding(32)
                .gap(24)
                .absolute()
                .inset(0)
                .children(
                    text("Field observations", size=28),
                    (
                        row()
                        .gap(16)
                        .children([metric(*reading) for reading in readings])
                    ),
                )
            )
        )
```

The description is submitted once. Its entrance motion is evaluated
natively on later frames; no Python update method is needed. A changing
model can submit another tree with `ctx.render(...)`. Keys match elements
across those descriptions, and native transition declarations animate a
property when its described target changes.

Container factories accept children directly:
`column(heading(), body()).gap(12)`. The fluent form
`column().gap(12).children(heading(), body())` is equivalent. This applies to
`box`, `row`, `column`, `stack`, `positioned`, and `layout(scheme, ...)`.
`row()` is `box().row()` and `column()` is `box().column()`. Constructors
do not take layout or styling keywords. Fluent methods keep their native names, such
as `.alignItems("center")`, `.fontSize(18)` and `.translateY(12)`.

Both factory arguments and `.children()` accept individual native elements
or one sequence of elements:
`.children(a, b)`, `.children([a, b])` and `.children((a, b))` are equivalent.
Pass a list comprehension directly for repeated content, or expand a sequence
with `*items` alongside other elements: `.children(heading(), *items, footer())`.
Generators and iterators work as a single iterable too.
Each call appends in order and returns the same element; `.children()` adds
nothing. Use ordinary Python conditions for optional content and `text(...)`
for strings. Nested sequences and `None` are not children. Components remain
ordinary functions returning an `Element`.

Text supports `text(value, size=None, color=None)` and `text(value, style)`
with a native `TextStyle`. Omitted size and color inherit from its container.
Font selection follows the host's native font context. Explicit retained
drawings use `graphics("identity", program)` followed by fluent properties.
The graphics key identifies its paint program as well as its node:
reusing it across descriptions promises an equivalent program. A drawing
that changes can read model state on its sketch instance; a replacement
closure that captures different values needs a different graphics key.

The builders return the actual native `Element` type. Native fluent
methods are available on that object, and `copy()` creates another native
description value. Python assignment aliases the same wrapper: use a
fresh component call or `copy()` before changing a reused description.
Descriptions already submitted to the native composer retain its
copy-on-write behavior.

## Compose inside Draw

`pen.element(tree, (x, y, width, height), index=...)` places a retained native
composition on the pen's current canvas. Pen transforms and clipping apply.
Each Python call site identifies a retained guest; use a stable `index` when
that call site places several independent trees in a loop. The guest retains
its own layout and motion state across frames.

```python
from sigil.compose import box, text
from sigil.draw import Pen

stamp = box().padding(8).fill("#356c69").children(text("Aa", size=24))


def paint(pen: Pen) -> None:
    for index in range(3):
        pen.element(stamp, (20 + index * 100, 20, 80, 60), index=index)
```

A custom brush tip receives a pen too and can call the same method.
`python_compose_stamps.py` shows both placement and a brush built from a
Compose tree. For a dense brush of identical static marks, render the mark
to an image and reuse it; a retained layout per dab has additional work.

## Memo, layouts and specimen kits

`memo(properties, describe).key("identity")` uses the native reconciler. Its
properties must support `copy.deepcopy` and equality; the description
captures a copied model and the current native environment. The builder
is a pure function of those values. A changed closure alone does not
invalidate the memo. Style the element the builder returns, rather than
the memo's description shell.

Stock layouts are native values from `sigil.compose.layouts`: `Grid`,
`Radial`, `AlongPath`, `Diagonal`, `BaselineGrid` and `Jittered`. Pass one to
`layout(scheme).children(...)`. Grid tracks use `px`, `content`,
`fr` and `minmax`, with named areas or child-owned cells. Relative text
lengths belong to `sigil.weave`; parent and percentage dimensions belong
to `sigil.compose`.

The specimen kit supplies the native page furniture and theme:

```python
from sigil.compose import text
from sigil.sketch import kit

look = kit.house_theme()
look.palette.ground = "#f4f0e6"
look.palette.ink = "#30372f"
look.type.title.size = 28

# Inside setup(self, ctx):
with kit.provide(look):
    kit.stage(ctx, size=(640, 360), capture_at=0)
    picture = kit.well(width=240, height=160).children(text("A specimen"))
    content = kit.caption(picture, label="Native type", note="One shared theme")
    page = kit.page(content, title="A specimen sheet", footer="Sigil")
ctx.render(page)
```

`stage`, `page`, `well`, `caption`, `cell`, `cells` and `panel_grid` call
SigilSketchKit. The neutral `sigil.compose.kit` supplies native wells,
captioned cells, sheets, panels, boards, construction circles, lines and
ladders. Both return the same native `Element`. Python normalizes keyword
fields and ordered children; native components own arrangement and
painting. Lowercase wrappers accept snake_case properties; native record
classes such as `Page`, `Well` and `Spacing` retain their native field names.

`house_theme()` supplies the default stock theme; `study_theme()` supplies
the stock theme for typographic studies. Both factories and `theme()` return
owned native values. Their embedded palette, type and spacing fields are
editable parts of the value.
`with kit.provide(look):` installs a snapshot in the native inherited
scope; nested scopes restore the enclosing theme. Close a provider in
reverse nesting order on its owning thread. Callback boundaries close a
provider accidentally left open. Bind around every function that
**describes** the tree, including a later update. A memo restores the
environment it captured when its builder runs after the authoring scope
has ended; an ordinary drawing callback should capture the needed colors
as values.

Optional nested records, such as `Well.content`, `recess` and `relief`,
return owned copies. Edit the copy and reassign it to change its parent.
A neutral `Caption` or `Sheet` can replace its native line part with a
Python callable accepting no arguments, the text, or the text and props.
The props passed to that callable are an owned native copy.

## Documents and role styling

`sigil.compose.document` supplies native document components with a default
type hierarchy: `article`, `section`, `h1` through `h6`, `paragraph`, `lead`,
`caption`, `label`, `eyebrow`, `footer`, `code`, `quote`, `list`, `item`,
`figure` and `rule`. Each returns the ordinary native `Element` and accepts
its fluent properties. A component carries a semantic role independently of
any extra `styleClass` names the author gives it.

```python
from sigil.compose import document as doc
from sigil.weave import Block, Leading, StyleSheet, Type, em, rule

content = doc.article(
    doc.eyebrow("FIELD JOURNAL"),
    doc.h1("A field worth noticing"),
    doc.paragraph("An ordinary observation can become a small story."),
    doc.list(doc.item("Notice the light."), doc.item("Follow its movement.")),
    doc.quote("Let the words find a comfortable measure."),
)
look = StyleSheet([
    rule("h1").font(Type(size=34, color="#88492e")),
    rule("paragraph").block(Block(leading=Leading.multiple(1.45))),
    rule("quote").font(Type(color="#687969")),
])
page = content.styleSheet(look).var(doc.measure, em(34))
```

A rule named `h1` styles every heading with that role in the subtree. Roles
and classes have separate membership and look up the same named rules. The
cascade applies inherited type, the component's fallback role, the sheet's
rule for that role, authored classes, then direct `font`/`block` declarations.
Build the children before their parent if useful; role styling resolves where
they land. A nested stylesheet can change one region's voice. The native
specimen theme uses `h1`, `lead`, `h2`, `label`, `caption`, `eyebrow` and `footer`
for its document lines; its editable type-register field names remain `title`,
`subtitle`, `captionLabel` and `captionNote`.

`article` bounds the reading measure. The inherited properties `doc.measure`,
`doc.gap`, `doc.list_gap` and `doc.quote_inset` control document geometry;
set them with `.var(name, value)` on an ancestor or the document itself.
Fallbacks are 38 em, 1 em, 0.4 em and 1 em respectively. A custom component can
declare a role using `.role("notice")` or `.role(rule("notice").font(...))`
and fallback properties with `.varDefaults({...})`. An ancestor's authored
property takes precedence over those fallback values.

Document containers accept individual Elements or one iterable, including
lists, tuples and generators. `doc.quote("words")` is also valid;
`doc.paragraph(rich_text)` retains a single native rich passage. `doc.item`
accepts words or an Element body and an optional marker. These are semantic
components; arbitrary text does not become an implicit child of an article.
`python_document.py` shows one description under two complete role themes.

## Authoring a Python kit

A kit can be an ordinary Python module exporting component functions and
paint factories. A component returns an `Element`; a paint factory returns
a native `material.Paint`. Both compose directly with the bound native kits:

```python
from sigil.compose import column, text
from sigil.material import Paint


def wash(accent):
    return Paint.linearUnit((0, 0), (1, 1), [(0, accent), (1, "#172b36")])


def card(title, detail, accent):
    return (
        column()
        .gap(14)
        .padding(24)
        .borderRadius(16)
        .ink("#ffffff")
        .fill(wash(accent))
        .children(
            text(title, size=26),
            text(detail, size=14),
        )
    )
```

Pass the returned card to `kit.page`, a layout, or another Python component.
The native composer owns layout, text shaping, reconciliation and painting.
Calling these factories describes values; it does not install a Python
callback for every frame. Native motion and time-dependent shader paints
continue running after description. A changed model submits another tree.

For project-specific design data, ordinary arguments or dataclasses keep
dependencies explicit. The bound specimen `Theme` has native inherited
scope; arbitrary Python theme types do not automatically join that scope.
Move reusable factories into a helper beside the sketch and import them
normally; local helper changes participate in hot reload.

`python_hello_compose` is a small complete example of this pattern.

## Motion and type values

`Output(value)` is a shared native cell. Set its `.value` from model logic
or a scene ticker, then feed `bind(output)` into native element properties.
The native binding chain supports domain mapping, wrapping, ping-pong,
wave forms, easing, quantization and seeded wiggle. A chain is mutable;
use `.copy()` before branching it. Descriptions retain shared ownership
of their source outputs, so collecting the Python wrapper does not leave
a pointer dangling in a retained tree.

`animate(from_(start).to(end), Transition(...))` declares an entrance;
`animate(to(target), Transition(...))` declares a transition.
`through([(time, value), ...])` declares keyframes. Python durations,
delays and keyframe times are **seconds**. Native easing values live in
`sigil.motion.ease`. The existing `entrance` and `transition` functions
remain concise wrappers over native declarations.

`sigil.weave` supplies native `Type`, `TextStyle`, `Block`, `StyleSheet`
and `rule` values. A partial type inherits unspecified fields; a complete
text style describes its own look. A page states its theme's stylesheet
on its root, and `styleClass` on a native element selects a class from
that cascade. Mixed runs and paragraph settings use the same native values.

## Typography

Compose accepts a native `RichText` value. Each added run carries plain text,
a partial `Type`, a complete `TextStyle`, or a class name. Children remain
composed elements; an inline object reserves its space with `slot` and matches
a child by its key:

```python
from sigil.compose import box, text
from sigil.weave import Block, Leading, Type, rich

passage = (rich().add("A ").add("warm", Type(color="#e39a60"))
           .add(" signal ").slot("status", (12, 12)).add(" arrives."))
label = (text(passage).width(320).font(Type(size=22, color="#d8e3e8"))
         .block(Block(leading=Leading.multiple(1.3)))
         .children(box().key("status").fill("#8bd0bd")))
```

`rich()` inherits its base from the element's type cascade; `rich(textStyle(...))`
states a complete base. `StyleSheet.types()` supplies the native `TypeSheet`
for explicit named-run resolution. `spanStyle` and `spanPaint` restyle selections
from `weave.selectors`; Compose's `selectors.style` and `selectors.inFrame`
address named runs and story frames. Selector ranges use native UTF-16 offsets.

`Block` exposes leading, alignment, justification, hyphenation settings, tab
stops, CJK line tables and writing mode. `ParagraphStyle` adds per-paragraph
spacing, indents, keeps, reservations and an initial letter. Give `paragraphs`
a list or tuple of those styles, or of stylesheet names. `Story(passage)` with
`frame(story).key(...).thread(...)` flows that same content through several
frames. `firstBaseline`, `distribute`, `initialLetter`, `reserve`, `live`,
`maxLines` and `ellipsis` use the native layout controls.

Glyph underlays, overlays and line decorations belong to `PaintStyle` or a
partial `Type`. `PaintLayer` accepts a configured Skia paint and an offset;
`Decoration` describes an underline, overline, strike or highlight. Optional
records and array properties are copies: edit and assign them back. A rich
value's `runs()` and a story's `blocks()` return independent snapshots.
`TextPath` and `onPath` place text on a native shape and accept shared motion
values for progress.

Direct editable paragraphs, font contexts, native flow geometry and annotation
values are available. Per-glyph effect tracks and custom Python implementations
of flow/hyphenation remain unfinished. A paint layer holds an optional native
material value, but no host installs the resolver that shades it, so the layer
draws with its Skia paint. The parity table tracks the remaining work
independently of which native libraries are linked.

## Data values and native resources

`sigil.data` supplies native `Json`, `Column`, `Table`, `Scale` and database
query values. Ordinary Python dictionaries and lists can construct JSON;
`to_python()` returns ordinary Python data. Native decoders read JSON and
CSV, and a table's cells are Python numbers, strings, booleans or native
`Instant` values. An `Instant` and a `Flag` compare, sort and hash as the
value each holds, and a `Flag` compares with the Python `bool` a boolean
cell reads as, so a group's key is a dictionary key and a column of times
sorts. JSON members and table columns are detached native copies.

```python
from sigil.data import Scale, decodeCsv

readings = decodeCsv("name,value\nnorth,12\nsouth,28\n")
height = Scale(domain=(0, 40), range=(0, 120))
bar_height = height.apply(readings.cell("value", 0))
```

A scale owns native domain mapping, transforms, overflow, ticks and band
placement. A database query view retains its connection and returns owned
tables; an owned connection from `Database.open()` or `memory()` also accepts
writes. Live byte feeds and file output are supplied by `sigil.io`.
`ctx.assets` routes resource loading through
the session's existing native services; the data values can also be used
from an ordinary installed Python process.

## Drawing beyond the basic pen

The pen exposes the geometry, shape contours, curves, clipping, dash,
color models, text, transforms, images, input state and loop controls used
by the Draw collections. `PARITY.md` maps those sketches to their native
requirements and distinguishes audited surfaces from rendered Python ports.
Seeded pen streams and `sigil.core.chance` keep deterministic models in the
same native random vocabulary.

Use `pen.canvas().drawPath(...)` or `drawPoints(...)` to submit native
batches. `drawPoints` accepts point iterables or a contiguous native-endian
float32 buffer with shape `(N, 2)` or an even-length flat buffer of x/y
pairs. NumPy arrays can pass directly without creating Python tuples:

```python
import numpy as np
from sigil.draw import PointMode

points = np.empty((count * 2, 2), dtype=np.float32)
# Fill alternating start and end points with NumPy array operations.
pen.canvas().drawPoints(PointMode.Lines, points, pen.strokePaint())
```

The call copies the coordinates into native storage before drawing; the
Python buffer can be reused after it returns. `sigil.skia` supplies paths,
a path builder, path operations, points, rectangles, matrices, images and
paints. The canvas expires with
the drawing callback. `pen.fillPaint()` and `strokePaint()` return an owned
paint copy, or `None` when that style is disabled. A four-number rectangle
tuple means `(x, y, width, height)`.

`Graphics(width, height)` from `sigil.draw` is an offscreen canvas. Call
`buffer.draw(pen, function)` to open and close it around a callback;
`buffer.begin(pen)` and `buffer.end()` are the explicit form. An unclosed
buffer closes when the host pen's callback ends, including on exceptions.
Its pixels survive resizing, and `pen.image(buffer, ...)` draws them.

Material paints come from `sigil.material`. The Skia backend is one
executor of that catalogue rather than a catalogue of peers beside `field`,
`kit` and `pattern`, and it is the only executor Python reaches, so its
paint, effect, fit and bloom stand in the material module itself:

```python
from sigil.material import Paint, field

paper = Paint.recipe(field.grain(0.02, seed=23))
glass = Paint.sksl(shader_source, {"uStrength": 42})
glass.slot("uSource", paper)
# Inside draw(self, pen): pen.fill(glass, CANVAS)
```

The paint factories include solid colors, gradients, images, native field
recipes, shader programs and layered blends. Compile a shader once in setup
and change a copy's uniforms when re-describing it. Native `uTime`,
`uResolution` and `uContentScale` retain their frame meanings. A child paint
fills a shader input through `slot`; it is the native material graph.

A custom SkSL paint can also be authored in Python. Compile a shader through
`sigil.skia.RuntimeEffect.MakeForShader`, keep that effect, and make paint
instances with `material.Paint.sksl(effect, uniforms)`. Reusing the compiled
effect avoids compiling source on each description. Use a fresh instance
or `paint.copy()` before changing uniform values or child slots on a shared
paint.

The native `sigil.material.kit.SurfaceParameters` record and its `surface`
and `unlit` factories describe mesh surfaces: base color, roughness, metallic
response, emission and transmission. The CPU executor, the only one Python
reaches, shades from base color and metallic response; emission and
transmission do not reach its pixels. The returned `sigil.material.Material`
is the same value a World element accepts. The full recipe-definition API,
uniform blocks, SDF catalogue, texture maps and environment maps remain
unbound. Material values support copying and conversion into a paint or effect. Paint uniform
setters currently take constant values; native frame uniforms animate
shader paints, while effect uniforms can also take bound motion values.
New backend-neutral recipes and unbound material catalogues still require
C++ bindings.

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

## Mesh geometry

`from sigil.geometry import mesh` exposes the native mesh currency, stock
solids, extrusion, lathing and parametric grids. `mesh.camera` owns cameras
and model transforms; `mesh.render` owns lights, surface styles and mesh
drawing. A Python function can define the parametric surface for
`mesh.grid`; native geometry computes its tessellation and normals. Mesh
array properties are copied Python values: assign the changed array back
to update the native mesh.

The mesh painter runs on its native CPU executor and works without a
display or GPU.

## Retained 3D scenes

`sigil.world` exposes native retained scene declarations and the CPU frame
executor. Import meshes and cameras from `sigil.geometry.mesh`, materials
from `sigil.material`, and animation values from `sigil.motion`. These are
the same values used by the direct bindings.

```python
from sigil import image, world
from sigil.geometry import mesh
from sigil.material import kit as surfaces

finish = surfaces.surface(
    surfaces.SurfaceParameters.dielectric("#87b9ad", roughness=0.45)
)
model = (world.Element().key("vessel")
         .mesh(mesh.torus(65, 20)).fill(finish).rotateX(25))
scene = world.Scene()
scene.render(world.kit.litSet(model, world.kit.Set(ground=0), seconds=1))
image.save(scene.image((640, 480), background="#141d25"), "world.png")
```

An `Element` supports keyed children, mesh and material values, tags,
transforms, cameras and lights. `children` takes variadic elements or one
iterable, including a tuple. Re-describing keyed nodes preserves native
identity and shares equal mesh resources. Scalar lanes accept native motion
outputs, bindings and transitions; `scene.advance(seconds)` advances the
owned ticker and samples the retained frame. Use it from an update callback
when the scene includes timed transitions. Explicit time can instead drive
fresh descriptions, as it does in `python_world_study`.

A `Frame` supplies a camera, pixel extent and passes. `geometryPass` writes
named resources; `postPass` can read, blur, grade and composite them. Add a
pass with `frame.pass_(pass_value)` and choose the image with
`frame.present("resource")`. Pass `reads` and `writes` accept variadic names
or one iterable. Selectors combine with `|`, `&` and `~`; `Selection.All`
disables selection filtering. Invalid frame graphs raise `ValueError`.

`Scene.image(size)` returns an owned native image with no window required;
`Scene.draw(pen)` draws into a live sketch's checked pen. A scene stays on
its creating thread and owns the ticker its native renderer borrows. Camera,
light and statistics getters return copies. The three-point rig, turntable
and lit-set kit compose ordinary native elements. Light and rig colors are
linear RGBA sequences, matching the native light records.

The CPU executor approximates material response through the native mesh
painter. Without an environment map, roughness does not change its
directional-light highlights; the study varies base color at a fixed
roughness. Device execution, environment lighting, texture-map authoring,
point-operator pipelines, custom pass callbacks and readbacks remain
unbound; the Python API does not claim GPU PBR parity.

## Example gallery

The wheel includes these examples; `sigil examples` lists their names.
Render one with `sigil render --example NAME -o preview.png`.

| Example | What it exercises |
| --- | --- |
| `python_hello` | A first drawing: a greeting, a moving circle and two constants to edit |
| `python_hello_compose` | A first retained composition: Python component and paint factories, native themed page and entrance motion |
| `python_type_atelier` | Mixed runs and inline objects, selector styling, initial letters, a threaded story and curved lettering |
| `python_document` | One semantic document under two inherited role stylesheets |
| `python_kit_specimen` | Native page and captions, stock layouts, scoped theme and deferred memo |
| `python_motion_signals` | Shared native outputs, ticker callbacks, binding chains and keyframe entrance |
| `python_memo_station` | Retained model descriptions, memo invalidation and native motion |
| `python_data_garden` | Native CSV tables, sorting, and linear, band and square-root scales |
| `python_mesh_observatory` | Parametric 3D knot, lathed vessel, regular solid, camera, native lighting |
| `python_world_study` | Retained keyed 3D scene, shared vessel mesh, material finishes and moving native light rig |
| `python_liquid_glass` | Shader-driven refraction, nested shader inputs, moving field uniforms, Bezier filaments |
| `python_botanical_study` | Layered natural media, native hatching and dry pigment |
| `python_liquid_layers` | Pressure-shaped ribbons, wet fibres, pigment blooms and pattern materials |
| `python_observable_flowfield` | NumPy vectorized angle field and packed line drawing; needs the `studies` extra |
| `python_observable_l_system` | Python string rewriting, turtle stack and native geometry |
| `python_observable_reynolds` | Stateful flocking, seeded initialization and transforms |
| `python_observable_reaction_diffusion` | NumPy Gray–Scott simulation and bulk pixel transfer; needs the `studies` extra |
| `python_compose_stamps` | Compose trees placed by a Draw pen and repeated as a custom brush tip |
| `python_live_signals` | Native UDP feed arrivals, JSON decoding, drawn traces and a reply to each sender |
| `python_dashboard` | Functions and iterables constructing retained native elements |
| `python_orbits` | An animated immediate drawing with fading trails computed from the scene clock |

## Scope

Imports follow library ownership: composition comes from `sigil.compose`,
motion from `sigil.motion`, drawing from `sigil.draw`, and sketch
declarations and rendering from `sigil.sketch`. Every module of the
extension surfaces under one of those names and under no other.

The package is an alpha Python frontend working toward complete sketch authoring
coverage. The remaining native capabilities in `PARITY.md` are implementation
work. Current tests do not establish full C++ catalog or GPU rendering parity.

Repeated drawing calls still cross into native code individually; a
larger geometry operation should use a native batch API when one is
available. Python supports experimentation and higher-level components
without restricting the full C++ API.

The examples are source-backed registry entries and also load by file path.
Reload replaces Python instance state; it does not migrate an existing
simulation or model into the new class.

The authoring contract tests run against the built extension. From
`apps/python`, using the Python interpreter that built the extension:

```sh
PYTHONPATH=../spell-circle-canvas/build/python python3 -m unittest discover \
  -s sigil/test -p 'test_*.py'
```

## Direct typography and data workflows

A standalone program can edit and measure native paragraphs without a sketch
host. Character ranges use UTF-16 offsets, matching the native engine; query
functions supply those ranges for Python strings, including supplementary-plane
characters. Font contexts stay on their creating thread. Layouts retain a
paragraph snapshot so their glyph pointers survive edits and
font-cache purges. Measurements and drawing reject a changed or unrelated
paragraph until it is laid out again; drawing accepts the checked sketch pen.

```python
from sigil import weave

fonts = weave.FontContext()
paragraph = weave.Paragraph("A measured paragraph.", weave.Type(size=24))
flow = weave.BlockFlow((0, 0, 320, 200))
layout = weave.layoutParagraph(fonts, paragraph, flow)
for line in layout.lineMetrics(paragraph):
    print(line.textBegin, line.textEnd, line.rect())
# In draw(self, pen): layout.drawBatched(pen, paragraph)
```

`PathFlow`, `VerticalBlockFlow`, `LineSetFlow` and `ExclusionFlow` use the native
layout engine. Collection properties return copies: assign a tuple, list or
other supported sequence back after changing it. `layoutParagraph(...,
hyphenator=patterns)` retains its native pattern table with the paragraph.
`weave.kit.PatternHyphenator` accepts UTF-8 pattern-file text.

```python
from sigil import data, io, material

hub = io.Hub()
hub.mount("res://", "./data")
data.registerDecoders(hub)
with hub.retain("res://*.csv") as lease:
    lease.preload()
    rows = hub.load(data.Table, "res://measurements.csv")
    if rows is not None:
        database = data.Database.memory()
        database.insert("measurements", rows)
        summary = database.query("SELECT count(*) AS count FROM measurements")

colors = material.harmony("#7bb7ba", material.Scheme.Triad)
ramp = material.Ramp(stops=(
    material.RampStop(0, colors[0]),
    material.RampStop(1, colors[1]),
))
tint = ramp(0.5)
```

`Database.open()` and `memory()` return owned writable connections. Databases
loaded through a sketch's `ctx.assets` hub or `fromBytes()` expose query views
that reject every writing statement, through `query()` as well as through
`execute()` and `insert()`, and a cached file is opened for reading only;
`registerDecoders` puts the table, JSON and database decoders on a standalone
hub, which then loads what a session hub loads. Failures raise an
exception; table/query results are independent copies. Protocol encoders accept
native Json values or ordinary Python dictionaries, tuples and lists and return
bytes; invalid packets decode to `None`. A `Schema` owns the binary schema it
loads and verifies conversions in both directions.

Material colors work in ordinary drawing and composition color inputs. Palette
indexing follows Python bounds and negative-index rules; native `at()` keeps its
clamping behavior. Ramp, palette extraction and dither computations execute in
C++. A paint layer's optional material is copied on assignment and retrieval;
reassign it after editing, just as for other optional native value records.

Resource leases stay on their creating thread. This prevents another Python
thread from closing residency while a blocking preload has released the GIL.
