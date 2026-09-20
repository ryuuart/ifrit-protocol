# Text on a path

A chapter of [TYPOGRAPHY.md](../TYPOGRAPHY.md), the type chapter of
[SigilCompose](../README.md).

`Element::onPath` makes a `TextPath` the run's BASELINE. The run is shaped
once — real kerning, real ligatures, real advances — and then laid out
through SigilWeave's own contour geometry: every contour of the resolved
`TextPath::path` is one interval of the run's one line, and the words fill
them in order.

```cpp
text(u8"SIGILLVM · DEI · AEMETH", inscription)
    .width(320).height(320)
    .onPath({.path = geometry::shapes::circle(),
             .at = &phase,                       // the marquee
             .align = TextPath::Align::Center,
             .orient = TextPath::Orient::Tangent})
    .fx({.effect = fx::rise(18)});
```

**`at` is where along the baseline the run sits**, as a fraction of the whole
path's length with the contours chained end to end — which is what lets seven
chords of a heptagon carry seven captions addressed by fraction alone.
`TextPath::align` measures the run against that point: `Start` begins there,
`Center` centres on it, `End` finishes there. It is an `Animatable<float>`,
so every `bind()` and `animate()` verb applies, and on a CLOSED baseline the
fraction WRAPS — a phase output running 0→1 forever is the infinite marquee,
with no seam. Moving it is PAINT-ONLY: the run is shaped and broken across
the contours once, and the phase re-places glyphs that were already placed,
so a marquee costs a repaint and never a reflow. It declares content
volatility while it runs and releases once it provably holds still.

**A CONTOUR BOUNDARY IS A BREAK.** A word that does not fit the contour it
reached starts the next one, rather than bending across the gap between two
disconnected curves. A run that outlasts the last contour simply stops, and a
run pushed off the end of an open baseline by its phase drops the glyphs that
ran off rather than piling them on the last point.

**`fx()` and `onPath()` compose; neither wins.** THE BASELINE PLACES THE
GLYPH, THEN THE TRACKS DEVIATE FROM THAT PLACEMENT, IN THE FRAME THE BASELINE
PUT IT IN. On a curve that means `fx::rise` lifts a letter off the CURVE
along its own local perpendicular rather than straight up the canvas, a
stagger's shove stays tangential to the lettering it belongs to, and a
track's rotation adds to the tangent the glyph was already turned to. Scale,
alpha, the colour multiplier and both substitutions are per-glyph dressings
and are untouched by the frame — so `variationDrive` and `fx::scramble` reach
curved lettering exactly as they reach straight lettering.

`Element::textFill` and `Element::textStroke` reach a path run like any
other, with one caveat: a metric-mapped material maps its unit square to the
run's STRAIGHT metric band, which is not where the type ended up. A flat
colour and a stroke are exact; a gradient across a ring is not what it
looks like.

`TextPath::orient` is `Tangent` (running lettering), `Radial` (the baseline
along the radius, for an astrolabe limb or a compass rose) or `Upright`
(level everywhere, for a calendar ring). `autoFlip` turns the RUN over once
so lettering on the lower half of a ring reads right way up — never each
glyph, which would reverse the reading order. `TextPath::offset` rides the
type off the baseline, positive to the LEFT of travel. Tangents snap to a
ladder of directions because each distinct rotation is a glyph-atlas
strike, and the ladder is cut per RENDERED SIZE — one angular step sweeps
a bigger glyph's extremity through more pixels, so display lettering on a
turning ring gets a proportionally finer ladder and does not tick letter
by letter as a marquee turns; `TextPath::exactTangent` is the opt-out,
for artwork that must hold the exact angle.

**A RUN IN MOTION PLACES ITS GLYPHS ON THE SUBPIXEL GRID; a run at rest
keeps whole-pixel origins.** A glyph mask is rasterized for a quantized
origin, so a ring creeping along by a fraction of a pixel per frame does
not creep at all on whole pixels: every letter stands still until its own
origin crosses a pixel boundary and then hops a whole one, at its own
moment. Nothing about the placement arithmetic causes it and no ladder
fixes it. Three declarations put a run on the finer grid, all of them the
question "does what this run draws land somewhere else next frame": a
BOUND or animated `TextPath::at`; a bound or animated `rotate()` (or
any other geometric transform) at or above the text node; and a live
`fx()` track whose effect moves glyphs. A phase written
as a plain number, or a figure turned by re-describing a literal angle,
declares nothing and is treated as type at rest. The grid is read off the
declaration and never off a frame-to-frame difference, so a marquee parked
at a phase keeps the placement it was turning with rather than taking one
last quarter-pixel shift the moment it settles.

**TYPE ON A TURNED PLANE IS PROJECTED AT DRAW, never resampled.** A text
leaf under `rotateX` or `rotateY`, or standing in a `preserve3d()` space,
is shaped and placed in its own plane and drawn through the flattened 4x4
that plane projects with — a projective matrix — so the near edge is as
sharp as the far one and nothing is rasterised flat first. A projective
matrix can push glyphs off the atlas path and onto path filling, which is
the cost the text bench's perspective arm measures against an affine
tilt of the same panel. A `Cache::Texture` on a turned text node bakes
the plane at the projection's largest local scale and lets the projection
resample the bake — sharp at the near edge, minified toward the far one —
and no device-space bake forms under a perspective. A plane whose
projection moves — its own lane, its host's turn, or the view above it —
puts the run on the subpixel grid exactly as a turning ancestor does.

**A track declares through two facts, and needs both.** Its progress must
be live — bound, or mid-transition — and its effect must actually move
glyphs, which is what `TextEffect::displaces` answers. That answer is
*inferred* almost everywhere: a preset knows its own deviation (`fx::rise`,
`fx::slide`, `fx::pop`, `fx::spinIn`, `fx::scatter` and `fx::waveLoop`
move glyphs; `fx::typeOn`, `fx::variableAxisSweep`, `fx::tint` and `fx::scramble` touch
coverage, colour or the outline and leave every pen position alone),
`fx::keys` reads its own table (any entry publishing an offset, a lean, a
shear or a growth), and `fx::sequence`, `fx::mix` and `fx::hold` derive from
their operands. `fx::pass` does not displace — its shader runs over pixels
already rasterized at the resting origins, so refining those origins says
nothing about where the pass puts its output. Only `fx::effect` has to be
told, because a lambda is opaque until it runs: it assumes the moving
answer, and `.displacing(false)` is the author's promise otherwise. A
karaoke wipe, a decoding scramble and a staggered fade therefore keep
whole-pixel origins and their bytes however hard they run, and a settled
displacing track goes back to them — its glyphs are standing somewhere
else and standing still.

**The baseline declares its own reach.** A resolved path is not bounded by
the node's box — a custom `Shape` may return a curve well outside it, and
`offset` rides the type further off again — so the cull grows by the curve's
bounds plus the glyph band and whatever the tracks reach, the same
over-reporting-is-safe contract `bleed()` and `reach()` carry. Nothing to
declare by hand; it follows from the baseline you gave it.
