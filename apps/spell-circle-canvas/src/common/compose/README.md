# SigilCompose

A C++20 static library that turns immutable, value-typed descriptions of a
2D scene into pixels on an `SkCanvas` the caller owns. It runs flexbox
layout through Yoga — with text leaves measured and drawn by SigilWeave,
the sibling paragraph-layout library — diffs each new description against
a retained tree to find what actually
changed, paints in an explicit CSS-like stacking order, and automatically
caches subtrees it can prove are not changing. Animation is Choreograph
outputs and timelines, stepped by a clock the host owns.

It owns no window, no surface, no render loop and no thread. You call
`Composer::render()` when your data changes and `Composer::draw()` inside
whatever paint callback your application already has, and it honours the
canvas's current matrix and clip like any other draw.

The problem it exists for is the middle ground between a paragraph layout
engine and a whole document engine: box-level composition of real
typography and arbitrary Skia drawing, sized by flexbox rules with
baseline alignment, layered with explicit z-order and blending, cached
like a display list, animated at scene rate, and refreshed from data
without rebuilding the world.

---

## Writing a component

A component is a free function from your data to an `Element`. There is no
base class, no lifecycle, and no state inside the library — the state is
the argument.

```cpp
#include <sigilcompose/Compose.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/Util.h>

#include <ranges>
#include <vector>

using namespace sigil::compose;
using namespace std::chrono_literals;

/// Your data. Copyable and equality-comparable — that is the whole contract.
struct Channel {
  std::string id;
  std::u8string label;
  float level = 0;     // 0..1
  bool alarm = false;
  bool operator==(const Channel &) const = default;
};

Element meter(const Channel &c) {
  const SkColor4f ink =
      c.alarm ? studio::hex(0xff5252) : studio::hex(0x8fd0ff);
  return box()
      .row()
      .gap(10)
      .padding(12)
      .corners({6})
      .fill(studio::hex(0x0e1218))
      .alignItems(Align::Center)
      // A mark on part of the boundary: L-brackets at every tangent break.
      .stroke(spans::corners(12), util::stroke(1.5f, Fill::color(ink)))
      .child(text(c.label, studio::type({.size = 13, .color = ink})))
      .child(box()
                 .grow()
                 .height(6)
                 .fill(ink)
                 .transformOrigin(0.0f, 0.5f)
                 // The bar ramps because the DESCRIBED value moved. Nobody
                 // steps it; the reconciler sees the change and starts a
                 // motion that retargets from wherever the bar is now.
                 .scaleX(animate(to(c.level), Transition{.duration = 220ms})));
}

Element dashboard(const std::vector<Channel> &channels) {
  return box()
      .column()
      .gap(8)
      .padding(24)
      .fill(studio::hex(0x05070a))
      .children(channels | std::views::transform([](const Channel &c) {
                  // memo() skips the describe call entirely while the props
                  // compare equal. key() is what the reconciler matches on
                  // across describes, so rows survive reordering.
                  return memo(c, meter).key(c.id);
                }));
}
```

The host side is three objects, and `util::Stage` bundles them:

```cpp
#include <sigilcompose/Util.h>

sigil::weave::FontContext fonts = /* yours */;
util::Stage stage({960, 540}, fonts);

stage.render(dashboard(model));           // whenever the data changes

// …inside your own paint callback:
bool wantAnotherFrame = stage.frame(canvas);   // tick, draw, needs-more
```

Spelled out, which is what to do when the clock or the ticker is shared
with something else:

```cpp
motion::FrameClock clock;
motion::Ticker ticker;
Composer composer(ticker, fonts);       // both must outlive the composer
composer.setClock(&clock);
composer.setSize({960, 540});
composer.render(dashboard(model));

const double dt = clock.tick();
const bool moving = ticker.tick(dt);
composer.draw(canvas);
const bool again = moving || composer.dirty() || ticker.active();
```

The other write path is a live binding — a `choreograph::Output` the host
mutates every frame, read straight out of paint with no `render()` call:

```cpp
choreograph::Output<float> spin{0.0f};
ticker.add([&](double) {
  spin = studio::phase(ticker.elapsed(), 6.0);   // a wrapping [0,1) phase
  return true;
});

box().rotate(bind(&spin).target(0, 360));
```

---

## The mental model

**An `Element` is a shared, copy-on-write description.** It is a value: you
build one with fluent setters, copy it, compare it, throw it away. Copying
an `Element` bumps a refcount; mutating one that is shared clones first, so
a description already handed to the composer can never be altered behind
its back. Hot fields live inline on the node; rare and kind-specific state
is pushed into out-of-line value-semantic blocks, so an absent feature
costs one null pointer.

**An `Instance` is the retained counterpart**, and you never see it: parent
and child pointers, the resolved description, a Yoga node, paint order,
text layout state, animated value slots, derived geometry, and every cache
slot. Elements are write-only. Reads target the composer, after layout —
`Composer::bounds`, `Composer::paragraphLayout`, `Composer::hitTest`,
`Composer::routesAt`, `Composer::stats`, `Composer::profile`. Querying a
description is not offered, because it would invent a second identity
system next to keys.

### The two write paths

There are exactly two ways to change what is on screen, and both are
*declared*:

1. **Describe** — `Composer::render()` and `Composer::renderSlot()`. This
   carries structure and discrete state. Children are reconciled by key,
   falling back to position among unkeyed siblings; keyed reconciliation
   *is* the child-swap API. There is no imperative node mutation, and that
   absence is deliberate: it is the door that would make every cache
   unsound.
2. **Bind** — store a pointer to a live `choreograph::Output` in the
   description and mutate it per frame. Bound properties are paint-only by
   contract. They never relayout, and the node's cached content replays
   under the new transform or the new value.

That split is the whole reason caching can be automatic. Volatility is
*derived from the declarations*, not sniffed at runtime, so "does this
subtree change" is a decidable property rather than a heuristic.

### Phase order

`render()` mounts or patches, as a recursive keyed reconcile. A memo
compares its captured environment snapshot and then the author's props
comparator; a hit reuses the previous payload without describing at all.
A structural equality check is the prune: equal means nothing is marked
dirty and no transition is applied, though children still reconcile.
Unequal means dirty marking up the tree, a Yoga style write, a text
content revision bump, and transition application. Paint order among
siblings is a stable sort by `zIndex` then declaration order. Then the key,
slot and edge indices rebuild.

`draw()` detects the backend and host scale, then runs layout: Yoga first,
then up to three convergence rounds of custom `layout()` schemes,
`centerAt` pins, and the derive phase, each of which may re-run Yoga.
Recordings whose baked geometry moved are invalidated. Derive resolves text
exclusions and connector/rail routing over flat edge lists, cycle-guarded.
`Element::flowAround` subtracts the target's SILHOUETTE when it declares one
— a `shape()`, a routed connector or rail — so text runs into a star's
notches and through an annulus, and its BOX when it declares none. A round
silhouette is subtracted analytically. The margin means the same standoff in
every case.
Released scalars are scanned and volatility computed in one walk. Then
paint runs, selecting a cache tier per node.

### Paint order inside a node

Fixed, and worth memorising, because several traps are just this list:

```
backgrounds · background span passes │ fill · echoes │ overlays │
content leaf │ children │ foregrounds · foreground span passes
```

Decorations dress the node's *outline*, so `clip()` does not clip them —
it bounds the fill, the content leaf and the children. A stacking context
forms on `zIndex`, opacity below 1, a blend mode, a transform, a clip, or a
layer effect, and children cannot interleave outside it: a component cannot
escape the z-order of the site it was composed into.

### Text fx

Motion inside a text leaf is a list of **tracks**. One `Track` is four
values — *which* glyphs (`Selector`), *what* deviation from rest
(`TextEffect`), *how* the beats spread (`Stagger`), and the master
`Animatable<float>` progress that drives it. `Element::fx` appends one;
several compose per glyph, with `GlyphMod` offsets and rotations adding
and scale and alpha multiplying.

```cpp
text(u8"ONE LINE, TWO MOVES", display)
    .fx({.effect = fx::rise(20), .stagger = stagger(unit::Word)})
    .fx({.where = sel::text(u8"TWO"),
         .effect = fx::waveLoop(),
         .progress = &phase});
```

**Units.** `Unit` is the granularity a selector slices and a cascade beats
over: `unit::Glyph`, `unit::Cluster`, `unit::Word`, `unit::Line`,
`unit::Sentence`. `unit::Cluster` is the default, and it is the one that
keeps text correct — a base letter and its combining marks are one unit
and never separate under a stagger.

**Selectors.** `sel::word`, `sel::words`, `sel::line`, `sel::sentence`,
`sel::range`, `sel::text` and `sel::regex` name a position in the text;
`sel::each` slices every unit of one granularity the same way, with
`Selector::take` and `Selector::drop` partitioning each unit exactly.
Combine with `|`, `&` and `!`. A default-constructed `Selector` addresses
everything. Selection is resolved once per (content, layout, selector) and
cached on the element; a pattern that does not compile selects nothing and
warns once.

**Cascades.** `Stagger` keeps the GSAP model — `eachMs` or `amountMs`,
`durationMs`, and a `Stagger::From` origin: `Start`, `Center`, `End`, a
seeded `Random` and a two-ended `Edges`. `Stagger::distribution` shapes
the start times across the cascade, and `Stagger::then` nests a second cascade
inside every beat of the first (`stagger(unit::Word, {…}).then(unit::Glyph,
{…})`).

**Effects are comparable values**, which is what lets text carrying tracks
prune like any other static leaf. A preset compares by its name and its
parameters; an ad-hoc body goes through `fx::effect`, which takes the key
its author gives it — two different bodies under one key compare equal and
one of them silently never draws. `fx::seq` remaps local time so each
phase sees a renormalised 0→1 (`TextEffect::until` sets the joint,
`Phase::xfade` lerps across it), and `fx::mix` evaluates several effects at
one time and composes them by the same algebra stacked tracks use.

**Effects get an `Rng`**, seeded from the glyph's identity, so a scatter is
the same scatter on every frame and after every relayout — which is what
lets it settle and cache instead of jittering forever.

**What a `GlyphMod` can say.** Beyond `dx`, `dy`, `scale`, `rotateDeg` and
`alpha`: `colorMul` multiplies every pass the glyph's style draws (a flat
pass multiplies its colour, a shader pass takes an equivalent modulation,
so a gradient keeps its ramp and wears the tint over it); `scaleX`,
`scaleY` and `skewXDeg` place the glyph with a full matrix, because an
RSXform carries a rotation and one scale and no shear at all; `axis` drives
a variable-font axis at draw time; and `codepoint` draws a different letter
in this one's place. The last two are SUBSTITUTIONS and compose
last-one-wins — a `fx::seq` crossfade cuts them at the middle of its window
rather than lerping, because there is no half-way glyph between two
outlines. (Two phases driving the *same* axis are the exception, and lerp.)

Both substitutions are GATED, because both keep the pen positions shaping
computed. `axis` is honoured only for an advance-invariant axis — the
runtime probes the face once per axis and refuses one that moves advances,
drawing at the shaped face and warning once. `codepoint` is honoured only
where the replacement has the original's advance; a proportional swap would
move every letter after it, which is a reshape and not a redraw.
`fx::axis` sets a coordinate (or sweeps between two across local progress)
and `fx::scramble` is the decoding-text preset built on the substitution:
each glyph churns through a charset and resolves to the true letter by
`t = 1`, seeded per glyph so it is the same churn on every frame.

`Element::variationDrive` is sugar over a whole-text `axis` track, so a
driven axis composes with entrances and loops instead of being a second
text path they would hide.

**Snapping, and `Track::continuous`.** Rotation, alpha, the colour
multiplier and the axis coordinate are quantized before they reach the
draw: each distinct value is a distinct batch bucket *and* a distinct
glyph-atlas strike. Set `Track::continuous` where the steps show and pay
for it — a continuous track mints a strike per value and rasterizes its
glyphs afresh every frame. A glyph any addressing track declares continuous
is continuous.

**Every track declares its `Track::reach`** — how far past the element's
box it may throw a glyph — or takes the number its effect declares. The
recording cull grows by it, on the same over-reporting-is-safe contract a
decoration's `bleed()` carries. Under-report and cached output is
truncated with no diagnostic.

`Element::textFill` and `Element::textStroke` combine with tracks and with a
path baseline alike: a letter in flight, and a letter on a curve, are painted
with the same glyph paint a resting one is. `Element::echo` skips fx text by
contract.

### Text on a path

`Element::onPath` makes a `TextPath` the run's BASELINE. The run is shaped
once — real kerning, real ligatures, real advances — and then laid out
through SigilWeave's own contour geometry: every contour of the resolved
`TextPath::path` is one interval of the run's one line, and the words fill
them in order.

```cpp
text(u8"SIGILLVM · DEI · AEMETH", inscription)
    .width(320).height(320)
    .onPath({.path = shapes::circle(),
             .at = &phase,                       // the marquee
             .align = TextPath::Align::Center,
             .orient = TextPath::Orient::Tangent})
    .fx({.effect = fx::rise(18), .stagger = stagger(unit::Cluster)});
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
fixed ladder of directions because each distinct rotation is a glyph-atlas
strike; `TextPath::exactTangent` is the opt-out, for static artwork set
large.

### Mixed text

**There is no markup language.** Text that is not all set the same way is a
`RichText` value plus selector styling, and the two cover different halves
of the problem: `rich()` says what the CONTENT is, and `spanPaint` /
`spanStyle` say what a RANGE of it looks like.

```cpp
auto p = rich(base)
             .add(u8"Signal ")
             .add(u8"woven", accent)
             .add(u8" through ")
             .add(u8"noise", mono);

text(p)
    .spanPaint(sel::regex(u8"[0-9]+"), sigil::weave::PaintStyle(SK_ColorRED))
    .maxLines(3)
    .ellipsis(u8"…");
```

`RichText::slot` reserves an INLINE SLOT in the run stream — a box of blank
space the flow weaves in, and the name a child of this text node is laid out
into:

```cpp
text(rich(body).add(u8"press ").slot("key", {28, 18}).add(u8" to continue"))
    .child(box().key("key").fill(ink).corners({4}));
```

The reserved box is one UNBREAKABLE word: no line breaks inside it, and it
opens its line when it is taller than the type. The child is an ordinary
subtree — it animates, caches and hit-tests like any other element — and it
re-lands wherever the placeholder lands when the text reflows. It is a
POSITIONED subtree: the placeholder rect is its box, so flex layout does not
run inside it and its own children take explicit rects, exactly as under
`positioned()`.

**A TEXT SLOT IS NOT A MOUNT SLOT.** `slot()` and `Composer::renderSlot` name
a hole a HOST fills from outside the description, and those names live in one
registry for the whole composition. These names live in the rich-text value
alone and are matched against the `key()` of that text node's own children —
so two captions may both reserve a slot called `"icon"` without colliding,
and neither is reachable by `renderSlot`. A child keyed for a slot the
content does not declare draws nothing and says so once; a slot the geometry
could not place is silent, like every other word that did not fit.

`RichText::add` takes a run in the base style, a run in its own
`sigil::weave::TextStyle`, or a run under a NAME resolved through a
`sigil::weave::StyleSet` — supplied by `RichText::styles` or inherited
through `env::Provide`. An explicit set beats the inherited one whichever
order the two are written in, and a name the set does not register resolves
to the base `rich()` was given, so a misspelling shows as content set in
the default rather than as content that did not draw. `RichText::runs` and
`RichText::base` read the finished value back.

**It is a comparable value, and that is the point.** Two rich texts with
the same base and the same runs in the same styles are equal, so a
component that rebuilds its spans every describe prunes exactly like a
static leaf. The `text(std::shared_ptr<sigil::weave::Paragraph>, options)`
overload cannot answer that question — a fresh pointer is a fresh identity
and reads as changed content every time — which is why it stays the escape
hatch for the passage too custom for either verb, not the way to set two
colours in a sentence.

**Selector styling.** `Element::spanPaint` and `Element::spanStyle` restyle
whatever the SAME `sel::` selectors the tracks use address, on every
content form alike — plain text, `rich()` spans and the paragraph overload.
`spanPaint` is paint only and NEVER re-shapes: the glyphs are the glyphs
the unrestyled text shaped, at the positions it shaped them. `spanStyle`
takes a complete style and re-shapes only the words its range covers.
Both are ordered lists — a LATER DECLARATION WINS on overlap, so a broad
rule followed by a narrow exception reads in the order it is written — and
both are comparable values, so a re-described restyle list prunes and only
a changed one re-resolves.

Selection resolves as TEXT RANGES rather than glyphs, because a restyle
runs on the paragraph before there are glyphs to point at: `sel::text` and
`sel::regex` through weave's query layer, `sel::word`, `sel::words`,
`sel::sentence` and `sel::range` through the paragraph's own structure, and
`sel::line` through the layout. Two consequences follow. `Selector::take`
and `Selector::drop` slice glyphs inside a unit, which no text range can
express — an `sel::each` selector restyles its whole units and the slice
warns once. And a `sel::line` restyle costs a second layout pass and
addresses the layout of the text BEFORE the restyle: it does not chase its
own result, so a `spanStyle` that moves the line breaks leaves the
selection where the first breaking put it.

**Layout options, fluently.** `Element::textAlign`, `Element::lineBreak`
(greedy or Knuth-Plass), `Element::hyphenation`, `Element::ellipsis`,
`Element::maxLines` and `Element::lastLine` set the general knobs of
`sigil::weave::ParagraphLayoutOptions` on any content form. The rest of
that struct — justification elasticity, Knuth-Plass tolerance, tab stops,
line-metric overrides — stays behind the paragraph overload, which takes
the whole options value. **On that overload the setters override FIELD BY
FIELD**, and only the fields actually set: everything a setter did not name
keeps the value that was passed in.

### Vertical CJK

`Element::writingMode` sets the passage running down the page.
`sigil::weave::WritingMode::kVerticalRL` is the CJK book layout: characters
top to bottom, columns advancing RIGHT TO LEFT from the node's right edge.
It is a field-masked override like every other layout setter, so it works on
plain text, on `rich()` spans, and on the paragraph overload — where a mode
nobody names leaves the paragraph's own mode standing.

```cpp
text(rich(mincho)
         .add(u8"平成")
         .add(u8"31", tateChuYoko)
         .add(u8"年、縦組みに対応した。"))
    .width(260).height(300)
    .writingMode(sigil::weave::WritingMode::kVerticalRL)
    .fx({.effect = fx::rise(24), .stagger = stagger(unit::Cluster)});
```

**BOTH AXES ARE MEASURES.** A horizontal passage reads its width as the
measure and grows down; a vertical one reads its HEIGHT as how far a column
runs before the next one starts, and grows LEFT. So a vertical leaf's
intrinsic size swaps: one column of type measures tall and one column pitch
wide, and giving the node no height gives it one endless column. It has no
baseline — the reading axis is y and a column's glyphs centre themselves
across the axis rather than standing on one — so `Align::Baseline` gets its
first character's own baseline, which lines a column's opening character up
with a horizontal neighbour's first line.

**Per character the orientation is UTR#50's**: ideographs stand upright and
take their `vert` forms, Latin lies on its side. A run that wants otherwise
says so in its own style — `sigil::weave::VerticalForm` is `kAuto`,
`kUpright`, `kRotated` or `kTateChuYoko` — set on a `rich()` run's
`sigil::weave::TextStyle` or through `spanStyle`. It is a SHAPING field, so
it re-shapes the words it covers and nothing else; there is no separate verb
because there is no separate concept. 縦中横 is the one to know: a short run
shaped horizontally and set upright across the column, which is how two-digit
numbers read in vertical prose.

**The engine runs in columns.** `unit::Line` IS A COLUMN here, so a
`stagger(unit::Line)` beats column by column and `sel::line(0)` addresses
the rightmost one; `unit::Cluster` runs down a column in reading order.
`spanPaint`, `spanStyle`, `textAlign` (start is the top of the column),
`maxLines` (which clamps COLUMNS), `lastLine`, `lineBreak`, `textStroke`,
`variationDrive` and `feed()`'s text tier all work as they do across a line.
`Element::textFill` maps its unit square onto the COLUMN BLOCK rather than
onto a cap band — a column's glyphs centre across its axis instead of
standing on a baseline, so there is no cap band to hang a ramp on — which
means a gradient authored in [0,1]² crosses the type reading DOWN the page.

**Track deviations apply in the frame the layout placed the glyph in**, the
same rule a path baseline follows — and in a column the placed frame is the
glyph's own vertical pose. An UPRIGHT glyph is not turned, so its frame is
the canvas frame: `fx::rise` lifts it up the page. A ROTATED one is turned
to the column, so its frame is turned with it and a rise lifts it off its own
baseline, across the column. A glyph's pivot moves too: an upright glyph
turns and scales about the point on the COLUMN AXIS its pen reached, not
about a point half a column pitch to its right.

**What does not follow the type down the page.** `onPath` ignores
`writingMode` entirely — a path run's baseline is its own geometry and has
no columns to advance — and setting both warns once and keeps the path.
`flowAround` exclusions are cut out of horizontal line bands and have no
column spelling: they warn once and the columns run clean. An `ellipsis`
marker needs a straight horizontal final line to land on, so a clamped
vertical passage reports its overflow without drawing one. Underlines and
strikethroughs skip vertical runs for the same reason.

**Ruby and kenten are not library features**, deliberately. Each is a few
lines over the placed runs of a finished layout — read
`Composer::paragraphLayout` and draw beside what it reports — and the shapes
that annotation takes differ enough per passage that a verb would fit none of
them. The SigilWeave gallery's vertical scene shows both.

---

## The header map

Everything lives in `namespace sigil::compose` under
`include/sigilcompose/`. The public include root is `include/` and nothing
else — the two internal headers beside the sources are not reachable from
outside the library.

**Kernel — `Compose.h`.** `Element` and its builders; the factories `box`,
`stack`, `positioned`, `text`, `image`, `custom`, `slot`, `connector`,
`rail`, `band`, `layout`; `Composer`; `memo`; the `env::` inherited-value
channel; the mixed-text value `rich` and the span-restyling verbs; the
comparable seam values (`Shape`, `Shaper`, `Profile`,
`Decoration`, `CrossingRule`); the stroke grammar (`spans::` and
`Element::stroke`); the masking family (`parts::`, `by::`, `Region`);
`Effect`; the one-shot verbs `snapshot`, `measure`, `metrics`,
`measureRun`; and re-exports of SigilMotion's animation vocabulary
(`Animatable`, `Transition`, `animate`, `bind`, `ease::`) so authoring
never has to name a second library. A user who reads only this header has a
complete and sound model; nothing below it changes kernel semantics.

**Paint values.** `Material.h` is the polymorphic paint value that
supersedes a flat `Fill` — gradients, images, raw SkSL with live uniforms,
blend stacks that compile to one shader, world-space anchoring. `Sdf.h`
gets shape, border, glow and soft shadow out of a single shader pass.
`Pattern.h` and `Patterns.h` bake tile recipes once into repeating
materials, plus stock generators. `Ocio.h` is an output-stage view
transform for `Composer::setView`, compiled only when the build finds
OpenColorIO.

**Geometry.** `Shapes.h` is the silhouette and curve library — every
generator is a comparable value, so a shaped node prunes like an unshaped
one. `Layouts.h` holds the placement schemes for the `layout()` seam
(`layouts::Radial`, `AlongPath`, `ModularGrid`, `Diagonal`, `BaselineGrid`,
`Scatter`). `Routers.h` holds the stock connector and rail routers
(`routers::straight`, `orthogonal`, `polyline`, `octilinear`, `orbit`).

**Marks.** `Decorations.h` has the concrete primitives that plug the
`Decoration` seam — `PathFormat` (stroke formatting), `Slice` (lattice
image mapping), `ContourWalk` (walk the outline and run a program at each
sample), `Wash`, `Border`. `Brushes.h` is the brush engine over them:
`brush::solid`, the composites `brush::layers` and `brush::weave`, and the
archetypes `brush::Scatter`, `brush::Pattern`, `brush::Ribbon`,
`brush::Art`. `Lines.h` is the cartography and diagram stroke vocabulary —
parallel casings, terminal caps, ties, waves. `LayerStyles.h` is the
Photoshop route to rich surfaces: bevels, sheens, inner shadows built from
gradients and blurs rather than shaders.

**Components.** `TextFx.h` supplies the stock effects, and the `fx::seq`
and `fx::mix` combinators, for the kernel's `Element::fx` seam.
`Feed.h` is the streaming collection — a `feed::Ring` of rows, windowed to
the newest `feed::Options::visible` and keyed by sequence id, so an append
costs one mount and every surviving row keeps its cached picture; rows of
text name their style in a `sigil::weave::StyleSet` (`feed::TextRow`,
`feed::TextOptions`), and `feed::plate` is the bordered strip several feeds
sit on. Built purely by composing the kernel. `Instances.h` renders
thousands of sprites as one leaf, with the pool on your side of the seam. `Web.h` makes a live
Ultralight page a leaf; it is a header-only adapter and the library does
not link SigilScry, so include it only in targets that do.

**Host and tooling.** `Util.h` is deliberately-demoted sugar — `util::Stage`
(the canonical host loop), gradient constructors, `util::stroke`,
`util::Shadow`, `util::disc`, `util::marquee`. `Studio.h` is the file
prelude: `studio::hex`, `studio::type`, `studio::pickFace`, `studio::ramp`,
`studio::phase`, `studio::fmt` — spellings, never decisions.
`GpuImage.h` holds `gpuimg::drawLattice` and `gpuimg::drawSpriteAtlas`,
which are mandatory rather than convenient (see the traps). `Debug.h`
verifies generated geometry — `debug::coverage`, `debug::check`,
`debug::report`, `debug::failures` — for tests and `--verify` paths, not
the paint loop.

**Kit — `kit/Kit.h`.** A tier above the library that adds no kernel state
and no new equality: `kit::Frame` and `kit::Grid` (figure-local polar and
unit coordinates), `kit::ticks` and `kit::chords` (division ladders as one
path), `kit::PixFont` (aliased bitmap-font bakes), `kit::Scrim` and the
halo/shade legibility helpers, and `kit/Strokes.h`'s shapers, profiles and
span compositions. The kit is a **separate CMake library**
(`SigilComposeKit`) whose only include path is compose's public headers,
which is how the public/internal boundary is proven rather than asserted.
Note that the umbrella header does not pull in `kit/Strokes.h`; include it
directly.

---

## The declared-volatility contract

The library caches provably-static subtrees on its own. A `Cache::Auto`
subtree with nothing volatile in it records into a picture; a node that has
been expensive for several consecutive frames may be re-baked into a raster
image and blitted thereafter. None of that is safe unless "static" is
*true*, and nothing in the library can introspect a type-erased value to
find out.

**So anything that changes without a re-describe must say so.** Concretely:

- A decoration whose paint moves — a bound dash phase, a walk keyed to
  elapsed time — declares `bool isAnimated() const`. The seam reads it off
  the value at construction; a scheme that stays silent is treated as
  static and its node's picture will be replayed forever.
- A `custom()` paint program that reads the clock (or anything else the
  library cannot see) must declare `.cache(Cache::None)`. It is the
  immediate-mode floor and it costs a repaint per frame, which is the
  point.
- A `Material` that reads `uTime` or carries a uniform bound to an
  `Output` is live by construction and declares itself; so is an `Effect`
  with a bound uniform or a live child. Tier inheritance is real: a live
  child makes the parent effect live, so no cache can freeze the
  parameter.
- A decoration that paints beyond the node's box declares `bleed()`, and
  one that needs to say how wide the *mark* is declares `reach()`. These
  are different numbers — an inner-aligned stroke bleeds zero while
  painting a mark several pixels wide. Over-reporting is safe;
  under-reporting silently truncates cached pictures.
- A decoration that reads another element's resolved path declares
  `borrows()`, so the element can register the keys without looking inside
  the value.

The counterpart obligation is equality. Anything read live must
participate in the reconciler's structural comparison, or a pruned node
reads a stale value forever. The conservative fallback is built in:
anything holding an incomparable callable compares *unequal* and never
prunes.

---

## Traps

### Silent no-ops, the largest class

Several correct behaviours produce nothing, with no diagnostic, and look
exactly like a layout bug.

- **An unknown key resolves to nothing, everywhere in the derive family.**
  `flowAround("typo")`, `spans::fit("typo")`, `around("typo")`, a
  `connector` to a node not in the tree, a `strand::from` on a missing key
  — every one draws nothing and says nothing. Check your keys first. (A
  `rich().slot()` name is the one that is LOUD, once: it names a mount point
  the author typed, not a geometry source.)
- **Hit testing returns any keyed node whose box contains the point,
  painted or not.** A keyed full-bleed layout shell with no fill therefore
  swallows every hit in the frame, and the failure is total and silent.
  The opt-out is `Element::hitTestable(false)`, which excludes the node's
  own box while still testing its children.
- **Skia's native lattice and atlas draws are not implemented on
  Graphite** in this Skia — they draw nothing. Worse, one recorded on a
  raster canvas still vanishes when the recording replays on Graphite, so
  a raster test cannot see it. Use `gpuimg::drawLattice` and
  `gpuimg::drawSpriteAtlas`, which decompose on every backend and never
  emit the native op. This is not an optimisation layer; it is the only
  correct path.
- **A `custom()` leaf sizes like an empty box.** It is literally a box with
  one background program, so it has no intrinsic size: dropped into an
  `absolute().inset(0)` parent it measures zero on the main axis and the
  program runs against a zero-height context. Give it dimensions, or make
  it `absolute().inset(0)` itself.
- **`.key()` on a `slot()` renames the mount.** A slot's name *is* its key,
  so `slot("hud").key("panel")` produces a slot called `"panel"` and
  `renderSlot("hud")` then finds nothing. It warns once.

### Lifetime

Every live binding is a **non-owning raw pointer** to an `Output` the
caller owns; the composer holds its `FontContext` and its `Ticker` by
reference, and both must outlive it. A recreated `Output` at a new address
does more than dangle: bindings compare by *identity*, so the new address
also breaks prune equality and re-patches the node on every describe. Hold
your outputs where you hold your model.

### Pruning

The raw-callable escape hatches — a `Shape` built from a lambda, an
unkeyed `custom()` program, a bare `PaintProgram` decoration — can never
compare equal to a separately constructed one, so their nodes re-patch on
every describe. The fix is to hold the value rather than re-minting it,
or to wrap the node in `memo()`. Two spellings avoid the problem outright:
`custom(key, program)` makes the key the program's identity, and every
`shapes::` generator is a comparable value.

This matters more than it sounds. An inherited value carried through
`env::` that holds a `std::function` is incomparable, and that turns every
`memo` below it into a permanent miss. Materialise derived values *into*
the type: run the function, store the result.

### Ordering

Decoration stacking is a contract, not a hint. `background()` paints
*beneath the fill*, so an opaque fill covers it completely — a bevel added
with `background()` on a filled node draws underneath its own surface and
looks like nothing happened. The slot between the fill and the content is
`overlay()`; above the children is `foreground()`.

Within `stroke()`, unqualified whole-boundary strokes paint first and
span-qualified passes paint over them, each group in declaration order.
Interleaving the two groups by call order is not expressible; make the
whole-boundary one a span pass (`spans::every(1)`) so both are in one list.

Span-qualified passes also *claim* the runs they resolve to, and two claims
that overlap are reported out loud. Stacked masks intersect where their
selections overlap — both gates must pass — and union is spelled inside one
gate value by combining spans with `|`, never across masks.

One coordinate convention, stated once and obeyed everywhere: **positive
`across` is to the LEFT of travel**, which in screen space (y down) is
outside a clockwise path. `bandPointAt`, `Profile::across`,
`strand::offset`, `TextPath::offset` and the `lines::` family all mean the
same side. Relatedly, **fraction 0 on a boundary is the bottom-left
corner**, running up the left edge — so `spans::upTo(0.25f)` on a square is
the left edge, not the top one.

---

## Boundaries

The library links `SigilImage`, `SigilMotion`, `SigilWeave` and Skia
publicly, and Yoga privately. OpenColorIO is optional and gates `Ocio.h`
alone.

Deliberately *not* linked: SigilScry (the web leaf is a header-only
adapter, exercised by its own test target), EnTT (the instancing header
keeps the registry on your side), SigilShape, Diligent, and Qt — Qt
identifiers are banned outright in exported headers.

What it refuses to be:

- **No markup, parser or external DSL.** Markup can only name
  pre-registered values; the vocabulary here is C++ values and callables.
  A serialization schema can be a *producer* of element values, never the
  API.
- **No imperative node mutation.** Describe or bind, and nothing else.
- **No timeline object.** Multi-beat choreography is windowed bindings
  over one phase output (`bind(&phase).window(lo, hi)`).
- **No surface, loop or thread ownership.** The composer is a guest in
  someone else's canvas, and a host that wants many surfaces makes many
  composers.
- **No depth and no perspective in the model.** There is no z, no
  `rotateX`, no projection. A camera, if you want one, is the host's
  matrix on the canvas — a recording is matrix-independent by
  construction, so a moving camera invalidates nothing the library holds.
  The places that pin pixels to a device rect refuse a perspective matrix
  explicitly.
- **Compositing happens in encoded sRGB, with no linear stage.** Every
  surface compose paints into is `N32Premul` with no colour space
  attached, so the `SkColor4f` you write is the display-encoded number
  that lands in the byte and a shader's channels are those same numbers.
  Any weighting of colour channels inside the library uses coefficients
  defined on encoded values. `Composer::declareInputSpace` lets you state
  what you believe your values are; a mismatched declaration warns once
  and performs **no** conversion, because a colour-managed surface would
  be a breaking change rather than a setting.

---

## Build and test

The library target is `SigilCompose`; the kit is `SigilComposeKit`. From
`apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Registered tests: `compose_test`, `compose_kit_test`, `compose_spike_test`,
`compose_gallery_test`, `compose_sketch_smoke`, `compose_sketch_stock`,
`compose_sketch_shape`, plus `compose_gpu_test` (Apple only, needs the
Graphite plumbing) and `compose_web_test` (needs the Ultralight SDK).
`compose_bench` and `compose_demo` are executables, not tests — anything
resembling a performance claim belongs to `compose_bench` and to the plate
ledger, never to prose.

**The gallery** is a macOS app bundle, so headless runs go through the
binary inside it:

```sh
build/bin/<config>/ComposeGallery.app/Contents/MacOS/ComposeGallery \
    --headless <outdir> [--gpu] [--scene <name|index>]
```

`--scene` takes a case-insensitive substring and renders just that one,
which is the loop for visual work. `--shot <png>` captures the application
window itself rather than a scene.

**The sketch host** is `ComposeSketch`, a live-coding loop: point it at a
single `.cpp` file, save, and the recompiled sketch hot-swaps into the
running canvas. It renders headlessly too (`--frame out.png --at <s>`), and
`--bench` reports frame-time phases. Study sketches are compiled into the
gallery as an object library, so one file is both a hot-reload sketch and a
gallery scene.

**Byte-identity sweeps** run through `scripts/plate_ledger.py`, which
renders every gallery scene in parallel, hashes the plates and compares
against a stored baseline. `--rebase` adopts a new baseline; `--stability N`
re-renders movers to separate a self-nondeterministic scene from a real
change.

### The generated doc-probe translation unit

`compose_test` builds a C++ file that does not exist in the source tree.
`test/docs/api_doc_probes.py` reads this document, extracts every qualified
name an author could copy out of it — from fenced code blocks **and** from
inline `code` spans, because the prose carries as many names as the
examples do — and emits probes that only compile if the headers still spell
those names that way. A member is probed through a `requires` expression, a
namespace-scope entity through a using-declaration, and a designated
initialiser through the initialiser itself, which is a stricter question
than whether the name resolves.

The consequence is the point: a name written here that drifts out from
under the prose is a build break, not a confident wrong answer. Names the
document mentions on purpose without their existing — a worked example's
own host type, a spelling recorded because it was removed — go in the
generator's exclusion table with a reason. A documented name that resolves
to nothing and is not excluded fails the generator, so the guard cannot go
quiet.
