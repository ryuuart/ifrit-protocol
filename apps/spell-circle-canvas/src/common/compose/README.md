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

**`reference/` is the catalogue.** This page is the model — the phases,
the write paths, the boundaries — and the chapters beside it carry the
rest of it: the cascade, the depth lanes, the header map, the caching
contract and the traps, each linked from the section it was written
under. Beside them,
`reference/ELEMENTS.md` lists every factory that starts a tree,
`reference/VERBS.md` every verb an Element takes in fourteen concern
groups, and `reference/VALUES.md` what those verbs accept and where one
comes from, with a page and a drawn example per entity under
`reference/pages/`.

**`TYPOGRAPHY.md` is the type chapter.** Everything a passage of type can
be told past `text(utf8, style)` — the per-glyph fx tracks, a run on a
path, span restyling, the paragraph controls, threaded frames over a
`weave::Story`, readings beside the type, a passage whose measure moves, and
vertical CJK — is indexed there, one chapter under `reference/` apiece,
and every one of them is checked against the headers by the same probe
this page is.

---

## Writing a document

`sigilcompose/kit/Document.h` supplies content components in
`sigil::compose::document`. They return ordinary Elements with semantic
roles and a default type hierarchy. A document can style every heading,
paragraph or caption through one inherited `weave::StyleSheet`:

```cpp
#include <sigilcompose/kit/Document.h>
#include <sigilweave/layout/StyleSheet.h>

namespace document = sigil::compose::document;
namespace weave = sigil::weave;

auto content = document::article({
    document::eyebrow("FIELD NOTES"),
    document::h1("A document has a voice"),
    document::lead("The content describes its purpose; the sheet chooses its look."),
    document::section({
        document::h2("One rule, every paragraph"),
        document::paragraph("This paragraph inherits the document's face and ink."),
        document::quote("A quoted passage keeps its own semantic role."),
        document::list({document::item("Headings establish the hierarchy."),
                   document::item("Captions stay beside the material they describe.")}),
    }),
    document::footer("An ordinary Compose tree, ready for a window or a snapshot."),
}).styleSheet({
    weave::rule("h1").font({.size = 36}),
    weave::rule("h2").font({.size = 24}),
    weave::rule("paragraph").block({.leading = weave::Leading::multiple(1.5f)}),
    weave::rule("caption").font({.size = 12}),
});
```

The roles are `article`, `section`, `h1` through `h6`, `paragraph`, `lead`,
`caption`, `label`, `eyebrow`, `footer`, `code`, `quote`, `list`, `item`,
`marker`, `figure` and `rule`. `heading(level, words)` accepts levels 1–6.
`paragraph` also accepts a `weave::RichText`; inline runs stay in one shaped
passage. `figure(body, note)` keeps a body and its caption together, and
`item(body, marker)` accepts composed content for nested lists. Container
factories take children directly or through the usual `children()` call.

Roles are independent of class membership; both look up stylesheet rules by
name. Resolution at each node is inherited
type and block, then role defaults, the stylesheet rule matching the role,
authored classes, and finally direct `font()` and `block()` declarations.
A later `styleClass("warning")` therefore keeps the element's paragraph
role; the warning rule overrides only what it states. Relative sizes are
resolved once against the inherited font. Content built before its parent
still adopts that parent's rules, including after a retained theme update.

`Element::role` is the underlying seam. It accepts a name or a `weave::Rule`
that supplies the role's fallback type and block. Missing role rules are
normal: the fallback remains in force. An unknown authored class still
reports a missing rule. Rules use exact names; there is no selector-string
parser or combinator matching.

The document's layout uses inherited length properties: `document::measure`
is the maximum article width, `document::gap` separates content blocks,
`document::listGap` separates list items and a figure's caption, and
`document::quoteInset` indents quotations. For example:

```cpp
content.var(document::measure, sigil::compose::Dimension(640))
       .var(document::gap, sigil::compose::Dimension(18));
```

The stock measure is 38 em and the flow gap is 1 em. These are configurable
defaults, not a character-count guarantee; typefaces have different widths.
An article also fits the width available from its parent. The defaults are
carried through `Element::varDefaults`, below inherited and directly stated
properties, so an outer theme can restyle components made elsewhere.
Explicit zero lengths and direct layout overrides remain meaningful.
Typography rules belong to SigilWeave; layout and document components belong
to Compose. Existing paragraph, story, writing-mode and exclusion controls
remain available on these Elements.

The specimen, page, panel and caption kits use this document vocabulary for
their content. Their geometry still belongs to their own layout components;
an existing specimen well does not become a prose column.

---

## Writing a component

A component is a free function from your data to an `Element`. There is no
base class, no lifecycle, and no state inside the library — the state is
the argument.

```cpp
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Transition.h>

#include <ranges>
#include <vector>

// Compose re-exports nothing: the motion words (`animate`, `to`,
// `Transition`, `bind`) are SigilMotion's and the text style is
// SigilWeave's, each spelled from its own library.
using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

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
      c.alarm ? hexColor(0xff5252) : hexColor(0x8fd0ff);
  return box()
      .row()
      .gap(10)
      .padding(12)
      .borderRadius({6})
      .fill(hexColor(0x0e1218))
      .alignItems(Align::Center)
      // A mark on part of the boundary: L-brackets at every tangent break.
      .stroke(spans::corners(12), stroke(1.5f, Fill::color(ink)))
      .children({text(c.label, weave::textStyle({.size = 13, .color = ink})), box()
                 .flexGrow()
                 .height(6)
                 .fill(ink)
                 .transformOrigin(pct(0), pct(50))
                 // The bar ramps because the DESCRIBED value moved. Nobody
                 // steps it; the reconciler sees the change and starts a
                 // motion that retargets from wherever the bar is now.
                 .scaleX(animate(to(c.level), Transition{.duration = 220ms}))});
}

Element dashboard(const std::vector<Channel> &channels) {
  return box()
      .column()
      .gap(8)
      .padding(24)
      .fill(hexColor(0x05070a))
      .children(channels | std::views::transform([](const Channel &c) {
                  // memo() skips the describe call entirely while the properties
                  // compare equal. key() is what the reconciler matches on
                  // across describes, so rows survive reordering.
                  return memo(c, meter).key(c.id);
                }));
}
```

The children are one block after the verbs. `children({…})` takes what
is in the node, in order — an element, or the list `each(range, make)`
builds from a range, mixed as they come — so braces on a description
mean children and nothing else:

```cpp
box().column().gap(8).children({
    heading(),
    each(channels, [](const Channel &c) { return memo(c, meter).key(c.id); }),
    footer(),
});
```

A range goes into `children()` as it stands. An element that needs an
identity of its own keys itself; the rest reconcile by position, as
unkeyed siblings do.

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
const bool again = moving || composer.active();
```

`Composer::active()` is the whole gate: it answers `dirty()` — a
description or a layout that changed — and, beyond it, whether a motion
is running or a retained binding can still move without another
`render()`, which is the one thing a host polling `dirty()` alone would
miss on a scene driven from outside.

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
compares its captured environment snapshot and then the author's properties
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
`Text::contentFlowAround` subtracts WHAT THE TARGET SAYS ITS EDGE IS,
which is the one property the target already carries for its own decorations:
`Element::decorationOutline`. Its glyph outlines under `Boundary::Glyphs`, so text
flows around a word; the silhouette of what it DREW under
`Boundary::Coverage`, at the coverage the same verb stated, so text
flows around a photograph's alpha, a clipped subtree or a masked node; its
`shape()`, routed connector or rail otherwise, so text runs into a star's
notches and through an annulus; and its BOX when it declares none. One
reading serves both, so a node cannot be dressed along one outline and
flowed around along another. A round silhouette is subtracted analytically.

The margin is a DISC — the set of points within that distance of the edge —
so a diagonal stands the text off by exactly what was asked and a corner
comes out round; it means the same in every case, and so does the writing
mode: a column a target crosses is cut into a head and a foot exactly as a
line is shortened beside it.

Every derivation DECLARES WHAT IT READS, in the same statement that stores the
key: `contentFlowAround`, `spans::fit`, `strand::from`, `band` around a key,
`connector`, `rail` and `textThreadTo` each record a `sigil::core::Read` — the
node waited for, and which `sigil::core::Facet` of it is needed (a box, an
outline, or the units a text produces). `sigil::core::orderByReads` turns those
declarations into the order the derived nodes are resolved in, so a rail
anchored on a connector written after it, or a frame threaded from a frame
written later, settles in the same pass instead of one behind. It is stable:
derivations that read none of each other are resolved in exactly the order they
were written in, which is nearly every tree. Nothing infers an edge from which
fields a node carries, so a derivation added later is ordered by its own
declaration and by no list that has to be found and extended.

Released scalars are scanned and volatility computed in one walk. Then
paint runs, selecting a cache tier per node.

### Paint order inside a node

Fixed, and worth memorising, because several traps are just this list:

```
backgrounds · background span passes │ fill · echoes │ overlays │
content leaf │ children │ foregrounds · foreground span passes
```

Decorations dress the node's *outline*, so `overflow(Overflow::Clip)` does not clip them —
it bounds the fill, the content leaf and the children. A stacking context
forms on `zIndex`, opacity below 1, a blend mode, a transform, a clip, or a
layer effect, and children cannot interleave outside it: a component cannot
escape the z-order of the site it was composed into. The one order that is not
tree order is a shared space's: the children of a node that opens one
are painted back to front by depth, whatever order they are declared
in.

### Type

`text(utf8)`, set in the font in force where it lands, `text(utf8, style)`,
set in one whole style, and `text(weave::rich(base).add(…))` are the three
content forms — the text a `std::u8string` or a plain string holding UTF-8,
a literal either way — and everything a passage can be told past that — the per-glyph fx
tracks and their selectors, a run riding a path, span restyling, the
paragraph controls, threaded frames over a `weave::Story`, readings set beside
the type, a passage whose measure moves, and vertical CJK columns — is in
**`TYPOGRAPHY.md`**, one file over, and the chapters it indexes. They are
checked against the headers by the same probe this page is.

The shape of it in one paragraph: a text leaf holds an ordered list of
`fx()` TRACKS, each `(selector, effect, stagger, progress)` — which
glyphs, what deviation from rest, how their start times spread, what
drives it — and the same `selectors::` vocabulary addresses glyphs for a track,
characters for a `spanStyle`, and units for anything standing beside the
passage. What a passage is SET like is `Text::paragraphStyles` and the
layout setters beside it, which map onto
`sigil::weave::ParagraphLayoutOptions` field by field.

### What a decoration dresses

Every decoration is drawn ACROSS AN OUTLINE, and the outline a node hands
its decorations has always been its own shape — which on a text leaf is a
rectangle, and is why a chrome style on a word bevelled a slab behind the
word. `Element::decorationOutline` says otherwise:

```cpp
text(u8"CHROME", display).decorationOutline(Boundary::Glyphs).layerStyle(kit::y2kChrome());
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
image(logo).decorationOutline(Boundary::Coverage).layerStyle(kit::y2kChrome());
image(photo).key("fig").decorationOutline(Boundary::Coverage, 0.35f);
text(body, bodyStyle).contentFlowAround("fig", 12);
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
  for: past it the raster is scaled down to fit and the steps grow. So a
  RECORDING THAT HOLDS ONE IS PINNED TO THAT SCALE: a picture replays
  under whatever matrix it meets, which is sound for every other op in it,
  and a traced boundary is one of the two answers inside that belong to
  the scale it was taken at. A recording carries the WINDOW of host scales
  everything in it is the same picture over — narrowed by its own nodes'
  rasters and by those of every held picture replayed into it — and is
  remade when the host leaves it, exactly as a recording holding a device
  blit is remade when its matrix moves. A trace narrows that window to a
  point, because one step per device pixel is as fine as a grid gets.
- **How much paint counts as ink is a dial.** A pixel joins the boundary
  when the node's paint reached the coverage `Element::decorationOutline` stated, a
  fraction of full opacity. The default is half — the rule an unantialiased rasteriser
  uses, which puts the traced edge where the drawn edge is — so a 30% wash
  traces to nothing and its decorations have nothing to dress. Lower it and
  the wash becomes silhouette; raise it and only the solid core does. It is
  what a soft-edged photograph needs, and text flowing around that node
  reads the same number.

The node's OWN marks are not in the trace — they are what dresses it, and
a mark that dressed itself would have no fixed point — while its fill, its
content, its children and their marks are. WHAT THE RASTER COVERS IS THE
NODE'S PAINT BOUNDS, not its box: the silhouette is the ink, so a declared
shape resolved past the box it was handed, a decoration's bleed, a glyph's
overhang and a routed path are all inside the traced surface and the
boundary is never cut square at the box's edge. The raster's grid is
placed on whole steps of the trace's own scale, so the pixels covering the
box are the same pixels whichever carrier widened the rect around them,
and the path comes back in the node's own space either way. A node that traced to nothing
keeps its shape, exactly as a text leaf with no glyph outline does. The
trace is re-run when the node's rendered layer is invalidated, which for a
volatile subtree is every frame.

`Boundary::Auto` is what a node that says nothing gets and means its own
shape: a caption with a drop shadow means the caption's box, and neither a
text leaf nor an image silently changes what it has always meant.

## 3D, the CSS way

The five depth lanes, the shared space `Element::preserve3d` opens, the
backface, and how a hit test goes back through the projection are in
[reference/DEPTH.md](reference/DEPTH.md).

---

## The cascade

What flows down the tree — the font, the ink, the block, the sheet and
the custom properties — and how a role or a class resolves against a
sheet are in [reference/CASCADE.md](reference/CASCADE.md).

---

## The header map

Every public header, feature by feature, with the names it owns, is in
[reference/HEADERS.md](reference/HEADERS.md).

---

## The declared-volatility contract

What a value that changes without a re-describe must declare, and what
a promoted node promises the live paint, are in
[reference/CACHING.md](reference/CACHING.md).

---

## Traps

The silent no-ops, the lifetime and pruning rules and the ordering
contracts are in [reference/TRAPS.md](reference/TRAPS.md).

---

## Boundaries

The kernel links `SigilCoreReconcile`, `SigilCoreCache`,
`SigilCoreComparable`, `SigilCoreCompute`, `SigilGeometryPath`,
`SigilImage`, `SigilMaterial`, `SigilMeasure`, `SigilMotion`,
`SigilSkiaDraw` (the direct draws the instanced leaf stamps through),
`SigilWeave` and Skia publicly, and Yoga and Boost's container and
unordered targets privately. The brush tier adds `SigilGeometryKit`, the
silhouette shelf a brush is applied to, and the typography tier, whose
vocabulary its text decorations are spelled in; the kit tier links the
brush tier — the arrow between those two points one way. Each tier also names, on its own link line,
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
`core::environment::` channel and the animation lane operations are its, and `Composer` is its
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

**A NUMBER DRAWN AGAINST A FRAME WITH SCALES is not here either.** A
domain, a range and the transform between them are ONE mapping value, and
that value is SigilData's, which no tier of this library links: a plot
whose two axes are those mappings is built where SigilData is in reach,
and what this library supplies it is the keyed recording, the layout
scheme a mark is placed by and the cascade the mark's colour is read out
of. What stays here is the READING of a column that states no scale at
all — `kit::bars`, N rows against an extent derived from the values,
which is a row reading that sizes itself from its content and needs no
box to be given.

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
own namespace. The clock is whoever steps the pen: a `compose::pen` or
`compose::graphics` node's pen reads the composer's clock through the
paint context, and a retained element's composer runs on a clock stepped
by the pen's frame delta, advancing on the frames it is painted and
standing still on the frames it is not. Neither side reads the wall,
which is what keeps a plate with a pen in it reproducible. The cascade
crosses in both directions too: a node's ink and resolved type seed the
pen it hosts, and a retained element is seeded from the pen's own
inherited pair.

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

[docs/overview/testing.md](../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under: one
`compose_test` over every feature's `test/` and one `compose_bench` over
every feature's `bench/`, ctest one entry per CASE, what a case may pin,
and what a label promises. What is only true of SigilCompose:

**The library is one feature target per directory, and a consumer links
the tier it draws with**: `SigilComposeCore` (`core/` — the kernel:
elements, layout, paint, transitions, text, the feed and the instanced
leaf, as the host of SigilCore's reconciler), `SigilComposeTypography`
(`typography/` — the text vocabulary and the engine behind dressed
type), `SigilComposeBrush` (`brush/` — decorations, lines, brushes, the
stroke grammar's engine and the mask gates, with `kit/Flourish.h`,
`kit/Ornament.h`, `kit/Plate.h` and `kit/Strokes.h`),
`SigilComposeTexture` (`texture/` — a scene painted into a surface and
handed out as a texture value), `SigilComposeVideo` (`video/` — a
streaming SigilVideo clip sampled from the motion clock),
`SigilComposeWeb` (`web/` — header-only, present only with SigilScry),
`SigilComposeDraw` (`draw/` — the door to SigilDraw's pen, both ways),
`SigilComposeTesting` (`testing/`) and `SigilComposeKit` (`kit/` — the
shelves: the silhouette catalog spelled for a node, the layout schemes
and the grid, the routers, the placers, the typesetting furniture and
the kinetic type presets). Each directory holds the target's sources,
its internal headers, its `test/` and its `bench/`; the public headers
sit under `include/sigilcompose/<feature>/`. A harness several features
compose against belongs to none of them, so the shared ones sit at the
library root: `test/support/`, `test/assets/` and `bench/BenchSupport.h`.
`SigilCompose` remains as the whole-library name for a consumer outside
this tree, the way `SigilWeave`, `SigilMotion` and `SigilGeometry` each
keep one: it is Kit, Brush and Typography, which between them reach
Core, never the web leaf. The sketch library links it — a sketch draws
with the whole vocabulary and names no tier — and every other consumer
here names the feature targets it draws with.

Where a suite sits is what locates it. The kernel's are in `core/test/`
(elements, the reconciler, layout, paint, transitions, text at rest, the
feed, the instanced leaf and the shelf it packs on, masks, the depth
lanes and the shared space, tethers and the field walks); the text
engine's in `typography/test/` (text data, the text pass, vertical
writing, motion along paths, the paragraph controls, rich spans, the
variation drive); the stroke and decoration engine's in `brush/test/`
(decorations on shapes and on type, lines, the brush kinds and the
engine under them, the stroke grammar, stamps and strips, the mask
gates, the paint values this tier spells over SigilMaterial, the pixel
styles and the kit's stroke presets); the kit's in `kit/test/` (the
kit's own values, the grid, columns of one story, silhouettes and layout
schemes, routers, placers, pixel art and its sheet, travel, and the
queries, studio and instruments over them); and one apiece in
`texture/test/` (textures as element content), `draw/test/` (a pen
program hosted in a node), `video/test/` (video frames as element
content) and `web/test/` (the Ultralight leaf, present only where the
SDK was found). The library's own sit at the root: the generated probes
over this page, `TYPOGRAPHY.md` and every chapter under `reference/`,
and the GPU read-backs. `compose_header_self_test` stands beside them —
every public header compiled first and alone, which is what makes "each
header stands on its own" a build fact rather than a claim.

The translation units share `test/support/Host.h` — the
composer-in-a-raster-surface harness — through a support header of their
own that includes only what they use. Committed test assets sit in
`test/assets/`, and the faces more than one library asks of are the
tree's, reached as `sigil::test::instrument::variable()` and its
siblings.

Labels are attached to the cases that need them rather than to the
binary: `gpu` on `ComposeGpu`, `DirectImageDraw` and `ComposeTexture`,
`ultralight` on `ComposeWeb`, and `fonts` on the two that ask the
MACHINE for a face — the vertical suite, whose Japanese prose needs a
whole CJK family, and the one case that asks the installed italics
whether their ink overhangs the advance. Every other case sets its faces
from the instruments this repository ships, so it answers the same on
any machine.

`compose_bench`'s arms sit in each feature's `bench/` over the shared
`bench/BenchSupport.h`. **A claim about how a cost GROWS lives there
rather than in a ctest wall-clock ceiling**, because a single size
cannot show a rate.

**Which node in a scene is slow** is a different question, and the
painter answers it two ways. `Composer::setProfiling` fills
`Composer::profile` with one row per node — its label, its total and self
milliseconds, the cache tier it took and the promotion verdict — for a
host that wants the table. `COMPOSE_PROF=<ms>` in the environment needs
no host at all: every draw over that many milliseconds prints as it
happens (a blit, a picture replay, a live paint, and the bakes those are
bought with), which is how a headless run says where its time went. Any
unparsable value means four milliseconds; unset, neither costs a clock
read.

**Everything the library says, it says through `SkDebugf`** — the
profile lines above, and every one of the once-per-cause diagnostics a
silent no-op carries, in every tier: the kernel, typography, brush and
kit alike. One channel, so a host that redirects Skia's debug output
captures the whole of what compose reports rather than half of it, and a
tier that cannot reach the kernel's internal headers still reports
through the same door. A warning is emitted at most once per distinct
cause, guarded by `thread_local` state, because a description re-runs
every frame and a mistake in one is a mistake in all of them.
