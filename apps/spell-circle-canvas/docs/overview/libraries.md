# The libraries

Twenty-odd independent libraries, each owning one thing. A consumer
includes a library's own headers, links its target and spells its
namespace: no library re-exports another's vocabulary, so the name you
read in a signature tells you which library to go to.

Libraries meant to be extracted into their own repositories carry the
`Sigil` prefix; product-side integrations keep `Ifrit`.

Each name below opens that library's own site, whose front page is its
README — the canon for the library, written for someone with no prior
context and compile-checked against its headers. Its chapters stand
beside it there.

## Drawing a 2D scene

**[SigilCompose](doxygen:SigilCompose)** — the retained, declarative way
to draw. Immutable value-typed descriptions of a 2D scene become pixels
on an `SkCanvas` the caller owns: flexbox layout through Yoga, text
leaves measured and drawn by the text engine, each new description
diffed against a retained tree so only what changed is touched, painted
in an explicit CSS-like stacking order, and cached automatically where
it is provably still. Its type chapter and its colour chapter stand on
that site; the catalogue over it is
[the SigilCompose reference](/reference/SigilCompose/index.html).

**[SigilDraw](doxygen:SigilDraw)** — the imperative way beside Compose:
a **pen** over an `SkCanvas` carrying p5's verbs with p5's names,
argument orders and defaults, so a sketch written for p5 pastes in and
runs. The two open onto each other. Its brush chapter is the
natural-media line vocabulary over the pen.

**[SigilWeave](doxygen:SigilWeave)** — styled Unicode text into
positioned glyph runs ready to draw. HarfBuzz for shaping, ICU for line
breaking, script itemization, bidi and case mapping, called directly
rather than through Skia's own shapers. The paragraph engine is rooted
here, with a feature catalogue and a kit of companion utilities for
consumers.

**[SigilGeometry](doxygen:SigilGeometry)** — the higher-level drawing
over Skia: path resampling, boolean and distortion operators over
`SkPath`, shape interpolation, a renderer-neutral triangle mesh with
procedural generators, splines with swept geometry, point clouds with
named attribute lanes, and the point-operator chain language over them.

## Colour, paint and pixels

**[SigilMaterial](doxygen:SigilMaterial)** — materials as recipe
instances. A **recipe** is a definition — a struct of uniform-typed
fields that is its ABI, one shader body per language, its slots and its
per-frame inputs — and a **material** is one instance of it. Beside that
sits the Skia paint, the post-processing effect, the colour value and
its reasoning spaces, ramps, palettes, harmonies and dithering, with a
colour chapter and a paint chapter on that site and
[its own value pages](/reference/SigilMaterial/types/index.html) here.

**[SigilSkia](doxygen:SigilSkia)** — Skia's Graphite GPU backend brought
up on a device someone else already owns: given a native device and
queue, it builds a `Context` and `Recorder` and wraps the caller's
textures.

**[SigilImage](doxygen:SigilImage)** — image *meaning*, both directions:
encoded bytes in and Skia images out, pixels in and encoded bytes out,
plus distance fields. (Resource *access* is SigilIO's; the split between
the two is deliberate.)

**[SigilVideo](doxygen:SigilVideo)** — video meaning: container bytes
open as a seekable streaming video, frames decode around the playhead
into a small presentation cache, and pixels flow the other way through
an incremental encoder.

**[SigilScry](doxygen:SigilScry)** — a headless web browser embedded in
a C++ application, handing back its output as Skia images. Optional: it
wraps a licensed SDK.

## Structure and motion

**[SigilCore](doxygen:SigilCore)** — the kernels a retained runtime is
built on: the reconciler and the shape of the tree it keeps, the memo
that skips a describe, the caching proof, the device seam, and the
compute values a drawing is drawn from.

**[SigilMotion](doxygen:SigilMotion)** — animation timing and animation
*values*, with no renderer in them: a monotonic frame clock, a ticker
over a Choreograph timeline, the value types that describe how a
property changes, bindings, and physics.

**[SigilData](doxygen:SigilData)** — tabular data, and the one value
that maps a domain onto a range. Named, typed columns as contiguous
spans a drawing walks straight down, reshaped by selecting, filtering,
sorting and grouping.

## Three dimensions

**[SigilWorld](doxygen:SigilWorld)** — a 3D scene as comparable values,
turned into a frame — a scene, an ordered list of passes, the readbacks
the caller asked for — and executed. It consumes SigilGeometry's types
and never the reverse.

**[SigilUsd](doxygen:SigilUsd)** and
**[SigilSubstance](doxygen:SigilSubstance)** — optional SDK
integrations: USD in and out, and Adobe Substance archives rendered to
images.

## Getting things in and out

**[SigilIO](doxygen:SigilIO)** — a runtime resource hub. Application
code asks for a resource by URI rather than by filesystem path; prefixes
mount onto directories, results are cached per resource, and a poll
re-stats what has been loaded so edited files reload without a restart.
Its publish chapter is native inter-application texture publication and
subscription.

**[SigilMeasure](doxygen:SigilMeasure)** — timing, statistics and check
reporting: stopwatches, lap timers, the frame timer whose marks feed a
render loop's lanes, and the report a check writes.

**[SigilPython](doxygen:SigilPython)** — the reusable native bindings
and the callback ownership rules behind them, with no sketch runtime,
application, window or interpreter startup in the target.

## Making and looking at pictures

**[SigilSketch](doxygen:SigilSketch)** — everything renderable as one
sketch each: a file that declares a scene, an entry in one registry, and
something Sketchbook opens live and hot-swaps on every save. Its kit is
the sheet a sketch stands on — the theme, the page and the furniture a
specimen is built out of — and its sketches are the reference studies.

**[SigilSeer](doxygen:SigilSeer)** — every wire, and what is going down
it: a tool that opens a wire, says what is coming down it and who is at
the other end, shows the newest message several ways, sends one back,
and previews shared textures.

**[Ifrit.Qt](doxygen:IfritQt)** — the reusable Qt Quick controls the
desktop tools share.

## How they relate

Two boundaries are easy to get backwards, and both are stated in the
READMEs they divide:

- **SigilIO owns resource *access*; SigilImage owns image *meaning*.** A
  URI, a mount, a cache and a reload are IO's. A decode, an encode and a
  distance field are Image's.
- **SigilWorld consumes SigilGeometry's types, never the reverse.**

And one rule decides where new work goes: a primitive is irreducible and
lives in a library's core; a stock value over a seam is kit; a device
executor stands beside the CPU executor of the seam it serves. Motion
primitives live in SigilMotion and the paragraph engine in SigilWeave —
if either cannot express what a consumer needs, that library grows
rather than the consumer inventing its own.
