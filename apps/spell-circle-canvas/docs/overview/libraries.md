# The libraries

Twenty-odd independent libraries, each owning one thing. A consumer
includes a library's own headers, links its target and spells its
namespace: no library re-exports another's vocabulary, so the name you
read in a signature tells you which library to go to.

Libraries meant to be extracted into their own repositories carry the
`Sigil` prefix; product-side integrations keep `Ifrit`.

## Drawing a 2D scene

**[SigilCompose](../../src/common/compose/README.md)** — the retained,
declarative way to draw. Immutable value-typed descriptions of a 2D
scene become pixels on an `SkCanvas` the caller owns: flexbox layout
through Yoga, text leaves measured and drawn by the text engine, each
new description diffed against a retained tree so only what changed is
touched, painted in an explicit CSS-like stacking order, and cached
automatically where it is provably still. Its type chapter is
[TYPOGRAPHY.md](../../src/common/compose/TYPOGRAPHY.md), and the value
index that answers "what do I pass here" is its
[reference](../../src/common/compose/reference/VALUES.md).

**[SigilDraw](../../src/common/draw/README.md)** — the imperative way
beside Compose: a **pen** over an `SkCanvas` carrying p5's verbs with
p5's names, argument orders and defaults, so a sketch written for p5
pastes in and runs. The two open onto each other. Its
[brush chapter](../../src/common/draw/brush/README.md) is the natural-media
line vocabulary over the pen.

**[SigilWeave](../../src/sigilweave/README.md)** — styled Unicode text
into positioned glyph runs ready to draw. HarfBuzz for shaping, ICU for
line breaking, script itemization, bidi and case mapping, called
directly rather than through Skia's own shapers. The paragraph engine is
rooted here, with [FEATURES.md](../../src/sigilweave/FEATURES.md) for the
catalogue, and a [kit](../../src/sigilweave/kit/README.md) of companion
utilities for consumers.

**[SigilGeometry](../../src/common/geometry/README.md)** — the
higher-level drawing over Skia: path resampling, boolean and distortion
operators over `SkPath`, shape interpolation, a renderer-neutral
triangle mesh with procedural generators, splines with swept geometry,
point clouds with named attribute lanes, and the point-operator chain
language over them.

## Colour, paint and pixels

**[SigilMaterial](../../src/common/material/README.md)** — materials as
recipe instances. A **recipe** is a definition — a struct of
uniform-typed fields that is its ABI, one shader body per language, its
slots and its per-frame inputs — and a **material** is one instance of
it. Beside that sits the Skia paint, the post-processing effect, the
colour value and its reasoning spaces, ramps, palettes, harmonies and
dithering, with chapters for [colour](../../src/common/material/COLOUR.md)
and [the paint](../../src/common/material/PAINT.md) and a
[value reference](../../src/common/material/reference/VALUES.md).

**[SigilSkia](../../src/common/skia/README.md)** — Skia's Graphite GPU
backend brought up on a device someone else already owns: given a native
device and queue, it builds a `Context` and `Recorder` and wraps the
caller's textures.

**[SigilImage](../../src/common/image/README.md)** — image *meaning*,
both directions: encoded bytes in and Skia images out, pixels in and
encoded bytes out, plus distance fields. (Resource *access* is
SigilIO's; the split between the two is deliberate.)

**[SigilVideo](../../src/common/video/README.md)** — video meaning:
container bytes open as a seekable streaming video, frames decode around
the playhead into a small presentation cache, and pixels flow the other
way through an incremental encoder.

**[SigilScry](../../src/common/scry/README.md)** — a headless web
browser embedded in a C++ application, handing back its output as Skia
images. Optional: it wraps a licensed SDK.

## Structure and motion

**[SigilCore](../../src/common/core/README.md)** — the kernels a
retained runtime is built on: the reconciler and the shape of the tree
it keeps, the memo that skips a describe, the caching proof, the device
seam, and the compute values a drawing is drawn from.

**[SigilMotion](../../src/common/motion/README.md)** — animation timing
and animation *values*, with no renderer in them: a monotonic frame
clock, a ticker over a Choreograph timeline, the value types that
describe how a property changes, bindings, and physics.

**[SigilData](../../src/common/data/README.md)** — tabular data, and the
one value that maps a domain onto a range. Named, typed columns as
contiguous spans a drawing walks straight down, reshaped by selecting,
filtering, sorting and grouping.

## Three dimensions

**[SigilWorld](../../src/common/world/README.md)** — a 3D scene as
comparable values, turned into a frame — a scene, an ordered list of
passes, the readbacks the caller asked for — and executed. It consumes
SigilGeometry's types and never the reverse.

**[SigilUsd](../../src/common/usd/README.md)** and
**[SigilSubstance](../../src/common/substance/README.md)** — optional SDK
integrations: USD in and out, and Adobe Substance archives rendered to
images.

## Getting things in and out

**[SigilIO](../../src/common/io/README.md)** — a runtime resource hub.
Application code asks for a resource by URI rather than by filesystem
path; prefixes mount onto directories, results are cached per resource,
and a poll re-stats what has been loaded so edited files reload without
a restart. Its [publish chapter](../../src/common/io/publish/README.md) is
native inter-application texture publication and subscription.

**[SigilMeasure](../../src/common/measure/README.md)** — timing,
statistics and check reporting: stopwatches, lap timers, the frame timer
whose marks feed a render loop's lanes, and the report a check writes.

**[SigilPython](../../src/common/python/README.md)** — the reusable
native bindings and the callback ownership rules behind them, with no
sketch runtime, application, window or interpreter startup in the target.

## Making and looking at pictures

**[SigilSketch](../../src/sketch/README.md)** — everything renderable as
one sketch each: a file that declares a scene, an entry in one registry,
and something Sketchbook opens live and hot-swaps on every save. Its
[kit](../../src/sketch/kit/README.md) is the sheet a sketch stands on —
the theme, the page and the furniture a specimen is built out of — and
its [sketches](../../src/sketch/sketches/README.md) are the reference
studies.

**[SigilSeer](../../src/seer/README.md)** — every wire, and what is going
down it: a tool that opens a wire, says what is coming down it and who is
at the other end, shows the newest message several ways, sends one back,
and previews shared textures.

**[Ifrit.Qt](../../src/common/qt/README.md)** — the reusable Qt Quick
controls the desktop tools share.

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
