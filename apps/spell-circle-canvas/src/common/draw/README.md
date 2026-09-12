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
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
using namespace sigil::draw;

struct Bounce final : sketch::Sketch {
  float x = 200, y = 100, vx = 3, vy = 2;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(400, 300);
    ctx.composer.render(
        compose::graphics("bounce.loop", [this](Pen& pen) { draw(pen); })
            .absolute()
            .inset(0));
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) pen.noStroke();
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
whoever steps the pen — here a `compose::graphics` node filling the
canvas of a `sketch::Sketch`, whose canvas is KEPT between frames, which
is what makes the translucent ground a trail. The program is the p5
`draw`, run once per frame with the node's own pen, and it honours
`noLoop`, `redraw` and `frameRate` as p5 does. What a p5 `setup` would
have set on the canvas — a style, a seed, a drawing made once — is the
program's first frame, which `pen.frameCount == 1` names; the size, the
ground and the capture moment are declared to the sketch's own context.
Everything else is the pen.

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
pen's text is ink; a fill a host seeded through `inherit` counts as set,
so text under an inherited ink is set in that ink. `point` is a disc of the stroke weight in the stroke
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
* **The pen can be told what it inherits.** `inherit(ink, font)` is what a
  host calls after `begin` each frame, and it seeds ONLY THE STYLE THE
  PROGRAM HAS NOT SET: the fill and the stroke take the ink until a `fill`
  or a `stroke` is called — the glyphs with them, since a seeded fill is
  a fill as far as text is concerned — and the text type takes the
  `weave::Type` until a `textFont`, a `textSize` or a `textStyle` is. After that the program's
  own choice holds from frame to frame the way every other p5 style does,
  and the inherited pair stops reaching it. Nothing else is touched, so a
  `noFill()` still means no fill whatever the ink is, and the inherited
  size seats the leading on the same five-quarters rule `textSize` does.
  The pair is remembered — `inheritedInk()` and `inheritedFont()`, black
  and `weave::initialType()` on a pen that was never told one — so
  whatever else a frame seeds reads it off the pen rather than keeping a
  copy of its own. **A pen nobody calls this on keeps p5's own defaults**:
  a white fill, a black stroke, text at twelve pixels. What has such a
  pair to hand over is a host with a cascade — a declarative tree where
  every node carries a resolved colour and a resolved type.
* **`createGraphics` is a value, not a call.** `Graphics buffer{w, h}`
  is p5's offscreen canvas — a surface with a pen of its own, kept by
  whoever declares it, because it lives across frames. `buffer.begin(pen)`
  opens a frame on it and hands back its pen, `buffer.end()` closes it,
  `pen.image(buffer, x, y)` puts it down and `buffer.image()` is what it
  holds as an `SkImage`. It is formed at the host pen's own density,
  through the host's canvas so it lives where the host draws, and placed
  by its CANVAS size rather than its pixel count; its clock and fonts
  are the host's, and its style and its pixels hold between frames as a
  pen's and a canvas's do. `resize(w, h)` gives it another canvas size,
  and A SURFACE THAT HAS TO BE REPLACED KEEPS THE PICTURE: when the
  density moves or the size does, what the old surface held is drawn into
  the replacement scaled to its extent rather than cleared out of it — a
  buffer is where earlier frames accumulate, and growing one must not
  erase them.

## The brush library

Natural media — device input, dabs, tools, fields and the interiors a
polygon receives — is `SigilDrawBrush`, a target of its own over the pen.
Its chapter is `brush/README.md` beside the code: the five parts, the
stock tools, the dynamics a stylus drives, the surfaces, and the brush
formats a tool is imported from and written back to.

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
which is also where `compose::pen(program)` hosts a pen program inside
a node — the same door from the other side.

**The clock is the runtime's.** A guest's own clock is stepped by the
pen's frame delta on the frames the guest is painted and stands still on
the frames it is not; nothing a guest holds ever reads the wall, which
is what keeps a plate with a guest in it reproducible.

**The guest begins in the pen's ink and font.** What a guest cascades from
is the pair the pen was told it inherits, read off `inheritedInk()` and
`inheritedFont()` by whoever wrote the `paintRetained` for it — so a tree
painted inside a pen program starts in the same colour and the same type
the pen's own verbs do, and a pen nobody told seeds it with black and the
initial type.

## Layout

```
src/common/draw/
  include/sigildraw/
    Draw.h        the umbrella
    Pen.h         the pen
    PenTypes.h    Frame, ClipOptions, and the Retainable and Silhouette
                  concepts
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
  Pen.cpp         the frame, the style, the colour model, the modes, the
                  random stream
  PenShapes.cpp   every shape, and the dash the outlines wear
  PenClip.cpp     the clip
  PenImage.cpp    images in p5's image modes
  PenTransform.cpp  translate, rotate, scale, shear, push, pop
  PenInternal.h   what more than one of those reads (PenInternal.cpp)
  Graphics.cpp    the offscreen buffer
  Text.cpp        text through SigilWeave
  Color.cpp       the colour models and the CSS string
  Noise.cpp       the layered field
  brush/          one source per header above that has a body to compile
                  (Shape.h, Dab.h, Field.h and Brush.h are declarations
                  alone); the executors (Stamps, Fibres,
                  Tips, Grain) and the engine's strokes and surfaces in their
                  own files; the private seams DabStyle.h, Executors.h,
                  HatchLines.h, PenUnits.h, PolygonMath.h; test/ and bench/
  brush/format/   the native reader and writer (Native), the two importers
                  (Photoshop, Procreate), and the private Import; test/
  shaders/        Subtract.sksl, the ground SUBTRACT is laid with
  test/           the pen's cases, one file per subject, and the Paper
                  fixture in support/
  bench/          draw_bench
```

## Boundaries

* **Links material's Skia paint, weave's shaping, layout and paint, and
  core's mixers.** A fill IS `material::skia::Paint`; text IS a
  `weave::Paragraph` laid out and drawn; the random stream and the noise
  corners ARE `core::noise`. None of that is re-spelled here.
* **Boost.Unordered and Boost's hash fold are in public headers.** The
  call-site store a retained mark is keyed in (`Retained.h`), the brush
  engine's direction registry (`brush/Engine.h`) and the brush catalogue
  (`brush/Catalogue.h`) each hold a Boost table or fold one of its keys,
  so a consumer of those headers compiles against Boost. Both are
  header-only and declared PUBLIC where they are named — `SigilDraw`
  takes both, `SigilDrawBrush` takes the table — and the keys live
  inside one run, which is why Boost's fold rather than the pinned one.
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

Targets: `SigilDraw`, `SigilDrawBrush` and `SigilDrawBrushFormat` — one
per feature directory (`.`, `brush/`, `brush/format/`) — with one test
binary, `draw_test`, over every one of their `test/` directories and one
benchmark binary, `draw_bench`, over the two `bench/` ones.

```sh
cmake --build build --config Release --target draw_test draw_bench
ctest --test-dir build -C Release --output-on-failure
./build/bin/Release/benches/draw_bench
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
density and put down in canvas units and keeping what it held across a
resize, a pen beginning in an inherited ink and font and dropping both
for whatever the program set itself, a unit-space material ramping
across the frame under `CANVAS` and across each box under `SHAPE`, a
built `SkVertices` drawn with the pen's fill and moved by the pen's
transform, and both paints answering null where the style says there is
nothing to draw with. The text cases hold text shaped and centred by its
alignment, seated by its box, black until a fill is set and in the ink
once one was seeded; they shape
against the tree's instrument face, so they pin relations rather than
pixels. The one case that names no face reads the machine's own
families, and it alone — the `PenMachineFace` suite — carries the
`fonts` ctest label. `draw_bench` times ten thousand circles filled and stroked,
ten thousand rects, a screen of text, a translucent background and a
thousand noise samples per frame; it builds through the `benches` target
and runs through `scripts/sigil.py bench`.

The brush cases are one file per subject: the sampler's spacing across
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
bounds; the stamp's spacing as a fraction of its width, its scatter in
both axes and the angle jitter on top of the heading; the grain standing
still in the pen's space or riding the stamp, and how much of the canvas
its depth may take; the trace that follows any callable direction, the
stock fields as values with the vortex turning clockwise, and the warp
that closes its path; and the engine — selection and state, the pen's units and clock,
one clip over every interior and the outline, a closed shape's outline
on its bent interior, plots placed where they were drawn, live input
across event batches, the first live dab's heading, and cancel.
The brush-format cases build every fixture in memory — a tip drawn
and encoded to PNG, a zip written entry by entry, an `.abr` written
field by field — so no case reads a file this repository ships: a native
directory through a table and through a hub, the packed archive, the
`.brush` giving up its two pictures, the `.abr` giving up both its
sampled tips at either subversion, and bytes that are no brush answering
nothing.

The cases that put pixels down draw on one fixture,
`test/support/Paper.h` — a pen
over a raster surface with the pixels readable back — whose font context
is the tree-wide `src/test/Fonts.h`, so one process shapes against one
memoised context. A case asserts one thing the pen promises and is
named that promise as a sentence; it pins only what editing this library
could falsify, and the one case whose claim depends on the machine's
faces — the `PenMachineFace` suite — carries the `fonts` label.

The brush arms of `draw_bench` measure sampling, a field-traced watercolor mark,
hatching, a curved dry mass, a pigment wash, and one stroke of an
imported brush — the shape stamped per dab, alone and under each of the
two grain spaces.
