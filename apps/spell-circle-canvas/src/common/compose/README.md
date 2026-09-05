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

**`TYPOGRAPHY.md` is the type chapter.** Everything a passage of type can
be told past `text(utf8, style)` — the per-glyph fx tracks, a run on a
path, span restyling, the paragraph controls, threaded frames over a
`Story`, readings beside the type, a passage whose measure moves, and
vertical CJK — lives there, and is checked against the headers by the
same probe this page is.

---

## Writing a component

A component is a free function from your data to an `Element`. There is no
base class, no lifecycle, and no state inside the library — the state is
the argument.

```cpp
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/typography/Typography.h>

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
      c.alarm ? hex(0xff5252) : hex(0x8fd0ff);
  return box()
      .row()
      .gap(10)
      .padding(12)
      .corners({6})
      .fill(hex(0x0e1218))
      .alignItems(Align::Center)
      // A mark on part of the boundary: L-brackets at every tangent break.
      .stroke(spans::corners(12), stroke(1.5f, Fill::color(ink)))
      .child(text(c.label, weave::textStyle({.size = 13, .color = ink})))
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
      .fill(hex(0x05070a))
      .children(channels | std::views::transform([](const Channel &c) {
                  // memo() skips the describe call entirely while the props
                  // compare equal. key() is what the reconciler matches on
                  // across describes, so rows survive reordering.
                  return memo(c, meter).key(c.id);
                }));
}
```

The host side is three objects — a clock, a ticker and the composer —
which the host owns and wires together:

```cpp
sigil::weave::FontContext fonts = /* yours */;
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

SigilSketch bundles exactly those three lines behind its own session, so
a sketch declares a scene and never a loop. That is a convenience of a
host and not of this library — spell the objects out when the clock or
the ticker is shared with something else.

The other write path is a live binding — a `choreograph::Output` the host
mutates every frame, read straight out of paint with no `render()` call:

```cpp
choreograph::Output<float> spin{0.0f};
ticker.add([&](double) {
  spin = motion::phase(ticker.elapsed(), 6.0);   // a wrapping [0,1) phase
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
every case, and so does the writing mode: a column a target crosses is cut
into a head and a foot exactly as a line is shortened beside it.

Every derivation DECLARES WHAT IT READS, in the same statement that stores
the key: `flowAround`, `spans::fit`, `strand::from`, `band` around a key,
`connector`, `rail` and `thread` each record a `sigil::core::Read` — the
node waited for, and which `sigil::core::Facet` of it is needed (a box, an
outline, or the units a text produces). `sigil::core::orderByReads` turns
those declarations into the order the derived nodes are resolved in, so a
rail anchored on a connector written after it, or a frame threaded from a
frame written later, settles in the same pass instead of one behind. It is
stable: derivations that read none of each other are resolved in exactly
the order they were written in, which is nearly every tree. Nothing infers
an edge from which fields a node carries, so a derivation added later is
ordered by its own declaration and by no list that has to be found and
extended.

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
escape the z-order of the site it was composed into. The one order that is not
tree order is a shared space's: the children of a node that opens one
are painted back to front by depth (see "3D, the CSS way").

### Type

`text(utf8, style)` and `text(rich(base).add(…))` are the two content
forms, and everything a passage can be told past that — the per-glyph fx
tracks and their selectors, a run riding a path, span restyling, the
paragraph controls, threaded frames over a `Story`, readings set beside
the type, a passage whose measure moves, and vertical CJK columns — is in
**`TYPOGRAPHY.md`**, one file over. It is checked against the headers by
the same probe this page is.

The shape of it in one paragraph: a text leaf holds an ordered list of
`fx()` TRACKS, each `(selector, effect, stagger, progress)` — which
glyphs, what deviation from rest, how their start times spread, what
drives it — and the same `sel::` vocabulary addresses glyphs for a track,
characters for a `spanStyle`, and units for anything standing beside the
passage. What a passage is SET like is `Element::paragraphs` and the
layout setters beside it, which map onto
`sigil::weave::ParagraphLayoutOptions` field by field.

### What a decoration dresses

Every decoration is drawn ACROSS AN OUTLINE, and the outline a node hands
its decorations has always been its own shape — which on a text leaf is a
rectangle, and is why a chrome style on a word bevelled a slab behind the
word. `Element::boundary` says otherwise:

```cpp
text(u8"CHROME", display).boundary(Boundary::Glyphs).style(kit::y2kChrome());
```

`Boundary::Glyphs` hands them the glyph contours the placement produced,
so every layer style already written works on letters with no new preset
and no second code path. The outline follows a wrapped line, a mixed-style
run's size, a path run's curve and a vertical column's axis, because it is
read off the placed glyphs.

The three answers are three MECHANISMS, and the third one is the only one
that looks at a pixel. `Boundary::Outline` is the node's SHAPE — its box,
its `shape()`, a routed path, a band's swept region. `Boundary::Glyphs` is
the PLACEMENT's contours. `Boundary::Coverage` is WHAT THE NODE DREW: its
rendered layer is rasterised into an alpha surface of its own and the
covered pixels are traced back into a path, which is the only answer that
knows about an image's alpha cut-out, a clipped or masked subtree, or
anything else whose visible silhouette is neither a shape nor a glyph run.

```cpp
image(logo).boundary(Boundary::Coverage).style(kit::y2kChrome());
```

Tracing a raster has three consequences and all three show:

- **The boundary is a staircase.** It is built from whole pixels, so its
  edges are axis-aligned steps and a decoration that dresses it dresses
  that staircase.
- **The step is one device pixel.** The trace rasterises at the node's own
  device scale, so the staircase is as fine as the edge the viewer is
  looking at — which is the whole reason to trace pixels rather than a
  shape — and a node that moves to a denser display is traced again. A
  ceiling on the raster's longer side bounds what a very large node asks
  for: past it the raster is scaled down to fit and the steps grow.
- **Paint below half coverage is not a silhouette.** A pixel joins the
  boundary when the node's paint covered at least half of it, so a 30%
  wash traces to nothing and its decorations have nothing to dress.

The node's OWN marks are not in the trace — they are what dresses it, and
a mark that dressed itself would have no fixed point — while its fill, its
content, its children and their marks are. A node that traced to nothing
keeps its shape, exactly as a text leaf with no glyph outline does. The
trace is re-run when the node's rendered layer is invalidated, which for a
volatile subtree is every frame.

`Boundary::Auto` is what a node that says nothing gets and means its own
shape: a caption with a drop shadow means the caption's box, and neither a
text leaf nor an image silently changes what it has always meant.

## 3D, the CSS way

A node is a **plane**. Five lanes turn it and move it in depth —
`rotateX`, `rotateY`, `translateZ`, `scaleZ`, and `rotateZ`, which is
`rotate` under its 3D name — and `perspective` on an ancestor is the
view that ancestor's children are seen through: a viewer standing that
many pixels in front of the ancestor's plane, over the point
`Element::perspectiveOrigin` names (its centre by default). Every one of
them is a `motion::Animatable` lane exactly like the 2D lanes: it
transitions, mounts, binds and prunes the same way, animating it never
relayouts, and `Element::transformOrigin3d` gives the pivot a depth. The
frame is CSS's — x right, y down, **+z toward the viewer** — so a
positive `translateZ` under a `perspective` comes closer and grows. The
three rotations compose as the CSS list `rotateX() rotateY() rotateZ()`,
x outermost, then the scale and the skew about the transform origin, with
the translate outermost of all.

```cpp
box().perspective(800)  // the view, for the children
    .child(box().rect(card).fill(paper).rotateY(bind(&turn).target(0, 360)));
```

**One 4x4 per node, flattened at paint.** A node composes its parent's
perspective, its layout offset and its own lanes about its origin into one
4x4 and projects its plane onto the plane its parent paints on — the 3x3
with a perspective row that Skia draws, one concat. Tree order stays draw
order; the node's fill, its text, its children and its caches all live in
its plane as they did before; a settled plane's recording is taken in
that plane and replayed through the projection, and a `Cache::Texture` on
a turned plane bakes in the plane at the projection's largest local
scale. A node with no depth lane is placed by the byte-exact elementary
ops it always was. A plane turned a quarter turn has no width and draws
nothing; a flattening with no inverse draws nothing and answers no hit.

**`Element::preserve3d` opens a shared space.** The children of such a
node keep the depth their lanes give them — their 4x4s compose with the
host's instead of flattening into it — and are painted **back to front by
the depth of their centres**, whatever order they were declared in. A
cube is six children of one host, each turned about its own centre and
then moved half an edge along the cube's axis in the host's frame, since
the translate is outermost:

```cpp
box().preserve3d().rotateY(bind(&yaw).target(0, 360))
    .child(face().translateZ(half))                 // front
    .child(face().rotateY(90).translateX(half))     // right
    .child(face().rotateX(90).translateY(-half))    // top
    .child(face().rotateY(-90).translateX(-half))   // left
    .child(face().rotateX(-90).translateY(half))    // bottom
    .child(face().rotateY(180).translateZ(-half));  // back
```

The view reaches into the space: the perspective declared on the host's
parent projects every face, a nested `preserve3d()` compounds the space,
and a child that does not declare it ends the space at its own plane, its
children flat inside it. The host's own paint stands at the front of its
own plane and is drawn before its children. An edge-on host still shows
the planes its space holds, because the space is drawn on the plane
beneath the host and takes no inverse of it. A host paints live — it
never records or bakes an artefact of its own, which is what keeps every
recording above the matrices it bakes — while its children keep theirs;
`Composer::profile` reports the refusal as `HostsSpace`.

**A grouping property flattens.** A node that composites as one layer — a
`clip()`, an opacity below 1, a blend that is not source-over, an
`effect()`, a `backdrop()`, a `mask()`, a `Boundary::Coverage` or an
explicit `Cache::Texture` or `Cache::Group` — cannot host a space, exactly
as CSS's grouping properties force a flat transform style: its children
are projected one by one onto its plane, in tree order, with no depth
between them.

**`Element::backface`** with `Backface::Hidden` draws nothing and answers
no hit while the plane's back faces the viewer — decided by the
inverse-transposed normal of the node's whole projection, so a half turn
about x or y hides it and a `scaleX(-1)` mirror does not. A flipping card
is a host turning on `rotateY` with a front and a back pre-turned half
round, both hidden.

**Hit testing goes back through the projection.** `Composer::hitTest`
inverts the flattened 4x4 the plane was drawn with — from the plane the
space is drawn on, for a node in a space — and a point whose pre-image is
at or behind the viewer, or off the plane's projection, misses; a host's
children are tested nearest first.

**What this is not.** Planes never intersect: a child that crosses
another is drawn whole, in the order their centres sort. Nothing is lit,
nothing casts, and a depth is not a position in a world. A scene with
those is a set — SigilWorld — and this stays the retained 2D tree with
CSS's model over it.

---

## The header map

Everything lives in `namespace sigil::compose` under
`include/sigilcompose/<feature>/`, one directory per feature target, and
the include spelling is the feature's: `<sigilcompose/core/Element.h>`,
`<sigilcompose/kit/Layouts.h>`. The public include root is `include/`
and nothing else — the internal headers beside each feature's sources are
not reachable from outside it. Each feature has an umbrella named after
it (`core/Core.h`, `kit/Kit.h`, `brush/Brush.h`,
`typography/Typography.h`) over its public headers, and
`<sigilcompose/Compose.h>` at the root is the umbrella over the kernel —
exactly `core/Core.h`. Each header stands on its own; include
the one a translation unit needs, from the feature whose target the
translation unit links.

**Kernel — `core/`.** A user who reads these headers has a complete and
sound model; nothing below them changes kernel semantics.

- `core/Paint.h` — the paint values: `Fill`, `Corners`, `Backface`,
  `PaintContext`,
  `StampCache`, and the colour spellings `hex`, `alpha`, `mul`, `lift`,
  `mix` over `SkColor4f`.
- `core/TextPainter.h` — the seam the kernel draws dressed type through:
  `TextPainterOps`, the operations the composer asks of text that is not
  resting on its own straight baseline, and `TextPainter`, that engine
  as the value a text verb installs on a description. It is spelled in
  the typography feature's vocabulary and only names it; the kernel
  holds the paragraph, lays it out and draws it at rest by itself.
- `core/Shape.h` — the comparable seam values `Shape` (with
  `ShapeScheme`), `MotionPath`, `Decoration` and its declared-volatility
  concepts, and `LayerStyle`.
- `core/Stroke.h` — the stroke grammar: `Spans` and `spans::`, `Across`,
  `Around`, `StrandPath` and `strand::`. The path arithmetic under it is
  SigilGeometry's — the width law `geometry::path::Profile` with
  `geometry::path::profile::self` / `offset`, the deviation
  `geometry::path::Shaper`, the band `geometry::path::bandRegion` on a
  `geometry::path::Formation`, and `geometry::path::CrossingRule` with
  `geometry::path::crossing::` deciding who passes over whom.
- `core/Mask.h` — the masking family: `Region`, `parts::`, `by::`, `Gate`,
  `Mask`.
- `core/Layout.h` — `Dim` and its literals, `Align`, `Justify`, `Echo`,
  `Cache`, `LayoutInput` / `LayoutScheme`, `CellSpan`, and the
  `ComponentProps` / `ComponentFn` concepts.
- `core/Element.h` — `Element` and its builders, the class alone.
- `core/Factories.h` — the functions that start one: `box`, `stack`,
  `positioned`, `text`, `frame`, `image`, `custom`, `slot`, `layout`,
  `memo`, with `toU8` for a call site holding a `std::string`.
- `core/Measure.h` — the one-shot verbs that take a tree without a live
  composer: `snapshot`, `intrinsicSize`, `metrics`, `measureRun`, `runPens`.
- `core/Tiles.h` — `tiles::`, the slicing of one baked picture into a run
  of tile-sized rasters.
- `core/Instances.h` — the instanced sprite leaf: `instancing::Pool`,
  the struct-of-arrays store on your side of the seam; `instancing::Atlas`,
  the cells baked once from element trees; `instancing::instances`, the
  leaf that stamps the pool in one draw; and `instancing::pick`, the
  inverse of the stamp. The fillers that arrange a pool are the kit's
  (`kit/Placers.h`).

  A pool can also carry ONE FLIGHT PER INSTANCE — `Pool::Flight`, an
  opt-in lane like `sizes()` and `alphas()`, holding where a sprite starts
  and lands in position, rotation, scale and opacity, and the second it
  leaves and how long it takes. `Pool::fly(seconds, ease)` steps them all
  and writes the lanes the stamp reads. The times are per instance because
  the STAGGER is what a field of thousands is: `motion::Spread` and
  `motion::Cascade` divide one progress between N units and are the right
  thing when the units are a run, while a field seeded from a distribution
  has its times already. One ease serves the whole pool, since the
  variation between sprites belongs in their times and not in their
  curves. It sits on the pool rather than among the placers because it is
  not an arrangement: a placer says WHERE the instances of a grid or a
  ring go, this says when each gets to where it is already going, and it
  reads state the pool itself holds. Motion that is not a flight stays the
  caller's — a per-frame shiver, a gate that fades a whole field at once,
  anything whose value depends on something besides this instance's own
  progress — and steps after `fly()`, over the lanes it wrote.
- `core/Derive.h` — `connector`, `rail`, `Anchor`, `band`, `bandPointAt`,
  and the `derive::` namespace that gathers the family.
- `core/Composer.h` — `Composer`, and `TextSettling`, what
  `Composer::settling` reports about a live passage's last layout.
- `core/Paint.h` — beside `Fill` and `PaintContext`: `frameOf`, `toFill`
  and `resolveFill`, the three lines that put SigilMaterial's
  `material::skia::Paint` on a node. The paint model itself is that
  library's — gradients, images, raw SkSL with live uniforms, blend
  stacks, world-space anchoring — and what is compose's is the routing: a
  static paint collapses to a `Fill` and rides the caching and prune path,
  a live or geometry-dependent one is kept whole on the node so the
  painter resolves it against the frame it is drawn at. The one-line
  gradient `Fill`s, `linearGradient` and `radialGradient`, are here too.
- `core/Feed.h` — the streaming collection: a `feed::Ring` of rows,
  windowed to the newest `feed::Options::visible` and keyed by sequence
  id, so an append costs one mount and every surviving row keeps its
  cached picture; rows of text name their style in a
  `sigil::weave::StyleSet` (`feed::TextRow`, `feed::TextOptions`). Built
  purely by composing the kernel; the bordered strip several feeds sit on
  is the kit's `kit::plate` (`kit/Plate.h`), with `kit::tinted` building
  the one-face style set its rows name, and `kit::console` is that plate
  over N feeds of one voice — each in its own column, or `Console::stacked`
  to a column — which is the verification plate a study prints its checks
  into.

**The animation vocabulary is SigilMotion's and is spelled that way.**
`motion::Animatable` is the property slot every setter here takes,
`motion::Transition` the eased change, `motion::animate` the keyframe
builder, `motion::bind` the shaped binding of a live `Output`, and
`motion::ease::` the curves — all from `<sigilmotion/Animation.h>`. The
SCHEDULE is the same value wherever it runs: a cascade over glyphs, over
a set's children or over a feed's rows is one `motion::Spread`, and what
compose adds to it — what a unit IS — sits beside it on the track. The
time helpers a scene reaches for are there too: `motion::ramp`, a delayed
eased transition in float milliseconds; `motion::phase`, a wrapping
`[0, 1)` over a period; `motion::quantizeTime` and its integer
counterpart `motion::stepIndex`; `motion::decay`, the open-ended settle a
duration-based curve cannot be. So is the whole of "is this value
moving": `motion::isLive` is the one body every volatility walk in this
library asks, and what it can and cannot say is stated in that library's
README.

What compose OWNS is resolution, not the value. An `Animatable` is
resolved against a `PaintContext`, taking node transitions, stagger,
mount entrances and the per-frame composer state into account; SigilMotion
supplies the value and compose decides what a described change means to a
node. That is also why a bound `Output<T>*` compares BY IDENTITY — the
pointer, not the number behind it — so a node holding one is declared
volatile and does not cache, and handing back a freshly constructed
Output at a new address breaks pruning even when the value is unchanged.

**Geometry — `kit/`.** The silhouette and curve catalog is
SigilGeometry's, spelled `geometry::shapes::` from
`<sigilgeometry/kit/Silhouettes.h>`: a comparable `path(SkSize)` value
needs nothing of a component tree, and every one of them prunes a shaped
node exactly as an unshaped one prunes. `kit/Layouts.h` holds the placement schemes for the `layout()`
seam (`layouts::Radial`, `AlongPath`, `ModularGrid`, `Diagonal`,
`BaselineGrid`, `Scatter`, `Table`).

**A scheme sees one thing about a child it could not measure: the cells
the child claimed.** `LayoutInput` carries the container's size, every
child's measured size and every child's first baseline — all facts a
layout pass established — plus `childCells`, one `CellSpan` per child,
written by `Element::cells` and `Element::cellAlign`. It is on the CHILD
and not in a list the scheme carries beside it, because a parallel list
has nothing to check itself against: insert or reorder one child and
every entry after it silently addresses the wrong one, taking another
cell's span, alignment and origin, with no error and a picture that still
looks plausible. `CellSpan::declared` is what a scheme reads to tell
"cell (0,0)" from "wherever you like", so a table can flow the children
that said nothing into the cells no child claimed. `layouts::Table` is
placed entirely by it; `ModularGrid` reads it too and falls back to its
own parallel `spans` list for a child that named no cells.

`layouts::Table` is the HTML automatic table layout: unequal columns
sized by what is in them, spans, and a surplus shared out in proportion.
It is not a modular grid under another name and it goes through none of
`geometry::arrange` — a module is one size repeated, and no column of a
table is the width of the next. Columns start at the widest child that
sits in one alone; spanning children then top their columns up, narrowest
span first, sharing a deficit in proportion to the widths already found;
and whatever the table is wider than its content is shared the same way,
which is what puts every column of a real page on a fractional pixel.
Rows take the first of those steps and deliberately not the second: the
whole of a rowspan's height deficit lands on the LAST row it covers,
because sharing it in proportion inflates the first row of every span and
drags everything below it down the page. `Table::solve` hands the
resolved column widths, row heights and origins back, so a study
reproducing a published table can print what it resolved and diff it
against what the original measured — numbers no placed rect carries,
since a column nothing fills leaves no trace in the rects at all. `kit/Routers.h` holds the stock connector and
rail routers (`routers::straight`, `orthogonal`, `polyline`,
`octilinear`, `orbit`).

Neither the schemes nor the pool fillers of `kit/Placers.h` derive a ring
or a grid for themselves. Where item i of n falls on a ring, and which
cell of a grid of modules it occupies, are functions of numbers alone —
they belong to SigilGeometry, in `<sigilgeometry/path/Arrange.h>`, and
both shelves step through those bodies. **One arithmetic, one place**: a
ring is one ring whether its items are measured children or sprite
positions in a buffer, and a second spelling would round its own way and
put the same ring a pixel off itself with nothing in either file to say
why. What the shelves keep is the decision on top — which radius per
child, where the anchor of a box is, what closes a run, which pool lanes
a parameter speaks to.

**Marks — `brush/`.** `brush/Decorations.h` has the concrete primitives
that plug the `Decoration` seam — `PathFormat` (stroke formatting) and
`stroke`, its one-line spelling; `Shadow` / `shadow`, the soft drop
shadow; `Slice` (lattice image mapping — its `density` is the source's pixels per
layout unit in the fixed bands, so a frame generated oversized to stay
sharp still draws its corners at the width it was designed for);
`ContourWalk` (walk the outline
and run a program at each sample); `Wash`; `Border`. `brush/Adaptors.h`
runs any of them on another outline than the node's own: `onEdges`,
against only the sub-contours facing chosen box edges, and `inset`,
against a concentric copy of the outline. The brush engine is
three headers: `brush/Layered.h`, the stroke stack (`StrokeLayer`,
`LayeredBrush`); `brush/GeometryOps.h`, the one mechanism door for
deviating an outline (`ops::`, `GeometryOp`); and `brush/Brushes.h`, the
brush kinds over them — `brush::solid`, the composites `brush::layers`
and `brush::weave`, and the archetypes `brush::Scatter`, `brush::Pattern`,
`brush::Ribbon`, `brush::Art`. A ribbon is the variable-width band, built
as one quadrilateral per sampled step rather than as one long contour,
because a band is the UNION of its cross-sections: zipped into a single
left-forward, right-back outline the inner rail crosses itself where the
spine turns hard, the crossing winds the wrong way, and the winding fill
DROPS the inside of the bend — a hole that opens once the band is wider
than about half the leg it turns on, and is then wider than the band.
`Ribbon::join` is what happens on the OUTSIDE of that corner, an
`SkPaint::Join` because it is the same decision a stroke makes: the
chord, the arc, or the point (bevelling past `Ribbon::miterLimit`, which
is also the one join whose bleed reaches past the width). `Ribbon::band`
hands that geometry back, so a study that MEASURES what was drawn does
not have to transcribe how it is built. `Ribbon::fillMaterial` paints the
band with a recipe instead of a `Fill` — the door `strokeMaterial` opens
on a stroke, mirrored here, so a ribbon beside a stroked outline does not
have to have the same paint written twice; `brush::taper` and
`brush::calligraphic` each take a `material::skia::Paint` beside a
`Fill`, and a live material declares the ribbon animated. The line vocabulary is three
more:
`brush/Lines.h`, the cartography and diagram stroke (`lines::Line` —
parallel casings, terminal caps, ties, waves); `brush/Rails.h`, N-rail
strokes where every rail is its own line; and `brush/Hatches.h`, the
parallel, radial and concentric hatches. `kit/Strokes.h` and
`kit/Plate.h` ship with this tier because they are spelled in its types.

**Fills.** The paint vocabulary is SigilMaterial's and is spelled there:
`material::skia::Paint` is what `Element::fill` takes, and
`material::sdf`, `material::pattern` and `material::field` are where the
signed-distance surfaces, the tiles and the fields come from.
`brush/LayerStyles.h` is the Photoshop route to rich surfaces — the
MECHANISMS: bevels, sheens, inner shadows, outer glows and overlays built
from gradients and blurs rather than shaders. The LOOKS they are bundled
into are the kit's, one era per header: `kit/Gel.h`, `kit/Chrome.h`,
`kit/Gloss.h`, each over SigilMaterial's colour tables.
`brush/PixelStyles.h` is the other route, the bitmap era's — strokes and
rectangles on the pixel lattice, never a blur: `styles::BevelPair`, a
light edge and a dark edge kept inside the silhouette, raised or sunken
as one value (`styles::bevelPair` states the two tones or derives them
from the face); `styles::Brackets`, the reticle's L's standing off a box
at the corners asked for; `styles::TickRail`, a ruler of marks along one
edge with every n-th one long; and `styles::Scanlines`, hard rows over
the outline through a blend mode.
`core/Pattern.h` adds the one thing a tile cannot do for itself — an
element tree AS the tile, baked through `snapshot()`. A recipe instance
becomes a paint through `material::skia::Paint::recipe`, an effect
through `material::skia::Effect::recipe`, and an output-stage view
transform for
`Composer::setView` is SigilMaterial's colour transform, compiled only
when the build finds OpenColorIO.

**Type — `typography/`.** The text vocabulary is this feature's, one
header per value family: `typography/Units.h` — `Unit` and `unit::`, the
granularity a selector slices and a cascade beats over, and `TextUnit`,
one unit as the layout placed it; `typography/Selector.h` — `Selector`
and `sel::`; `typography/TextEffect.h` — `GlyphInfo`, `GlyphMod`,
`TextEffect`, `Phase`, and the effects the runtime evaluates by
structure: `fx::scramble`, the `fx::keys` keyframe table, the `fx::pass`
shader pass, the `fx::seq`, `fx::mix` and `fx::hold` combinators, and the
`fx::effect` door; `typography/Track.h` — `Track`, `Beats` and `Beat`;
`typography/RichText.h` — `rich` / `RichText` and `Story`;
`typography/Annotation.h` — `Annotation`; `typography/TextPath.h` —
`TextPath`; and `typography/Typography.h`, the umbrella over them. The
kernel describes its text leaf in this vocabulary — a description stores
tracks, runs and readings — and every member it stores, compares or
evaluates is defined in the header that declares it, so the kernel links
no engine to do so; what the feature's archive holds is the members that
carry a diagnostic, the mixed-text builders, and the engine behind
dressed type. The stock effects over the seam are stock values, and so
the kit's — `kit/Kinetic.h`, below. A text verb takes this
vocabulary and a text query answers in it, and the kernel's own headers
only name it: a call site that dresses its type, or reads a beat or a
unit back, includes the typography header that spells the value. A
style's own numbers are SigilWeave's: `weave::textStyle` builds a
`weave::TextStyle` from the designated-init `weave::Type`
(`<sigilweave/style/Type.h>`), and `weave::ports::pickTypeface` resolves the
first installed family of a fallback chain
(`<sigilweave/ports/SystemFontManager.h>`). `kit/Legibility.h` ships with
this tier.

**Leaves with their own targets.** `video/Video.h` makes a streaming
`SigilVideo` clip a live leaf. `video(clip)` takes its intrinsic dimensions
from the encoded frame, samples presentation time from the composer's motion
clock, and disables picture caching while the clip's own decoded-frame cache
stays active. On a Graphite canvas the leaf passes its recorder to the video
device executor, so a native YUV frame remains on the GPU through composition;
the compose kernel links no codec. `VideoFit::Cover`, `VideoFit::Contain` and
`VideoFit::Stretch` state how the decoded frame meets its box. The leaf's
`VideoOptions` also carries opacity and blend mode into its single image draw,
so an additive black-backed effect does not need a grouping layer.
`video(clip, playback)` is the many-video form: share one playback scheduler
across the scene so decode work is coalesced on a bounded worker pool and no
leaf waits for its decoder during paint; the clip registers with the
scheduler once, so describing the scene again reuses its handle. Passing that
handle explicitly to several video leaves fans one decoded frame out to
several compositions. `web/Web.h` makes a live
Ultralight page a leaf; it is a header-only adapter and the library does
not link SigilScry, so include it only in targets that do.
`texture/Texture.h` is the door OUT of this library: a scene painted into
a surface and handed over as a SigilMaterial texture value, in its own
target `SigilComposeTexture` — see Boundaries. `draw/Draw.h` is the door
to the imperative pen, both ways, in its own target `SigilComposeDraw`:
`compose::pen` takes a `PenProgram` — a function of a `draw::Pen` — and
makes the node `custom()` would, at `Cache::None`, with the pen's width
and height the node's box and its transform starting at the box's
corner, so a declarative scene drops into p5's verbs for one node; and
`compose::paintRetained` is where the pen's `element(...)` lands, an
`Element` painted inside an imperative loop and RETAINED — reconciled
against what the composer kept for that call site, so its layout, its
shaping, its caches and its bindings carry from frame to frame.

**Testing — `testing/Checks.h`.** A separate target, `SigilComposeTesting`,
whose one header verifies generated geometry and reads back what was
drawn, in `namespace test` (GoogleTest owns `::testing`): `test::coverage`, `test::widthAlong`, `test::endpointDegrees`,
`test::rasterize` and the feed `test::report`. The checks a plate
reports — `measure::check` and `measure::failures`, with
`measure::finding`, `measure::reading` and `measure::heading` for the
rows that stand beside claims, and `measure::Table` for the run of them
— are SigilMeasure's, spelled under its own name from
`<sigilmeasure/check/Check.h>`; only the geometry readers and
`test::report` are this library's. `test::widthAlong` is the width
question `test::coverage` cannot answer: the shortest chord of a drawn
band through each station of its spine, against the
`geometry::path::Profile` the band claims. Total ink is the cheap version
and is blind to a corner defect — a band that loses the inside of a bend
and gains an outer chord loses and gains nearly the same area, so the sum
agrees while the picture is torn — and a width is a LOCAL property only a
local measurement finds. The chord it takes is of the band's FILLED
REGION: every crossing along the ray is kept, sorted, and the fill rule
accumulated through them, so a shared seam — two coincident edges of
opposite sense — cancels and is not a boundary. Resolving the union into
an outline first would not do: the outline of a run of hundreds of
overlapping steps walks in and out along the interior seams, enclosing no
area and carrying edges all the same, and the shortest chord lands on one
of those excursions. It skips half a width at each end,
where the shortest chord through a point runs out through the cap rather
than across the band. `test::report` writes one check or a
whole `measure::Table` into a `feed::TextRing`, each row in the ink its
standing and verdict choose from a `test::ReportStyles` — the pass, fail,
finding, reading and heading names a plate's tinted set registers — so
the verification block of a study is one table, printed as it runs, and
no verdict is ever typed into a row's text. Test
binaries link it, and so does the sketch library, so a sketch can report
its own checks; nothing that ships does, which is what keeps a
point-sampled coverage scan out of a paint loop.

**Kit — `kit/Kit.h`.** A tier above the library that adds no kernel state
and no new equality: `kit::disc` (a node about a centre, at a radius or
at a `geometry::path::Frame`'s — a braced pair is the centre, and a
frame is spelled as one) and `kit::at` (a box pinned at absolute
coordinates, for the plate that has no layout at all), `kit::dotSprite`
(the round stamp a point sink draws each point with),
`kit::PixFont` (aliased bitmap-font bakes), `kit::Scrim` and the
halo/shade legibility helpers, the stock text effects over the
`Element::fx` seam in `kit/Kinetic.h` — `fx::rise`, `fx::slide`,
`fx::pop`, `fx::spinIn`, `fx::typeOn`, `fx::waveLoop`, `fx::scatter`,
`fx::variableAxisSweep` and `fx::tint`, each a comparable `TextEffect`
built from the constructor any caller may use — with `kit::marquee`,
the seamless ticker built from a clipped strip and a wrapping phase,
`kit/Placers.h`'s `place::grid`, `place::ring` and `place::repeat`, the
fillers of an instanced leaf's pool — the first two over the same ring
and grid arithmetic the layout schemes use, which is SigilGeometry's —
the two instruments for text in motion —
`kit::trackMeter` (a cascade's schedule drawn, one cell per beat at its
rect, filled by its local time — `MeterPlacement` stands the cells over
the beats or under them as a rule, for a track whose own letters are
what is being watched) and `kit::restGhost` (the same word
undeformed under the moving one) — the furniture of a specimen sheet in
`kit/Specimen.h` — `kit::cell`, a body with a label and a note set
beside it as a `kit::Caption` says (`Caption::Where` puts the note under
the body, or both lines above it, or both below, and `labelMeasure` and
`noteMeasure` wrap either line at a stated width so a long one does not
widen the cell it captions), `kit::well`, the fixed, clipped surface a
specimen is drawn into with every size, fill and padding supplied by the
caller, `kit::format`, the dynamically sized printf-style reading those
captions use, `kit::cells`, a run of
them along one axis with a hairline between neighbours, and
`kit::sheet`, the titled and footed page that rules its header and
footer off from the content between them; every face, size and distance
is the `Caption`'s and the `Sheet`'s, so the kit decides no look — and,
shipped with the tiers whose
types they are spelled in, `kit/Strokes.h`'s braid, bracket spans, brush
presets and `kit::groove` — the engraved cut across a disc's stroke, a
radial ramp concentric with the circle so it is dark on the inner wall
and lit on the outer, as the comparable `kit::grooveRamp` paint or the
`PathFormat` that wears it — and `kit/Plate.h`'s bordered feed plate
(Brush), and
`kit/Legibility.h` (Typography). The kit
is a **separate CMake library** (`SigilComposeKit`) whose only include
path is compose's public headers, which is how the public/internal
boundary is proven rather than asserted. Note that `kit/Kit.h` does not
pull in the headers shipped with other tiers; include them directly.

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
- A `material::skia::Paint` that reads `uTime` or carries a uniform bound
  to an `Output` is live by construction and declares itself; so is a
  `material::skia::Effect` with a bound uniform or a live child. Tier
  inheritance is real: a live child makes the parent effect live, so no
  cache can freeze the parameter.
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
  a raster test cannot see it. Use SigilSkia's `skia::draw::drawLattice`
  and `skia::draw::drawSpriteAtlas`
  (`<sigilskia/draw/Direct.h>`), which decompose on every backend and
  never emit the native op. This is not an optimisation layer; it is the
  only correct path.
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
`geometry::shapes::` generator is a comparable value.

This matters more than it sounds. An inherited value carried through
`core::env::` that holds a `std::function` is incomparable, and that turns every
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
outside a clockwise path. `bandPointAt`, `TextPath::offset`, the
`lines::` family and every signed distance in `geometry::path` — a
`Profile`'s `across`, `geometry::path::profile::offset`,
`geometry::path::parallel` — all mean the same side. Relatedly, **fraction 0 on a boundary is the bottom-left
corner**, running up the left edge — so `spans::upTo(0.25f)` on a square is
the left edge, not the top one.

---

## Boundaries

The kernel links `SigilCoreReconcile`, `SigilCoreCache`,
`SigilCoreComparable`, `SigilCoreCompute`, `SigilGeometryPath`,
`SigilImage`, `SigilMaterial`, `SigilMeasure`, `SigilMotion`,
`SigilSkiaDraw` (the direct draws the instanced leaf stamps through),
`SigilWeave` and Skia publicly, and Yoga privately. The brush tier adds
`SigilGeometryKit`, the silhouette shelf a brush is applied to; the kit
tier links the brush tier — the arrow between
those two points one way. Each tier also names, on its own link line,
every library its headers include, so no tier reaches a library through
the kernel's.

**What compose IS, after all of those: the element runtime.** It
reconciles a description against a retained tree, lays it out, paints it
in a stated stacking order, caches what it can prove is still, and holds
the text element and the marks that dress an outline. What it does not
hold is any of the four vocabularies it draws with. A silhouette, a width
law, a deviation, a band, a crossing and a figure's coordinate frame are
`geometry::`; a paint, a post-processing effect, a signed-distance
surface, a tile and a field are `material::`; a style, a face and a
paragraph are `weave::`; an animatable, a transition and a cascade are
`motion::`. Each is spelled at its own origin here — compose re-exports
none of them. `SigilCoreReconcile` is the reconciler: the
keyed and positional match, the memo, the identity prune, the
`core::env::` channel and the animation lane operations are its, and `Composer` is its
host — the description comparators, Yoga, text and paint stay here.
`SigilCoreCache` is the caching kernel, and `Composer` is its host too:
the three-valued cache policy (`cachePolicy` maps this library's
five-valued `Cache` onto it, keeping the TIER — picture, texture,
group — on this side), the fold that turns one node's declarations and
its children's verdicts into what a subtree promises, the stability
release that proves a node declaring volatility is holding still, and the
three-way bake decision are its. What every term MEANS is compose's: which
Skia paint moves pixels off the describe clock, which of its lanes a value
memo can compare, what a recording is and when it may be replayed.
`SigilGeometryPath` supplies the contours, polylines, poses, seeded
noise, width laws, shapers, bands and crossings that every outline walker
here reads through, and compose adds no path geometry of its own. `travel()`'s motion path is the worked example:
the curve is measured into that library's contours once per shape and
size, and each frame's position is one pose read along them, walked as a
single arc-length coordinate. What stays here is the
POLICY the verb states — the fraction wraps on a closed curve and clamps
on an open one, the tangent angle comes from a look-ahead chord, and the
path outranks the translate lanes.

`SigilComposeTexture` is the one feature that owns a SURFACE, and it is
the exception the bullet below states. `compose::TextureScene` keeps a
composer and the surface it paints into, and hands the picture over as a
SigilMaterial texture value: a consumer that samples an image samples
that one with no knowledge that a composer made it, and nothing above has
to learn what an `Element` is. `compose::texture` is the one-shot form,
for a picture described once. The version the value carries counts
PAINTS, not describes — a frame whose reconcile moved nothing leaves the
value equal to the frame before's, which is what lets a consumer prune on
it. The surface is a raster one by default and a texture on a GPU device
when a host hands the scene one, so a renderer standing on that same
device binds the pixels where they were painted rather than copying them.
The arrow points one way: this feature links SigilMaterial's texture
feature, SigilSkia's graphite feature and SigilCore's hardware device,
and nothing that samples the value links compose.

`SigilComposeDraw` is the feature that meets SigilDraw's pen, and the
arrow between the two libraries points one way: this feature links
SigilDraw, and SigilDraw names nothing of compose — the pen reaches a
retained `Element` through a seam it declares for any guest,
`paintRetained`, which this feature defines for `Element` in compose's
own namespace. The clock is whoever steps the pen: a `compose::pen`
node's pen reads the composer's clock through the paint context, and a
retained element's composer runs on a clock stepped by the pen's frame
delta, advancing on the frames it is painted and standing still on the
frames it is not. Neither side reads the wall, which is what keeps a
plate with a pen in it reproducible.

Deliberately *not* linked: SigilVideo and SigilScry (their live leaves are
header-only adapters with their own targets), EnTT (the instancing header
keeps the registry on your side), SigilGeometry beyond the path leaf and
the mesh its silhouette shelf rests on (no camera, curve, point operator,
renderer, codec or device), Diligent, and Qt — Qt identifiers are banned
outright in exported headers.

What it refuses to be:

- **No markup, parser or external DSL.** Markup can only name
  pre-registered values; the vocabulary here is C++ values and callables.
  A serialization schema can be a *producer* of element values, never the
  API.
- **No imperative node mutation.** Describe or bind, and nothing else.
- **No timeline object.** Multi-beat choreography is windowed bindings
  over one phase output (`bind(&phase).window(lo, hi)`).
- **No surface, loop or thread ownership — outside `texture/`.** The
  composer is a guest in someone else's canvas, and a host that wants
  many surfaces makes many composers. `compose::TextureScene` is the one
  place a surface is owned, because a picture another library samples has
  to live somewhere and the alternative is every such consumer writing
  the same three lines.
- **No scene.** The depth lanes are CSS's model over the retained 2D
  tree — a node is a plane, projected onto its parent's — and nothing
  more: planes never intersect, nothing is lit or cast, and a depth is
  not a position in a world. A camera over the whole picture is still
  the host's matrix on the canvas — a recording is matrix-independent
  by construction, so a moving camera invalidates nothing the library
  holds — and the places that pin pixels to a device rect refuse a
  perspective matrix explicitly, the host's or a plane's own.
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

The library is one feature target per directory, and a consumer links the
tier it draws with: `SigilComposeCore` (`core/` — the kernel: elements,
layout, paint, transitions, text, the feed and the instanced leaf, as
the host of SigilCore's reconciler),
`SigilComposeTypography` (`typography/` — the text vocabulary and the
engine behind dressed type), `SigilComposeBrush`
(`brush/` — decorations, lines, brushes, the stroke grammar's engine and
the mask gates, with `kit/Strokes.h` and `kit/Plate.h`),
`SigilComposeTexture` (`texture/` — a scene
painted into a surface and handed out as a texture value),
`SigilComposeVideo` (`video/` — a streaming SigilVideo clip sampled from the
motion clock),
`SigilComposeWeb` (`web/` — header-only, present only with SigilScry),
`SigilComposeDraw` (`draw/` — the door to SigilDraw's pen, both ways),
`SigilComposeTesting` (`testing/`) and `SigilComposeKit` (`kit/` — the
shelves: the silhouette catalog spelled for a node, the layout schemes,
the routers and the kinetic type presets). Each directory holds the target's sources,
its internal headers, its `test/` and its `bench/`; the public headers
sit under `include/sigilcompose/<feature>/`. A harness several features
compose against belongs to none of them, so the shared ones sit at the
library root: `test/support/`, `test/assets/` and `bench/BenchSupport.h`. Every consumer in this
repository — SigilSketch, the benches and the tests — links the
feature targets it draws with by name, so a dependency on a tier is a
stated fact.
`SigilCompose` remains as the whole-library name for a consumer outside
this tree, the way `SigilWeave`, `SigilMotion` and `SigilGeometry` each
keep one: it is Kit, Brush and Typography, which between them reach
Core, never the web leaf, and nothing here
links it. From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Registered tests, one binary per feature target so that each links only
the target it exercises and a test reaching past its tier fails to link:
`compose_core_test` (the kernel — elements, the reconciler, layout, paint,
transitions, text, the feed, the instanced leaf, masks and the field
walks; links `SigilComposeCore` alone), `compose_shape_test`
(silhouettes, layouts, routers, placers, rails, travel),
`compose_text_test` (text data, the text pass,
vertical writing, motion along paths, the text-fx presets, rich spans),
`compose_brush_test` (decorations, lines, brushes, the stroke grammar,
the kit's stroke presets), `compose_paint_test` (patterns, SDF materials,
layer styles, colour management), `compose_kit_test` and
`compose_studio_test` (the kit, and the queries, the studio and the
instruments over it), `compose_texture_test` (textures as element
content), `compose_draw_test` (a pen program hosted in a node),
`compose_video_test` (video frames as element content),
`compose_spike_test` (the Yoga+SigilWeave
measurement contract, with `core/`), and the library's own:
`compose_docs_test` (the engine walkthroughs and the generated probes
over this page and `TYPOGRAPHY.md`) and `compose_api_doc_probes_self_test`,
plus `compose_gpu_test` (Apple only, needs the Graphite plumbing) and
`compose_web_test` (needs the Ultralight SDK). Each binary's translation
units share `test/support/Host.h` — the composer-in-a-raster-surface
harness — through a support header of their own that includes only what
they use. The benchmarks are executables, not tests.
There is one benchmark binary per tier — `compose_core_bench`,
`compose_shape_bench`, `compose_brush_bench`, `compose_paint_bench`,
`compose_text_bench`, `compose_texture_bench`, `compose_draw_bench` —
each in its feature's `bench/` over the shared
`bench/BenchSupport.h`, linking only the library it measures, all built
by the `benches` target and run by `scripts/bench_ledger.py`; anything
resembling a performance claim belongs to them and to the plate ledger,
never to prose.

**Looking at any of it** goes through SigilSketch, which is where every
renderable thing in this repository lives: one file per scene, one
registry, one application (Sketchbook) and one headless renderer.
`src/sketch/README.md` is the canon for it — how a sketch is written and
registered, how the live host reloads one, and how the plates a
byte-identity sweep hashes are made. Nothing here hosts a catalogue of
its own.

### The generated doc-probe translation unit

`compose_docs_test` builds a C++ file that does not exist in the source tree.
`test/docs/api_doc_probes.py` reads this document AND `TYPOGRAPHY.md` —
both are the library's canon, and prose nobody compiles is prose that goes
stale — extracts every qualified name an author could copy out of them — from fenced code blocks **and** from
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
