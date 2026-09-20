---
kind: type
library: SigilCompose
name: PathFormat
qualified: sigil::compose::PathFormat
header: sigilcompose/brush/Decorations.h
group: Shape and edge
python: sigil.compose.PathFormat
status: stable
---

# PathFormat

HOW A STROKE ALONG THE NODE'S OUTLINE IS FORMATTED, as one comparable
value: a width and a paint, and then — if the mark is not a plain line —
a dash pattern, a path stamped repeatedly along the contour, a trim
window, or any `SkPathEffect` at all. In the brush taxonomy it is
`brush::Solid`, the plain stroke, and it is the same type under both
names.

It reads only `PaintContext::outline`, so it is not restricted to a
node's own shape: geometry built inside a `custom()` program wears the
same value through `decorations::paintOn`.

## Anatomy

`PathFormat::width` and `PathFormat::strokeFill` are the mark.
`PathFormat::strokeFill` is a `SurfacePaint`, so the stroke takes
everything a fill does — including `Fill::currentInk()`, which is its
default: a stroke that names no colour is painted in the ink in force,
and a recoloured ancestor recolours it.

`PathFormat::align` is the stroke-position control an image editor has.
`PathFormat::Align::Center` straddles the outline;
`PathFormat::Align::Inner` clips the stroke inside the silhouette, which
is the border that never fattens the shape;
`PathFormat::Align::Outer` clips it outside, which is the keyline. Inner
and Outer mean something only on a CLOSED outline — an open rail has no
inside — and they keep that meaning when a stroke is sliced to chosen
edges or revealed by a span, because the clip then stands on the outline
the runs were cut from.

`PathFormat::dashIntervals` and `PathFormat::dashPhase` are the dash;
`PathFormat::dashPhaseBinding` replaces the phase with an animatable and
makes the dashes MARCH. `PathFormat::stampPath` and
`PathFormat::stampAdvance` repeat a path along the contour, rotated to
follow it — vines, chains, ornament runs. `PathFormat::effect` is the
escape hatch, and supersedes both.

`PathFormat::trimStart`, `PathFormat::trimEnd` and
`PathFormat::trimOffset` are a window on the arc length, per DECORATION
rather than per node — so one node carries a full static band and a
marching sliver as two strokes, with no second element duplicating the
path. `PathFormat::trimPhase` binds the offset the way the dash phase
binds.

`PathFormat::cap`, `PathFormat::join` and `PathFormat::antiAlias` are the
Skia paint's own three. Turning smoothing off puts the stroke on whole
pixels — the one-pixel rule of an interface that was screen-shot rather
than drawn — and it costs the smoothing on every mark the format makes,
dashes and stamps included.

`PathFormat::bleed` is how far the mark escapes the node's box, which the
recording cull grows by; `PathFormat::reach` is how wide the mark is
either side of the outline, which is not the same number — an inner
stroke escapes by nothing while painting a band `width` px wide inside
the shape.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `compose::stroke(width, fill)` | C++ | the plain stroke, centred |
| `compose::stroke(width, fill, align)` | C++ | the same, positioned |
| `compose::stroke(width)` | C++ | a stroke in the ink in force |
| `brush::solid(width, fill)` | C++ | the same value under its brush-taxonomy name |
| `PathFormat{.width = 2.0f, .dashPhase = 4.0f}` | C++ | designated initialisers, for the fields the factories do not reach |
| `compose.stroke(width, paint, align)` | Python | the factory, taking the whole surface-paint union for its paint |
| `compose.PathFormat(...)` | Python | direct, when the dash, stamp or trim fields are wanted |

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::stroke` | verb | SigilCompose — along the node's own outline |
| `Element::background`, `Element::foreground`, `Element::overlay` | verb | SigilCompose |
| `Decoration` | type | SigilCompose — the implicit constructor every decoration slot takes |
| `brush::layers`, `brush::weave` | function | SigilCompose — as one strand of a composite |
| `Strand::brush` | field | SigilCompose |
| `decorations::paintOn` | function | SigilCompose — against geometry you built yourself |

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `compose::stroke` | function | SigilCompose |
| `brush::solid` | function | SigilCompose |

## Description

Every field is a number, a small enumeration, a vector of numbers or a
value that compares structurally, which is what lets a stroked, dashed or
stamped border prune with no memo. A custom `PathFormat::effect` compares
by pointer identity, so holding one effect and rebuilding the wrapper
around it still prunes; building a fresh `SkPathEffect` per describe does
not.

`PathFormat::isAnimated` answers true for a bound trim phase, a bound
dash phase, or a live stroke paint, and the node then repaints every
frame without needing a re-describe. Say nothing and the node is treated
as static: its first frame is recorded and replayed forever, with no
error and no warning. Nothing introspects on your behalf.

A trim window COMPOSES with the pass's own span. A decoration receives
the already-claimed run, so its window is a fraction of the revealed
part: a second stroke trimmed to the last tenth is a bright sliver riding
the head of a self-drawing line.

## See also

- `brush/Decorations.h` — the header: `PathFormat`, `stroke`, `Shadow`,
  `shadow`, `Wash`, `Slice`, `ContourWalk`, `PathSample`, `paintOn`
- [SurfacePaint](SurfacePaint.md) — what `PathFormat::strokeFill` takes
- [Shadow](Shadow.md) — the other value decoration
- [Corners](Corners.md) — the outline a box stroke follows
