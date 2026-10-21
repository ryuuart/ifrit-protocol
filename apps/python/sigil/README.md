# Python sketches

Python is an alternative authoring language for the native scene2d session,
included in every Grimoire build.
A saved `.py` file is imported into a fresh sketch instance without a C++
compile or link. Composition, layout, text, motion and drawing still run
through the same native libraries as a C++ scene2d sketch.

The Python frontend has two authoring paths: ordinary Python functions
that produce retained native elements, and an immediate drawing method
that receives the native pen. The package also imports in a matching
standalone Python interpreter, including headless file rendering.

## Quick start: a sketch project whose editor stays current

Two things read a sketch, and they take `sigil` from different places.
**Grimoire runs it** with the bindings compiled into the application, so
a sketch is as current as the Grimoire that opens it, whatever the
project's environment holds. **The editor and a standalone `uv run` type
and import it** from the `sigil-sketch` installed in the project's `.venv`,
so that copy is the one to keep in step with the checkout.

Make the project and install `sigil-sketch` editable from the checkout:

```sh
uv init --package my_sketches
cd my_sketches
uv python pin 3.14
uv add --editable /path/to/ifrit-protocol/apps/python/sigil
```

`uv add --editable` writes the source into `pyproject.toml`; a relative
path works as well as an absolute one:

```toml
[tool.uv.sources]
sigil-sketch = { path = "/path/to/ifrit-protocol/apps/python/sigil", editable = true }
```

The first install compiles the extension, which takes minutes; later ones
are incremental, because the native build settings in the user's uv
configuration under `[config-settings-package.sigil-sketch]` name one
persistent build tree that every editable install shares. Point the
editor's interpreter at the project's `.venv/bin/python`.

What to run after a change:

| What changed | Running side (Grimoire) | Editor side (the project's `.venv`) |
| --- | --- | --- |
| The sketch or its helpers | nothing: saving hot reloads | nothing |
| Python files of the `sigil` package | reopen the sketch: a build from this checkout imports the package from `apps/python/sigil` | nothing: an editable install reads them from the checkout |
| C++ bindings | `mise run python:native` refreshes the package's own modules, which are committed; then rebuild Grimoire | `uv sync --reinstall-package sigil-sketch` in the project, then restart the language server |

`uv sync` alone never picks up a binding change: the package's version did
not move, so uv sees nothing to install. The same holds for a project
pinned to a wheel file, which also has to be rebuilt first
(`uv build --package sigil-sketch --wheel` from `apps/python`); pin a wheel
once the surface an author uses has stopped moving, and stay editable until
then. `mise run python` and `mise run python:native` do for the workspace
environment at `apps/python/.venv` what the two `uv sync` spellings above do
for a project.

An editor that only needs declarations, with no environment at all, can
instead name `apps/spell-circle-canvas/build/python` as an extra analysis
path: the application's build writes the package and its declarations
there whenever `sigil_python` links.

## Direct bindings and convenient authorship

There is one importable package, `sigil`, and its library modules are the
direct binding surface: `compose`, `draw`, `material`, `geometry`, `media`,
`weave`, `core`, `motion`, `sketch`, `data`, `io`, `skia` and `world`.
Each one carries the bound native types and verbs, plus the few things
Python adds by hand — `row()` and `column()` as shortcuts for a box's flow
direction, `media.load()` and `media.save()`, the `@sketch` decorator.
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

Compose shades nothing. A material that states a `surface(...)` paints its
colours on a page, flat, as a material without one does; light belongs to a
World scene, which lights a page through its surface maps. `material.Light`
describes one emitter for that scene and answers `copy.copy` and
`copy.deepcopy`, so a memo model can hold one; a copied light keeps reading
the live cells it was built with.

`compose.relief(material, shoulder=2, depth=1)` is a decoration that puts
a material over the rounded relief of the outline it decorates: attach it
with `background()` for a shape, or with `foreground()` and
`decorationOutline(compose.Boundary.Glyphs)` for type. `shoulder` is the
width of the rounded edge in logical pixels and `depth` its height relative
to that width; a negative depth impresses the outline. The material's own
normal map is reoriented into the relief, in the normal map a World scene
lights the page by; on a page the material paints its colours flat.

`compose.brush.ribbon(width, paint)` is a band swept along the outline it
strokes: give it to `stroke()` on a node whose `shape` is the spine, as in
`box().shape(compose.heldPath(outline)).stroke(compose.brush.ribbon(8,
material))`. Its width is a `geometry.path.Profile` — a number for one
width the whole way, or a list of `(along, width)` stops along a fraction
of the spine — and its paint a colour, a fill or a material, which keeps
its layers, surface and effects. A keyline `stroke(material, width)` over
an open outline is likewise one band centred on it: an open outline has no
inside, so it has no side to keep. With `nibAngleDegrees` the width is
one number and the band is a calligraphic stroke, that wide where the
outline runs across the nib and `nibContrast` of it where it runs along;
with `widthEnd` it tapers from the width to `widthEnd`:
`compose.brush.ribbon(12, ink, nibAngleDegrees=30)`. `Ribbon.band(spine)` hands
back the region a ribbon fills, to be measured.

`material.noise(frequency, cellular=True, jitter=1)` is cellular noise: one
seed per cell of a lattice `frequency` cells per pixel, the distance to the
nearest seed in red, to the second in green, and their difference — zero on
the walls between cells — in blue, so a crackle or a cell network is a fill
read through `material.channel(cells, material.ColorChannel.Blue)`. It has a
body for a page and one for a lit body alike.

`geometry.mesh.extrude(path, ExtrudeOptions(...))` turns the edge between a
cap and the walls with `edge=mesh.ExtrudeEdge.Round` or `Bevel` over
`edgeSize` in `edgeSegments` steps, round holes too; `wallTexture=
mesh.WallTexture.Projected` runs the cap's page down the walls; and
`faceSets=True` lets a World body wear its caps and its walls as two
surfaces, `world.mesh(solid).fill([face, sides])`.

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
disagree. A container returns a native `Element`, and a leaf factory returns
that leaf's own class — `Text`, `Image` or `Band` — so the verbs only that
leaf states stay in reach along the chain; material factories return native
paints or materials.
Supported input forms use unions and overloads. Fluent
methods retain their native signatures,
and memo builders preserve the type of their model. Decorating a sketch retains
its class type.

A leaf is a class of its own rather than a subclass of `Element`, because
each shared verb hands the leaf's own type back. It converts into an
element wherever one is taken, so every slot that holds a node — one child,
a container's arguments, a kit piece's or a document component's content,
a memo builder's answer, the tree an instance recipe or a feed row makes —
is declared as `NodeLike`, the union of the four, beside `ColorLike`,
`FillLike` and the other conversion unions. Annotate a helper of your own
that hands a leaf back with the leaf's class, not `Element`.

Bound callables name their inputs in both runtime help and editor signatures:
for example, `pen.line(x1, y1, x2, y2)` and
`element.size(width, height)`. These names also support keyword calls.
Positional APIs, such as `.children(*children)`, declare that distinction in
their overloads.

Every name is the native one, camelCase as SigilSketch and the libraries
spell it: `kit.featureTheme`, `kit.panelGrid`, `document.listGap`,
`Json.toPython`, and a kit record's fields, `paddingVertical=` and `rowGap=`. A
native name that is a Python keyword takes a trailing underscore — `from_`,
`pass_`, `with_`, `is_`, `in_`, and `None_` for an enumerator named `None` —
and where a value can stand without its name it does: `animate(0, 1)` and
`Tween(0, 1)` take the start and the end by position, `element.mask(gate)`
takes its gate by position, and `world.Frame(scene, passes=[...])` takes a
frame's passes as a list beside `frame.pass_(...)`.

A callback is handed the longest run of leading positional arguments it
accepts, so a positional parameter with a default receives the host's
argument: a paint program `lambda pen, width=64: …` is called with the
paint context in `width`. Keep a captured value in a keyword-only
parameter, `lambda pen, *, width=64: …`, or in a closure, and name the
context, `def paint(pen, context):`, when you want it.

Annotate the parameters of your own callbacks so an editor knows which
native service you receive:

```python
from sigil.compose import Text, text
from sigil.draw import Pen
from sigil.sketch import Scene2DContext, sketch


def caption(label: str) -> Text:
    return text(label, size=18, color="#e8eef2")


@sketch(size=(640, 420))
class Hello:
    def setup(self, ctx: Scene2DContext) -> None:
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
.venv/bin/sigil render --example python_hello --output hello.png
```

The optional `studies` extra adds NumPy for the vectorized examples. The
base package has no third-party Python runtime dependencies.

The installed command renders directly through the native scene2d session.
It needs no Grimoire window, display server, `PYTHONPATH`, or source
checkout. `python -m sigil render ...` is the same command. The Python API
is also available from the installed environment:

```python
from sigil.sketch import renderFile

renderFile("sketch.py", "preview.png", at=2.0)
```

The renderer accepts strings and `pathlib.Path` objects. Omitting `at`
uses the capture moment declared by the sketch, or 1.5 seconds if none is
declared. Grimoire and standalone Python step alike: whole steps at 60
FPS followed by the fractional remainder, then the picture is taken as a
plate is: a scene2d sketch draws one more frame of no length to take it,
so the picture is the scene at the moment named, after one more update at
that moment. Zero runs that one update without advancing time, so a
picture asked for at zero shows the scene at zero. The headless host steps the
scene clock from zero, so native entrances and pen history are present
in a capture. A fraction of a pixel is dropped, as a plate drops it.
The CLI creates output directories as needed and
returns a nonzero status when import, setup or rendering fails.

What Grimoire's browser and its plate ledger know about sketches is
available as values too. `catalog(files=[], sketchDirectory=None,
workspace=None)` answers one `CatalogRow` per file — its key, the runtime
it draws through (a Python file always scene2d, a C++ file none until it
has been built), its entry path and what its opening comment says in
`source` — after one row per registry entry, whose files it looks for
under `sketchDirectory`; `registryRows(kind="")` answers the registry
alone, which is empty outside Grimoire because this package compiles no
sketch in.
`sigil examples` lists the packaged examples from `catalog`. `compare(first,
second)` differences two directories of `plate_<name>.png` files, as a
headless sweep writes them, into one `PlateComparison` row per plate:

```python
from sigil.sketch import PlateOutcome, compare

for plate in compare("plates/before", "plates/after").plates:
    if plate.outcome != PlateOutcome.Compared or plate.worst:
        print(plate)  # the row as one line
```

`renderFile` is one session driven through SigilProtocol, and a script
can drive one itself, by either of two routes that answer the same
generated domain classes. `sigil.testing.InProcess(state)` is a sketch
host in this process — the registry, session and clock agents, with every
command answered before it returns — and `sigil.protocol.launch(state=…)`
starts a headless Grimoire serving the protocol and connects to it over
its socket, as `sigil.protocol.connect(address)` attaches to any host
already running at a `ws://` address or through the state directory that
names one:

```python
from sigil.protocol import Clock, Session
from sigil.protocol.clock import Policy
from sigil.testing import InProcess

host = InProcess("state")                    # or launch(state="state")
Clock(host).setPolicy(policy=Policy.Advance)
Session(host).open(sketch="sketch.py")
Clock(host).step(seconds=2)
print(Session(host).still(density=2).path)   # written under state/
```

The client is spelled in the package's camelCase: a command is its own
name (`setPolicy`, `setTimeScale`, `pinDensity`), an event's listener is
`on` and the event's name (`onBudgetExpired`), and a table's fields are
attributes such as `budgetSeconds`, carried on the wire under the
definition's own member names (`budget_seconds`) by `toJson` and
`fromJson`.

Under Advance nothing moves but by a step, and a still taken then is the
plate a sweep writes of that moment; a paused clock photographs the
frame it holds. `sigil.protocol.launch` finds Grimoire through its
`executable` argument, `SIGIL_GRIMOIRE` or the path, and closing the
connection ends the process.

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
through `Host::Options::pythonLoader`. Grimoire and the standalone
renderer always supply that function. Both use the same module registration
and scene2d session implementation. Grimoire initializes the interpreter
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
publish no distributions.

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

`PARITY.md` is generated the same way, after the modules it reads:
`typing/parity.py` writes it from the extension, the sketch
registrations and the audit under `typing/ledger`, which records what
each C++ sketch needs and which Python spellings stand for each native
surface. A surface no Python name can stand for says why in its record,
and the ledger lists it as a claim the extension cannot judge. Prose sits
between its `<!-- prose -->` markers and is carried over as written, and
holds no table, since every table is generated; edit the ledger data or
that prose, never the rest, and `--check` names the first line that
disagrees. The application's `generate` target runs both scripts in
order. The ledger describes an extension built with every licensed SDK:
against one built without, `--check` exits as skipped, which ctest and
`sigil.py check` report as skipped, and the `generate` target fails
without writing `PARITY.md`.

Generation uses the compiled binding signatures plus explicit refinements
for conversion boundaries such as colours, callbacks and property records.
New C++ APIs are not automatically bound. Bound API changes update generated
declarations; a changed conversion contract may also need its refinement
updated. CTest checks native export coverage and static authoring when the
checker is available in the configured development environment. Declaration
drift is checked when the configured interpreter or one of those local
environments has the pinned stub generator and matches the extension's
Python version.

## Develop with Grimoire

Use Grimoire's **Open → Open Sketch…** or **Open → Open Workspace…**
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
is discoverable as a sketch. A file of functions — `setup`, `draw` or
`describe` defined at its top level — opens by path and joins the bundled
catalogue; a workspace scan finds decorated classes only. A file called
`__init__.py` initializes an
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
that Grimoire does not invoke. Several `.py` entries can share the same
project and package. Adding an entry requires reopening the workspace;
editing an already open entry reloads its running sketch.

For a Python project, Grimoire finds its nearest `pyproject.toml` or
`.venv`. It uses uv to synchronize a project's dependencies, or reuses an
existing plain virtual environment. Preparation reports progress and errors
in the opening window. Standalone sketches use the host's default Python.
The project does not need to install `sigil-sketch` to draw in Grimoire:
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
entry `<name>/<name>.py` declaring a `@sketch` class, or defining `setup`,
`draw` or `describe` as top-level functions, joins the registry on the
next build. Helpers without a declaration stay out of the catalogue. Registration
reads metadata without executing the sketch; every session imports current
source, including thumbnail and headless sessions.

The bundled catalogue shares the uv project and lockfile under
`apps/grimoire/sketches/`. Browse Examples prepares its
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

A bundled sketch is registered under its file stem, which stays the key
`--sketch`, `--list` and the registry answer to. Two more literal module
declarations say how the catalogue shows it: `NAME = "The Library of
Babel"` is the name a browser files it under, and `CATEGORY = "Study ·
Literature"` the category it is filed in, as a C++ study states both
beside its key. Without them a sketch reads as its stem in the `Python`
category. Each must be a literal, non-empty string; registration reads
them from the source without running it, and refuses anything else. A
workspace's sketch opened by path is catalogued under the same two
declarations, read the same way, and otherwise under its stem in its
workspace group.

The application's normal build setup includes Python support. From
`apps/spell-circle-canvas`, build the host and standalone extension:

```sh
cmake --build build --config Release --target Grimoire sigil_python
```

Open either example by path:

```sh
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/python_hello.py

build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/python_instrument_board.py
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
uv run sigil open sketch.py --grimoire /path/to/Grimoire
```

Use the actual wheel filename and the Python version it targets. Grimoire
is built or installed separately; the wheel provides the bindings,
headless renderer and launcher. `--grimoire` names the executable inside
the application bundle on macOS. It can be omitted when `SIGIL_GRIMOIRE`
names that executable, `Grimoire` is on `PATH`, or the native build is
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

## A window in your own interpreter

`sigil.live` puts a window on screen in the interpreter that imports it.
The sketch draws there, in the same process as the prompt, and its
objects are ordinary variables: nothing crosses a socket and no text is
sent anywhere.

```pycon
$ python -i sketch.py
>>> from sigil import live
>>> window = live.open(size=(960, 540))
>>> hello = window.show(Hello)
>>> hello.accent = "#ff7a6b"
>>> window.still("hello.png")
'hello.png'
>>> window.close()
```

`live.open(size, background=..., title=..., assets=...)` opens the
window. `window.show(what)` draws a compose tree on a canvas the window's
size over `background`, runs a `@sketch` class as Grimoire runs it —
setup, update, draw or describe, its timers, its engine and its feeds —
or runs an instance of one as it stands; it answers the running instance,
which stays reachable as `window.sketch`. A `draw` or `describe` is looked
up on the class at every frame, so an attribute assigned at the prompt,
or a method assigned on the class, is what the next frame draws.
`window.restart()` runs setup again on the same instance, for a sketch
whose setup builds its tree once. `window.still(path, density=1.0)` writes
the sketch on screen as a picture. The session's hub, fonts and `ctx` are
a Grimoire session's, so `ctx.fonts`, `weave.fonts`, bundled faces and
feeds work unchanged; `res://` mounts `assets`.

The window draws on the thread that opened it — the main thread, which
macOS requires of a window — and only while that thread turns it. At
`python -i`, at the prompt of `code.interact`, and at plain `python`'s
own prompt, it turns while the prompt waits for a line: the extension
installs Python's input hook, `PyOS_InputHook`, which the interpreter
calls whenever it waits on input, and which handles the window's events
and draws its frames until a line is there to read. Frames are drawn
inside that wait, on Qt Quick's basic render loop, so a sketch's Python
runs on the prompt's own thread and never beside a line being run. While
a line runs, the window holds still, and it picks up where it was at the
next prompt.

Under IPython in a terminal, `live.open` registers the same wait as an
IPython input hook named `sigil` and enables it, as `%gui` does for a
toolkit. In a Jupyter kernel, a task on the kernel's own event loop turns
the window between cells. A script that is not interactive calls
`window.run()`, which turns the window until the reader closes it, or
`window.run(seconds)`; `window.pump(seconds)` turns it once, and
`live.pumpOpenWindows()` turns every open window once for a loop of the
script's own.

`live.open(..., headless=True)` opens no window at all: each turn steps
the sketch on the wall's clock and `still` photographs it, which is what
a test or a machine without a display opens. The window library and Qt are
loaded the first time a window opens and never by an import, and an
interpreter that cannot reach Qt is told so by `live.open`, naming what is
missing. Grimoire shows its sketches through the same window library.

### A prompt into a window elsewhere

A window in another process, or an interpreter that is not the one
drawing — a remote kernel, a Grimoire already open on a file — is reached
through a prompt that sends its text there. `sigil repl sketch.py` is
Python's interactive prompt, with each input run inside the sketch
Grimoire is drawing, between two of its frames:

```sh
uv run sigil repl sketch.py
>>> COLOR = "#7fd1c7"
>>> SPEED * 2
2.4
```

The input runs in the sketch module's own namespace, beside the running
instance as `self`, its context as `ctx`, and the `sigil` modules the
module did not import itself. An expression prints its value, a statement
what it printed, and a raise its traceback; the sketch runs on either way.
A sketch whose `setup` returns its tree is asked for it again after every
input that ran to its end, and a `draw` or `describe` assigned on the
class is the one the next frame calls. `:reload` imports the file again
as saving it does, `:still path.png` writes the sketch on screen as a
picture, and `:quit` leaves.

The prompt reaches Grimoire through its protocol's `session.evaluate`,
which runs any code a client on this machine sends, so Grimoire opens that
door only when started with `--repl`. With no Grimoire answering, `sigil
repl sketch.py` starts its window on the file with the door open, finding
it as `sigil open` does, and ends it on `:quit`; `--to HOST:PORT` or
`--state DIRECTORY` names a Grimoire already running with `--repl`.

## A sketch as a file of functions

The shortest sketch is a file of functions, the way p5.js writes one: no
class and no decorator.

```python
from sigil.draw import Pen
from sigil.sketch.globals import *

size(640, 360)


def draw(pen: Pen) -> None:
    pen.background("#121720")
    pen.fill("#f1bb7b")
    pen.circle(mouseX, mouseY, 48)
```

`sigil open sketch.py` opens it in a window, and `sigil render sketch.py
-o still.png` takes its still. A file that states no `NAME` is listed
under its stem. The loader wraps the module's functions into the class
the `@sketch` decorator makes, so everything below about a sketch class
holds for a file of functions, and what the file defines decides its kind
by the same rule:

| Function | Called |
| --- | --- |
| `setup()` | once, when the session opens; a tree it returns is rendered |
| `draw(pen)` | every frame, with a full-canvas pen |
| `update(elapsed)` | every step, on the session clock, before the frame |
| `describe(seconds)` | every step, returning the tree at that scene time; annotated `-> world.Frame`, it makes a scene3d sketch |

`setup`, `draw` and `update` may also take the context as their last
argument, as the methods do. `size(width, height)`, `background(color)`,
`captureAt(seconds)` and `theme(look)` declare the canvas, at the top of
the file or inside `setup`; `captureAt = 2.0` as a module constant says
the same. A declaration inside any other hook is refused, because the
canvas is already drawing: `pen.background(...)` paints the ground there.

Before each hook runs, the loader writes these readings into the sketch
module's own namespace, so a hook reads them as plain names; `from
sigil.sketch.globals import *` declares them for an editor and brings in
the four declarations:

| Reading | What it holds |
| --- | --- |
| `width`, `height` | the canvas size in canvas units |
| `frameCount` | frames drawn since the sketch began |
| `seconds` | scene seconds since the sketch began |
| `deltaTime` | milliseconds the last frame took |
| `mouseX`, `mouseY` | where the pointer stands on the canvas |
| `mouseIsPressed` | whether the pointer's button is down |
| `key`, `keyCode` | the name and code of the last key pressed |
| `keyIsPressed` | whether any key is down |

`mouseMoved()`, `mouseDragged()`, `mousePressed()`, `mouseReleased()`,
`keyPressed()` and `keyReleased()`, defined at the top of the file, are
called before `draw` when the pointer or the keyboard changed since the
previous frame. The pointer and the keyboard are read from the pen a
`draw` is handed; a sketch without `draw` reads them off the context's
`pointer` and `keys` before its `update` or `describe` runs, and is
stepped for its input hooks even where it defines no `update`. A function redefined while the sketch runs — at `sigil repl`,
say — is the one the next frame calls, and text run there sets the sketch
up again only where its `setup` returned a tree, so a `setup` that only
sets state up runs once.

`describe(seconds)` returning a tree is the HTML half: the page is the
program, and the host draws it again at every step.

```python
from sigil.compose import Element, box
from sigil.sketch import kit
from sigil.sketch.globals import *

size(480, 200)
theme(kit.featureTheme(kit.Density.Spacious))


def describe(seconds: float) -> Element:
    bar = box().width(40 + 160 * min(seconds, 1.5)).height(24).fill("#8bd0bd")
    return kit.page(bar, title="Growing")
```

A file that declares a sketch class and also defines these functions runs
the class, and the loader warns that the functions are not its hooks.

## A drawing

The class form is the second tier: one class holding the same hooks as
methods, its canvas stated on the decorator.

```python
from math import cos, sin
from sigil.draw import Pen
from sigil.sketch import sketch


@sketch(size=(640, 420), background="#121720", captureAt=2.0)
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
element. A setup that returns a tree renders it, and that tree replaces
the default drawing element, as an explicit `ctx.render(...)` does. An
optional `update(self, elapsed, ctx)` reads elapsed scene seconds before
the next frame.

The decorator reads four hook words off the class, `setup`, `update`,
`draw` and `describe`, and engages each only when the class defines it.
`describe(self, seconds)` returns the sketch at a scene time and takes
exactly one argument, the seconds; a class that defines `describe` with
any other signature is refused with a `TypeError` naming the expected
one. A `describe` that returns a tree makes a scene2d sketch that is
described again at every step, and the reconciler patches what changed:

```python
from sigil.compose import Element, box
from sigil.sketch import sketch


@sketch(size=(320, 120), captureAt=1.5)
class Bar:
    def describe(self, seconds: float) -> Element:
        return box().width(40 + 160 * min(seconds, 1.5)).height(24).fill("#f1bb7b")
```

A `describe` annotated `-> world.Frame` makes a scene3d sketch, described
under [Retained 3D scenes](#retained-3d-scenes). `draw` and `describe`
each render the whole sketch, so a class defines one of them.

`setup`, `update` and `draw` may omit trailing arguments: `setup()` or
`setup(ctx)`, `update()`, `update(elapsed)` or `update(elapsed, ctx)`,
and `draw()`, `draw(pen)` or `draw(pen, ctx)`. The adapter resolves each
signature when the class is decorated. A drawing with two arguments
receives the same checked session context as setup.

`size`, `background` and `captureAt`, the scene time a still is taken
at, declare the canvas. `theme=`, a kit theme, is provided around setup,
update, describe and draw, and stages the canvas with the theme's ground
where no background is given:

```python
from sigil.sketch import kit, sketch


@sketch(size=(640, 360), captureAt=0, theme=kit.featureTheme(kit.Density.Spacious))
class Sheet:
    def setup(self):
        return kit.page(kit.well(width=240, height=160), title="A specimen sheet")
```

The host owns scheduling and teardown. Keep model data freely on the
sketch, and use a pen during the drawing callback that supplied it.
Access through a pen after its callback, or a context after its session
ends, raises an exception. Context and pen operations run on the
session's owning thread.

## Session services

The context exposes checked views of the native services:

- `ctx.composer` renders trees and named slots, reads keyed bounds and hit
  tests, reports settling and cache statistics, and purges caches.
- `ctx.engine` is the one clock the sketch's motion runs on:
  `animate(target, tween)` runs a tween on a live value, `timeline()`
  places tweens and calls in time, and `timer(callback, ...)` runs a
  callback every frame, at a fixed `stepRate`, or throttled to a
  `frameRate`. A timer callback may take no arguments, `delta`, or
  `delta, elapsed`, in seconds; returning `False` stops it and returning
  `None` keeps it. Each answers a playback — `play`, `pause`, `seek`,
  `cancel`, `revert`, `onComplete` and the rest — and a fixed-rate timer
  reports `betweenSteps()`, the render interpolant, and whether a frame
  `droppedTime()`. The host moves the clock; a body never does.
- `ctx.assets.hub().load(media.Image, uri)` reads an owned native image,
  `None` until the file is there; `frameAt` supplies its image, and the
  host sets the sketch up again when a file it asked for appears or
  changes. `hub().load(data.Json, uri)` and `load(data.Table, uri)`
  return owned data snapshots, and `load(data.Database, uri)` a native
  query view. `material.shader(ctx.assets.hub(), uri, parameters)` reads
  an `.sksl` or `.slang` file through the hub as a `Material`, compiled
  anew when the file changes; a file that is missing or does not compile
  paints nothing. `ctx.assets.hub()` provides
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
from sigil.sketch import Scene2DContext


def setup(self, ctx: Scene2DContext) -> None:
    self.input = ctx.assets.hub().listen("udp://:27021")
    if not self.input.state().isOpen():
        raise RuntimeError(self.input.state().error)
    self.level = 0.0


def update(self, elapsed: float, ctx: Scene2DContext) -> None:
    while (arrival := self.input.receive()) is not None:
        document = decodeJson(arrival.payload.decode("utf-8"))
        if document is not None:
            self.level = document["level"].number(self.level)
            reply = encodeJson({"level": self.level}).encode("utf-8")
            self.input.send(reply, to=arrival.sender)
```

`receive()` takes the next queued message and returns `None` immediately when
the queue is empty. `latest()` reads the newest message whole without draining
the queue. A `Message` owns its `payload` and carries
`revision`, `arrivedAt` and `sender`; it remains readable after the feed closes.
`ListenOptions(capacity=...)` bounds the queue, and `state().dropped` reports
overflow. Present connection status from `state()`: its `readiness`, a
`ReadyState`, and its `error`.

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

A listening UDP feed replies to the sender a message names with
`send(payload, to=message.sender)`.
A peer feed such as `hub.listen("udp://127.0.0.1:27021")` uses `send` to reach
its configured destination. A WebSocket listener broadcasts with `send` or
addresses one peer with `send(payload, to=peer)`. These calls return whether the transport
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

Standalone Python uses the same bindings without a Grimoire process:

```python
from sigil.io import Hub, registerTransports

hub = Hub()
registerTransports(hub, ["udp"])
peer = hub.listen("udp://127.0.0.1:27021")
try:
    if not peer.send(b'{"sequence": 1, "pressure": 0.6, "flow": 0.4}'):
        raise RuntimeError(peer.error() or "Could not send readings")
finally:
    peer.close()
```

`registerTransports(hub, ["udp"])` installs only the transports that answer
the schemes named; `registerTransports(hub)` installs the complete native
set. A standalone program calls `hub.advance(time)`
when replaying recordings. Sketches leave that call to their host.
`feed.record(path)` writes arrivals for later playback and returns a
`Recording` that stops when it is stopped, when it is garbage collected, or
at the end of a `with` block. `hub.replay(uri, path)` plays that file back at
the feed URI — before or after the URI is opened — so the same authoring code
reads the same input again. `RecordingWriter` and `readRecording` expose the
recording format for generated fixtures, and a test that stands in for a
transport puts messages on a feed through `io.testing.inletOf(feed)`, whose
`deliver` a reader of the feed does not have.
Use a recording or explicit sample data when `ctx.deterministic` is true.

The `wardnet_live` sketch in the catalogue reads JSON messages through a UDP
door and a WebSocket door, replies to every sender with what it understood,
and replays a recording when `ctx.deterministic` is true. The packaged
companion sends it a load and rows through a standalone SigilIO hub, prints
each reply and can export the last one when requested:

```sh
uv run python -m sigil.examples.tools.send_live_signals
uv run python -m sigil.examples.tools.send_live_signals --export output/readings.json
```

### Any Python library, on the sketch's own loop

A SigilIO door is one way in; any asyncio library is another. Every
scene2d sketch has `self.loop`, an asyncio event loop made the first time
it is asked for and given one turn a frame on the sketch thread, before
`update` and `describe`: the turn polls the loop's sockets without
waiting and runs every callback that is ready. A task started on it
resumes on the thread that runs the hooks, so it writes what it receives
onto the sketch as ordinary attributes and the next description reads
them, with no worker thread, queue or encoding between the library and
the page:

```python
import asyncio
import json

from sigil.compose import text
from sigil.sketch import Scene2DContext, sketch


@sketch(size=(640, 120))
class Price:
    loop: asyncio.AbstractEventLoop  # supplied by @sketch

    def setup(self, ctx: Scene2DContext) -> None:
        self.price = "waiting"
        if not ctx.deterministic:
            self.loop.create_task(self.listen())

    async def listen(self) -> None:
        import websockets

        uri = "wss://data-stream.binance.vision/ws/btcusdt@trade"
        async for connection in websockets.connect(uri):
            async for message in connection:
                self.price = json.loads(message)["p"]

    def describe(self, seconds: float):
        return text(self.price, size=48)
```

When the session ends every task still pending is cancelled, given a
quarter of a second to unwind, and the loop is closed. A library that
runs a thread of its own hands each result over with
`self.loop.call_soon_threadsafe(method, value)`, and a blocking call
goes to `asyncio.to_thread`. A class that defines its own `loop` keeps
it; a scene3d sketch, whose frame is a function of the scene time, has
none. A capture never awaits the network: under `ctx.deterministic`,
replay a recording through the same methods a task calls. The
catalogue's `python_market_tape` sketch feeds one page from
`websockets` and `httpx` this way and replays its recordings for a
still.

### Publish the rendered canvas

Python sketches use Grimoire's existing frame output. In a matching Python
environment, open the sketch with a named publication:

```sh
uv run sigil open sketch.py --publish "Live Sketch"
```

On macOS, a Syphon client subscribes to `Live Sketch` and receives THE
SKETCH'S OWN CANVAS: the size the sketch declared, at one texture
pixel per canvas unit, over the ground it declared and with none of the window
around it. `--publish` without a name publishes under the sketch file's stem.
Ctrl-P toggles output in the window, and the status line shows the publication
name. Drawing with either Python or C++ uses this same path.

Any Syphon client checks it: the client lists what is being offered by name,
and `Live Sketch` among them is the sketch's canvas.

A ground with alpha below one publishes as it reads, so a sketch meant to be
composited over another application's scene declares one:

```python
@sketch(size=(640, 360), background=(0, 0, 0, 0))
class Overlay:
    ...
```

`background` takes a `Color`, a CSS string, or a sequence of three or four
channels between zero and one; the fourth is the ground's alpha, and zero
publishes a fully transparent ground. `python_alpha_ground` in the sketch
directory is the example.

Frame publication belongs to the host that owns the GPU. SigilIO handles
resource and data bytes; it does not turn image bytes into a Syphon stream.
Grimoire must be running on its Metal/Graphite backend to publish. The
standalone `sigil render` command remains a headless PNG renderer and does
not start a persistent GPU publisher.

## A retained composition

Components are ordinary Python functions returning native elements:

```python
from sigil.compose import box, column, row, text
from sigil.motion import animate
from sigil.sketch import sketch


def metric(label, value, accent):
    return (
        column()
        .gap(10)
        .padding(24)
        .fill("#1b2735")
        .borderRadius(16)
        .flexGrow(1)
        .opacity(animate(0, 1, duration=0.6))
        .children(
            text(label, size=13, color="#92a4b6"),
            text(value, size=42, color=accent),
        )
    )


@sketch(size=(800, 320), background="#101923", captureAt=1.0)
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
as `.alignItems("center")`, `.fontSize(18)` and `.translateY(12)`; the
layout keywords CSS writes as words take the word or the enumeration alike,
`.overflow("clip")` beside `.overflow(compose.Overflow.Clip)` — and
`.overflow("hidden")`, CSS's most common spelling, which clips as `"clip"`
does — and an unknown word raises `ValueError`.

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
Font selection follows the host's native font context. `fontFamily` takes
CSS's family list as one string, on an element, a rule and a `SpanStyle`
alike: `.fontFamily("Inter, Helvetica Neue, sans-serif")` sets the run in
the first family installed, and the generic names `serif`, `sans-serif`,
`monospace` and `system-ui` are the platform's own families. A font file
the sketch carries is a family too once its hub has loaded it, as the
fonts passage under Typography shows.
`textShadow(TextShadow(offset=(2, 2), blur=4, color=…))`, or a list of them,
casts a shadow from each glyph, CSS's `text-shadow`, which fades, tints and
deforms with its glyph; its `color` is a colour, `Fill.var(name)` read where
the shadow is stated, or by default the ink in force. `compose.textStyleOf(root,
key)` answers the `weave.TextStyle` the keyed leaf is set in, a typeface's
`variationAxes()` the `weave.VariationAxis` ranges it carries, and a paragraph
states its page's bidirectional base with `Paragraph.setBaseDirection` or
`ParagraphBlock(baseDirection=…)`. Explicit retained
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

A node states typed FACTS about itself with `attribute(name, value)`, or a
whole `Attributes` table with `attributes(facts)`, and says nothing about
who reads them. The value reads back in the type it was stated in: a
bool, a whole number, a number, a string, a list of either, or a
`skia.Point`, or a `compose.pin.Request` — a sequence of numbers is a list of numbers whatever its
length, so a fact meant as a point is stated as a point. `point()` is a
node with no extent that exists to carry a key and facts.

`operators([...])` is what reads them. Each entry is a stock arranging
value, one of the kit's adders, or a Python object of your own. The adders
are `compose.connect.Between`, `ByLane` and `Along` — the last a wire
through a run of stops, each an `Anchor`, a key, a `(key, (x, y))` pair or
a free point; `compose.pin.ByLane`, which hangs the element of every
`pin.Request` a node states under its lane; `compose.outline.Around` and
`Hull`; and `compose.stamp.ByLane(lane, make, key=)`, which attaches what
`make(node)` answers to every node stating the lane and prunes by its key:

```python
import math
from dataclasses import dataclass
from sigil import compose

@dataclass(frozen=True)
class AroundRing:
    fraction: float = 0.8

    def arrange(self, arrangement):
        middle = arrangement.box.centerX(), arrangement.box.centerY()
        radius = min(middle) * self.fraction
        for child in arrangement.children:
            degrees = (child.number("hour") or 0.0) * 30.0 - 90.0
            radians = math.radians(degrees)
            child.centreAt((middle[0] + radius * math.cos(radians),
                            middle[1] + radius * math.sin(radians)))
            child.turn(degrees + 90.0)

dial = compose.box().operators([AroundRing()]).children(hours)
```

An object with `arrange(arrangement)` places the node's direct children
during layout, reading each one's measured size and facts; one with
`add(scope)` runs once layout has settled and attaches elements to a node
of its scope or to the scope itself. A value cannot do both, because the
two run in different phases of one layout. THE CLASS'S OWN EQUALITY IS
WHAT LETS THE NODE PRUNE: a frozen dataclass compares by its fields and
an unchanged tree patches nothing, while a plain class equals nothing but
itself and its node is described afresh every frame.
`compose.Operator(value)` is the same value with `zIndex` and
`styleClass` on it, which are where an adding operator's attachments
paint and what dresses them. `compose.drawWith(program)`, keyed or not,
is the imperative door: one pen over the scope's box, handed the pen and
the scope. Both the arrangement and the scope are lent for the one call
and refuse every reading once it has returned.

The specimen kit supplies the native page furniture and theme:

```python
from sigil.compose import text
from sigil.sketch import kit

look = kit.houseTheme()
look.palette.ground = "#f4f0e6"
look.palette.ink = "#30372f"
look.type.title.size = 28

# Inside setup(self, ctx), where the decorator states no theme:
with kit.provide(look):
    kit.stage(ctx, size=(640, 360), captureAt=0)
    picture = kit.well(width=240, height=160).children(text("A specimen"))
    content = kit.caption(picture, label="Native type", note="One shared theme")
    page = kit.page(content, title="A specimen sheet", footer="Sigil")
ctx.render(page)
```

`@sketch(theme=look)` says the same in one place: the decorator provides
the theme around every hook and stages the canvas over it.

`stage`, `page`, `well`, `caption`, `cell`, `cells` and `panelGrid` call
SigilSketchKit. The neutral `sigil.compose.kit` supplies native wells,
captioned cells, sheets, panels, boards, construction circles, lines and
ladders. Both return the same native `Element`. Python normalizes keyword
fields and ordered children; native components own arrangement and
painting. The wrappers take each field by its native name, as the native
record classes such as `Page`, `Well` and `Spacing` spell it: `paddingVertical=`,
`rowGap=`. A snake_case spelling such as `padding_vertical=` is refused with a
`TypeError` that names the field it missed.

The rest of the sketch kit is bound the same way, each piece a record of
keyword properties and the function that sets it: `legend(*entries)` over
`LegendEntry`, `swatchStrip` and `chip`; `meter` and `gauge`; `ticker(content)`
and `timeline(*marks)` with `Timeline.Mark`; `scrollbar` over a `Scrolled`;
`backdrop`, `panel(content)` and `frame(screen)`; `titleCard`, whose lines
may be given as their words; `console(*rings)` over `compose.feed.TextRing`s;
`instrument(picture, readings)`; and `connectionReadout(connection)`, a
door's state as the compose kit's `readout` rows, followed by the sketch's
own `compose.kit.Reading`s. A chart is `plot(key, frame, layers)` over a
`Plot` of two `data.Scale`s: `axis`, `rules`, `trace` (a function of one
number or a measured series), `area`, `path` and `label` take keywords;
`marks`, `bands` and `segments` read each row through `x=` and `y=`
functions; and a Python function of the frame, the key and the index is a
layer too. `python_kit_pieces` sets nine of them on one sheet.

`houseTheme()` supplies the default stock theme; `studyTheme()` supplies
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
from sigil.compose import StyleSheet, rule
from sigil.compose import document
from sigil.weave import ParagraphBlock, Leading, Type, em

content = document.article(
    document.eyebrow("FIELD JOURNAL"),
    document.h1("A field worth noticing"),
    document.paragraph("An ordinary observation can become a small story."),
    document.list(document.item("Notice the light."), document.item("Follow its movement.")),
    document.quote("Let the words find a comfortable measure."),
)
look = StyleSheet([
    rule("h1").font(Type(size=34, color="#88492e")),
    rule("paragraph").paragraph(ParagraphBlock(leading=Leading.multiple(1.45))),
    rule("quote").font(Type(color="#687969")),
])
page = content.applyStyleSheet(look).var(document.measure, em(34))
```

A rule for the bare word `h1` styles every heading with that role in the
subtree; `.name` names a class. The cascade applies inherited type, the
component's fallback role, the matched rules by CSS's specificity — a class
over a role — then direct `font`/`paragraph` declarations.
Build the children before their parent if useful; role styling resolves where
they land. A nested stylesheet can change one region's voice. The native
specimen theme uses `h1`, `lead`, `h2`, `label`, `caption`, `eyebrow` and `footer`
for its document lines; its editable type-register field names remain `title`,
`subtitle`, `captionLabel` and `captionNote`.

`article` bounds the reading measure. The inherited properties `document.measure`,
`document.gap`, `document.listGap` and `document.quoteInset` control document geometry;
set them with `.var(name, value)` on an ancestor or the document itself.
Fallbacks are 38 em, 1 em, 0.4 em and 1 em respectively. A custom component can
declare a role using `.role("notice")` or `.role("notice", font=Type(...))`
and fallback properties with `.varDefaults({...})`. An ancestor's authored
property takes precedence over those fallback values.

Document containers accept individual Elements or one iterable, including
lists, tuples and generators. `document.quote("words")` is also valid;
`document.paragraph(rich_text)` retains a single native rich passage. `document.item`
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
    return Paint.linearGradient((0, 0), (1, 1), [(0, accent), (1, "#172b36")])


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

Every property that can move takes an `Animatable`: a constant, a
described motion, a live value somebody writes, or a live value followed
through a `Binding`. `animatable(value)` makes a live value — a number, a
colour or a fill — and every copy of it reads and writes one cell, so a
property handed it follows whatever writes `.value` next, from model logic
or an engine timer. Descriptions share that cell, so collecting the Python
wrapper leaves nothing dangling in a retained tree.
Material's `Light(color=...)` takes those same color cells; its read-only
`color` property returns the current Color sample.
`bind(source, from_=(a, b), alternate=..., envelope=..., ease=...,
quantize=..., reverse=..., to=(c, d), wrap=..., wiggle=Wiggle(...),
clamp=(e, f))` follows a live number through those stages, which run in
that order whatever order they are named in; `envelope.cosine()`,
`square()`, `trapezoid()` and `shaped()` shape the phase.

A curve is part of a binding's identity, so a tree described again with an
equal binding patches nothing. A native ease from `sigil.motion.ease` and an
`Oscillator` are values: handed to `ease=` or `envelope.shaped(...)`, they
compare by their settings, and two bindings built the same way compare
equal. A Python function is held as an opaque callable — nothing outside
the function can say what it computes — so a binding shaped by one compares
equal to no other, itself included, and a node bound through it is patched
every time it is described; state the curve as an `Oscillator` or a native
ease where it can be one.

Stillness is asked in three words. `value.isRunning()` says a value is
DECLARED to move — a live value always is. `motion.isRunning(held, value)`
says it is moving NOW: a `motion.HeldMotion` is the motion a consumer keeps
for one number, started or bent by `motion.retarget(engine, held, previous,
next, transition=0.3)` or `motion.enter(engine, held, value)`, and it runs
while a motion is writing it and stops when it lands; `motion.valueOf(held,
value)` reads the number this frame. Whether a value has provably held
still is the third question, which only comparing it across frames
answers; an engine's own `isRunning()` asks whether anything it runs is
declared to move.

`animate(start, end, duration=..., delay=..., ease=...)` declares an
entrance; the start and the end come first by position, because `from` is a
word Python reserves, and `from_=` and `to=` name them by keyword.
`animate(to=target, ...)` alone eases the property whenever a later
description changes its target; `keyframes=[Keyframe(value,
duration=...), ...]` declares a path, which is an entrance too: with no
`from_` it sets out from where it rests, and a first keyframe lasting
nothing is where it visibly begins. `Tween(start, end, ...)` is the same
description as a value, which `animate(tween)` and `ctx.engine.animate` take. `from_`,
`to`, `duration` and `delay` each take a `stagger(...)` as well — a delay
ladder, a spread from the first child's value to the last's, or a table of
`cues` — which the host resolves at each child's `Place`. A length of time
is a number of seconds or a `datetime.timedelta`, and one read back is a
number of seconds. Native easing values live in `sigil.motion.ease`.

`.exit()` on a keyed element says it leaves as it entered. When a later
description leaves it out, it keeps its place in the layout, answers no
hit, and plays every entrance under it backwards — each `animate(from_=a,
to=b)` back to `a` over the same duration on its curve run backwards, and
a text entrance's track back to the start of its schedule — then goes on
the frame the last of them lands. Rows left out together leave as their
stagger reversed, the last in the first out; one named again before it
has gone turns round where it stands. An element with no key or with
nothing that entered goes at once.

`sigil.weave` supplies native `Type`, `TextStyle`, `ParagraphBlock` and `TypeSheet`
values. A partial type inherits unspecified fields; a complete text style
describes its own look. A page applies its theme's stylesheet on its root,
and `styleClass` on a native element names a class its rules speak about.
Mixed runs and paragraph settings use the same native values.

### Selector sheets

`sigil.compose.StyleSheet` is the one sheet an author writes: rules whose
`compose.ElementSelector` says which elements they speak about, put in
force with `Element.applyStyleSheet`.

```python
from sigil.compose import StyleSheet, box, rule, text
from sigil.weave import Type

house = StyleSheet([
    rule(".card").font(Type(size=18)),
    rule(".card > .title").ink("#2d1b12"),
    rule(".row:nth-child(odd)").var("tint", "#f4efe7"),
])
page = box().applyStyleSheet(house).children(
    box().styleClass("card").children(text("A title").styleClass("title"))
)
```

`compose.selector(cssText)` parses CSS's own grammar — a bare word is a
role, `.name` a class, `*` any element, with the combinators `>`, `+`, `~`
and a space, the structural pseudo-classes, `:is`, `:where` and `:not`,
and `:has`, whose relative selectors open with `>`, `+`, `~` or nothing.
The builders under `sigil.compose.select` produce the same value name by
name: `styleClass`, `role`, `any`, `is_` (spelled with the trailing
underscore Python's keyword forces), `where`, `notAnyOf` and `has`, with
`child`, `next` and `sibling` opening a `compose.RelativeSelector` — a
type only `has` takes, so a relation cannot open a rule; a plain selector
stands for one reached anywhere underneath. The
operators are the house's three — `a | b` a selector list, `a & b` a
compound on one element, `~a` a negation — and the combinators stay named
methods, because they relate two elements rather than combining sets on
one. `ElementSelector.specificity()` answers the `compose.Specificity`
pair CSS weighs, which compares and orders; a text the library does not
read warns once and matches nothing, which `matchesNothing()` reports.
Sheets join with `+` and may hold one another. A rule's verbs are a
node's own, bound once for both — the box (`padding`, `width`,
`borderRadius`, `fill`, `opacity`, the 2D transform and the rest), the
cascade (`font` and its longhands, `paragraph` and its longhands, `ink`,
`var`) and the text properties (`maxTextLines`, `textStroke` and the
rest), which a rule states for the text leaves it matches — and the
node's own verb stands over the rule's: `rule(".card").padding(8)`.

**An applied sheet sees only its own subtree.** Every compound of a
selector — the subject and every ancestor or sibling it names — must match
the applying node or something below it, so a sheet that must name an
outer element is applied at or above that element. The applying node is
inside its own sheet and is that sheet's `:root`. Among matched rules the
order is CSS's, specificity first; the node's own `font`, `paragraph`, `ink`
and `var` still stand over all of them.

## Typography

Compose accepts a native `RichText` value. Each added run carries plain text,
a partial `Type`, a complete `TextStyle`, or a class name. Children remain
composed elements; an inline object reserves its space with `slot` and matches
a child by its key:

```python
from sigil.compose import box, text
from sigil.weave import ParagraphBlock, Leading, Type, rich

passage = (rich().add("A ").add("warm", Type(color="#e39a60"))
           .add(" signal ").slot("status", (12, 12)).add(" arrives."))
label = (text(passage).width(320).font(Type(size=22, color="#d8e3e8"))
         .paragraph(ParagraphBlock(leading=Leading.multiple(1.3)))
         .children(box().key("status").fill("#8bd0bd")))
```

`rich()` inherits its base from the element's type cascade; `rich(textStyle(...))`
states a complete base. A named run resolves through the rules of the sheets
in force, as a virtual child of its text leaf whose class is the name, or
through a `weave.TypeSheet` the value names with `styles()`. `span` restyles a
selection from `weave.selectors` with a `compose.SpanStyle` — the font and
ink verbs, re-shaping only where a shaping field is stated; Compose's `selectors.style` and `selectors.inFrame`
address named runs and story frames. Selector ranges use native UTF-16 offsets.

`ParagraphBlock` exposes leading, alignment, justification, hyphenation settings, tab
stops, CJK line tables and writing mode. `HyphenationOptions(patterns=...)` holds
the table a word breaks by, so `hyphens(HyphenationOptions(patterns=kit.englishHyphenator()))`
hyphenates a text leaf by pattern; `weave.kit.englishHyphenator()` is one shared
English table. `weave.kit.hanging.latin()`, `hanging.japanese()` and
`kinsoku.japanese()` are the stock line-edge tables a block's `hanging` and
`kinsoku` take. `ParagraphStyle` adds per-paragraph
spacing, indents, keeps, reservations and an initial letter. Give
`paragraphStyles` a list or tuple of those styles, or of stylesheet names.
`Story(passage)` with `frame(story).key(...).textThreadTo(...)` flows that same
content through several frames. `textFirstBaseline`, `textVerticalAlign`,
`initialLetter`, `textLineMargin`, `textWillChange`, `maxTextLines` and
`textOverflow` use the native layout controls.

Glyph underlays, overlays and line decorations belong to `PaintStyle` or a
partial `Type`; a `PaintStyle`'s `foregroundMaterial` is an optional
material for the main glyph pass, which its copies share.
`PaintLayer` accepts a configured Skia paint and an offset;
`Decoration` describes an underline, overline, strike or highlight. Optional
records and array properties are copies: edit and assign them back. A rich
value's `runs()` and a story's `blocks()` return independent snapshots.
`TextPath` and `textOnPath` place text on a native shape — a
`geometry.path.Outline` such as `path.curveThrough(points)` as well as a Skia
path — and accept shared motion values for progress.
`TextPath(fit=TextPath.Fit.Size)` scales a run's type until it girds its
baseline's whole length, and `Fit.Spacing` spreads its letters instead,
so a ring run meets itself at the seam; `fill` is the share of the length
the fitted run takes, and `Fit.None_` leaves the run as it is set.
`Composer.beatsOf(key, trackIndex)` reads a track's schedule back as
`compose.Beat` values — the unit, its start, its local progress and the
rect the text put it in — and `Composer.units(key, selector, unit)` the
units a selector addresses as `compose.TextUnit` values.

The native entrance runs on a text leaf's glyphs as a track:
`text("KINETIC").textFx(textFx.entrance(textFx.rise(26)))`. A tween of
`textFx.Displaced` stops says where each glyph starts and how it comes home
— the stock `rise`, `slide`, `pop`, `spinIn`, `scatter`, `typeOn` and
`variableAxisSweep`, or a `textFx.DisplacedTween` of your own — and its
duration and stagger are the track's schedule; a `compose.Track` passed
beside it says which glyphs, by what unit, and the master `progress`, which
binds any motion value. It plays once when the text mounts: a track whose
`progress` is left at its default of one runs from 0 to 1, straight, over
the span its schedule takes on the laid-out text (`Track.progressOverSpan`),
so the last glyph lands as the run ends; a progress the track states, such
as `compose.Track(progress=reveal)`, stands as written.

Every other effect a track runs is a `textFx.TextEffect` value, handed to
`Text.textFx` with the track's other fields as keywords —
`text("DECODE").textFx(textFx.scramble("0123456789"), progress=reveal)` —
or set as `compose.Track(effect=...)`. `scramble` churns each glyph through a
charset before it lands; `tween` walks `textFx.GlyphModifier` stops, a bare
stop taking an equal share of the path and a `GlyphModifierKeyframe` naming
its own share and curve, and each of a stop's three colour terms is a colour,
the ink in force (`Fill.currentInk()`) or a custom property (`Fill.var(name)`)
read where the glyph is painted, so a table follows the sheet; `tint` reveals
colour stops toward the colour the leaf is set in; `hold` paints nothing
before a unit's beat opens; `sequence` plays phases written
`effect.until(0.4)`, each with an optional `crossfade`, one after another in
local time; `mix` runs effects at once; `variableAxis` holds a variable axis;
`enter` is an entrance tween as an effect for the combinators; and `waveLoop`
bobs the glyphs on a wrapping phase. `Text.variationDrive` drives a variable
axis from a number or a motion value, and `Text.textAttach` anchors a node to
the unit a selector resolves to.

Direct editable paragraphs, font contexts, native flow geometry and annotation
values are available. An effect body written in Python, a shader pass as a
track's effect, and custom Python implementations of flow/hyphenation remain
unfinished. A paint layer's `material` and a paint
style's `foregroundMaterial` are shaded over the glyphs their pass covers by
the resolver a sketch session installs; drawn where no resolver is installed,
the pass keeps its Skia paint. The parity table tracks the remaining work
independently of which native libraries are linked.

### Fonts

A family is named, the way CSS names one: `.fontFamily("Avenir Next,
Helvetica Neue, sans-serif")` on an element, a rule or a span sets the run
in the first family the session has. A font file the sketch carries is one
of those families after one line that loads it:

```python
from sigil import compose, weave


def setup(self, ctx):
    ctx.assets.hub().load(weave.Face, ctx.local("data/MachineType.ttf"))
    ctx.render(compose.text("SIGNAL 42").fontFamily("Muthur Study"))
```

The hub reads the file as it reads an image, keeps the face and watches the
file, and the session's font context adopts the face under the family the
file names, so every later `fontFamily("Muthur Study")` — and Weave's own
`ctx.fonts.familyTypeface("Muthur Study")` — answers that held face. A file
edited on disk is read again, its new face takes the family over, and the
declaration that re-runs is set in it. Load the files a sketch names; a
host shares one font context among the sessions it opens, so a family
another sketch loaded may answer too, but only by the order they ran. Where
the face is already in hand, `.fontFamily(face)` states it directly, with
no lookup by name.

#### Exploring type

`weave.fonts` reads the type there is as values to print and look through.
At a prompt with a window open, the whole path from a family's name to a
line set in one of its faces is a handful of lines:

```pycon
>>> from sigil import compose, live, weave
>>> [name for name in weave.fonts.families() if name.startswith("Avenir")]
['Avenir', 'Avenir Next', 'Avenir Next Condensed']
>>> avenir = weave.fonts.family("Avenir")
>>> [style.name for style in avenir.styles]
['Book', 'Roman', 'Book Oblique', 'Oblique', 'Light', 'Light Oblique', 'Medium', 'Medium Oblique', 'Heavy', 'Heavy Oblique', 'Black', 'Black Oblique']
>>> avenir.style("Roman")
Style(Roman · 400 Normal · width 5 · upright)
>>> avenir.style("book oblique")
Style(Book Oblique · 400 Normal · width 5 · italic)
>>> book = weave.fonts.find("Avenir", "Book")
>>> roman = weave.fonts.find("Avenir", "Roman")
>>> book, roman
(Face(Avenir, 400), Face(Avenir, 400))
>>> book == roman
False
>>> weave.fonts.find("Avenir", weave.FaceStyle(weight=400)) == book
True
>>> print(weave.fonts.find("Avenir", "Hairline"))
None
>>> window = live.open(size=(640, 200))
>>> specimen = window.show(compose.column(
...     compose.text("Avenir Book").fontFamily(book).fontSize(48),
...     compose.text("Avenir Roman").fontFamily(roman).fontSize(48),
... ))
>>> window.still("avenir.png", density=2)
'avenir.png'
```

`weave.fonts.families()` lists every family name, and
`weave.fonts.family(name)` one family: `print` it for a specimen line, or
read its `styles`. A family's `style(name)` answers the style its font
names so, and `weave.fonts.find(family, name)` the face under it, with
case and spaces aside, so "book oblique" finds "Book Oblique"; either
answers None where no style goes by the name. The font's own name is the
way to a face CSS cannot pick: Book and Roman are both an upright 400, so
`find` at a `FaceStyle` reaches only one of them, and the name reaches
either. `.fontFamily(face)` sets a text in the face
found, and so does `pen.textFont(face)` in a drawing.

At `sigil repl sketch.py` the same lines run inside the sketch Grimoire
is drawing: `weave` is already in scope, and a face found there goes into
the tree the sketch's `setup` returns or into its `draw`.

A `Style` carries `weight`, its CSS name `weightName`, `width`, `slant`,
the font's own `name` for it and, for a bundled face, the `uri` it was read
from; `style.faceStyle()` is what `find` takes. A `Family` carries its
`styles`, the variation `axes` its faces carry and whether it is
`bundled`. Handed a font context — `ctx.fonts` in a sketch — each answers
the families its hub loaded first, marked bundled:
`weave.fonts.family("Muthur Study", ctx.fonts)`, and
`weave.fonts.find("Muthur Study", "Regular", ctx.fonts)`, where a family
the context adopted answers alone. `weave.fonts.weightName(600)` is
"Semi Bold".

#### Faces and styles

Underneath a family name is a face, `weave.Face`, as in C++: the load
above answers one, `kit.houseFace(kit.Voice.Book)` answers one, a theme's
`type.sans` and `type.mono` are its two (None is the default family), and
`Theme.sans(size, color)` and `Theme.mono(...)` set a line in either. A
face is taken wherever a Skia typeface is and the other way round, since
the two hold one typeface shared; `face.typeface()` and
`weave.Face(typeface)` say so explicitly, and `load(skia.Typeface, ...)`
answers the same face as a typeface. A face goes into a style as
`weave.Type(face=face)`, which `.font(...)`, a style sheet's rule,
`pen.textFont` and a shaping style's `typeface` take. The hub keeps one
face per resource, so a C++ sketch asking `hub.load<weave::Face>` for the
same URI gets the same face. A font context of a program's own,
`weave.FontContext()`, adopts a face by hand with
`fonts.adopt(face, origin)`, and an owned hub knows no font until
`weave.registerDecoders(hub)` puts the decoder on it.

## Data values and native resources

`sigil.data` supplies native `Json`, `Column`, `Table`, `Scale` and database
query values. Ordinary Python dictionaries and lists can construct JSON;
`toPython()` returns ordinary Python data. Native decoders read JSON and
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
`Filter` is the effects stage over a rendered layer — blurs, shadows, glow,
`bloom(BloomOptions(...))` and `glass(GlassOptions(ior=..., thickness=...,
sampleRadius=..., normal=..., normalDirectX=...))`, which refracts the layer
through an encoded normal map while keeping its alpha; `ior` and
`thickness` take `set` and `bind`, and `sampleRadius` is fixed when the
filter is made.

A custom SkSL paint can also be authored in Python. Compile a shader through
`sigil.skia.RuntimeEffect.MakeForShader`, keep that effect, and make paint
instances with `material.Paint.sksl(effect, uniforms)`. Reusing the compiled
effect avoids compiling source on each description. Use a fresh instance
or `paint.copy()` before changing uniform values or child slots on a shared
paint.

The native `sigil.material.surface.SurfaceParameters` record and its
`program` and `unlit` factories describe mesh surfaces: base color, roughness, metallic
response, emission and transmission. A surface is shaded by the World device
executor, which a scene3d sketch reaches by describing a `world.Frame` on a host
that brought a device up. The returned `sigil.material.Material`
is the same value a World element accepts. `surface.normalFromHeight(height,
HeightNormalOptions(depth=..., step=..., directX=...))` differentiates a
grayscale height into an encoded normal map, and `surface.blendNormals(base,
detail, NormalBlendOptions(...))` reorients a detail normal map into a base
one, each map's green-axis convention stated on the options. The full
recipe-definition API, uniform blocks, SDF catalogue, texture maps and
environment maps remain unbound. Material values support copying and
conversion into a paint or effect.
Material and paint `bind` takes a number, an `Animatable` or a `Tween` for a
scalar uniform, and for a colour uniform any colour spelling — a `Color`, a
CSS string, an RGB or RGBA tuple or list — or a `ColorAnimatable` or
`ColorTween`. A bound value is one number or one colour, so every sequence
`bind` is given is read as a colour. `ColorAnimatable` retains its shared
cell through copies; a constant colour writes a constant four-component
binding. Paint's `set` and `sksl` uniform dictionaries accept typed color
motion values too; there numeric tuples and lists keep their array meaning,
and a colour is the colour class or a CSS string. Native frame uniforms retain their time,
resolution and content-scale meanings; effect bindings accept scalar motion.
The `material.shader` parameter dictionary or named tuple also accepts
scalar and color motion values. Its field layout follows their sampled
types, and the constructed material retains their live cells. CSS strings
declare color fields; numeric four-tuples declare ordinary four-component
vector fields.
New backend-neutral recipes and unbound material catalogues still require
C++ bindings.

`sigil.media.fromRgba(buffer, width, height)` copies a contiguous RGBA byte
buffer into an immutable native image. It accepts `bytearray`, `memoryview`
and compatible NumPy arrays. This is the bridge for Python simulations:
compute with an array library, then hand the finished pixels to the pen in
one call. `media.decode(media.Image, data)` answers the decoded document —
still or animated, `frameAt(elapsed).image` the picture at a time — and
`media.encode` its bytes; `load`/`save` provide ordinary Python file
access, the suffix choosing the format. A `media.Video` is opened through a
hub, `hub.load(media.Video, uri)`, and read the same way; `media.Encoder`
appends pictures and finishes as a movie's bytes. Native `weave.Type`, relative lengths and
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
and model transforms; a mesh is drawn as a `world.mesh` body, lit by the
World. A Python function can define the parametric surface for
`mesh.grid`; native geometry computes its tessellation and normals. Mesh
array properties are copied Python values: assign the changed array back
to update the native mesh.

`mesh.pop` builds point-operator chains: `pop.on(loop)`, `pop.on(model,
count)` or `pop.on(chain)` enters points, each verb (`count`, `spread`,
`window`, `jitter`, `vary`, `fade`, `select`, `masked`, `keep`, `twist`, …)
appends one operation over lanes named by `pop.Attribute` or a string, and
`chain.instances(stamp, attributes)` or `chain.stamps(stamp)` cooks it on the
CPU executor into the `mesh.Instances` or `mesh.Mesh` a World body wears.

## Retained 3D scenes

`sigil.world` exposes native retained scene declarations. A scene3d
sketch is a class whose `describe(self, seconds)` is annotated
`-> world.Frame`: the host describes it at every step and draws the frame
on its device, as it draws a C++ scene3d sketch, and lists the sketch
unavailable where there is no device. The host reads the kind from the
file's text when the sketch is listed, by the same rule the decorator
applies when it loads the class, so a scene3d sketch states nothing else.
An optional `setup(self, ctx)` is handed a `sketch.Scene3DContext` while
it runs: `ctx.camera(lens)` is the lens a frame holding no camera of its
own is seen through, and `ctx.page(...)` and `ctx.textureScene(...)` hand
out the pages and screens its bodies wear. The context answers only
during setup; `describe` is a function of the scene time and is handed
nothing else. A scene3d sketch has no `update` or `draw`, and its setup
returns nothing. Import meshes and cameras from `sigil.geometry.mesh`,
materials from `sigil.material`, and animation values from `sigil.motion`.
These are the same values used by the direct bindings.

```python
from sigil import world
from sigil.geometry import mesh
from sigil.material import surface as surfaces
from sigil.sketch import Scene3DContext, sketch


@sketch(size=(640, 480), background="#141d25", captureAt=1.0)
class Vessel:
    def setup(self, ctx: Scene3DContext) -> None:
        self.finish = surfaces.program(
            surfaces.SurfaceParameters(baseColor="#87b9ad", roughness=0.45)
        )
        self.torus = mesh.torus(65, 20)
        ctx.camera(mesh.camera.Camera(eye=(0, 120, 420), target=(0, 0, 0)))

    def describe(self, seconds: float) -> world.Frame:
        model = (world.mesh(self.torus).key("vessel")
                 .fill(self.finish).rotateX(25).rotateY(seconds * 20))
        return world.Frame(world.kit.litSet(model, world.kit.Set(ground=0),
                                            now=seconds))
```

A scene2d sketch can still show one frame as a picture:
`ctx.bakeScene3D(frame, size, camera=…, background=…, at=…, density=…)`
draws it on the device the host brought up and answers `None` where there
is none, and a class that bakes declares `needsDevice = True` on a line of
its own so the host brings the device up before the sketch opens. `size` is
the panel the picture fills, in canvas units; the image is `size` times the
density in pixels, and the density is the one the sketch is drawn at — a
plate's capture density, a window's pixel ratio — unless `density=` states
one. Paint the image stretched over `size`, as
`compose.image(still).width(…).height(…)` or `pen.image(still, x, y, width,
height)` does, and it lands on the device's pixels.

A scene is typed nodes: `world.group(...)` holds children, given as
variadic nodes or one iterable, including a tuple; `world.mesh(m)` is a body
that takes `fill`, `backface`, `castShadow` and `receiveShadow`;
`world.directionalLight`, `world.pointLight` and `world.spotLight` make a
`Light`, which takes `lightLink` and `shadowLink`, and casts shadows given
`castShadow=True`, made as `shadow=world.LightShadow(radius=2)` says; `world.camera(lens, exposure=…)` makes a
`Camera`. Every kind takes `key`, `collection`, `at`, the rotation and scale
lanes and `visible`, and `along(rail, distance)` stands it `distance` along
a `geometry.mesh.curve.Spline3`, in the rail's own units and turned onto
its moving frame; a live distance tows it along. `world.kit.rail(table)`
is the closed loop a `Turntable` rides, and `world.kit.wave(...)` and
`world.kit.winding(...)` make rails that rise and fall or wind a shell.
Re-describing keyed nodes preserves native identity and
shares equal mesh resources. Scalar lanes accept animatables,
bindings and tweens; `scene.advance(seconds)` moves the owned engine and
samples the retained frame. Use it from an update callback
when the scene includes timed transitions. Explicit time can instead drive
fresh descriptions, as it does in `python_world_study`.

A `Frame` names its camera by key, its environment —
`frame.environment(map, intensity=…, shown=…)` over a map
`world.kit.environmentMap(...)` bakes from a gradient and soft boxes — a
pixel extent and passes. `geometryPass` writes named resources; `postPass`
can read, blur, grade and composite them. Add a pass with
`frame.pass_(pass_value)`, or hand the frame its passes as it is made with
`world.Frame(scene, passes=[...])`, and choose the image with
`frame.present("resource")`. Pass `reads` and `writes` accept variadic names
or one iterable; `only` takes a collection's name or a `world.Collection`,
whose `include` and `exclude` hold collection names and `/key` subtrees;
`Selection.None_`, the native `Selection::None`, disables selection filtering. Invalid frame graphs raise `ValueError`, and so
does a frame that declares passes to a `Scene`, which has no device to run
them on.

A scene stays on its creating thread and owns the engine its native
reconciler borrows. Camera,
light and statistics getters return copies. The three-point rig, turntable
and lit-set kit compose ordinary native nodes. Light and rig colors are
linear RGBA sequences, matching the native light values.

A surface's maps take textures: `material.Texture(picture)` over a Skia
image, a `media.Image` or a `media.Video`, with `tile`, `region`,
`sampling`, `colorSpace` and `mipmaps`; `material.image(texture)` is it as
a material, `material.flipbook(sheet, material.FlipbookOptions(...))` plays
a sheet of frames — on the clock, at a bound `frame` or `progress`, or at
the scalar each instance carries under `frameAttribute` — and
`material.flowmap(source, material.FlowmapOptions(flow=…))` moves a
material along a texture of directions. `python_flipbook_fire` wears all
three.

A body's material can be a Compose page. `ctx.page((width, height))` makes
a `sketch.Page` the session keeps until the sketch declares again: ask for it
in `setup` and hold it. `page.render(tree)` is the pass that paints the tree
as the maps of a surface — base colour, normal, roughness, metalness,
occlusion, emission and clear coat, each where the tree's materials state
them — and `page.surface()` is the material a body wears to show it. Put the
pass on the frame `describe` returns with `frame.pass_(...)`; the frame runs
it at its moment, before anything draws a body, and a body wearing the surface
on a frame without the pass is an error naming both. Every lit material in
the tree paints its own maps — a `surface(...)` fill, an `ink`, a
`compose.relief(...)` decoration for raised or impressed type — and content
that states no surface is the stock surface, lit, shadowed and reflecting
like the rest of the body; only content stated unlit or with an emission
is its own light. A tree that has not changed
since the page last painted is not painted again. `python_plaque` wears one.
A fill stating `material.from_(c).surface(transmission=1.0, ior=1.33,
thickness=20)` on the page is glass on the body that wears it, seeing the
scene behind where the page transmits, and an `emissionStrength` past one
keeps its light past white. Every field of the native surface options is a
keyword of `surface(...)` under its native name — `flatBack=True` for a
plano-convex body whose back is the plane at its own zero depth,
`relief=material.ReliefOptions(...)` and `anisotropyAlongUnit=True` among
them — and `material.surface.lower(material)` answers the program it
states, whose fields `Material.get(name)` reads back; a
`surface.SurfaceParameters` carries every number the native one does.

```python
@sketch(size=(960, 540), captureAt=2.0)
class Plaque:
    def setup(self, ctx: Scene3DContext) -> None:
        self.page = ctx.page((768, 384))
        self.plate = mesh.quad(320, 160)
        ctx.camera(lens)

    def describe(self, seconds: float) -> world.Frame:
        body = world.mesh(self.plate).key("plaque").fill(self.page.surface())
        return world.Frame(world.group(light, body)).pass_(self.page.render(face()))
```

A 2D picture that is its own light, or that lies over the finished frame,
is a `compose.TextureScene` — a class of the compose bindings, which imports
with `sigil.compose` alone: `ctx.textureScene((width, height))` at setup,
then in `describe` `screen.render(tree, seconds)` and `screen.texture()` in
a material's slot or on the frame with `frame.overlay(...)`.
`python_world_study` lays its words over its set that way.

The device runtime itself, texture-map sets, point clouds,
sweeps, custom pass callbacks and readbacks remain unbound; the Python API
does not claim GPU PBR parity.

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
| `python_bindings` | One oscillator's live value read through every binding stage: range, wiggle, clamp, envelope, quantize, a fold and an ease |
| `python_instrument_board` | Readings reported at a cadence and drawn through memo, beside an envelope entering on one stagger |
| `python_data_garden` | Native CSV tables, sorting, and linear, band and square-root scales |
| `python_world_study` | Retained keyed 3D scene: a lathed vessel, a parametric knot and a regular solid on three plinths, material finishes and a moving native light rig |
| `python_flipbook_fire` | Generated textures: a flame sheet played as a flipbook, a flow map, embers reading a per-instance cell, bloom |
| `python_liquid_glass` | Shader-driven refraction, nested shader inputs, moving field uniforms, Bezier filaments |
| `python_botanical_study` | Layered natural media, native hatching and dry pigment |
| `python_numpy_bridge` | A NumPy angle field drawn as one packed line batch and a NumPy Gray–Scott reaction through one pixel transfer; needs the `studies` extra |
| `python_compose_stamps` | Compose trees placed by a Draw pen and repeated as a custom brush tip |
| `python_mixed_materials` | One Compose page of many lit materials worn by one quad, with plated vias and a pulse in a live window |

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
    material.ColorStop(0, colors[0]),
    material.ColorStop(1, colors[1]),
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
