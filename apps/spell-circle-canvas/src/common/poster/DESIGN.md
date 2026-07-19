# IfritPoster — design proposal

Layered, styled, z-ordered typographic layouts ("poster" compositions)
on the Skia canvases, animated and data-refreshed at runtime. Status:
**design + feasibility spike** — the API below is a proposal for
discussion; only the spike test is implemented.

## Problem

We can lay out paragraphs beautifully (TextFlow: Knuth–Plass, shaped
flows, path text, per-glyph choreography) and we can lay out documents
completely (IfritWeb: full CSS through Ultralight). What's missing is
the middle: a *box* layout system for composing TextFlow-quality
typography into layered posters —

- boxes that size themselves to measured text and to each other
  (flexbox-style), with baseline alignment across mixed type sizes;
- painting with explicit stacking (z-index), opacity groups, blend
  modes, transforms;
- animation of both layout and paint properties, plus TextFlow's
  existing per-glyph choreography;
- cheap incremental relayout so content can refresh per frame
  (scores, clocks, feeds) without rebuilding the world.

IfritWeb can do all of this in CSS, but through WebCore's text stack
(not TextFlow), at Ultralight's 60 FPS repaint cap, one thread hop away.
The poster path exists for when the typography is the product.

## Why Yoga

[Yoga](https://github.com/facebook/yoga) (vcpkg `yoga` 3.2.1, MIT, C++
core) is the flexbox engine under React Native/Litho. What makes it the
right foundation here:

- **Measure callbacks are the text contract.** A leaf node can defer
  sizing to a callback given (availableWidth, widthMode) — exactly how
  React Native sizes native text. Our callback runs
  `textflow::layoutParagraph` with a `BlockFlow` at the constraint
  width and returns the measured extent. Validated by the spike test.
- **Baseline callbacks.** Yoga's `align-items: baseline` asks leaves
  for a baseline offset; TextFlow's `LineMetrics::baseline` provides it
  — mixed-size type on one line aligns typographically, which is the
  poster use-case in one feature. Also spike-validated.
- **Incremental.** Dirty-marking relayout is microseconds-scale; layout
  properties are animatable per frame.
- **Boring and proven.** C ABI, tiny, no runtime, a decade of React
  Native production behind its quirks.

What Yoga deliberately does **not** do — paint, z-index, stacking,
styling, animation — is the API we own. That's the design below.

Considered alternatives: **Taffy** (has CSS Grid, but Rust FFI friction),
**Clay** (young), **reusing Ultralight** (see above). Yoga's missing
grid is mitigated by nested flex + absolute positioning; revisit if
grid becomes essential.

## Architecture — three layers

Split borrowed from Flutter's widget/render separation and the web's
layout/compositor split.

### 1. Node tree (retained)

`PosterNode` = Yoga node handle + `Style` (paint properties) + content:

- **Box** — pure container/decoration;
- **Text** — a `textflow::Paragraph` + `ParagraphLayoutOptions`;
- **Image** — an `ifrit::image::ImageAsset` frame;
- **Web** — an `ifrit::web::WebView` frame (posters can embed live web
  content; conversely a poster can render into a `WebImage` slot);
- **Custom** — a paint callback `void(SkCanvas&, SkSize)`.

Layout properties (flex direction/grow/shrink/basis, padding, margin,
gap, min/max, absolute + insets, percentages) forward to Yoga verbatim.
Paint properties are ours: fill (color/gradient/shader — including
`textflow::PaintShaders`), border + radius, shadow, opacity, blend
mode, transform (paint-only, see below), clip, zIndex.

Nodes carry stable string keys so data-driven rebuilds preserve
identity (React's reconciliation-keys lesson) — running animations and
per-glyph state survive a content refresh.

### 2. Layout pass

`YGNodeCalculateLayout` over dirty roots. Text measurement is cached
keyed on (content revision, width, widthMode) — Yoga may probe a leaf
several times per pass (the React Native text-cache lesson); the final
layout at the resolved width is kept for painting, so measure and paint
never disagree.

### 3. Paint pass — stacking model

CSS's model, simplified and explicit (no surprises, no spec trivia):

- Within a parent, children paint in `(zIndex, declaration order)`,
  stable-sorted. Default zIndex 0 keeps document order.
- A node establishes a **stacking context** when it sets zIndex,
  opacity < 1, a blend mode, a transform, or clips — children cannot
  interleave with the outside (CSS semantics, which keeps `saveLayer`
  bounds well-defined and its cost opt-in).
- **Transforms are paint-only** — they never affect layout (the web's
  compositor-property lesson: transform/opacity are the cheap-to-animate
  properties precisely because they skip relayout). Anchor via
  `transformOrigin`.
- Output is ordinary `SkCanvas` drawing: a poster composes onto the
  scene canvases next to `SceneRenderer` content, raster or Graphite.

## Animation model

Two property classes, priced differently (and documented as such):

- **Paint properties** (opacity, transform, colors, shadows): tweening
  touches no layout — arbitrarily many at 120 FPS.
- **Layout properties** (insets, basis, gaps, sizes): tweening dirties
  Yoga — still cheap, but text leaves under a changing width re-measure;
  prefer transform animation for movement, layout animation for actual
  reflow moments.

Driving them: **superseded by COMPONENTS.md** — instead of a bespoke
keyframe timeline, animation is sansumbrella Choreograph (packaged in
the sigil-vcpkg-registry) stepped by IfritTick (`src/common/tick`),
whose Ticker reports "needs more frames" for event-driven redraw.
Implicit SwiftUI-style transitions on prop change are part of the
component design.

**Per-glyph choreography composes.** Text leaves expose their
`ParagraphLayout`, so `textflow::Choreograph`'s `forEachPlacedGlyph`
(enumeration stable across relayouts — glyph particle state survives
data refreshes by design) drives letter-level effects inside a
box-level animated poster. Box motion via the poster timeline, glyph
motion via choreography, one canvas.

**Data refresh:** `node.setText(...)`, `.setImage(...)` mark the leaf
dirty; the next tick re-measures just that subtree. Identity keys keep
everything else undisturbed.

## API sketch (proposal)

```cpp
using namespace ifrit::poster;

Poster poster({1080, 1350});
auto &root = poster.root().column().padding(64).gap(24)
                 .background(Fill::linearGradient({...}));

auto &headline = root.text(u8"IFRIT\nPROTOCOL", display96)
                     .zIndex(2)
                     .transformOrigin({0.f, 1.f});

auto &band = root.box().row().gap(12).alignBaseline().zIndex(1);
band.text(u8"vol. 4", mono14);
band.text(u8"summer program", serifItalic21);

root.image(coverAsset).absolute().inset(0).zIndex(0).opacity(0.35f);

Timeline &tl = poster.timeline();
tl.tween(headline, Prop::TranslateY, 60.f, 0.f, 700ms, Ease::OutExpo);
tl.tween(headline, Prop::Opacity, 0.f, 1.f, 500ms);

// per frame, event-driven:
bool needsMore = poster.tick(nowMs);
poster.draw(canvas);   // stacking-order paint onto any SkCanvas
```

Open API questions to settle together:

1. Builder-with-handles (above) vs. a declarative rebuild-each-frame
   description that reconciles against the retained tree (more
   React-like; friendlier to a future TouchDesigner/Python authoring
   path, at the cost of a diffing layer).
2. Whether `Style` should be a value type snapshotted per frame
   (trivially animatable/serializable) or live mutable state on nodes.
3. Timeline ownership: per poster (above) vs. a shared clock across
   posters for synchronized multi-canvas choreography.
4. Serialization: a FlatBuffers schema mirroring the scene pipeline
   would let Python/TouchDesigner author posters over UDP like scenes —
   large win, deliberately out of scope for v1.

## Phasing

1. **Spike (this directory, done):** Yoga dependency + proof that
   measure/baseline callbacks drive TextFlow correctly.
2. **Core:** node tree, styles, layout, stacking paint; golden-image
   tests like the textflow demo panels.
3. **Animation:** Timeline + Choreograph bridge.
4. **Leaves:** image/web/custom content, SceneRenderer embedding.
5. **Authoring:** serialization schema, Python bindings.

## Spike findings

`test/PosterSpikeTest.cpp` (passing) validated: measure callbacks size
leaves from `layoutParagraph` + `BlockFlow`; the width constraint
genuinely reaches TextFlow's line breaker (narrow boxes wrap); Yoga's
`align-items: baseline` aligns mixed type sizes via
`LineMetrics::baseline`. One API lesson: flexbox's `align-items`
default is *stretch*, which overrides measured text height on the cross
axis — the poster API should default text leaves to start-aligned so
"box fits the type" is the out-of-box behavior.

## Risks

- Yoga probes measure functions repeatedly and with loose modes —
  cache discipline is the known sharp edge (mitigated as above).
- Animating text-affecting layout properties re-measures per frame;
  acceptable for headlines, wrong for body copy — the two-price
  property model makes the cost visible in the API.
- `saveLayer` per opacity/blend group on Graphite needs measuring once
  real posters exist; stacking contexts keep it opt-in.
