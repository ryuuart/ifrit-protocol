# Findings

## Documentation probes do not recognize newly supplied namespace aliases

`src/test/docs/api_doc_probes.py` emits aliases supplied through `--alias`
into the generated C++ translation unit, but does not add their names to the
namespace set used while resolving documented names. An alias such as
`doc=sigil::compose::document` therefore reports `doc::article` as an unknown
type even though the target function exists. Aliases already present in the
scanner's built-in namespace set can appear to work.

An explicitly supplied alias should participate in both name resolution and
generated C++ declarations. A regression should supply a new alias for a
declared namespace, resolve a qualified function through that alias, and
compile the resulting probe. Documents spelling the original namespace pass.

## Billboard point clouds are substantially slower on the GPU lane

`pop_billboards` completes its raster plate, but its headless GPU sweep takes
long enough to exceed a 150-second capture timeout. An isolated GPU sweep
eventually completes with the same visible composition. Sampling the process
shows Graphite's Vulkan submission and fence waits dominating the main thread.
The point renderer in `src/common/geometry/mesh/pop/Billboards.cpp` creates a
tint color filter and submits one image rectangle for each splat.

Reproduce both paths with:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless /tmp/billboards-gpu --gpu --sketch pop_billboards
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless /tmp/billboards-raster --sketch pop_billboards
```

The GPU path should handle dense clouds through batches of compatible sprites
while preserving depth order, per-point tint and size, atlas windows, and the
requested blend mode. A renderer benchmark should cover the default soft dot,
the ring sprite, and atlas windows at increasing cloud sizes. Plate comparisons
should assert that batching preserves those visual properties on raster and
GPU backends. Runtime measurements belong to the benchmark results.

## Sketchbook's OpenGL raster fallback presents the canvas upside down

With `QSG_RHI_BACKEND=opengl`, Sketchbook uploads its raster frame into the
Qt Quick texture with the opposite vertical orientation from the displayed
canvas. The sketch is upside down while the surrounding controls remain
upright. The Metal path displays the same sketch correctly.

The raster and GPU paths should present the same top-to-bottom orientation.
Reproduce with:

```sh
QSG_RHI_BACKEND=opengl build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  --sketch spacing_passes --window-bench 30 --window-size 900x600 \
  --shot /tmp/sketchbook-opengl.png
```

A window regression should render an asymmetric scene with distinct top and
bottom markers through the OpenGL raster fallback, capture the presented
window, and assert that the markers have the same orientation as the raster
reference. The upload and presentation meet in
`src/sketch/book/SketchbookRenderer.cpp`.

## Closing a shared macOS window can fault during observer cleanup

An interactive Sketchbook session can exit with `SIGSEGV` after publication
is started, stopped, and started again while the macOS window-sharing overlay
is present. The captured stack enters Foundation's
`_NSKeyValueRetainedObservationInfoForObject` from ViewBridge's
`NSRemoteView` observer cleanup, through `NSWindow` ordering out and Qt's
window destruction. The publication request and the sharing overlay are
observed conditions, not established causes.

Quit should release the window and publisher without a fault. Separate
stopped and publishing sessions both exit successfully through the same
Quit action, so the failure is intermittent and its trigger remains
unresolved. A reproduction should exercise publication toggles with an
active macOS window-sharing overlay, then assert that quitting completes
with exit status zero. Inspect the native window's observation and class
lifetime if that sequence reproduces the fault; the shared background
rendering helper changes its Objective-C class.

## Database query views can write through `query()`

`src/common/python/DataBindings.cpp` rejects `execute()` and `insert()` on a
database loaded through a sketch hub or `Database.fromBytes()`, reporting that
the view is read-only. `query()` on the same view runs any statement, and a
hub-cached SQLite file is opened in place for reading and writing, so
`ctx.assets.database(uri).query("DELETE FROM totals")` removes rows from the
file on disk.

A view that refuses writes should refuse them through every method. A
regression should load a SQLite file through `ctx.assets` and through
`fromBytes()`, run a writing statement through `query()`, assert that it
raises and that the file is unchanged, and assert that reading queries still
answer.

## A standalone hub cannot load a database from Python

`data.registerDecoders(hub)` installs only the table and JSON decoders, and no
Python call installs the database decoder, so `io.Hub().load(data.Database,
uri)` returns `None` for an existing SQLite file. The generated declarations
still offer that overload, and `ctx.assets.hub()` loads the same file because
the sketch host registers the decoder itself.

An owned hub should load what a session hub loads once its decoders are
registered. A test should mount a directory holding a SQLite file on
`io.Hub()`, call `registerDecoders`, load it as `data.Database` and assert that
a query returns its rows.

## `Flag` and `Instant` lose their native comparisons in Python

`data.Flag(True) == data.Flag(True)` is `False`, and comparing two
`data.Instant` values with `<` raises `TypeError`, although the native types
define equality and ordering. Table cells hand these values to Python code,
which then cannot compare or sort them directly.

Bound values should compare as the native values do. A test should assert
that equal flags compare equal and unequal flags do not, and that two decoded
instants order chronologically.

## Some bound keyword names break keyword calls

`Table.group` names its parameter `names` although it takes one column name,
the native parameter is `name`, and the neighbouring table methods use `name`.
`sketch.kit.Theme.font` names its one-argument parameter `register` while its
two-argument overload and `Theme.style` name the same register `line`.
`Element.padding` and `Element.margin` are bound over positional arguments
only, so they accept no keyword at all. A keyword call written from one form
fails on another, although the bindings name inputs so that keyword calls work.

Keyword names should agree with the native parameters and across overloads. A
test should call `table.group(name="kind")`, `theme.font(line=register)` with
and without `ink`, and `padding` and `margin` with named lengths.

## Pen declarations omit forms the binding accepts

`src/common/python/PenBindings.cpp` accepts `pen.fill(paint, CANVAS)` and
`SHAPE`, `pen.fill(material)`, the same forms for `stroke`, and
`pen.background(paint)`. The declarations generated through
`apps/python/sigil/typing/refinements.py` have none of these overloads, so the
README's `pen.fill(glass, CANVAS)` and `python_liquid_glass.py` fail strict
type checking. The same refinements declare the silhouette of
`pen.shape(silhouette, x, y, width, height)` as `skia.Path`, but the binding
calls `silhouette.path((width, height))`, so a path raises `AttributeError`.

The declarations should describe the calls the binding accepts. A typing
fixture should check each accepted form, and reject a `skia.Path` passed as a
silhouette in favour of an object with a `path(size)` method.

## `sigil render` ignores a sketch's `REQUIRES`

Sketchbook registration reads a sketch's `REQUIRES` tuple and marks the entry
unavailable when a module is missing. The `sigil render` command imports the
sketch without reading it, so a missing module surfaces as a raw
`ModuleNotFoundError` from the sketch's own import line, as it does for the
two NumPy studies in an interpreter without NumPy.

The command should report the declared module that is missing and exit
nonzero. A test should render a sketch declaring an absent module and assert
that the message names the module and the process fails.

## A paint layer's material is held but never shades

`weave::PaintLayer` carries an optional `material::Material`, and
`src/sigilweave/paint/Paint.cpp` shades a layer with it only when a resolver
installed through `weave::paint::setMaterialResolver` is present. Nothing
outside the paint test installs one, so a material on a paint layer draws with
the layer's plain paint in Sketchbook, the standalone renderer and Python
alike.

A host that draws retained text should install the resolver. A regression
should set a material on a text paint layer, render it through the sketch
host, and assert that the pixels differ from the same layer without the
material.

## A sketch comment says the clock survives reload

`src/sketch/include/sigilsketch/canvas/Sketch.h` says every reload constructs
a fresh instance "while the shared clock keeps running, so elapsed time is
continuous across an edit". `Host::restartSession` in
`src/sketch/live/Host.cpp` opens a session that owns a fresh clock and resets
the host clock, so elapsed time restarts. The comment should describe the
restart; a live-host test should assert that elapsed time after a reload
starts again from zero.
