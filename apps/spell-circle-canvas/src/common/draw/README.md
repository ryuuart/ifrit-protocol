# SigilDraw — an immediate-mode canvas with p5's brevity

A **pen** over an `SkCanvas`, carrying p5's verbs with p5's names,
argument orders and defaults, so a sketch written for p5 pastes in and
runs. Compose is the declarative way to draw here; this is the
imperative way beside it, and the two open onto each other.

Namespace `sigil::draw`, one static target `SigilDraw`, every public
header under `include/sigildraw/` with `<sigildraw/Draw.h>` as the
umbrella.

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
is closed there, back to the style that stood when it opened. `clip` runs the function it is given with the shape verbs
RECORDED rather than drawn — each in the space it was called in, so a
transform inside the function moves the mask with it — and confines
everything drawn after to what those shapes covered, until the matching
`pop()` or the end of the frame; the verbs that carry no outline, a
`line`, an `image`, a `text`, a `background`, add nothing to a mask. The
blend mode is style like any other, so `push` and `pop` carry it, and it
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
  glyphs whatever the type's own colour says.
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
  leave the transform and the clip as they were found.
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
  Pen.cpp         the frame, the style, the shapes, the transform, the streams
  Graphics.cpp    the offscreen buffer
  Text.cpp        text through SigilWeave
  Color.cpp       the colour models and the CSS string
  Noise.cpp       the layered field
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
* **Never reads the wall.** Every number a pen answers about time comes
  from the frame it was given.

## Build and test

From `apps/spell-circle-canvas`:

```sh
cmake --build build --config Release --target draw_test draw_bench
ctest --test-dir build -C Release -R draw_test --output-on-failure
./build/bin/Release/draw_bench
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
units.
`draw_bench` times ten thousand circles
filled and stroked, ten thousand rects, a screen of text, a translucent
background and a thousand noise samples per frame; it builds through the
`benches` target and runs through `scripts/bench_ledger.py`.
