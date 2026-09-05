# SigilDraw — an immediate-mode canvas with p5's brevity

A **pen** over an `SkCanvas`, carrying p5's verbs with p5's names,
argument orders and defaults, so a sketch written for p5 pastes in and
runs. Compose is the declarative way to draw here; this is the
imperative way beside it, and the two open onto each other.

Namespace `sigil::draw`, with the `SigilDraw` pen and the `SigilDrawBrush`
procedural tools over it under `sigil::draw::brush`. Every public header
is under `include/sigildraw/`; `<sigildraw/Draw.h>` is the pen's umbrella
and `<sigildraw/brush/Brush.h>` is the brush library's.

## A p5 sketch, pasted in

The canonical bouncing ball with a trail, as p5 has it and as a sketch
here has it. What differs: the `setup`/`draw` signatures, `pen.` in
front of every verb, and the sketch's variables living in the sketch.

```js
let x = 200, y = 100, vx = 3, vy = 2;

function setup() {
  createCanvas(400, 300);
  noStroke();
}

function draw() {
  background(20, 30);
  x += vx;
  y += vy;
  if (x < 20 || x > width - 20) vx = -vx;
  if (y < 20 || y > height - 20) vy = -vy;
  fill(255, 120, 80);
  circle(x, y, 40);
}
```

```cpp
#include <sigilsketch/draw/Draw.h>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

struct Bounce final : sketch::DrawSketch {
  float x = 200, y = 100, vx = 3, vy = 2;

  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(400, 300);
    ctx.pen.noStroke();
  }

  void draw(sketch::DrawContext& ctx) override {
    Pen& pen = ctx.pen;
    pen.background(20, 30);
    x += vx;
    y += vy;
    if (x < 20 || x > pen.width - 20) vx = -vx;
    if (y < 20 || y > pen.height - 20) vy = -vy;
    pen.fill(255, 120, 80);
    pen.circle(x, y, 40);
  }
};

SIGIL_SKETCH(Bounce, "Draw", "The bouncing ball, pasted from p5.")
```

`createCanvas`, `loadImage` and the moment a plate is taken belong to
whoever steps the pen — here the `sketch::DrawSketch` runtime, whose
canon is `src/sketch/README.md` — and both `setup` and `draw` speak to
that runtime through its context, which hands over the pen: in `setup`
for anything a p5 setup would have set on the canvas, in `draw` as the
pen of the frame. Everything else is the pen.

**The one deliberate departure is the pen itself.** p5's verbs are
globals over one canvas; here they are members of a value that holds the
style, the transform, the seeded streams and what it keeps between
frames. Two pens draw side by side, a pen draws inside a compose node,
and nothing in a process is global.

## The pen alone

The pen needs a canvas and a frame; who supplies them is not its
concern. Between `begin` and `end` every verb draws; between frames the
style holds, as it does in p5, and the transform starts over at whatever
the canvas carried when the frame began.

```cpp
#include <sigildraw/Draw.h>

using namespace sigil::draw;

Pen pen;
Frame frame;
frame.width = 400;
frame.height = 300;
frame.seconds = elapsed;       // the caller's clock, never the wall
frame.deltaSeconds = step;
frame.frameCount = count;      // 1 on the first frame
frame.fonts = &fontContext;    // what text is shaped with

pen.begin(canvas, frame);
pen.background(20);
pen.stroke(255);
pen.line(0, 0, pen.width, pen.height);
pen.end();
```

`Frame` also carries the pointer and the keys a host fed, which the pen
reads into `mouseX`, `mouseY`, `pmouseX`, `pmouseY`, `mouseIsPressed`,
`keyIsPressed`, `key` and `keyCode`, and answers `keyIsDown(code)`
from. A pen begun with no fonts draws no text.

## p5's semantics, kept

Every verb below takes what p5's takes, in p5's order, with p5's
default.

| what | verbs |
| --- | --- |
| the ground | `background(gray)`, `(gray, alpha)`, `(r, g, b)`, `(r, g, b, a)`, `("#hex")`, `clear()` |
| colour | `fill`, `noFill`, `stroke`, `noStroke` with the same argument forms; `color(...)` builds one; `lerpColor`; `colorMode(RGB \| HSB \| HSL[, max][, max1, max2, max3[, maxA]])` with p5's ranges — 255 across for RGB, 360/100/100/1 for the hue models |
| the stroke | `strokeWeight`, `strokeCap(ROUND \| SQUARE \| PROJECT)`, `strokeJoin(MITER \| BEVEL \| ROUND)`, `smooth`, `noSmooth` |
| blending | `blendMode(BLEND \| ADD \| DARKEST \| LIGHTEST \| DIFFERENCE \| EXCLUSION \| MULTIPLY \| SCREEN \| REPLACE \| REMOVE \| OVERLAY \| HARD_LIGHT \| SOFT_LIGHT \| DODGE \| BURN \| SUBTRACT)` |
| modes | `rectMode`, `ellipseMode`, `imageMode` over `CORNER \| CORNERS \| CENTER \| RADIUS` with p5's defaults (rect and image at the corner, ellipse at the centre); `angleMode(RADIANS \| DEGREES)`, radians by default |
| shapes | `point`, `line`, `rect(x, y, w, h[, r \| tl, tr, br, bl])`, `square`, `ellipse(x, y, w[, h])`, `circle(x, y, d)`, `arc(x, y, w, h, start, stop[, OPEN \| CHORD \| PIE])`, `triangle`, `quad`, `bezier`, `curve`, `curveTightness` |
| vertices | `beginShape([POINTS \| LINES \| TRIANGLES \| TRIANGLE_FAN \| TRIANGLE_STRIP \| QUADS \| QUAD_STRIP])`, `vertex`, `curveVertex`, `bezierVertex`, `quadraticVertex`, `beginContour`, `endContour`, `endShape([CLOSE])`; `fill` between two `vertex` calls colours the corners either side of it |
| the clip | `clip(shape)`, `clip(shape, {.invert = true})` |
| text | `text(str, x, y[, w, h])`, `text(number, x, y)`, `textSize`, `textFont(family[, size])`, `textAlign(LEFT \| CENTER \| RIGHT[, TOP \| CENTER \| BOTTOM \| BASELINE])`, `textLeading`, `textStyle(NORMAL \| BOLD \| ITALIC \| BOLDITALIC)`, `textWidth`, `textAscent`, `textDescent` |
| images | `image(img, x, y[, w, h])` and the nine-argument source-rect form, over an `sk_sp<SkImage>` or a `Graphics` |
| transform | `translate`, `rotate`, `scale(s \| sx, sy)`, `shearX`, `shearY`, `push`, `pop`, `resetMatrix`, `applyMatrix(a, b, c, d, e, f)` |
| numbers | `random()`, `random(max)`, `random(min, max)`, `randomGaussian`, `randomSeed`, `noise(x[, y, z])`, `noiseSeed`, `noiseDetail` |
| the loop | `frameCount`, `deltaTime` (milliseconds), `millis()`, `frameRate()`, `frameRate(fps)`, `noLoop`, `loop`, `redraw` |
| constants | `PI`, `TWO_PI`, `TAU`, `HALF_PI`, `QUARTER_PI`, and every word above, in `sigil::draw` |

The details p5 states and this pen keeps: `arc` draws clockwise from
`start` to `stop` in the current angle mode, fills the pie unless the
mode is `CHORD`, and strokes the arc alone under `OPEN`, closed by its
chord under `CHORD`, closed through the centre under `PIE`; an ellipse's
arc angles are corrected from the geometric angle a sketch means to the
parametric one the ellipse is traced by, as p5 corrects them. `bezier`
and `curve` fill as well as stroke, the way an open shape does.
`background` covers the whole canvas whatever the transform stands at
and blends when it carries alpha, which is what makes a trail.
`noSmooth` turns off antialiasing AND image smoothing, so a small source
blown up is blocks rather than a blur, and `smooth` puts both back. A
shape whose corners were added under DIFFERENT fills is filled as a
triangle mesh with the colour interpolated across it — a ramp along a
streak, a lit facet, a heat gradient, without one shape per band — while
its stroke still follows the outline; one fill across the shape is one
path, as before, and only the triangle and quad kinds have a mesh. Text is
black until a fill is set and stroked only once a stroke is, so a fresh
pen's text is ink. `point` is a disc of the stroke weight in the stroke
colour. `textSize` sets the leading to five quarters of the size until
`textLeading` says otherwise. `push` saves the style and the transform
together and `pop` restores both; a push left open at the end of a frame
is closed there, back to the style that stood when it opened. `clip`
runs the function it is given with the shape verbs RECORDED rather than
drawn — each in the space it was called in, so a transform inside the
function moves the mask with it — and confines everything drawn after to
what those shapes covered, until the matching `pop()` or the end of the
frame; the verbs that carry no outline, a `line`, an `image`, a `text`,
a `background`, add nothing to a mask. The blend mode is style like any
other, so `push` and `pop` carry it, and it
reaches every verb that puts pixels down — a fill, a stroke, a glyph, an
image, the triangle mesh a per-corner shape is drawn as, and the ground a
`background` lays. `SUBTRACT` takes the source's colour out of the
canvas's and leaves the canvas's alpha alone, so an opaque ground goes
dark rather than transparent.

The pure calculations — `map`, `lerp`, `constrain`, `dist`, `mag`,
`norm`, `sq`, `radians`, `degrees` — are free functions in
`<sigildraw/Math.h>`: they read no pen, so they take no `pen.`. `sin`,
`cos`, `floor`, `round`, `min`, `max`, `abs` and `sqrt` are the standard
library's.

## Where ours differs

Each of these is an ADDED overload on the same verb, or a value standing
beside the verbs, never a renamed one.

* **A material is a fill.** `fill(material::skia::Paint)` and
  `stroke(material::skia::Paint)` take this repository's paint value —
  a gradient, an image, an SkSL effect, a blend — and
  `fill(material::Material)` takes a recipe instance as a shader. A
  static paint resolves once, when set; a live one, and one that reads
  the box it paints, is resolved against the pen's clock and canvas on
  every draw. A paint's coordinates are the pen's current space, so a
  gradient authored in pixels follows the transform.
* **A material can be fitted to the shape.** `fill(paint, SHAPE)` and
  `stroke(paint, SHAPE)` measure the material against the BOUNDS OF EACH
  SHAPE the pen draws — the box's top-left is the material's origin and
  the box is its unit square — so `linearUnit`, `radialUnit`, `glowUnit`
  and anything else reading `uResolution` land on the shape. `CANVAS` is
  the default and measures against the frame. A compose leaf has this and
  needs no word for it, because a node paints inside its own laid-out box;
  a pen has one canvas and many shapes, so which one a material is a unit
  of has to be said — and it is said on the fill, because it is a fact
  about that material. A fill set without the word goes back to the
  canvas. Every verb that fills a shape wears it, and a `line` and a
  `point` on the stroke side; text, images and `background` are always the
  canvas, and a box with no width or no height falls back to it rather
  than dividing by zero. It is style, so `push` and `pop` carry it.
* **A mesh is a shape.** `vertices(sk_sp<SkVertices>)` draws a mesh built
  somewhere else — a triangulated field, a lit strip, a deformed grid, a
  marching-squares contour — with the pen's fill, blend, clip and
  transform, so it lands in the same place and the same order as the pen's
  own verbs and nothing has to go through `canvas()` to put one down.
  Where the mesh carries its own corner colours and the fill is a plain
  colour, the corners paint it, which is the rule `vertex()` follows when
  the corners disagree; where the fill is a material, the material paints
  the whole mesh, and `fill(paint, SHAPE)` makes its unit square the
  mesh's own bounds. A mesh has no outline, so it is not stroked and it
  adds nothing to a clip mask. Building it is Skia's business.
* **A stroke can dash.** `strokeDash({on, off, ...}[, phase])` and
  `noDash()` stand beside `strokeWeight`, `strokeCap` and `strokeJoin`,
  because p5 has no word for a dashed stroke and reaches through to
  `drawingContext.setLineDash`. An odd run repeats itself, so `{6}` is
  six drawn and six skipped; the phase starts the run partway in, which
  is what marches the ants; the lengths are the pen's own units measured
  along the path, so a dashed shape under a `scale` dashes at the scaled
  length. Every stroked verb wears it — a line, a rect, an ellipse, an
  arc, a `beginShape` outline, the outline of a per-corner mesh, a
  glyph's stroke — except `point`, which is a disc and not a stroke.
* **A silhouette is a shape.** `shape(silhouette, x, y, w, h)` fits any
  value with `path(SkSize)` — the geometry kit's `star`, `polygon`,
  `squircle`, `blob`, `annulus`, or one of your own — to the box the
  rect mode reads from the four numbers, and `shape(SkPath)` draws a
  path as it stands. p5's primitives themselves go straight to the
  canvas: a circle is `drawOval`, a rounded rect is an `SkRRect`, an arc
  is `drawArc`, and nothing here re-derives what Skia already draws.
* **Text is shaped.** `text` goes through SigilWeave — kerned, itemised,
  with fallback faces where the face lacks a glyph, and wrapped by the
  paragraph engine in the boxed form. `textFont(family)` matches a
  family through the font context's manager, `textFont(sk_sp<SkTypeface>)`
  takes a face, and `textFont(weave::Type)` takes a whole type: size,
  tracking, condensation and variable axes at once. The fill colours the
  glyphs whatever the type's own colour says. In the boxed form the BOX
  IS THE EXTENT the vertical alignment distributes over, so CENTER seats
  the passage on half the room it left over and BOTTOM on all of it; in
  the unboxed form there is no room to distribute and the alignment
  places the block against the point instead.
* **`noise` is a field over core's mixer.** It has p5's shape — octaves
  at doubling frequency, a falloff between them, a cosine blend, a value
  in [0, 1) — but every lattice corner is core's `lattice` word rather
  than a permutation table, so a pasted sketch draws with p5's character
  and not p5's exact pixels. `NoiseField` is the value behind it.
* **`random` is a seeded stream.** Every pen starts on the same seed and
  `randomSeed` moves off it, so a sketch stepped from zero draws the
  same picture on every run and every machine; p5 seeds from the wall.
* **The clock is the caller's.** `millis`, `deltaTime` and `frameRate()`
  read the frame the pen was begun with — a stepped clock on a plate,
  the wall in a window — and the pen never reads the wall itself.
  `frameRate(fps)` is a request the runtime honours by skipping draws,
  since the runtime owns the clock.
* **Colour strings are a short list.** Hex in its four lengths and the
  named colours a sketch reaches for; anything else reads as black.
* **`beginShape()` with no kind is `POLYGON`**, the one word p5 does not
  spell, and no sketch needs to.
* **The canvas is reachable.** `pen.canvas()` is the `SkCanvas` the pen
  paints on, carrying the pen's current transform — p5's
  `drawingContext`, and the door out of p5's vocabulary. Another
  library's drawing takes an `SkCanvas&`, and this is the one to hand
  it, beside `pen.fillPaint()` and `pen.strokePaint()` for the style the
  pen stands at and `pen.contentScale()` for the device pixels one
  canvas unit covers. What is drawn through it lands in the same place
  and the same order as the pen's own verbs, since there is one canvas;
  leave the transform and the clip as they were found. **Both paints are
  null where there is nothing to hand over** — `fillPaint()` under
  `noFill()`, `strokePaint()` under `noStroke()` or a zero weight — because
  that is what those words mean, so a caller checks before it dereferences
  exactly as every verb in the class does. The pen's blend, its
  antialiasing and its dash ride these paints, so under `noFill()` there
  is nowhere to read them from either: take them off the stroke, or set a
  fill.
* **`createGraphics` is a value, not a call.** `Graphics buffer{w, h}`
  is p5's offscreen canvas — a surface with a pen of its own, kept by
  whoever declares it, because it lives across frames. `buffer.begin(pen)`
  opens a frame on it and hands back its pen, `buffer.end()` closes it,
  `pen.image(buffer, x, y)` puts it down and `buffer.image()` is what it
  holds as an `SkImage`. It is formed at the host pen's own density,
  through the host's canvas so it lives where the host draws, and placed
  by its CANVAS size rather than its pixel count; its clock and fonts
  are the host's, and its style and its pixels hold between frames as a
  pen's and a canvas's do.

## The brush library

A natural-media mark has five independent parts: DEVICE INPUT, an evenly
spaced stream of DABS, a TOOL that says how each dab lands, optionally a
FIELD that bends the centreline, and a polygonal SURFACE that receives a
wash, a hatch or a mass of gestures. `SigilDrawBrush` keeps each as its
own value under `sigil::draw::brush`, so one path can be tried with five
tools and one polygon can receive several interiors. Selection lives in
an explicit `Engine`, never in a process global.

```cpp
#include <sigildraw/brush/Brush.h>

namespace brush = sigil::draw::brush;

brush::Engine brushes;                       // a member of the sketch
brushes.scaleBrushes(2);
brushes.set("HB", {0.10f, 0.18f, 0.28f, 1}, 1.4f);
brushes.wiggle(3);
brushes.line(pen, {30, 220}, {570, 220});

brush::Tool lead = brush::pencil({0.1f, 0.1f, 0.12f, 1}, 2.0f);
brush::line(pen, lead, {30, 260}, {570, 262}, 0.9f, 0.2f);
```

Two conventions run through the whole library. **Angles are the pen's**:
clockwise-positive on the y-down canvas, as `pen.rotate` turns, so
`pen.arc` and `brushes.arc` sweep the same quadrant and a sketch's own
field agrees with the stock ones. A scalar angle passed beside a pen —
`flowLine`, `arc`, `move`, `endStroke`, the scalar `hatch` — is in the
pen's angle mode; an angle inside a value (`Hatch`, `Wash`, `Plot`, a
field's answer) is radians. **The clock is the pen's**: every engine verb
that takes a pen reads its field at `pen.millis()`, so a time-varying
field moves with the frame and stands still on a plate.

### Tools

| word | what it is |
| --- | --- |
| `Tool` | one tool, plain data: tip, colour, width, spacing, opacity, scatter, density, bristles, pressure envelope, blend, rotation, aspect, the jitters, the speed, pressure and tilt responses, `sharpness`, `noise`, `markerTip`, a shape source, a grain source, the dynamics, a custom tip |
| `Tip` | `Dust` (dry particles around the centreline), `Fibres` (parallel hairs, intermittently dry), `Nib` (one pressure-width mark), `Scatter` (particles around each dab), `Image` (the tool's shape source stamped per dab), `Custom` (a callback per dab) |
| `Rotation` | how a shape or custom tip turns: `Fixed`, `Natural` (with the heading), `Random`, `Tilt` (with the stylus azimuth) |
| `Pressure` | the envelope along a stroke: a three-point start/middle/end, or a bell (`gaussianProfile`), or a caller's `curve`; `variation` and the bell's jitters are re-rolled per stroke by `prepareStroke`, which is what makes two strokes with one tool differ |
| `pencil`, `charcoal`, `marker`, `watercolor`, `spray` | stock values; every field stays public |
| `Catalogue` | named tools; `Catalogue::stock()` holds `2B`, `HB`, `2H`, `cpencil`, `pen`, `rotring`, `spray`, `marker`, `marker2`, `charcoal`, `hatch_brush`, `pastel`, `crayon`; `scale` multiplies width, scatter AND spacing |
| `weightedChoice(pen, {{value, weight}…})` | a value in proportion to its weight from the pen's stream; empty answers nothing |

Opacity is the tool's load and the colour's own alpha multiplies it.
Density is how much a dry tip deposits: the probability a dust particle
lands and the share of fibres and scatter particles that deposit, so a
value above one only lets a light pressure keep depositing. A custom tip is called with the pen
translated to the dab, rotated to its angle, scaled to its size and
aspect, the pigment as fill and stroke, and the default rect and ellipse
modes; the transform is restored after every dab, and those four style
words are reset before the next.

### Dabs and deposition

| word | what it is |
| --- | --- |
| `Input` | one device observation: position, pressure, tilt (0 upright, 1 flat), barrel rotation, seconds on the host's clock, tilt direction |
| `Dab` | one deposition event: position, pressure, tilt, barrel, direction, speed, distance, unit progress, tilt direction |
| `Sampler` | live input resampled one spacing apart on `geometry::path::Stride`, which carries the unspent part of an interval across events; speed through a first-order filter of `kSpeedFilterSeconds`; the first dab is held until the first movement gives it a heading, and a stroke that never moves is one dab at direction zero |
| `dabs(input, spacing)` | a whole recorded path resampled, with progress assigned |
| `deposit(pen, tool, dabs, options)` | THE EXECUTOR SEAM: a stored path, a live stylus and generated geometry all reach it. Dust, nib and scatter dabs go down as one sprite batch per stroke; fibres, shape and custom tips and the SUBTRACT blend draw through the pen's verbs dab by dab. `markerTip` pools pigment at the ends the options name; a standing grain puts the whole run of dabs in one layer and takes its coverage out of that |
| `spacingOf(tool)` | how far apart the tool lays its dabs, in canvas units. Every resampling in the library asks this rather than reading `spacing`, because a tool with a shape states its spacing against the stamp |
| `paint(pen, tool, stroke)` | rolls the tool's randomness once, then deposits along a stroke. The dabs carry no speed: a stored path has no clock, so `speedSize` and `speedOpacity` act on live input only |
| `line`, `spline`, `flowLine` | the conveniences over `segment`, `spline` and `trace` |

Deposition pushes and pops the pen around the mark, so the style and
transform it found are restored while the transform still moves the
mark. The round tips' sprite is promoted to a texture once per pen and
kept in the pen's `Retained` store.

### Strokes and fields

| word | what it is |
| --- | --- |
| `Sample`, `Stroke` | `{position, pressure}` and a vector of them: reusable geometry, painted by any tool. The pressure is the LANE a `geometry::path::Polyline` carries, which is why every resampling below interpolates it without being told to |
| `segment(from, to, spacing, p0, p1)` | a straight centreline with a linear pressure ramp: `path::subdivide`, so no step is longer than the spacing |
| `spline(controls, spacing, curvature)` | `path::catmullRom` through the controls, blended toward the chord by `1 − curvature`, pressure interpolated |
| `Direction`, `DirectionField` | the field seam: anything answering a heading in radians for `(SkPoint, float seconds)` |
| `trace(start, length, spacing, seconds, field)` | integrates a start through a field |
| `warp(polygon, spacing, amount, seconds, field)` | a polygon subdivided and displaced along the field, closed |
| `Curl`, `Vortex`, `Wave` | stock fields as values; `Curl` owns its own seeded noise, so two curls with one seed agree whichever pen paints them |
| `stockFields()` | the seven named fields an engine starts with: `hand`, `curved`, `zigzag`, `waves`, `seabed`, `spiral`, `columns` |

### Interiors

| word | what it is |
| --- | --- |
| `Hatch`, `hatch(pen, tool, polygon, style)` | `path::lattice` at `angle` radians and `spacing`, cut to the even-odd interior so holes are skipped; `jitter` moves each mark's ends after the cut, by up to twice that fraction of the spacing, so a jittered mark may cross the edge; `gradient` opens or crowds the lattice's gaps by a tenth per lane; `continuous` joins the marks into one serpentine line. Every mark is thinned at both ends |
| `Wash`, `wash(pen, pigment, polygon)` | a wet interior: `layers` translucent deposits, each the polygon's edge pushed out by a gaussian of the `bleed` and rippled by noise, blooms lifted out and grains settled in by `texture`, pigment gathered at the edge by `border`, the whole composited once with `blend`. It is built in one layer on the pen's canvas, so its pixels are wherever the pen's are |
| `Mass`, `mass(pen, tool, polygon, style)` | chords across the shape at the tool's scatter, each bent into an arc around a pivot outside it and painted only where the arc stays inside; `strength` sets one to three passes, later passes displaced by up to twice the scatter; `precision` steadies the hand — narrower lane jitter, less wobble on each arc; `outline` finishes the boundary |

### Stored geometry

| word | what it is |
| --- | --- |
| `Polygon` | vertices, the whole of its state; `intersect(line)` (`path::edgeCrossings`, nearest the line's start first), `translated`, and `draw`/`fill`/`wash`/`hatch`/`mass` with a tool or through an engine; `show` is every active interior in the engine's order |
| `Plot` | a path by turns: `addSegment(angle, length, pressure)`, `endPlot`, `rotate`; `angle(distance)` and `pressure(distance)`; `path(origin, spacing, curvature, scale)` and `polygon(x, y, …)` place it anywhere at any scale. `fromStroke` records a stroke's turns relative to its first sample. A plot is always relative |
| `PlacedPlot` | a plot and the origin it was first drawn at — what the engine's `circle`, `arc`, `spline` and `endShape` answer |
| `Position` | a cursor: `moveTo(direction, length, step)` walks with its field's answer added to the direction, `plotTo(plot, length, step, scale)` walks a plot's headings; `plotted()` accumulates; with bounds it stops once it has left them by half their size |
| `hatchArray`, `massArray` | one gesture through an even-odd collection: the first polygon is the boundary, the rest cut holes or stand as islands |

The geometry an engine answers is the geometry as sampled, before the
field bent it.

### The engine

`Engine` owns a catalogue, the selected tool with its colour and weight,
a pigment wash and a flat wash (independent, both can be active), a
hatch with an optional dedicated tool, a mass with its tool, a field with
its influence, and a clip; `push`/`pop` save and restore all of it. A
name lookup — `add`, `pick`, `set`, `hatchStyle`, `mass` — answers the
tool it found or null; `field` answers whether the name is known; every
other setter and verb answers nothing.

| verb | what it does |
| --- | --- |
| `set(name, colour, weight)`, `pick`, `stroke`, `noStroke`, `strokeWeight`, `tool()` | the selection; the weight scales width and scatter |
| `fill(colour, opacity)`, `fillBleed`, `fillTexture`, `noFill` | the pigment wash (`Wash`) |
| `wash(colour, opacity)`, `noWash` | the flat wash |
| `hatch(Hatch)`, `hatch(pen, spacing, angle, …)`, `hatchStyle`, `noHatch` | the hatch; the value's angle is radians, the scalar's is the pen's mode; until `hatchStyle`, the selected tool hatches |
| `mass(name, colour, Mass)`, `noMass` | the mass and its tool |
| `field(name)`, `addField(name, field, units)`, `listFields`, `noField`, `wiggle(amount)` | the field; `wiggle` selects `hand` and scales its influence |
| `clip(rect)`, `clip(pen, rect)`, `noClip` | a rectangle every mark, interior and outline is confined to; with a pen, captured in the pen's space at the call and applied there whatever the transform is later — for that canvas |
| `paint`, `line`, `flowLine`, `spline` | strokes with the selected tool through the field; `spline` answers its plot |
| `polygon`, `rect(…, mode)`, `rect(…, radius)`, `circle(…, irregularity)`, `arc`, `beginShape`/`vertex`/`endShape` | surfaces: wash, fill, mass, hatch and the outline in that order, all under the clip, all through one bent boundary; `rect` takes p5's `CORNER`, `CORNERS` or `CENTER` |
| `draw`/`fill`/`wash`/`hatch`/`mass(pen, Polygon)` and `(pen, Plot, x, y, scale)` | one interior over stored geometry |
| `hatchArray`, `massArray` | over a collection |
| `position(x, y)`, `position(pen, x, y)` | a cursor through the field; with a pen, at the pen's clock and bounded by the canvas |
| `beginInput`, `moveInput`, `endInput`, `cancelInput` | live input through the sampler and the executor; nothing is deposited before the first movement, and the tool's randomness is rolled once at `beginInput` |
| `beginStroke(kind, at)`, `move(pen, angle, length, pressure)`, `endStroke(pen, angle)`, `cancelStroke` | a stroke by turns, in the pen's angle mode |

A closed shape's interiors and outline come from one boundary: the
polygon is resampled at a fixed step, bent through the field once, and
the drift the bends accumulate is taken back out along the way, so the
outline sits on the wash's edge and the shape closes. Two engines draw
through one pen without seeing each other's state.

### Custom brushes

A tool built from pictures is the brush a painting program means by the
word: an artwork stamped along the stroke, a texture the mark is laid
through, and curves the device drives. Three values on `Tool` say it,
and a tool that sets none of them behaves exactly as a tool without
them.

| word | what it is |
| --- | --- |
| `Shape` | the SHAPE SOURCE: the artwork stamped at every dab, its `mask` saying whether coverage is the alpha channel or one minus the luminance — so dark artwork on white works as drawn — plus `spacing`, `scatter` and `angleJitter`. Those three are FRACTIONS OF THE STAMP, not canvas units, which is how a brush that travels between programs states them: a `spacing` of a tenth is a dense continuous mark and one is a chain of separate stamps |
| `Grain` | the GRAIN SOURCE: a texture tiled in both axes whose LUMINANCE is coverage — the mark survives where the texture is white and is taken away where it is black — with `scale` in the pen's space and `depth` for how much may be taken |
| `GrainSpace` | `Stroke`, the texture fixed in the pen's space, so two marks crossing one place meet one surface; `Dab`, the texture riding each stamp, turning and travelling with it. A dab-space grain needs a stamp to ride, so it applies to a shape tip; every other tip deposits as one sprite batch and takes its grain standing still whichever space it asks for |
| `Curve` | one response curve, `minimum` at zero to `maximum` at one with a `bend` between them, or a caller's own function. The answer is a MULTIPLIER on what the tool already decided, so a flat curve at one changes nothing |
| `Drive` | what a curve reads, each arriving as a unit value: `Pressure` (the stylus with the tool's envelope already applied), `Velocity` (one at `speedReference` units per second and above), `Tilt` |
| `Dynamics` | a `Response` — a curve and its drive — for `size`, `opacity` and `flow`. Opacity is the tool's load and flow is the one dab's; nothing buffers a dab before the canvas here, so the two multiply into the same alpha, and they are separate because one may follow the stylus while the other follows the hand |

```cpp
brush::Tool ink = brush::marker({0.1f, 0.1f, 0.12f, 1}, 26.0f);
ink.tip = brush::Tip::Image;
ink.shape = brush::Shape{.image = tipArtwork, .spacing = 0.08f,
                         .scatter = 0.15f, .angleJitter = 0.4f};
ink.grain = brush::Grain{.image = paperTexture, .scale = 1.5f, .depth = 0.7f};
ink.dynamics.size =
    brush::Response{.drive = brush::Drive::Pressure,
                    .curve = {.minimum = 0.25f, .maximum = 1.0f}};
brush::line(pen, ink, {30, 200}, {570, 240});
```

**The native format is a directory.** `<name>.sigilbrush/` holds
`brush.json`, `shape.png` and an optional `grain.png`, so the artwork
stays a picture a painting program can open and edit in place and the
numbers stay a text file a person can read. `brush.json` names the tool's
own fields, a `shape` and a `grain` object, and a `dynamics` object of up
to three responses; every key it leaves out keeps the library's default.
`format::encodeBrush(tool)` writes that text back.

**Loading is `SigilDrawBrushFormat`, and it never opens a file.**
Everything there takes bytes — from a hub, a fixture or a caller's own
array — and the pictures inside them are decoded by SigilImage:

```cpp
#include <sigildraw/brush/format/Load.h>
namespace format = sigil::draw::brush::format;

hub.registerDecoder<brush::Tool>(format::BrushDecoder{});
auto ink = format::loadBrush(hub, "res://brushes/ink.sigilbrush");   // a directory
auto abr = hub.load<brush::Tool>("res://brushes/library.abr");       // one file
```

A directory has no bytes of its own, which is why loading one goes
through a byte source: the three parts are three ordinary resources
under it. Anything that is ONE file — a packed `.sigilbrush` archive, a
Photoshop `.abr`, a Procreate `.brush` — is what `decodeBrush` sniffs and
what a hub's registered decoder runs, so one decoder answers for every
form a brush arrives as.

**The two importers are bounded, and each header states its bounds.**
`decodePhotoshopBrushes` reads `.abr` versions 6, 7 and 10 and answers
every SAMPLED tip's bitmap, raw or PackBits-compressed, 8 or 16 bits
deep. It does not parse the Photoshop DESCRIPTOR those versions keep the
names, spacing, scattering, shape dynamics, texture, dual brush and
transfer in, and computed brushes — the ones with no bitmap — are left
out; an imported tool is its shape and this library's defaults for
everything else. `decodeProcreateBrush` reads the shape and grain
pictures out of the zip a `.brush` is. It does not read `Brush.archive`,
the NSKeyedArchiver property list in Apple's binary encoding that holds
every number the brush states, so the same applies: the pictures are the
file's and the numbers are this library's.

### Sketches

`brush_live_tutorial` moves through field lines, the stock-tool wheel,
overlapping hatches, accumulating watercolor and pressure-bearing splines
in one authoring example. `brush_engine_atlas` and `brush_dynamics` are
compact plates for tool definitions and stylus input. `brush_rain`,
`brushwork_currents` and `brush_botanical_study` use the same parts in
complete compositions. `bristle_bloom` and `bristle_current` are
lower-level companion studies that build brush bundles from the pen's
curves, lines and blend modes with no brush library in them.

## Not provided

p5 verbs and variables a pasted sketch has to replace, stated so nobody
searches for them: `createVector` and `p5.Vector` (glm has the
vectors), `tint`, `filter`, `erase`/`noErase`, `beginClip`/`endClip`
(`clip` takes the shape as a function),
`loadPixels`/`updatePixels`/`pixels`/`get`/`set`,
`save`/`saveCanvas`/`saveFrames`/`saveGif`, `describe`/`textOutput`,
`cursor`/`noCursor`, `fullscreen`,
`windowWidth`/`windowHeight`/`displayWidth`/`displayHeight`/`windowResized`,
`pixelDensity`, `textWrap` (words wrap), `curveDetail`/`bezierDetail`,
`beginShape(TESS)`, `texture`, `mouseButton`, `movedX`/`movedY`,
`touches`, `mouseWheel`, `random(array)`, `shuffle`, `print`, `preload`,
`loadFont`/`loadJSON`/`loadStrings`/`loadSound`, the DOM
(`createButton`, `createSlider`, `select`), and WEBGL with everything
under it (`box`, `sphere`, `rotateX`, `camera`, `lights`) — a lit set is
`sketch::Set`.

## The other way through the door: a retained guest

A pen program is imperative; what it paints each frame is gone the
moment the frame is. Some things are worth keeping between frames — a
compose element tree with its layout, its text shaping, its bindings and
its caches — and the pen paints those as **guests**:

```cpp
pen.element(card, SkRect::MakeXYWH(40, 40, 320, 180));  // one card
for (int i = 0; i < 3; ++i)
  pen.element(row(i), SkRect::MakeXYWH(40, 260 + i * 60, 320, 50), i);
```

A guest is told apart by the CALL SITE — file, line and column — and by
the index a loop adds; the pen keeps one retained value per slot, in
`Retained`, and hands it back next frame. What a guest is and how it is
painted is its own library's business: that library declares
`paintRetained(Pen&, const Guest&, const SkRect&, Slot)` in the guest's
own namespace, argument lookup finds it, and this library names no
guest. SigilCompose declares it for `Element`, in its `draw` feature,
which is also where `compose::draw(program)` hosts a pen program inside
a node — the same door from the other side.

**The clock is the runtime's.** A guest's own clock is stepped by the
pen's frame delta on the frames the guest is painted and stands still on
the frames it is not; nothing a guest holds ever reads the wall, which
is what keeps a plate with a guest in it reproducible.

## Layout

```
src/common/draw/
  include/sigildraw/
    Draw.h        the umbrella
    Pen.h         the pen, Frame, and the Retainable and Silhouette concepts
    Constants.h   p5's words and angles
    Color.h       ColorMode, colorFrom(), parseColor()
    Noise.h       NoiseField
    Retained.h    Slot and Retained
    Graphics.h    the offscreen buffer, p5's createGraphics
    Math.h        the pure calculations
    brush/
      Brush.h     the brush library's umbrella
      Tool.h      Tool, Tip, Rotation, the stock tools, prepareStroke, spacingOf
      Shape.h     Shape and ImageMask, the shape source
      Grain.h     Grain and GrainSpace, the grain source
      Dynamics.h  Curve, Drive, Response, Dynamics
      Pressure.h  the pressure envelope
      Catalogue.h named tools and the stock catalogue
      Choice.h    weightedChoice
      Dab.h       Input and Dab
      Sampler.h   the live sampler and dabs()
      Deposit.h   deposit, paint, line, spline, flowLine
      Stroke.h    Sample, Stroke, segment, spline
      Field.h     Direction, DirectionField, trace, warp
      Fields.h    Curl, Vortex, Wave, stockFields
      Hatch.h     Hatch and hatch
      Wash.h      Wash and wash
      Mass.h      Mass and mass
      Polygon.h   Polygon, hatchArray, massArray
      Plot.h      Plot and PlacedPlot
      Position.h  the cursor
      Engine.h    the engine
      format/
        Load.h       assembleBrush, decodeBrush, encodeBrush, BrushDecoder,
                     loadBrush over any byte source
        Photoshop.h  the .abr reader and what it honours
        Procreate.h  the .brush reader and what it honours
  Pen.cpp         the frame, the style, the shapes, the transform, the streams
  Graphics.cpp    the offscreen buffer
  Text.cpp        text through SigilWeave
  Color.cpp       the colour models and the CSS string
  Noise.cpp       the layered field
  brush/          one source per header above; the executors (Stamps, Fibres,
                  Tips, Grain) and the engine's strokes and surfaces in their
                  own files; the private seams DabStyle.h, Executors.h,
                  HatchLines.h, PenUnits.h, PolygonMath.h; test/ and bench/
  brush/format/   the native reader and writer (Native), the two importers
                  (Photoshop, Procreate), and the private Zip and Images;
                  test/
  test/           draw_test, draw_text_test and the Paper fixture in support/
  bench/          draw_bench
```

## Boundaries

* **Links material's Skia paint, weave's shaping, layout and paint, and
  core's mixers.** A fill IS `material::skia::Paint`; text IS a
  `weave::Paragraph` laid out and drawn; the random stream and the noise
  corners ARE `core::noise`. None of that is re-spelled here.
* **Knows no runtime and no compose.** The pen is handed a canvas and a
  frame; the sketch runtime that steps it and the compose feature that
  hosts it both stand above this library. A guest reaches the pen
  through the `paintRetained` seam, never through a type named here.
* **The brush library reaches down to the pen, and to SigilSkia's direct
  drawing for one thing.** Its tools, hatches, washes and masses are
  arrangements of paths and points over the pen's public verbs, its
  strokes are plain data, and its fields are callable values; the sprite
  batch a round tip goes down as is `sigilskia/draw/Direct.h`'s. A
  consumer with its own tools links `SigilDraw` without `SigilDrawBrush`.
* **The geometry under a mark is SigilGeometryPath's.** The walk that
  spaces dabs along a stroke is `path::Stride`, the centrelines are
  `path::subdivide` and `path::catmullRom` over a `path::Polyline` whose
  lane is the pressure, and what a surface's interior is filled and
  tested with is `path::lattice` and `path::containsEvenOdd`. What stays
  here is what a device and a tool know and geometry does not: pressure,
  tilt and speed, the pen's random stream, the grain and the pigment.
* **A brush is decoded where the brush lives, and only from bytes.**
  `SigilDrawBrushFormat` is a target of its own beside the tools, so a
  consumer that paints links `SigilDrawBrush` alone. It speaks
  SigilIOSource's byte vocabulary, which is what lets a loader run
  against a fixture and behind a hub unchanged, and it hands the
  artwork inside a brush to SigilImage. Where the bytes came from —
  URIs, mounts, caching, reload — is SigilIO's, and no file is opened
  here in either direction.
* **Never reads the wall.** Every number a pen answers about time comes
  from the frame it was given.

## Build and test

From `apps/spell-circle-canvas`:

```sh
cmake --build build --config Release --target draw_test draw_text_test \
  draw_brush_test draw_brush_format_test draw_bench draw_brush_bench
ctest --test-dir build -C Release -R '^draw_' --output-on-failure
./build/bin/Release/draw_bench
./build/bin/Release/draw_brush_bench
```

`draw_test` holds p5's semantics to the pen — a rect at `rectMode(CENTER)`
lands where p5 says, `push`/`pop` restores fill and transform, an arc
fills the pie unless `CHORD`, one seed gives one sequence on every pen
and a draw lands inside the range it was asked for, `noise` at a
lattice corner is the corner the field names, `noSmooth` sampling an image
nearest-neighbour, a `fill` between two vertices colouring the corners
either side of it — and this library's own: a material as a fill, a
silhouette as a shape, a guest retained per call site, the canvas
carrying the pen's transform, an offscreen buffer formed at the host's
density and put down in canvas units, a unit-space material ramping
across the frame under `CANVAS` and across each box under `SHAPE`, a
built `SkVertices` drawn with the pen's fill and moved by the pen's
transform, and both paints answering null where the style says there is
nothing to draw with. `draw_text_test`, under the ctest label `fonts`,
holds text shaped and centred by its alignment and seated by its box; it
shapes against the machine's system fonts, so it pins relations rather
than pixels. `draw_bench` times ten thousand circles filled and stroked,
ten thousand rects, a screen of text, a translucent background and a
thousand noise samples per frame; it builds through the `benches` target
and runs through `scripts/bench_ledger.py`.

`draw_brush_test` is one file per subject: the sampler's spacing across
uneven events and the first dab's heading; segment and spline pressure;
the envelope, the per-stroke roll and the weighted choice; the stock
catalogue and lookup by view; every tip, the stylus dynamics, the sprite
batch, a stored path's zero speed and the custom tip's contract — with
batching stated as a growth claim, five times the dabs recorded in
fewer than twice the ops, rather than as an op-count ceiling; hatches
inside their polygon, even-odd across a collection, and the default
angle under a pen in degrees; the wash's interior and its closed layer;
the mass inside its surface, with holes, under the engine's clip; the
polygon's edges derived from its vertices; relative plots placed and
scaled by the caller; the cursor through its field and inside its
bounds; and the engine — selection and state, the pen's units and clock,
one clip over every interior and the outline, a closed shape's outline
on its bent interior, plots placed where they were drawn, live input
across event batches, the first live dab's heading, and cancel.
`draw_brush_format_test` builds every fixture in memory — a tip drawn
and encoded to PNG, a zip written entry by entry, an `.abr` written
field by field — so no case reads a file this repository ships: a native
directory through a table and through a hub, the packed archive, the
`.brush` giving up its two pictures, the `.abr` giving up both its
sampled tips at either subversion, and bytes that are no brush answering
nothing.

Every binary here draws on one fixture, `test/support/Paper.h` — a pen
over a raster surface with the pixels readable back — whose font context
is the tree-wide `src/test/Fonts.h`, so one process shapes against one
memoised context. A case asserts one thing the pen promises and is
named that promise as a sentence; it pins only what editing this library
could falsify, and the one binary whose claims depend on the machine's
faces carries the `fonts` label.

`draw_brush_bench` measures sampling, a field-traced watercolor mark,
hatching, a curved dry mass, a pigment wash, and one stroke of an
imported brush — the shape stamped per dab, alone and under each of the
two grain spaces.
