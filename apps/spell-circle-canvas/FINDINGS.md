# Findings

## The Sigillum heptagram self-audit finds only three of seven crossings

`sigillum_aemeth/Settings.h` constructs seven open line paths through the
vertices of a regular `{7/2}` star, then calls `path::discoverCrossings`.
The rendered audit reports three discovered crossings instead of seven,
and three passes out of alternation instead of zero. Its crossing patches
therefore cover only part of the intended interlace.

The seven non-adjacent edge intersections should be discovered, while
the seven shared endpoints should be excluded. A regression should use
the study's translated pixel-scale star and assert seven crossings,
fourteen traversal passes, and an alternating over/under decision along
each strand. The same construction translated and scaled should preserve
that topology. The current evidence identifies the failing construction;
the defect's location within discovery or its inputs remains unconfirmed.

## Group ruby repeats a full reading on separately addressed CJK units

`ruby_kenten` passes one reading over `書物` and `国語辞典` with
`kit::ruby(selectors::text(...), Unit::Word, ...)`. Compose resolves word
units from the paragraph's line-break words, which can divide a Japanese
compound into multiple units. `TextAnnotations.cpp` repeats a one-item
reading list on every unit; it also repeats that reading on every piece
of a unit broken across columns. The GROUP and SPLIT specimens therefore
show overlapping repeated furigana instead of one reading over the compound.

Group ruby should associate one reading with the selected compound and
distribute it across the compound's placed fragments. The current unit
vocabulary has glyph, cluster, word, line and sentence, but no selected
range unit. A regression should annotate a multi-character Japanese
compound with one reading, assert one reading on an unbroken base, then
force the base across columns and assert that the fragments partition
the reading without repetition. Mono ruby and repeated kenten marks
should retain their per-cluster behavior.

## Initial-letter layout loses the space after the opening word

`bullets_dropcap` supplies the unchanged passage beginning `When the first`
to `document::paragraph(...).initialLetter({.lines = 3, .margin = 8, ...})`
at 14 px in Iowan Old Style. In all three initial-letter panels the large
`W` is followed by `henthe first`: the space between the opening word and
`the` has no visible advance. The same passage with its first character
removed and placed through `flowAround` displays `hen the first` correctly.
Nested span styling does not change the failure. The exact placement stage
responsible remains unconfirmed.

An initial letter should consume only its selected glyphs; the remaining
text must preserve inter-word spacing. A regression should lay out
`When the first` with a three-line initial, inspect the positions around
`hen the`, and assert the original space advance survives. Repeat with
nested word and delimiter styling, and compare the remainder with an
ordinary paragraph under the same face, size and available measure.

## Varying profile rails leave inner-corner spurs

`src/common/geometry/path/Band.cpp` delegates constant profiles to `parallel`
but builds varying profiles by sampling each point along the contour and
displacing it along that point's single tangent normal. On a clockwise
hexagon, `bandRegion` with `profile::taper(4, 22)` and `Formation::Inward`
leaves small triangular loops at the inner corners; `Formation::Centered`
shows the same defect on its inner rail. A closed taper also has a width
discontinuity at its seam because its endpoint widths differ; that seam
does not account for the loops at the other corners.

A varying rail should join adjacent offset edges at real contour vertices
without the sampled path doubling back into corner loops. A regression
should use a positive varying profile with matching endpoint widths on a
clockwise polygon, inspect both inward and centered bands, and assert
simple rails with no corner self-intersections while preserving the width
law along each edge. Repeat on an open polyline to separate corner joining
from closed-contour seam behavior.

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

## A bright pass in an emitted light softens the layer it is emitted over

`skia::Effect::brightPass` is a runtime shader over the layer. When one
sits anywhere in an image-filter graph, Skia evaluates that graph in the
layer's local coordinates rather than device pixels and resamples the
result onto a scaled canvas, so `Effect().emit(brightPass().then(...))`
softens the sharp source it keeps, even where the emitted light is fully
transparent. On a 2× headless GPU sweep of `ksp_mapview`, a transparent
light through a bright pass moves thin orbit lines by up to 152 levels
against the same map with no effect, while a transparent colour-filter
light leaves it exact. `ksp_mapview` therefore still describes its map
twice, so the bright pass only ever filters the second copy.

`emit` should keep its source at device resolution whatever the light is
made of. The bright pass is per-pixel, so it can be a runtime colour
filter, which carries no matrix constraint; its threshold and knee
should still take part in equality as they do now. With that,
`ksp_mapview` should need one map.

A test should render, on a canvas scaled by two, a layer with
`Effect().emit(brightPass().then(<transparent>))` and assert it identical
to the layer with no effect, and assert that two bright passes with equal
threshold and knee still compare equal.

## A study turns a screen's light off and builds it again outside the recipe

`eva_magi_interior/EvangelionUi.h` sets `uBloom` to zero on `kit::crt`
and adds the tube's light itself — two Gaussians, two weighting matrices
and a table holding the sum at half — because the recipe used to gather
that light per pixel. It does not any more: the light is a slot an
executor fills with the layer blurred once, at a cost that follows its
own radius. The comment above the workaround states a tap count the
shader no longer spends, and because a recipe's slots are read from its
body text rather than from the strength of a uniform, the study still
pays for one blur of the whole layer every frame that nothing reads.

The three MAGI studies should take the recipe's own light, at the radius
and strength that match the look they have now, and drop the hand-built
one. `uBloomRadius` is a Gaussian sigma rather than the reach of a
fixed-tap gather, so the number is a fresh choice. A plate rebase names
the cause.

## The thumbnail's PNG encode runs on the render thread

`SketchbookRenderer::render` calls `refreshThumbnail()` once a sketch has
settled, and that call runs `Host::capture` to completion inside the
frame: the readback, and then `SkPngEncoder` over the whole still. On
`eva_magi_deliberation` the encode alone holds `QSGRenderThread` for
about 90 ms a second after the sketch opens, and a still full of grain
and scanlines deflates slowly, so the hitch grows with exactly the
sketches that look richest. The store is keyed by a content hash, so
every change to a sketch or to an effect it uses retakes the still on
the next open.

The readback has to happen on the render thread; the encode and the
write do not. `Host::capture` should hand the pixmap to a worker and
return, as the browser's lazy thumbnail render already runs off the
render thread. A test should open a sketch in the window lane and assert
no frame after the settled moment exceeds the frame budget by the
encode's cost, and that the thumbnail still lands.

## Opening a sketch builds its effect pipelines on the render thread

Graphite creates a Metal graphics pipeline the first time each distinct
draw appears, and `DrawPass::prepareResources` waits for the creation
task on `QSGRenderThread`. A sketch whose root carries a chain of
runtime-shader and runtime-colour-filter stages — the MAGI studies'
bloom and CRT are about a dozen — pays one pipeline per stage on its
first frame: profiling `eva_magi_deliberation` opening in the window
shows the render thread stalled under `MtlGraphicsPipeline::Make` for
several hundred milliseconds in the first half second, while the
sketch's own `Composer::draw` costs about a millisecond and the scene
then caches to a texture. Metal serves already-compiled shaders from its
disk cache, so the stall is shorter on a second open and longest after
any edit to a shader, which is every edit during effect work. The
startup warm-up (`warmStockMaterials`) compiles each stock recipe's
SkSL, which is not the pipeline.

The pipelines a sketch's effects need should be built before its first
frame and off the render thread. Graphite ships `PrecompileContext` for
exactly this, and `ContextOptions` can report each pipeline's key as it
is created so a later launch precompiles the set; the material warm-up
is the seam it belongs beside, and a stock effect stack — the bloom's
stages, the CRT — should be part of what it warms. A test should open a
sketch twice in one process and assert that the second open's first
frame carries no pipeline creation, and that precompiling a recorded
key set before the first open removes the stall from it too.

