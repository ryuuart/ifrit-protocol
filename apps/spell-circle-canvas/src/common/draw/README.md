# SigilDraw — an immediate-mode canvas with p5's brevity

A **pen** over an `SkCanvas`, carrying p5's verbs with p5's names,
argument orders and defaults, so a sketch written for p5 pastes in and
runs. Compose is the declarative way to draw here; this is the
imperative way beside it, and the two open onto each other.

Namespace `sigil::draw`, with the `SigilDraw` pen and the `SigilDrawKit`
stock tools over it. Every public header is under `include/sigildraw/`;
`<sigildraw/Draw.h>` is the pen's umbrella and
`<sigildraw/kit/Brushwork.h>` is the brush kit's.

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

  void draw(Pen& pen) override {
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
canon is `src/sketch/README.md` — and `setup` speaks to that runtime
through its context, which hands over the pen for anything a p5 setup
would have set on the canvas. Everything else is the pen.

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

## The brush kit

A natural-media drawing has five independent parts: DEVICE INPUT, an evenly
spaced stream of DABS, a TOOL that says how each dab lands, optionally a FIELD
that bends the centreline, and a polygonal SURFACE that can receive a wash,
hatch or mass of gestures.
`SigilDrawKit` keeps those as separate values under `sigil::draw::brush`, so
one path can be tried with five tools and one polygon can receive several
interiors. Mutable selection lives in an explicit `Engine`, never in process
globals.

```cpp
#include <sigildraw/kit/Brushwork.h>

namespace brush = sigil::draw::brush;

brush::Engine brushes;
brushes.scaleBrushes(2);
brushes.angleMode(DEGREES);
brushes.set("HB", {0.10f, 0.18f, 0.28f, 1}, 1.4f);
brushes.wiggle(3);
brushes.line(pen, {30, 220}, {570, 220});
```

`Input` carries position, pressure, tilt, tilt direction, barrel rotation and
host time. Tilt is zero with the stylus upright and one with it flat against
the surface; both orientations are radians.
`Sampler` converts any event rate into evenly spaced `Dab` values while
carrying the unused part of a spacing interval between events. Its speed is a
first-order filtered value, so size and opacity dynamics do not chatter when a
device reports uneven intervals. `deposit` is the executor seam. A stored path,
a live stylus and generated geometry all reach the same deposition code. Round
grain, nib and scatter tips accumulate their independently coloured and sized
dabs into sprite vertices and submit the whole stroke in bounded batches;
custom callbacks and the subtract blender retain their direct executor.

`Brush` is public data. `Tip::Grain` deposits dry-media particles,
`Tip::Fibers` carries persistent hairs with clustered dry gaps, and `Tip::Nib`,
`Tip::Scatter`, `Tip::Image` and `Tip::Custom` provide the other deposition
mechanisms. Width, spacing and scatter are canvas units; opacity is a unit
value; grain is a deposit density; bristle count, blend, aspect, fixed,
natural, random or stylus-tilt rotation, size and opacity jitter, speed
response and a pressure curve are direct knobs. Pressure can drive size and
opacity. Tilt can
drive size, opacity, aspect and offset independently, while barrel rotation
remains a separate input. An image tip treats dark artwork on white as its mask
by default, matching common brush-tip assets; `ImageMask::Alpha` selects an
authored alpha channel instead. A callback tip draws around the origin in a
one-unit square after the engine applies the dab's position, orientation, size
and aspect. `pencil`, `charcoal`, `marker`, `watercolor` and `spray` remain
plain stock values.

`Box` is an instance-owned catalogue. `Box::stock()` contains `2B`, `HB`, `2H`,
`cpencil`, `pen`, `rotring`, `spray`, `marker`, `marker2`, `charcoal`,
`hatch_brush`, `pastel` and `crayon`. `Engine` owns one box and the current
brush, colour, weight, watercolor fill, flat wash, hatch, mass, field and clip.
Fill and wash are independent and can be active together. A surface is composed
in wash, fill, mass, hatch and outline order. A hatch uses the current brush
until `hatchStyle` supplies a dedicated one. Its `add`, `box`, `scaleBrushes`,
`pick`, `set`, `stroke`, `noStroke` and `strokeWeight` form the stateful
convenience over the same public values. Two engines can draw through one pen
without seeing one another's state. `angleMode(RADIANS | DEGREES)` sets the
units of every angle-taking engine operation. Public directions are
anticlockwise on the downward-y canvas, while reusable values remain normalized
to radians internally.

A `Stroke` is a vector of `{position, pressure}` samples. `segment`
samples a line, `spline` smooths control samples and `trace` integrates
through ANY callable answering a direction in radians for `(point,
seconds)`. The stock `fields::Curl`, `fields::Vortex` and `fields::Wave`
are ordinary callables; a lambda or a sketch's own field drops into the
same seam. `paint` deposits a brush along an existing stroke, while
`line`, `spline` and `flowLine` are the direct conveniences.

`hatch` clips parallel brush marks to a polygon. Its `Hatch` value carries
spacing, angle, placement jitter, continuous serpentine lines and a spacing gradient while the ordinary
`Brush` still controls how each line deposits. `wash` builds a polygonal
interior from independently perturbed translucent layers; its `Wash` value
controls colour, opacity, bleed, granulation, edge pooling, layer count and
blend. `mass` intersects spaced scanlines with the actual even-odd surface,
projects those chords around a shared outside pivot and paints only arcs whose
interior samples remain in the shape. Up to three translated layers make the
edge and gesture family vary together; precision controls their displacement,
strength controls the layer count, and gradient and outline remain independent.
None leaves a clip or style change behind.

`Engine` carries the built-in fields `hand`, `curved`, `zigzag`, `waves`,
`seabed`, `spiral` and `columns`; `addField` accepts any callable answering an
angle from a point and time, with radians as the default and degrees accepted
explicitly. `field`, `noField`, `refreshField`, `listFields` and `wiggle`
control the current instance. Geometry is integrated through the selected
field as it travels; `wiggle` scales its angular influence. Closed geometry
removes accumulated drift before it becomes a surface. The free `warp`
operator remains the direct displacement form over the same callable seam.

`Polygon` stores vertices and sides and can draw, fill, wash, hatch and mass
itself. `show` composes every active surface operation in the engine's order.
`Plot` stores relative segments, angles and pressures; it can be rotated,
sampled by distance, scaled, converted into a polygon and replayed through the
same operations. Plots returned by strokes and primitives retain their sampled
geometry exactly. `Position` exposes `x`, `y` and accumulated `plotted`
distance, moves directly or through a `Plot` while sampling the active field,
and provides `angle`, `reset`, `update`, `isIn` and `isInCanvas`. A position
made with `Engine::position(pen, ...)` stops after leaving the canvas plus its
half-canvas working margin. `hatchArray` and `massArray` accept either one
polygon or a collection and treat a collection as one even-odd surface, so
nested contours cut holes and disjoint contours remain part of the same
gesture.

Painting uses the pen's seeded random stream, colour, blend and primitive
verbs. It pushes and pops the pen around the mark, so the style and
transform it found are restored, while the transform still moves the
mark itself. Seed the pen once and the same brushwork is reproduced on a
plate and in a live session. `wRand` uses that stream to choose from weighted
values and returns no value for an empty distribution.

The p5.brush-shaped surface maps to the kit as follows:

| area | surface |
| --- | --- |
| target, transform and seeds | the caller begins a `Pen` on its target; the pen's transform and seeded streams are inherited by every mark |
| brush definitions | named box, scaling, dry grain, persistent fiber, nib, spray, custom and luminance/alpha image tips, four rotation modes, piecewise, callback and Gaussian pressure curves, sharpness, grain, stroke noise and scattered endpoint buildup |
| dynamics | pressure-to-size/opacity, tilt-to-size/opacity/aspect/offset, tilt direction, barrel rotation, speed-to-size/opacity and per-dab size/opacity/spacing jitter |
| stroke state | set, pick, stroke, noStroke, strokeWeight, captured rectangular clip, noClip, angleMode, push and pop |
| strokes | line, flowLine, spline, relative beginStroke/move/endStroke and live beginInput/moveInput/endInput over the same sampler and executor |
| fields | all seven stock names, field/noField/refreshField/listFields/addField/wiggle and field-aware Position |
| interiors | independent fill/noFill and wash/noWash, directional fillBleed, fillTexture, hatch/noHatch/hatchStyle/hatchArray and mass/noMass/massArray |
| primitives | rect, rounded rect, circle, arc, polygon and beginShape/vertex/endShape |
| exposed values | Brush, Input, Dab, Stroke, Box, Engine, Hatch, Wash, Mass, Polygon, Plot, PlacedPlot and Position, including their direct draw/fill/wash/hatch/mass/show operations |
| utility | deterministic random/noise through Pen and weighted choice through wRand |

The host operations do not need brush-kit aliases. `DrawContext::canvas` and
`Pen::begin` provide canvas creation, target loading and instance selection;
`Pen::end` renders the drawing pass and `Pen::clear` clears it.
`Pen::random`, `Pen::noise`, `Pen::randomSeed` and `Pen::noiseSeed` own every
brush random stream. The pen's `push`, `pop`, `translate`, `rotate` and `scale`
already carry the canvas state the kit draws through, and `Engine::angleMode`
both sets and reads brush angle units. `SkColor4f` is the plain colour value.
An image-tip URI is resolved by SigilIO and SigilImage before its `SkImage`
is assigned to the plain `Brush` value.

The timed `brush_live_tutorial` moves through field lines, the stock-tool
wheel, overlapping hatches, accumulating watercolor and pressure-bearing
splines in one authoring example. `brush_engine_atlas` and `brush_dynamics`
remain compact diagnostic plates for tool definitions and stylus input.
`brush_rain`, `brushwork_currents` and `brush_botanical_study` use the same
parts in complete compositions rather than isolating one call. Every sketch
seeds both fields and deposition and produces the same image on every fresh
run. `bristle_bloom` and `bristle_current` are lower-level companion studies:
they build brush bundles directly from the pen's curves, line segments, blend
modes, persistent canvas and fixed-step runtime when a sketch needs a
deposition model outside the stock kit.

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
    kit/
      Brushwork.h the brush kit's umbrella
      Brush.h     tools, pressure envelopes and dab deposition
      Box.h       instance-owned named definitions
      Dab.h       device input, filtered sampling and deposition records
      Engine.h    selection, live strokes and surface-effect state
      Path.h      pressure-bearing paths and field tracing
      Fields.h    curl, vortex and wave direction fields
      Geometry.h  reusable Polygon, Plot and field-aware Position values
      Shape.h     clipped hatches, pigment washes, mass and field warping
  Pen.cpp         the frame, the style, the shapes, the transform, the streams
  Graphics.cpp    the offscreen buffer
  Text.cpp        text through SigilWeave
  Color.cpp       the colour models and the CSS string
  Noise.cpp       the layered field
  kit/            the stock brush target, test and benchmark
  test/           draw_test
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
* **The kit reaches down only to the pen.** Its brushes, hatches and washes
  are arrangements of paths and points over public verbs, its paths are
  plain data, and its fields are callable values over `NoiseField`. A
  consumer with its own tools links `SigilDraw` without `SigilDrawKit`.
* **Never reads the wall.** Every number a pen answers about time comes
  from the frame it was given.

## Build and test

From `apps/spell-circle-canvas`:

```sh
cmake --build build --config Release --target draw_test draw_kit_test \
  draw_bench draw_kit_bench
ctest --test-dir build -C Release -R 'draw(_kit)?_test' --output-on-failure
./build/bin/Release/draw_bench
./build/bin/Release/draw_kit_bench
```

`draw_test` holds p5's semantics to the pen — a rect at `rectMode(CENTER)`
lands where p5 says, `push`/`pop` restores fill and transform, an arc
fills the pie unless `CHORD`, seeded `random` repeats, `noise` at a
lattice corner is core's word, `noSmooth` sampling an image
nearest-neighbour, a `fill` between two vertices colouring the corners
either side of it — and this library's own: a material as a fill, a
silhouette as a shape, text shaped and centred by its alignment, a guest
retained per call site, the canvas carrying the pen's transform, an
offscreen buffer formed at the host's density and put down in canvas
units, a unit-space material ramping across the frame under `CANVAS` and
across each box under `SHAPE`, a built `SkVertices` drawn with the pen's
fill and moved by the pen's transform, and both paints answering null
where the style says there is nothing to draw with.
`draw_bench` times ten thousand circles
filled and stroked, ten thousand rects, a screen of text, a translucent
background and a thousand noise samples per frame; it builds through the
`benches` target and runs through `scripts/bench_ledger.py`.
`draw_kit_test` holds pressure interpolation and custom curves, path endpoints,
event-rate-independent dab spacing, filtered speed, tilt and barrel-angle
interpolation, stylus dynamics, luminance image masks, weighted choice, the
complete stock box, custom-tip transforms, isolated engine and angle-mode
state, captured clipping, integrated callable fields, reusable and scaled
plots, bounded positions, even-odd array effects, pen-style restoration,
polygon clipping, independent fill and wash, wash deposition and mass fill.
`draw_kit_bench` measures sampling, a field-traced watercolor mark, hatching,
a curved dry mass and a pigment wash.
