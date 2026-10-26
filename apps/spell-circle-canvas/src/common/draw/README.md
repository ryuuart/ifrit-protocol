# SigilDraw — an immediate-mode canvas with p5's brevity

A **pen** draws on a borrowed `SkCanvas` with p5's names, argument
orders and defaults. Its style, random streams and retained guests live
with the pen. The host supplies the canvas, clock, fonts and input.

Include `<sigildraw/Pen.h>` and link `SigilDraw` for `sigil::draw`.
Natural-media tools live in `sigil::draw::brush`: include
`<sigildraw/brush/Brush.h>` and link `SigilDrawBrush`.

| What you need | Use | What persists |
|---|---|---|
| Draw once on a canvas | `draw::on(canvas, size, program)` | Nothing after the call |
| Draw frames in your own host | `Pen::begin` / `Pen::end` | Pen style and guests; the host owns the pixels |
| Accumulate pixels in an offscreen buffer | `Graphics` | Buffer pixels and pen style |
| Build a picture from named pixel layers | `Stack` and `Layer` | Every layer's pixels and settings, and the composite |
| Run a program inside a Compose node | `compose::pen` | Pen style and guests |
| Accumulate pixels inside a Compose node | `compose::graphics` | Buffer pixels and pen style |

The Compose forms belong to `SigilComposeDraw`, through
`<sigilcompose/draw/Draw.h>`. They manage pen frames and borrow the
composers' clock, fonts and input.

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
#include <sigildraw/Pen.h>
#include <sigilsketch/scene2d/Scene2D.h>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
using namespace sigil::draw;

struct Bounce {
  float x = 200, y = 100, vx = 3, vy = 2;

  void setup(sketch::Scene2DContext& ctx) {
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

`compose::graphics` keeps the canvas between frames, so the translucent
background leaves a trail. It honours `noLoop`, `redraw` and `frameRate`.
Use `pen.frameCount == 1` for drawing setup. Canvas size, asset loading
and capture belong to the host. Each pen has its own state.

## The pen alone

Use `begin` and `end` when you own the frame loop. Style survives across
frames; transforms start from the canvas's matrix at `begin`, and `end`
restores that canvas. Pair the calls even when drawing throws. Hosted
Compose programs and `draw::on` close the frame for you.

```cpp
#include <sigildraw/Pen.h>
#include <sigildraw/PenTypes.h>

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
from. Keep the font context alive while the pen uses it for text drawing
or measurement. A pen begun with no fonts draws no text.

For a one-shot drawing, `draw::on(canvas, size, program)` manages a
temporary pen:

```cpp
on(*surface->getCanvas(), {120, 80}, [&](Pen& pen) {
  pen.background(20);
  pen.rect(8, 8, 40, 40);
});
```

Its `millis()` and `deltaTime` are zero and `frameCount` is one. Pass a
font context as the fourth argument when the drawing contains text.

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
| shapes | `point`, `line`, `rect(x, y, w, h[, r \| tl, tr, br, bl])`, `square`, `ellipse(x, y, w[, h])`, `circle(x, y, d)`, `arc(x, y, w, h, start, stop[, OPEN \| CHORD \| PIE])`, `triangle`, `quad`, `bezier`, `curve`, `curveTightness`; `point`, `line` and `circle` also take `SkPoint`s, since a point here is a value and every verb beside them takes one |
| vertices | `beginShape([POINTS \| LINES \| TRIANGLES \| TRIANGLE_FAN \| TRIANGLE_STRIP \| QUADS \| QUAD_STRIP])`, `vertex`, `curveVertex`, `bezierVertex`, `quadraticVertex`, `beginContour`, `endContour`, `endShape([CLOSE])`; `fill` between two `vertex` calls colours the corners either side of it |
| the clip | `clip(shape)`, `clip(shape, {.invert = true})` |
| text | `text(str, x, y[, w, h])`, `text(number, x, y)`, `textSize`, `textFont(family[, size])`, `textAlign(LEFT \| CENTER \| RIGHT[, TOP \| CENTER \| BOTTOM \| BASELINE])`, `textLeading`, `textStyle(NORMAL \| BOLD \| ITALIC \| BOLDITALIC)`, `textWidth`, `textAscent`, `textDescent` |
| images | `image(img, x, y[, w, h])` and the nine-argument source-rect form, over an `sk_sp<SkImage>` or a `Graphics`; `image(frame, x, y, w, h, fit)` over a `media::Frame` — any picture source's frame at a time, an `Image`'s, a `Video`'s, a publication's — meeting its box under a `material::Fit`, a frame standing on a device bound for the pen's own canvas |
| transform | `translate`, `rotate`, `scale(s \| sx, sy)`, `shearX`, `shearY`, `push`, `pop`, `resetMatrix`, `applyMatrix(a, b, c, d, e, f)` |
| numbers | `random()`, `random(max)`, `random(min, max)`, `randomGaussian`, `randomSeed`, `noise(x[, y, z])`, `noiseSeed`, `noiseDetail` |
| the loop | `frameCount`, `deltaTime` (milliseconds), `millis()`, `frameRate()`, `frameRate(framesPerSecond)`, `noLoop`, `loop`, `redraw` |
| constants | `PI`, `TWO_PI`, `TAU`, `HALF_PI`, `QUARTER_PI`, and every word above, in `sigil::draw` |

These details affect a ported sketch:

- `arc` runs clockwise in the current angle mode. It fills a pie unless
  `CHORD` is selected; its stroke follows the arc under `OPEN`, closes
  across the chord under `CHORD`, and closes through the centre under
  `PIE`. Ellipse angles describe geometric directions. `bezier` and
  `curve` can fill as well as stroke.
- `background` covers the whole canvas regardless of the transform.
  Its alpha blends with the existing picture, which makes trails. A
  translucent solid under `BLEND` moves every channel at least one 8-bit
  step toward its colour, so a trail laid every frame reaches the
  ground and leaves no ghost.
  `noSmooth` disables both antialiasing and image smoothing; `smooth`
  restores them.
- Changing `fill` between vertices interpolates corner colours for
  triangle and quad shapes. Their stroke still follows the outline.
  A shape with one fill uses a path. `point` is a disc of the stroke
  colour and weight.
- A `LINES` shape is one draw for the whole run, and the same picture as
  a `line` per pair: overlaps meet as separate lines do. A dash, or a
  stroke fitted to each segment's box, keeps a draw per pair. A `POINTS`
  shape resolves the stroke once and draws a disc per point.
- Text starts black, gains the selected or inherited fill, and gains a
  stroke only when one is selected. `textSize` sets leading to five
  quarters of the size until `textLeading` overrides it.
- `push` saves style and transform; `pop` restores both. `end` closes
  unmatched pushes. Blend mode belongs to the style and reaches every
  drawing verb. `SUBTRACT` subtracts source colour while preserving
  destination alpha.
- `clip` records the callback's shape outlines in their current spaces
  and clips subsequent drawing until the matching `pop` or `end`.
  Lines, images, text and backgrounds add no outline to the mask. A
  throwing callback installs no clip and leaves the pen able to draw;
  any style or transform changes made by the callback still apply.

The pure calculations — `map`, `lerp`, `constrain`, `dist`, `mag`,
`norm`, `sq`, `radians`, `degrees` — are free functions in
`<sigildraw/Math.h>`: they read no pen, so they take no `pen.`. `sin`,
`cos`, `floor`, `round`, `min`, `max`, `abs` and `sqrt` are the standard
library's.

## Where ours differs

The pen adds material, geometry and text overloads to the p5 verbs.

* **Materials.** `fill(material::Paint)`, `stroke(material::Paint)` and
  `background(material::Paint)` accept gradients, images, effects and
  blends. `fill(material::Material)` and
  `background(material::Material)` accept recipe instances. Static
  paints resolve when set; live paints and paints that read their box
  resolve for each draw against the pen's clock and canvas. Coordinates
  follow the pen's current transform. A world-space material spans the
  frame's root (`Frame::rootWidth`, `rootHeight`, `toRoot`; the pen's own
  canvas where the frame names none) and resolves again at each draw, so
  it stays anchored there through the pen's transforms.
* **Material space.** `CANVAS`, the default, measures paint against the
  frame. `fill(paint, SHAPE)` and `stroke(paint, SHAPE)` measure against
  each shape's bounds: the top-left is the origin and the bounds are the
  unit square; a `material::Material` takes the same word. This applies to shape fills and strokes, including lines
  and points. Text, images and backgrounds use the canvas, as do shapes
  with a zero width or height. A later fill or stroke without `SHAPE`
  restores canvas space. `push` and `pop` save this choice.
* **Meshes.** `vertices(sk_sp<SkVertices>)` draws a mesh with the pen's
  fill, blend, clip and transform. Corner colours supply a plain colour
  fill; a material fill paints the whole mesh. `SHAPE` uses the mesh's
  bounds. Meshes have no stroked outline and add nothing to a clip mask.
* **Dashes.** `strokeDash({on, off, ...}[, phase])` and `noDash()` apply
  to every stroke except `point`. An odd run repeats itself, so `{6}`
  means six drawn and six skipped. Lengths and phase use the pen's units
  along the path and follow its scale.
* **Silhouettes.** `shape(silhouette, x, y, w, h)` fits a
  `geometry::shapes::Silhouette` to the box read by `rectMode`. The
  comparable value supplies `outline(glm::vec2)`, as the geometry kit's
  `star`, `polygon`, `squircle`, `blob` and `annulus` do.
  `shape(SkPath)` draws a path directly. Primitive shapes use Skia's
  native drawing operations.
* **Shaped text.** SigilWeave provides kerning, itemisation, fallback
  faces and boxed paragraph layout. `textFont` accepts a family name,
  an `sk_sp<SkTypeface>` or a `weave::Type` with size, tracking,
  condensation and variable axes. The pen's fill supplies glyph colour.
  Boxed vertical alignment distributes the space left by the passage;
  unboxed alignment places the block against the given point.
* **Noise and random values.** `NoiseField` provides p5's octave shape
  and values in [0, 1), using core's mixer for lattice corners. Its
  output differs from p5's permutation table. Every pen starts on the
  same random seed; `randomSeed` selects another reproducible stream.
* **Host clock.** `millis`, `deltaTime` and `frameRate()` read the supplied
  frame. `frameRate(framesPerSecond)` requests a draw rate from the host, which owns
  scheduling.
* **Small compatibility differences.** Colour strings accept hex in its
  four lengths and the supported named colours; unknown strings become
  black. `beginShape()` defaults to `POLYGON`.
* **Direct canvas access.** `pen.canvas()` supplies the current
  `SkCanvas`, carrying the pen's transform and clip, during an open frame.
  Restore both after direct drawing. `pen.contentScale()` reports device
  pixels per canvas unit. `pen.fillPaint()` and `pen.strokePaint()` carry
  blend, antialiasing and dash settings, but return null under `noFill`,
  `noStroke` or a zero stroke weight. Check before dereferencing them.
* **Inherited style.** A host calls `inherit(ink, font)` after `begin` to
  seed fill, stroke and text type until the program selects its own.
  `noFill` remains effective, and selected styles persist across frames.
  `inheritedInk()` and `inheritedFont()` expose the host's pair for guests;
  their defaults are black and `weave::initialType()`. A pen with no
  inherited style keeps a white fill, black stroke and twelve-pixel text.
  What a guest's own library inherits beside the pair — Compose's custom
  properties, paragraph setting, image sampling and sheets in force, one
  value of each type — rides with `inheritForGuests(value)` for one frame, read
  back by type through `guestInheritance<T>()`; the pen never reads it and
  `end` drops it. `inheritFrom(host)` hands a pen opened inside another's
  frame all of the host's inheritance.
* **Offscreen pixels.** Keep `Graphics buffer{w, h}` across frames.
  `buffer.begin(pen)` borrows the host's clock and fonts and returns its
  own pen; pair it with `buffer.end()`. `pen.image(buffer, x, y)` places
  it in canvas units, and `buffer.image()` exposes the `SkImage`.
  Density is the greater of the host's density and
  `buffer.setDensityFloor(px)`. Style and pixels persist. `resize(w, h)`
  or a density change scales the previous picture into the replacement
  surface. The buffer forms where its host draws — on that host's device
  when it has one, on raster when its canvas makes no surface — and
  follows it: begun by a host on a device that cannot reach it (a buffer
  on raster, on another device's context, or on a recorder of another
  thread), it is formed again there, its picture kept where that device
  can read it; `buffer.standsWhere(host)` says whether it would be, and
  `buffer.follow(host)` re-forms it without opening a frame. A host on no
  device leaves it where it stands. Dimensions clamp to at least one
  canvas unit and must be finite, as must the density floor. Invalid inputs throw
  `std::invalid_argument` without changing them. `begin` throws
  `std::length_error` for an unrepresentable pixel extent or
  `std::bad_alloc` for allocation failure; the previous image remains
  available in either case.

## Layers and the stack

A `Stack` is a picture made of named pixel layers at one canvas size.
Each `Layer` is an offscreen buffer drawn with its own pen, as a
`Graphics` is, plus the settings that say how it meets the layers
beneath it. The stack composites them bottom to top into a buffer of its
own and puts that down on any pen.

```cpp
#include <sigildraw/Stack.h>

using namespace sigil::draw;

Stack stack{1200, 800};               // kept across frames

void draw(Pen& pen) {
  if (pen.frameCount == 1) {
    Layer& rings = stack.layer("rings");
    Pen& ink = rings.begin(pen);
    ink.noFill();
    ink.strokeWeight(10);
    for (float radius : {120.0f, 180.0f, 240.0f})
      ink.circle(600, 400, 2 * radius);
    rings.end();

    Layer& cut = stack.layer("cut");
    cut.begin(pen).circle(600, 400, 180);
    cut.end();
    cut.blendMode(REMOVE);            // deletes what lies beneath it
  }
  stack.layer("rings").transform({.origin = {600, 400},
                                  .scale = {0.85f, 1.0f},
                                  .rotation = pen.frameCount * 0.01f});
  stack.composite(pen);
}
```

The p5 words a stack replaces:

| p5 | here |
| --- | --- |
| `createGraphics(w, h)` kept and drawn in | `stack.layer(name)`, then `layer.begin(pen)` and `layer.end()` |
| `erase()` … `noErase()` | a layer blended `REMOVE`: it deletes the pixels beneath it wherever it has ink |
| `image(buffer, x, y)` for each buffer, in order | `stack.composite(pen)` |
| `tint(255, alpha)` on a buffer | `layer.opacity(amount)` |

What each layer carries:

| setting | verbs | meaning |
| --- | --- | --- |
| blend | `blendMode(mode)` | the pen's blend words, `BLEND` by default |
| opacity | `opacity(amount)` | from 0 to 1, clamped |
| placement | `transform(LayerTransform)`, `resetMatrix()` | an affine about `origin`: `translation`, `scale` along x and y, `shear` and `rotation`, angles in radians, built in the order the pen's verbs build it |
| mask | `mask(layer[, MaskChannel::Luminance])`, `noMask()` | shows only where another layer of the same stack has ink, read by alpha or by luma |
| clip | `clip(shape[, options])`, `noClip()` | shows only where `shape`, run with a pen in stack units, covers |
| visibility | `show()`, `hide()` | a hidden layer composites nothing and still serves as a mask |

These details affect a picture built from layers:

- `stack.layer(name)` answers the existing layer of that name, or makes
  it on top. Asking again edits the layer rather than piling up another.
  `reorder(name, position)` counts from the bottom, which is zero;
  `remove(name)` removes the layer and every mask it served; `names()`
  lists the layers bottom to top. References to a removed layer dangle.
- A mask reads the masking layer's pixels where its own placement puts
  them, whatever its blend, opacity or visibility. The mask and the clip
  are in the stack's canvas units, so they stay where they are while the
  masked layer moves. A mask and a clip together keep what falls in both.
- `layer.bake(pen, element, size)` clears the layer and paints a retained
  guest, such as a Compose element, into it through `Pen::element` at the
  layer's density, with its corner at the layer's origin. The guest is
  kept between bakes, so a tree baked again keeps its layout and caches.
  Include `<sigilcompose/draw/Draw.h>` for a Compose guest, as for
  `pen.element`.
- Every layer counts its changes in `revision()`: each `begin`, `end`,
  `bake`, and each setting given a new value. The composite is re-made
  only when a layer's revision moved, a layer was added, removed or
  reordered, the stack was resized, or the host's density changed; a
  still stack costs one image drawn. `stack.revision()` counts the
  re-makes. A clip shape is read only at a re-make, so a shape reading
  changing state is set again when that state changes.
- `stack.composite(pen[, x, y])` puts the composite down at the stack's
  canvas size under the pen's own blend, transform, clip and image mode.
  `stack.image(pen)` answers it as an image, re-made first when anything
  moved; `stack.image()` is the composite as last made.
- Every layer and the composite are formed at the host's density or
  `stack.setDensityFloor(px)`, whichever is greater. `resize(w, h)`
  resizes every layer, keeping its pixels as `Graphics` does.
- The composite is a function of the layers' pixels and settings, so a
  sketch drawing a stack is as deterministic as its layers are.

### Drawing with no frame open

A layer, like any buffer, is formed on the device of the host that first
begins it and stays there. Where no frame's pen is at hand — a stack
drawn at a prompt, a layer baked before any window opens — a
`HostlessPen` from `<sigildraw/Hostless.h>` stands in as the host: a pen
begun with no frame, at a stated density, with a font context, whose
buffers are formed on the process's host-less device.

```cpp
#include <sigildraw/Hostless.h>

HostlessPen host(2.0f, stack.width(), stack.height(), &fonts);
Pen& ink = stack.layer("rings").begin(host.pen());
ink.circle(600, 400, 240);
stack.layer("rings").end();
const sk_sp<SkImage> composite = stack.image(host.pen());
```

This library owns no device, so which device that is is a seam:
`HostlessDevice` makes a surface on it, reads one of its images back to
the CPU and says whether a canvas records onto it, and
`useHostlessDevice` installs one for the process. A host that holds a
device installs an executor standing on it, so a layer drawn with no
frame open is formed on the device the frames it is put down in are
drawn on; with none installed, the host-less pen draws on raster, which
is the path for a machine with no device. Two crossings keep a device
picture where it stands until something needs it elsewhere:

- `HostlessPen::drawableOn(pen, image)` answers the image itself where
  the pen records onto the device it stands on, so a stack put down in a
  frame on that device is drawn with no copy, and a copy read back to the
  CPU only where the pen draws on raster or on another device.
- `HostlessPen::rasterImage(image)` reads a device image back to the CPU,
  for a picture that is encoded, compared or handed to code that reads
  pixels.

## The brush library

Natural media — device input, dabs, tools, fields and the interiors a
polygon receives — is `SigilDrawBrush`, a target of its own over the pen.
[brush/README.md](brush/README.md) owns the tools, dynamics, surfaces and
brush formats. Link `SigilDrawBrushFormat` when importing or exporting a
brush.

Draw's brush feature paints strokes and pigment with a tool. Compose's
brush feature decorates an element's outline. Their similar words refer
to different values: Draw's `Hatch` places tool marks, `Wash` deposits
pigment and `Shape` supplies stamp artwork; Compose uses those words for
a stroked hatch, a material flood and a node's silhouette.

## Not provided

p5 verbs and variables a pasted sketch has to replace, stated so nobody
searches for them: `createVector` and `p5.Vector` (glm has the
vectors), `tint` (a layer's opacity fades a whole buffer), `filter`,
`erase`/`noErase` (a layer blended `REMOVE` erases), `beginClip`/`endClip`
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
a `sketch::Scene3DSketch`.

## Retained guests

A pen can host a retained value, such as a Compose tree with layout,
shaped text, bindings and caches:

```cpp
pen.element(card, 40, 40, 320, 180);  // one card
for (int i = 0; i < 3; ++i)
  pen.element(row(i), 40, 260 + i * 60, 320, 50, i);
```

A `Slot` identifies a guest by file, line, column and an integer index.
`Retained` keeps its state for the next call to the same slot. Use a
stable index for each logical item in a loop; use a position only when
state should follow that position. Generating new IDs every frame
accumulates guests. Skipping a slot preserves its state; the store has
no automatic retirement. Call `pen.retained().erase(slot)` when a guest
is permanently removed. It releases that entry, returns whether one was
removed, and preserves all other entries. Drawing the
same slot later creates a fresh guest. References returned by `get` for
an erased entry must no longer be used. `pen.retained().clear()` releases
every guest; use it when all retained state should restart.

A helper wrapping `pen.element` must forward the caller's location, or
all calls through the helper share its internal call site:

```cpp
#include <source_location>

void cardAt(sigil::draw::Pen& pen, const sigil::compose::Element& card,
            float x, float y, int index = 0,
            std::source_location where = std::source_location::current()) {
  pen.element(card, x, y, 320, 180, index, where);
}
```

Save a location with the owner's state and pass it on every draw that
should use that slot:

```cpp
const auto cardSite = std::source_location::current();
cardAt(pen, card, 40, 40, cardId, cardSite);
```

When that card is permanently removed, retire exactly the same identity:

```cpp
pen.retained().erase(sigil::draw::Slot::at(cardSite, cardId));
```

The guest's library declares
`paintRetained(Pen&, const Guest&, const geometry::path::Rect&, Slot)`
in its own namespace; argument lookup selects it. SigilCompose provides
the implementation for `Element` in `SigilComposeDraw`. Include
`<sigilcompose/draw/Draw.h>` and link that target when using Compose
guests. Draw itself depends on no guest library.

A Compose guest advances by the pen's frame delta only when painted. It
starts with `inheritedInk()` and `inheritedFont()`, which are black and
the initial type when the host supplies neither, and under the custom
properties, the paragraph setting, the image sampling and the sheets in force
a Compose host carried through `inheritForGuests`. Captures in a retained
callback must remain valid for as long as that callback can run.

## Headers and implementation

| header under `include/sigildraw/` | purpose |
| --- | --- |
| `Pen.h` | drawing verbs and frame lifecycle |
| `PenTypes.h` | `Frame`, `ClipOptions` and the `Retainable` concept |
| `Constants.h` | p5 mode words and angles |
| `Graphics.h` | an offscreen surface with its own pen |
| `Stack.h` | `Stack`, `Layer`, `LayerTransform` and `MaskChannel`: pixel layers composited bottom to top |
| `Hostless.h` | `HostlessPen` and the `HostlessDevice` seam: a pen begun with no frame, and the device it draws on |
| `Retained.h` | `Slot` identity and guest storage |
| `Color.h` | `ColorMode`, `colorFrom` and `parseColor` |
| `Noise.h` | `NoiseField` |
| `Math.h` | calculations that read no pen |

The implementation separates frame/style state (`Pen.cpp`), shapes
(`PenShapes.cpp`), clipping (`PenClip.cpp`), images (`PenImage.cpp`),
transforms (`PenTransform.cpp`), shaped text (`Text.cpp`), the layer
stack (`Stack.cpp`) and the host-less pen (`Hostless.cpp`). Brush
implementations, tests and benchmarks live under `brush/`; importers and
their tests live under `brush/format/`.

## Boundaries

* **Import the origin.** Fills use `material::Paint`, text uses
  `weave::Paragraph`, and random/noise calculations use `core::noise`.
  Include those libraries' headers when using their values.
* **The host owns execution.** Draw receives a canvas and frame. Sketch
  runtimes and Compose adapters sit above it; `paintRetained` connects a
  guest without making Draw depend on its library.
* **Brush features are optional.** `SigilDrawBrush` consumes the pen,
  SigilGeometryPath and SigilSkia's sprite batching. It owns device/tool
  behaviour: pressure, tilt, speed, grain and pigment. Consumers with
  their own tools can link `SigilDraw` alone.
* **Formats consume bytes.** `SigilDrawBrushFormat` uses SigilIOSource
  and hands artwork to SigilMedia. SigilIO owns access, caching and
  reload; the brush format feature opens no files. Its doors that can
  refuse — `brush::format::decodeBrush`, `brush::format::assembleBrush`,
  `brush::format::loadBrush` and the hub's `brush::format::BrushDecoder` —
  answer `std::expected<brush::Tool, core::Error>`, the error saying which
  form the bytes looked like and why it did not read.
* **Public storage exposes Boost.** `Retained.h`, `brush/Engine.h` and
  `brush/Catalogue.h` use Boost tables or hash folding. Their originating
  targets propagate the header-only Boost dependencies to consumers.

## Build and test

The feature targets are `SigilDraw`, `SigilDrawBrush` and
`SigilDrawBrushFormat`. One `draw_test` collects their cases and one
`draw_bench` collects their benchmarks. Tests live beside the feature
they exercise. Pixel cases use `test/support/Paper.h`, a pen over a
readable raster surface; text cases use the instrument face and assert
layout relations. `PenMachineFace` uses installed fonts and carries the
`fonts` label.

Build `draw_test` and `draw_bench` from the app's build directory. Ctest
discovers each case, so `ctest -R '^Pen\.'` selects the pen suite.
[The testing guide](../../../docs/overview/testing.md) describes the
shared commands, labels and benchmark workflow.
