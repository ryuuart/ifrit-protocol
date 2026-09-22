# The header map

A chapter of [SigilCompose's README](../README.md).

Everything lives in `namespace sigil::compose` under
`include/sigilcompose/<feature>/`, one directory per feature target, and
the include spelling is the feature's: `<sigilcompose/core/Element.h>`,
`<sigilcompose/kit/Layouts.h>`. There are two public include roots and
no others: `include/`, and `testing/include/`, which the testing target
adds and which carries the harness a consumer's own tests reach for. The
internal headers beside each feature's sources are not reachable from
outside it. Each feature has an umbrella named after it (`core/Core.h`,
`kit/Kit.h`, `brush/Brush.h`, `typography/Typography.h`) over its public
headers. Umbrellas are conveniences; a consumer can include each header
from its owning feature. `<sigilcompose/Compose.h>` at the root is the umbrella over the
kernel — exactly `core/Core.h`. Each header stands on its own; include
the one a translation unit needs, from the feature whose target the
translation unit links.

**Kernel — `core/`.** A user who reads these headers has a complete and
sound model; nothing below them changes kernel semantics.

- `core/SurfacePaint.h` — `SurfacePaint`, a component prop accepting a
  Fill, an animatable Fill or a material. Pass it to Element's fill verb.
  Empty paint preserves the element's fill; bindings retain their source
  identity and materials retain their frame-dependent behavior. Neutral
  wells and sheets accept this same value as their ground.
- `core/Paint.h` — the paint values: `Fill`, `Corners`,
  `PaintContext`, `KeyState` — the keys a host fed, as a paint program
  reads them beside `PaintContext::pointer` — `PromotionPolicy`, what
  decides a texture promotion, which `PaintContext::promotion` carries as
  the policy the painting composer runs under, so a program keeping a
  composer of its own runs it under the same rule — `PaintProgram`, a
  drawing on
  a canvas that NAMES ONLY THE PARAMETERS IT READS: the canvas and the
  context are both offered, so `[](SkCanvas& c) {…}`,
  `[](SkCanvas& c, const PaintContext& ctx) {…}` and `[] {…}` are all paint
  programs and nothing spells a parameter in order to ignore it —
  `StampCache`, and `hexColor`, the one colour spelling here: a source
  palette's hex integer as an `SkColor4f`. A `Fill` may be written as a
  REFERENCE the tree resolves at paint — `Fill::currentInk`, the ink in
  force, and `Fill::var`, a custom property — through `resolveRef`, which
  every consumer holding a `PaintContext` runs a fill through; the
  context carries the node's `PaintContext::ink`, `PaintContext::font`
  and `PaintContext::vars` for it. What a colour BECOMES is
  SigilMaterial's vocabulary, spelled from it — `material::skia::withAlpha`,
  `material::skia::scale`, `material::skia::lighten` and
  `material::skia::mixLinear`.
- `core/TextPainter.h` — the seam the kernel draws dressed type through:
  `TextPainterOperations`, the operations the composer asks of text that is not
  resting on its own straight baseline, and `TextPainter`, that engine
  as the value a text verb installs on a description. It is spelled in
  the typography feature's vocabulary and only names it; the kernel
  holds the paragraph, lays it out and draws it at rest by itself.
- `core/Shape.h` — the comparable seam values `Shape` (with
  `ShapeScheme`), `MotionPath`, `Decoration` and its declared-volatility
  concepts, and `LayerStyle`; and `Boundary`, which outline a node hands
  its decorations.
- `core/Stroke.h` — the stroke grammar: `Spans` and `spans::`, `Across`,
  `Around`, `StrandPath` and `strand::`. The path arithmetic under it is
  SigilGeometry's — the width law `geometry::path::Profile` with
  `geometry::path::profile::self` / `offset`, the deviation
  `geometry::path::Shaper`, the band `geometry::path::bandRegion` on a
  `geometry::path::Formation`, and `geometry::path::CrossingRule` with
  `geometry::path::crossing::` deciding who passes over whom.
- `core/Mask.h` — the masking family: `Region`, `parts::`, `by::`, `Gate`,
  `Mask`.
- `core/Layout.h` — `Dimension` and its literals, `Edges`, `FlexDirection`,
  `FlexWrap`, `Display`, `BoxSizing`, `Align`, `Justify`, `Echo`, `Cache`,
  `LayoutInput` / `LayoutScheme`, `CellSpan`, and the
  `ComponentProperties` / `ComponentFunction` concepts. A `Dimension`
  also takes SigilWeave's `Length` (`em`, `rem`, `lh`) and a `VarRef`, the
  relative units the cascade resolves, with `pw` and `ph` — the canvas's own
  width and height — resolved in the same pass. `LayoutInput::attribute`
  reads a child's fact by index and name.
- `core/Attributes.h` — `Attributes`, the typed facts a node states
  about itself (`Element::attribute` writes one, `Attributes::get` reads
  it back in the type it was written in, `Attributes::merge` lays one
  table over another), and the `AttributeValue` concept: anything that
  copies and compares.
- `core/Operator.h` — the operator seam: `Operator`, the comparable value
  `Element::operators` holds, built from anything satisfying `Arranging`
  or `Adding` (with `ArrangingOperator` and `AddingOperator` the
  comparable forms) or `LayoutScheme`, carrying the `zIndex` and
  `styleClass` its additions take; `Arrangement` and
  `Arrangement::Child`, what an arranging operator is handed — each
  child's size, baseline, cells, area and facts, and `place`, `centreAt`
  and `turn` to answer with; `Scope` and `Scope::Node`, what an adding
  operator is handed — every settled node's key, facts, classes, bounds
  and outline, `find`, `having` and `withClass` to reach them, and
  `attach` on the scope or on a node to answer with; and
  `ReadsChildMinSizes`, the opt-in that fills each child's content
  minimum.
- `core/Var.h` — `VarRef`, the reference a custom property's name
  interns to, with `var` to make one and `varName` to read it back.
- `core/Cascade.h` — `VarValue`, what a custom property holds, and
  `VarTable`, the properties in force at a node.
- `core/Declarations.h` — `NodeHandle`, the copy-on-write handle a
  description value is, `Declaring`, what every value that declares a
  node holds, and `NodeAccess`, the one door the verb mixins reach it
  through.
- `core/verbs/Box.h` — `BoxVerbs`: `gap`, `padding`, `margin`, `width`,
  `height`, `minWidth`, `maxWidth`, `minHeight`, `maxHeight`, `aspectRatio`,
  `boxSizing`, `display`.
- `core/verbs/Flex.h` — `FlexVerbs`: `flexDirection`, `row`, `column`,
  `flexWrap`, `flexGrow`, `flexShrink`, `flexBasis`, `alignItems`, `alignSelf`,
  `justifyContent`.
- `core/verbs/Placement.h` — `PlacementVerbs`: `absolute`, `cover`,
  `inset`, `left`, `top`, `right`, `bottom`, `centerAt`, `gridCells`,
  `gridArea`, `gridCellAlign`, `rect`, `at`. `cover` is the one that says a
  node FILLS the box it stands in, which `absolute` and `inset` said
  between them.
- `core/verbs/Shape.h` — `ShapeVerbs`: `borderRadius`, `shape`, `overflow`.
- `core/Band.h` — `BandVerbs`: `bandAlignment`, and `Band`, the leaf
  that has it.
- `core/verbs/Mask.h` — `MaskVerbs`: `mask`, in both its forms.
- `core/verbs/Cascade.h` — `CascadeVerbs`: `font`, `block`, `ink`,
  `var`, `varDefaults`, `imageRendering` — what a node declares to everything
  under it.
- `core/verbs/Paint.h` — `PaintVerbs`: `fill`, in every form a surface
  can be painted with.
- `core/verbs/Decoration.h` — `DecorationVerbs`: `background`,
  `overlay`, `foreground`, `stroke`, `layerStyle`, `decorationOutline`.
- `core/verbs/Effects.h` — `EffectVerbs`: `opacity`, `blendMode`,
  `filter`, `backdropFilter`.
- `core/verbs/Transform.h` — `TransformVerbs`: `translateX`,
  `translateY`, `travel`, `rotate`, `scale`, `scaleX`, `scaleY`,
  `skewX`, `skewY`, `transformOrigin`, `zIndex`.
- `core/verbs/Depth.h` — `DepthVerbs`: `rotateX`, `rotateY`,
  `translateZ`, `scaleZ`, `perspective`, `perspectiveOrigin`,
  `preserve3d`, `backface`.
- `core/verbs/TextStyle.h` — `TextStyleVerbs`: `paragraphStyles`,
  `initialLetter`, `textFirstBaseline`, `textVerticalAlign`,
  `textLineMargin`, `textWillChange`,
  `textOverflow`, `maxTextLines`, `textFill`, `textStroke`, `contentFlowAround`.
- `core/Text.h` — `TextContentVerbs`: `fx`, `variationDrive`, `textAttach`,
  `textAnnotation`, `textThreadTo`, `textThreadBalance`, `textOnPath`,
  `spanPaint`, `spanStyle`, `atRest`; and `Text`, the leaf that has them
  and the text properties both.
- `core/Image.h` — `ImageVerbs`: `imageRegion`, and `Image`, the leaf
  that has them.
- `core/verbs/Structure.h` — `StructureVerbs`: what a node IS rather
  than how it looks — the cascade it NAMES with `styleSheet`,
  `applyStyleSheet`, `role` and `styleClass`, the anchor it hangs off
  with `tether`, its identity with `key`, `hitTestable`, `cache`,
  `cacheScale`, `transition` and `staggerChildren`, and `children`.
- `core/verbs/Node.h` — `NodeVerbs`: the verb families EVERY node has,
  gathered so the list is stated once. A kind of node with verbs of its
  own inherits this and adds them.
- `core/Element.h` — `Element`: `NodeVerbs` and nothing more, the
  `Children` value a `children({…})` run is — which takes an element or
  any typed leaf — and `sameDescription`, whether two descriptions would
  reconcile to the same tree, which a value carrying an element compares
  by. This is the header a consumer includes: it brings
  every verb family and every kind of node with it, and a family's own
  header is for a value that inherits the family.
- `core/Factories.h` — the functions that start one: `box`, `stack`,
  `positioned`, `text`, `frame`, `image` (an `image::ImageAsset`, or a
  raw `SkImage` with a `material::skia::Fit` — `Stretch`, `Contain`,
  `Cover` or `Native` — which is
  the wrap written once and the fit said as LAYOUT rather than as a
  matrix the caller builds, so the node itself carries the picture's
  proportions), `picture` (a recorded
  `SkPicture` as a leaf, sized at what it was recorded at — the door out
  of a `snapshot()` that keeps the pruning and the caching the bake was
  taken for), `pathFigure` (a path already in canvas coordinates,
  re-based into its own bounds), `custom`, `slot`, `layout`, `point` (a
  node with no extent that carries a key and facts), `memo`.
  `text` takes `Utf8`, so `text("…")`, `text(u8"…")`, `text(std::string)`
  and `text(std::u8string)` are one factory and nothing widens a string to
  reach it; `Text::textOverflow` takes the same value. `each(range, make)`
  is the children a range describes, `each(count, make)` the children a
  COUNT describes — one per index, for the run whose items are their own
  place in it — and `each(range, make, between)`
  interleaves a separator — one before every item but the first, which is
  what a nav bar's hairlines and a legal strip's dots are. The separator
  is an Element copied between the items or a function of the item that
  FOLLOWS it, called with the parameters it names; a run of one has
  nothing between it and an empty one nothing at all, which is what keeps
  a strip from ending on a dot.
- `core/Utf8.h` — `Utf8`, the value a prop or a parameter that takes TEXT
  is declared as: it accepts `"…"` and `u8"…"`, a `std::string` and a
  `std::u8string` alike, holds the bytes as a `std::u8string`, and
  `bytes()` reads them back out. The kit's text props are these, and so is
  every text this library's own verbs take, so a call site writes the words
  and never a conversion around them. It also takes ANY VALUE THAT READS
  ITSELF OUT AS TEXT — anything answering `text()` with something a
  `std::string_view` reads, which is what a node of a decoded document is —
  so a caller whose words live in a file writes the node where the words
  would go. The constraint is what keeps this library from naming the
  vocabulary such a value belongs to: a string spelling has no such member
  and keeps its own constructor, so a literal stays unambiguous, and
  nothing here includes or links a data library.
- `core/Measure.h` — the one-shot verbs that take a tree without a live
  composer: `snapshot`, `intrinsicSize`, `metrics`, `measureRun`,
  `runPens`, and the two that solve a style BACKWARDS from a size the
  drawing states — `atCapHeight` (the number a reference actually quotes
  about lettering) and `fitRun` (a run solved onto a width).
- `core/Tiles.h` — `tiles::`, the slicing of one baked picture into a run
  of tile-sized rasters.
- `core/Shelf.h` — `shelve`, the packing several small drawings share one
  image by: boxes laid left to right on a shelf as deep as its deepest
  box, wrapping at `ShelfOptions::maxWidth`, and `Shelved::cells` handed
  back PARALLEL to the boxes given so a frame index indexes both. A caller
  that wants the denser sheet sorts its own boxes by height first; sorting
  here would break the parallel every consumer reads the result through.
  It is what `instancing::CellSheet` packs its cells with and what
  `kit::SpriteSheet` packs its sprites with.
- `core/Instances.h` — the instanced sprite leaf: `instancing::Pool`,
  the struct-of-arrays store on your side of the seam; `instancing::CellSheet`,
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
- `core/Derive.h` — `connector`, `rail`, `Anchor` (ONE OF TWO THINGS,
  and `where` says which: a normalised point on a keyed node's bounds, or
  a free waypoint at a point in the rail's own coordinates, so a bend
  that clears a corner costs no node — spelled `on` and `at`, with `key`
  answering which it is), `Tether` (where a box hangs off a keyed one:
  `on`, the point of the anchor it hangs from, `at`, the point of itself
  that lands there, an `offset`, and `fallbacks`, the places tried in
  order when the first will not fit `within` — written with
  `Element::tether`), `band`, `bandPointAt`, and the `derive::` namespace
  that gathers the family. A tether resolves in the derive pass before the
  routes, so a connector that ends on a tethered box routes to where it
  came to rest; when no place fits, the stated one stands, and a key that
  names nothing places nothing. `routeBetween` is the path a route draws
  between two rects with its ends pulled back by a gap — the one
  statement a connector and a connecting operator both route by. The
  two ROUTE seam values are here too —
  `Router` over a pair of rects (with `RouteScheme`) and `RailRouter`
  over an ordered anchor run (with `RailScheme`) — each a comparable
  value, with `Router::comparable` and `RailRouter::comparable` reporting
  whether the one a node holds can prune.
- `core/Composer.h` — `Composer`, and `TextSettling`, what
  `Composer::settling` reports about a live passage's last layout;
  `Composer::setInherited` is what the root inherits from;
  `Composer::setPointer` and `Composer::setKey` are the input a host
  feeds, which a pen or paint program under a node reads.
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
  cached picture; rows of text name their class in a
  `sigil::weave::StyleSheet` (`feed::TextRow`, `feed::TextOptions`). Built
  purely by composing the kernel; the bordered strip several feeds sit on
  is the kit's `kit::plate` (`kit/Plate.h`), with `kit::tinted` building
  the one-face sheet whose classes differ in colour alone, and
  `kit::console` is that plate
  over N feeds of one voice — each in its own column, or `Console::stacked`
  to a column — which is the verification plate a study prints its checks
  into.

**The animation vocabulary is SigilMotion's and is spelled that way.**
`motion::Animatable` is the property slot every setter here takes,
`motion::Transition` the eased change, `motion::animate` the keyframe
builder, `motion::bind` the shaped binding of a live `Output`, and
`motion::ease::` the curves — each from the SigilMotion header that
declares it, under `<sigilmotion/values/>` and `<sigilmotion/bind/>`. The
SCHEDULE is the same value wherever it runs: a cascade over glyphs, over
a set's children or over a feed's rows is one `motion::Spread`, and what
compose adds to it — what a unit IS — sits beside it on the track. The
time helpers a scene reaches for are there too: `motion::ramp`, a delayed
eased transition in float milliseconds; `motion::phase`, a wrapping
`[0, 1)` over a period; `motion::quantizeTime` and its integer
counterpart `motion::stepIndex`; `motion::decay`, the open-ended settle a
duration-based curve cannot be. So is the whole of "is this value
moving": `motion::isLive`, declared in
`<sigilmotion/values/Animated.h>`, is the one body every volatility walk
in this library asks, and what it can and cannot say is stated in that
library's README.

**NO ENTRANCE HAS A VERB OF ITS OWN.** Every one of them — a fade, a
scale out of a base, a slide, a spin — is `animate(from(a).to(b), how)`
at the property it moves, so the fade every card, panel and strip says
as it arrives is `opacity(animate(from(0).to(1), how))` and reads the
same way as the rest. A property with an entrance is at `b` once the
ramp lands and behaves from then on as any other stated value does.

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
seam (`layouts::Radial`, `AlongPath`, `Diagonal`,
`BaselineGrid`, `Jittered`) — each one a placement FUNCTION an author
could have written out. `core/Table.h` stands beside the seam instead,
because the auto table is an algorithm and not a formula, and so does
`core/Grid.h`.

`layouts::Grid` is the one arrangement a page divides into — equal
shares, unequal columns sized by what is in them, a fixed rail beside a
flexible body — because a `layouts::Track` carries a SIZING FUNCTION
rather than a width. There are four of them: `layouts::px`, a length
that neither grows nor shrinks; `layouts::content`, as wide as the widest
thing in the track and no wider; `layouts::fr`, a weighted share of what
is left over and no floor of its own; and `layouts::minmax`, one under
the other. `layouts::repeatTrack` is n copies of one track, which is how "four
equal columns" is spelled.

The rule, per axis, is initialize from the floors, resolve the content
narrowest-span-first, maximize toward the ceilings, and divide the
remainder among the shares. What makes a share behave is that weights
summing under one take only their own share and a share that would fall
under its floor freezes there and leaves the division, so a squeezed
container never resolves negative widths. What is still free after that
stays free, which is why a row of `layouts::content` tracks packs at the
start of its container instead of stretching to fill it. Both axes run
that one rule — a row span's deficit is shared across its rows in
proportion exactly as a column span's is, where the auto table beside it
drops a rowspan's whole deficit on its last row — and what differs
between them is only the DEFAULT track: an unstated column is a share of
the width and an unstated row is as tall as what is in it, so a grid
fills its container across and grows down the page. A track past the end
of a list that was given is content-sized.

`Grid::areas` is a picture of the grid drawn out of names, one string per
row and one token per cell, and `Element::gridArea` is how a child claims one
of those regions. A name survives what four integers do not: insert a row
into the picture and every child stays in the region it named, where
every numbered child after the insertion would have moved a cell. A name
the picture does not carry is silent, as an unknown key is everywhere
here, and the child flows instead: into the next free cell, never
backtracking past the cursor unless `Grid::dense` is set, in which case
it fills the earliest hole that will take it. A name scattered over cells
that do not form a rectangle is reported once and placed at the rectangle
that bounds it: a picture the author can see is wrong is worth saying so
about, and refusing to lay the page out at all is not.
`Grid::across` and `Grid::down` say how a child sits in the box its cells
make when the child itself said nothing with `Element::gridCellAlign`.
`Grid::solve` hands back a `Grid::Resolved` — the track sizes and origins
the rule arrived at — for the same reason the auto table exposes its own:
a track nothing fills leaves no trace in the placed rects, so a study
reproducing a printed page cannot read the grid back off them.

A content floor is the second intrinsic contribution, and it is
`LayoutInput::childMinSizes`: a text leaf's longest unbreakable run,
measured at a nil width, and everything else's measured size. A scheme's
placed width remeasures horizontal text before its content-sized rows settle;
the placed depth does the same for vertical text and content-sized columns.
The scheme does not re-describe children or infer a smaller intrinsic size
for a box. The minimum is
filled only for a scheme that declares `readsChildMinSizes` — the concept
`SizesFromContentMinima` — because the text minimum costs a measure per
child. THE ONE THING A CONTENT TRACK NEEDS FROM ITS CONTAINER is that the
container range its children at their own size: a container that stretches
them measures every one at its own width, so the track would be sized by
the container the content is about to be fitted into and the two would
chase each other.

**A scheme sees one thing about a child it could not measure: what the
child CLAIMED of it.** `LayoutInput` carries the container's size, every
child's measured size and every child's first baseline — all facts a
layout pass established — plus `LayoutInput::childCells`, one `CellSpan`
per child, written by `Element::gridCells` and `Element::gridCellAlign`, and
`LayoutInput::childAreas`, the region name `Element::gridArea` wrote, empty
for a child that named none. The name sits beside the span rather than in
it because a string on the properties of every node in the tree is what the
node's size budget forbids, and a named region is rare. Both are on the
CHILD and not in a list the scheme carries beside it, because a parallel list
has nothing to check itself against: insert or reorder one child and
every entry after it silently addresses the wrong one, taking another
cell's span, alignment and origin, with no error and a picture that still
looks plausible. `CellSpan::declared` is what a scheme reads to tell
"cell (0,0)" from "wherever you like", so a table can flow the children
that said nothing into the cells no child claimed. `layouts::Table` and
`layouts::Grid` are placed entirely by it — the grid resolving a name to
one first. Equal modules use repeated fractional tracks on both axes;
placement stays on each child:

```cpp
layout(layouts::Grid{
    .columns = layouts::repeatTrack(4, layouts::fr()),
    .rows = layouts::repeatTrack(4, layouts::fr()),
    .gap = {8, 8}})
    .children({header().gridCells(0, 0, 2, 1), sidebar().gridCells(3, 0, 1, 3), body()});  // flows into the next unoccupied cell
```

`layouts::Table` is the HTML automatic table layout: unequal columns
sized by what is in them, spans, and a surplus shared out in proportion.
It is not a modular grid under another name and it goes through none of
`geometry::arrange` — a module is one size repeated, and no column of a
table is the width of the next. Columns start at the widest child that
sits in one alone; spanning children then top their columns up, narrowest
span first, sharing a deficit in proportion to the widths already found;
and whatever the table is wider than its content is shared the same way,
which is what puts every column of a real page on a fractional pixel.

A column is solved BETWEEN TWO WIDTHS and not from one. What its content
wants is the widest thing in it; what its content needs is the narrowest
that thing goes without spilling, which is `LayoutInput::childMinSizes`
and is why the scheme declares `readsChildMinSizes`. Given less room than
the columns want, each gives up the same fraction of the distance between
its two widths, so a column with nothing to give up gives nothing;
narrower still and they stand at what they need and the table overflows,
which is what a browser does rather than dropping content.
`layouts::Table::declaredWidths` is the width the markup gave a column, where it
gave one — a fixed column, out of both divisions, which what is in it can
still widen, since no column is narrower than the narrowest thing in it.
It is ONE `Dimension` per column, the same length the rest of the library is
laid out in: pixels for a `<COL WIDTH=120>`, percent for a
`<COL WIDTH="30%">`, and `autoDimension()` — or no width at all — for a column
sized by what is in it. A percentage is a share of the room the columns
divide, the table's width less its padding and spacing, so it resolves
only once the table's own width is known; the columns the markup left
alone then divide what is left of that room by the auto rule above, and a
percentage the content will not fit into is widened by the content
exactly as a stated pixel width is.
`layouts::Table::fit` is what the table does with room it does not need:
`layouts::Table::ColumnSizing::Fill` shares the surplus, which is a table whose markup
states a width, and `layouts::Table::ColumnSizing::Shrink` stops at the content, which is
shrink-to-fit — a table whose markup states none.

Rows take the first of those steps and deliberately not the second: the
whole of a rowspan's height deficit lands on the LAST row it covers,
because sharing it in proportion inflates the first row of every span and
drags everything below it down the page. `layouts::Table::solve` hands the
resolved column widths, row heights and origins back, so a study
reproducing a published table can print what it resolved and diff it
against what the original measured — numbers no placed rect carries,
since a column nothing fills leaves no trace in the rects at all.

`kit/Connect.h` holds the connecting operators, `connect::Between` — one
wire between two keyed nodes, the pairing stated in the operator — and
`connect::ByLane` — a wire per pairing the nodes state under a lane, one
key or a list of them — each routed by a `Router` with a gap as a
connector is, dressed by the decoration it carries, keyed by the pair it
joins and attached to the scope; `connect::wire` is the figure itself.
`kit/Pin.h` holds `pin::ByLane`, which hangs an element off every node
stating a `pin::Request` — the element, the box it is given and a
`Tether` for where, its key ignored since the stating node is the
anchor — at the stated place or the first fallback that fits the scope.
`kit/Outline.h` holds `outline::Around`, a band along one node's
resolved outline attached to that node, and `outline::Hull`, the hull
enclosing every node stating a lane or naming a class, grown by a
margin and attached to the scope. `kit/Stamp.h` holds `stamp::ByLane`,
one element per node stating a lane, made by a `stamp::Maker` given the
node and attached to it, the operator's key vouching for the maker.
`kit/Routers.h` holds the stock routers. `routers::straight`,
`routers::orthogonal` (with `routers::Bend` saying whether the leg turns
at the midpoint or at either end) and `routers::arc` are `Router`s,
between one pair of rects; `routers::manhattan`, `routers::polyline`,
`routers::octilinear` and `routers::orbit` are `RailRouter`s, over a
whole run of anchors, and `routers::fromPairwise` adapts a `Router` into
one by stitching its legs into a single contour, so terminal caps and
casings fire at the run's ends rather than at every waypoint. The two
orthogonal spellings also take a `routers::Stamp`, which lays a route
out FOR A STAMPED BRUSH rather than for a stroke: `advance` is the tile
pitch every bend the policy puts on a leg is moved to a whole number of,
and `endInset` is how much of each terminal leg the route gives up so
the first tile stands clear of the thing it leaves. A route whose marks
are stamps IS its marks, and a leg that is not a whole count of them
either stretches its tiles or opens a seam; both zero — the default — is
every route that is stroked. Every one
of them is a comparable VALUE, the same seam a `Shape` rides: a `Router`
or a `RailRouter` holds either a scheme — a value with `route(…)` and
`==`, which is what each stock factory answers — or a raw callable. Two
routers built from the same parameters are equal, so a connector or rail
re-described with an unchanged route SETTLES: it prunes and replays the
recording it already made. A raw callable compares equal to nothing but
its own copies and re-patches every describe, which is what the escape
hatch costs; holding one Router and re-using it — rather than re-minting
the lambda each describe — restores the prune, since copies of one value
share their state.

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
that plug the `Decoration` seam — `PathFormat` (stroke formatting, whose
`antiAlias` is how a hard 1 px rule stands beside the equally hard
`styles::BevelPair`, `Brackets` and `TickRail`) and
`stroke`, its one-line spelling; `Shadow` / `shadow`, the soft drop
shadow; `Slice` (lattice image mapping — its `density` is the source's pixels per
layout unit in the fixed bands, so a frame generated oversized to stay
sharp still draws its corners at the width it was designed for);
`ContourWalk` (walk the outline
and run a program at each sample — its `draw` is handed the canvas, the
sample and the paint context and names only the ones it reads, its
`stampAt` the sample and the sample's index); `Wash`; `Border`. `brush/Adaptors.h`
runs any of them on another outline than the node's own: `onEdges`,
against only the sub-contours facing chosen box edges, and `inset`,
against a concentric copy of the outline. **A stroke sliced to chosen
edges keeps its alignment.** The runs `onEdges` hands down are open and
bound no area, so the outline they were cut from rides along in
`PaintContext::silhouette` and that is what an alignment clips against:
`onEdges(mask, stroke(w, fill, PathFormat::Align::Inner))` paints the
half of its width inside the shape along the chosen edges and nowhere
else — the band a `kit::BevelInner` masked to the same edges with
`styles::BevelEnds::Sliced` draws — `PathFormat::Align::Outer` the half
outside, and a centred stroke straddles the runs. **A stroke revealed by
a span keeps it the same way.** The run a span gate shows is open until
the reveal is complete, so the span pass leaves the shape it cut in
`silhouette` too: `.stroke(spans::upTo(t), stroke(2, ink,
PathFormat::Align::Inner))` paints that fraction of the same inner band
at every value of `t`, and at `t == 1` it is the unspanned stroke. **So
does a bevel ring**, which is the same rule at a different mark:
`styles::BevelPair` — and `kit::Bevel` over it — builds its bands from
the shape, because a band's facing is classified against a bounds centre
a run does not have, and the narrowed outline then says how much of the
ring is SHOWN. A `kit::bevelled` panel under `spans::upTo` draws that
fraction of its ring inside the shape instead of losing the whole ring
until the run closes. The
brush engine is
five headers: `brush/Layered.h`, the stroke stack (`StrokeLayer`,
`LayeredBrush`); `brush/GeometryOperations.h`, the one mechanism door for
deviating an outline (`operations::`, `GeometryOperation`); `brush/Brushes.h`, the
brush and its composites — `brush::solid`, `brush::layers`,
`brush::weave` and `brush::Restyled`; and the two shelves of leaf kinds
beside it, `brush/Stamps.h` for the STAMPED ones (`brush::Scatter`,
`brush::Pattern`, with `brush::Placement` and `brush::CornerArt`) and
`brush/Ribbons.h` for the SWEPT ones (`brush::Ribbon`, `brush::Art`). A ribbon is the variable-width band, and the
GEOMETRY is SigilGeometry's `geometry::path::sweptRegion`: the union of
the band's cross-sections rather than one long contour, because zipped
into a single left-forward, right-back outline the inner rail crosses
itself where the spine turns hard, the crossing winds the wrong way, and
the winding fill DROPS the inside of the bend — a hole that opens once
the band is wider than about half the leg it turns on, and is then wider
than the band. `Ribbon::join` is what happens on the OUTSIDE of that
corner, an `SkPaint::Join` because it is the same decision a stroke
makes: the chord, the arc, or the point (bevelling past
`Ribbon::miterLimit`, which is also the one join whose bleed reaches past
the width). What stays HERE is the WIDTH LAW — the linear taper, the
`Profile`, and the calligraphic nib, whose width is a function of the
spine's direction and so of a `geometry::path::SweepStation` rather than
of arc length. `Ribbon::band` hands the geometry back, so a study that
MEASURES what was drawn does not have to transcribe how it is built. `Ribbon::fillMaterial` paints the
band with a recipe instead of a `Fill` — the door `strokeFill` opens
on a stroke, mirrored here, so a ribbon beside a stroked outline does not
have to have the same paint written twice; `brush::presets::taper` and
`brush::presets::calligraphic` each take a `material::skia::Paint` beside a
`Fill`, and a live material declares the ribbon animated. The line vocabulary is three
more:
`brush/Lines.h`, the cartography and diagram stroke (`lines::Line` —
parallel casings, terminal caps, ties, waves); `brush/Rails.h`, N-rail
strokes where every rail is its own line; and `brush/Hatches.h`, the
parallel, radial and concentric hatches — each of the three a MECHANISM
with every field open. The finished ones over them, whose constants are
chosen (`cased`, `triple`, `arrow`, `railway`, `wavy`, `rails(n, …)`,
`quad`, `hatch`, `crosshatch`, `radialHatch`, `concentric`), stand a
namespace apart as `lines::presets::` in `kit/Strokes.h`, which — with
`kit/Plate.h`, `kit/Ornament.h` and `kit/Flourish.h` — ships with this
tier because each is spelled in its types.

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
from the face), meeting at the two corners they collide on as
`styles::BevelCorner` says — `Square` (the near band full width, the far
band under it), `Mitre` (the 45° step, the corner pixel to the near band)
or `MitreFar` (the same diagonal one pixel over, which is what an inner
ring wants so a groove closes); `styles::Brackets`, the reticle's L's
standing off a box at the corners asked for; `styles::TickRail`, a ruler
of marks along one edge with every n-th one long; `styles::Scanlines`,
hard rows over the outline through a blend mode; and `styles::Stipple`,
one colour laid through a repeating 1-bit mask — the mask as BITS rather
than an image, so a stippled node compares equal to itself, with
`styles::stipple` for the 50 % checkerboard and `styles::dither` for one
tone of an ordered dither.

A ring can also be drawn on SOME of its sides: `BevelPair::edges` is the
edge mask, and `BevelPair::ends` says what a band does at a corner the
mask left the other band out of — `styles::BevelEnds::Mitred` runs it
into the corner the whole ring would have made, `styles::BevelEnds::Sliced`
cuts it flush one missing band's depth short. A half ring is how a
deepened shadow on two sides is said, and it carries its own tone where a
stroke sliced to the same edges does not; with every edge drawn the two
`ends` are the same mark.

The bevel as a LOOK is `kit/Chrome.h`'s `kit::Bevel`: the tones, the
depth, the corner, `sunken`, the edge mask and its `Bevel::ends`, a
`softness` that turns the drawn edge into a moulded one, and an optional
`kit::BevelInner` — the second ring a gap further in, `inverted` for the
groove a separator is and carrying its own `BevelInner::edges` and
`BevelInner::ends`, which is the doubled edge a key wears on its two
shaded sides alone. It is a value
decoration in its own right, so `.overlay(theBevel)` dresses a panel, and
it is a theme token: `environment::Provide<kit::Bevel>` over a subtree and
`kit::ambientBevel()` at each use site puts one era on every button,
panel and well under it. `kit::bevels::motif`, `motifEtched`, `flash`,
`skin` and `plate` are the token sets four toolkits' edges resolve to.
`core/Pattern.h` adds the one thing a tile cannot do for itself — an
element tree AS the tile, baked through `snapshot()`. A recipe instance
becomes a paint through `material::skia::Paint::recipe`, an effect
through `material::skia::Effect::recipe`, and an output-stage view
transform for
`Composer::setView` is SigilMaterial's colour transform, compiled only
when the build finds OpenColorIO. A view handed over as a Material is
KEPT rather than built once: how cheaply a colour transform can run is a
fact about the surface — one whose channels are independent is a
per-channel table on an eight-bit surface and a full-canvas program on
any other — so `draw` lowers it against `canvas.imageInfo().colorType()`,
once per colour type, and a host whose surface never changes pays one
comparison a frame. A view handed over as an `Effect` is already built
and is taken as it stands.

**Type — `typography/`.** The DRESSING is this feature's, one header per
value family: `typography/TextUnit.h` — `TextUnit`, one unit as the layout
placed it; `typography/Selector.h` — `selectors::style` and `selectors::inFrame`, the
two selector forms whose subject is a description of this library;
`typography/TextEffect.h` — `GlyphInfo`, `GlyphModifier`, `GlyphModifierFunction`,
`TextEffect` and `Phase`, the value the seam is made of;
`typography/TextFx.h` — the effects the runtime evaluates by structure:
`fx::scramble`, the `fx::keys` keyframe table, the `fx::pass` shader pass,
the `fx::sequence`, `fx::mix` and `fx::hold` combinators, and the `fx::effect`
door; `typography/Track.h` — `Track`, `Beats` and `Beat`;
`typography/Annotation.h` — `Annotation`; `typography/TextPath.h` —
`TextPath`; and `typography/Typography.h`, the umbrella over them. The
TEXT ITSELF is SigilWeave's and is included from there: `weave::rich` /
`weave::RichText` and `weave::Story` for the content, `weave::Unit` for
the granularity, `weave::Selector` and `weave::selectors::` for what a track
addresses.

The kernel describes its text leaf in that vocabulary — a description
stores tracks, runs and readings — and every member it stores, compares or
evaluates is defined in the header that declares it, so the kernel links
no engine to do so; what the feature's archive holds is the members that
carry a diagnostic and the engine behind dressed type. The stock effects
over the seam are stock values, and so the kit's — `kit/Kinetic.h`, below.
A text verb takes this vocabulary and a text query answers in it, and the
kernel's own headers only name it: a call site that dresses its type, or
reads a beat or a unit back, includes the header that spells the value —
this feature's for the dressing, SigilWeave's for the text. A
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
the compose kernel links no codec. `material::skia::Fit` states how the decoded frame meets its box, under
the same four names every other source meets one by. The leaf's
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
target `SigilComposeTexture`, which links the Graphite context and the
hardware device its GPU path stands on. `draw/Draw.h` is the door
to the imperative pen, both ways, in its own target `SigilComposeDraw`,
and holds `drawWith`, the operator family's imperative door: an adding
operator that attaches one pen over the scope and hands a
`ScopeProgram` the pen and the settled `Scope`, keyed or not as
`compose::pen` is. `compose::pen` takes a `PenProgram` — a callable that NAMES ONLY THE
PARAMETERS IT READS out of a `draw::Pen` and the node's `PaintContext`,
exactly as a paint program does, so a program that hands a path to the
decoration grammar reads the context instead of rebuilding one — and
makes the node `custom()` would, at the CACHE THE VERB WAS GIVEN
(`Cache::None` by default, so a drawing that is made once says
`Cache::Texture` there rather than chaining a correction after), with the
pen's width and height the node's box and its transform starting at the
box's corner, so a declarative scene drops into p5's verbs for one node, and
the pen begins in the node's own ink and resolved type, so an unset
`fill` or `textSize` is what the cascade says rather than p5's white and
twelve. The pen's `mouseX`, `mouseY`, `mouseIsPressed` and keys are what the host
fed `Composer::setPointer` and `Composer::setKey`, the pointer mapped
into the node's own box, so a program that follows the pointer reads
the same numbers it would on its own canvas; a paint program reads them
as `PaintContext::pointer` and `PaintContext::keys`. `compose::graphics`
is the same door onto a canvas that is KEPT:
the program draws on a `draw::Graphics` the size of the box, which stands
between frames and is put down on the node's canvas each one — `pen`
repaints from nothing every frame, `graphics` keeps what earlier frames
drew, which is what makes a trail, an accumulation and a picture drawn
once possible. p5's loop words are therefore live on that program's own
pen: `noLoop()` stops running it while the surface goes on being put
down, `redraw()` runs it once more, `frameRate(fps)` runs it at most that
often. And `compose::paintRetained` is where the pen's `element(...)`
lands, an `Element` painted inside an imperative loop and RETAINED —
reconciled against what the composer kept for that call site, so its
layout, its shaping, its caches and its bindings carry from frame to
frame, and seeded from the pen's inherited ink and font so the tree
cascades from where the pen stands.

**Testing — `testing/Checks.h`.** A separate target, `SigilComposeTesting`,
which verifies generated geometry and reads back what was
drawn, in `namespace test` (GoogleTest owns `::testing`): `test::coverage`, `test::widthAlong`, `test::endpointDegrees`,
`test::rasterize` and the feed `test::report`. Both geometry checks ask
one question of a figure hundreds of thousands of times, so both resolve
the figure ONCE into an index beside them in `testing/Index.h` and read
their answers out of it: `test::RowIndex` turns each row of a sampling
lattice into the crossings the path makes with it, so a sample is a
binary search rather than a walk of every verb, and `test::CellIndex`
files a flattened figure's edges into square cells, so a cast tests the
edges along its own line rather than every edge of a band. Both answer
what the long way answers — the row index counts crossings under the
rule the path's own containment test counts them by, and asks the path
itself about any point lying ON one; the cell walk leaves out only edges
the ray's line misses. The checks a plate
reports — `measure::check` and `measure::failures`, with
`measure::finding`, `measure::reading` and `measure::heading` for the
rows that stand beside claims, and `measure::CheckTable` for the run of them
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
whole `measure::CheckTable` into a `feed::TextRing`, each row in the ink its
standing and verdict choose from a `test::ReportStyles` — the pass, fail,
finding, reading and heading names a plate's tinted set registers — so
the verification block of a study is one table, printed as it runs, and
no verdict is ever typed into a row's text. Test
binaries link it, and so does the sketch library, so a sketch can report
its own checks; nothing that ships does, which is what keeps a
point-sampled coverage scan out of a paint loop.

**Kit — `kit/Kit.h`.** A tier above the library that adds no kernel state
and no new equality: `kit::disc` (a node about a centre, at a radius or
at a `geometry::path::PolarFrame`'s — a braced pair is the centre, and a
frame is spelled as one) with `kit::ring` and `kit::dot` beside it, the
stroked circle and the filled one, since a box of radius r about a point
is not yet a circle and the three verbs that make it one are ceremony
wherever they are written; and `kit::at` (a box pinned at absolute
coordinates, for the plate that has no layout at all), `kit::dotSprite`
(the round stamp a point sink draws each point with),
`kit::PixFont` (aliased bitmap-font bakes, in `kit/PixelType.h`),
`kit::line` (`kit/Frame.h`: a mark of `Line::thickness` running
`Line::length` in the ink in force unless a fill is stated, across or —
`Line::column` — down, held off at both ends by `Line::inset`, and
stretched across the flow it stands in where no length is given; a
hairline is its default thickness and a tick is the same call at another,
which is why a separator, a rule, a tick, a caret and a whisker are one
name — with `Line::pair`, the second rail a masthead, a colophon and a
specimen sheet's row are ruled with, drawn as ONE node whose two rails
share one route, so a dotted companion's dashes register against the
heavy rule instead of drifting off it), `kit::ladder` (the same header:
N rules at one PITCH, each sitting on its own line of the rhythm — the
ruled bed behind a grid specimen and the baseline rhythm a page is set
to, which a caller otherwise writes as a padded column at a gap of
`pitch - thickness`, the one subtraction a ruled bed spells twice and
gets wrong once) and `kit/Frame.h`'s nine-slice frame,
`kit::centred` (a container whose content stands in the middle both ways,
since `alignItems(Center)` and `justifyContent(Center)` always travel together
and say one thing between them, with an overload round the one child most
of them hold),
the pixel art in `kit/Sprites.h` — `kit::PixelInk`, a canvas and a cell
size with the three verbs a pixel artist has, and `kit::Sprite`, the same
verbs recorded as `kit::SpriteRun` marks over a palette, so WHAT is
painted is separate from WHICH COLOUR each mark takes; `kit::pixelMap`
reads one out of a character grid through a `kit::SpriteKey` (and REFUSES
a character the key does not carry rather than leave an unfindable hole),
`kit::pixelSprite` presents it as nodes, `kit::spriteImage` bakes it,
`kit::indexImage` bakes the INDICES instead for a shader that recolours
per draw, and `kit::SpriteSheet` holds sprites under names and packs them
onto one image, each handed the rectangle it occupies — `kit::Scrim` and the
halo/shade legibility helpers, the stock text effects over the
`Text::fx` seam in `kit/Kinetic.h` — `fx::enter`, the one entrance
every unit-offset reveal is a setting of, with `fx::rise`, `fx::slide`,
`fx::pop`, `fx::spinIn` and `fx::scatter` over it, and `fx::typeOn`,
`fx::waveLoop`, `fx::variableAxisSweep` and `fx::tint` beside them, each
a comparable `TextEffect` built from the constructor any caller may use —
with `kit/Marquee.h`'s `kit::marquee`, the seamless ticker built from a
clipped strip and a wrapping phase, whose every dial is one options
struct,
`kit/Board.h`'s `kit::board`, the ground a placed drawing stands on — a
`stack` at its own size on its own ground, each child keeping the rect it
was built with, which is the root of a plate that has no layout at all
and names no class, states no sheet and sets no font, so a page-less
drawing states its own with `Element::styleSheet` on what it returns —
with `kit::panel` beside it, the titled region a page divides itself
into: an eyebrow over a title in the roles `eyebrow` and `h1`, a
note at the far edge of the head's last line in `caption`, each of
the three a part (`Panel::eyebrowLine`, `Panel::titleLine`,
`Panel::noteLine`), a hairline under the head where `Panel::rule` names
one, and the whole standing in `Panel::body`, which is a `kit::Well` —
its head is `kit::sheet`'s, so the rule bisects the distance to the
content instead of adding to it —
`kit/Placers.h`'s `place::grid`, `place::ring` and `place::repeat`, the
fillers of an instanced leaf's pool — the first two over the same ring
and grid arithmetic the layout schemes use, which is SigilGeometry's —
the two instruments in `kit/Instruments.h` —
`kit::trackMeter` (a cascade's schedule drawn, one cell per beat at its
rect, filled by its local time — `MeterPlacement` stands the cells over
the beats or under them as a rule, for a track whose own letters are
what is being watched) and `kit::restGhost` (the same word
undeformed under the moving one) — a component's own lines as parts in
`kit/Part.h` — `kit::Part`, one such line as a function of what the
component offers about it, the text and then the component, handed any
callable whose parameters are a prefix of that offer (`core::Callable`
over an Element, the one prefix search in the tree — the same one a paint
program and an outline are taken through, and the same rule a range's
children follow in taking a function of the item or of the item and its
index), so a sketch sets one line otherwise and nothing else under the
component changes — the furniture of a specimen sheet in
`kit/Specimen.h` — `kit::cell`, a body with a label and a note set
beside it as a `kit::Caption` says (its `label` and `note` are parts
that default to `captionLabel` and `captionNote`, leaves in the
document roles `label` and `caption`; `Caption::Where` puts the note under
the body, or both lines above it, or both below, and `labelMeasure` and
`noteMeasure` wrap either line at a stated width so a long one does not
widen the cell it captions; `Caption::body` states the body's own well, so
a cell and its well are one call, `Caption::justify` makes that well HOLD
the body rather than write itself onto it, so a picture smaller than its
plate keeps the measure it was drawn at, and `Caption::reading` writes a
figure over the body's corner on a scrim of that well's ground, in the
class `readout`, through the part `readingLine`), `kit::well`, the fixed,
clipped surface a specimen is drawn into with every size, fill, padding,
corner radius and keyline supplied by the caller — the element it is
handed IS the well, the spec written onto it, unless `Well::content`
states the other reading, where the plate is a surface of its own and
holds that element at its own measure, ranged both ways and centred where
it says nothing else — `Well::placed` making it a `stack` rather
than a box, for the plate whose children carry their own rects — `Well::paddingY` where a plate is set
tighter down than across, and `Well::keyline` drawn INSIDE the well's own
box, because a plate that is not the width it was given is the one thing a
fixed surface may not be — `kit::formatted`, the dynamically sized
printf-style reading those
captions use, `kit::panelGrid`, equal-width panels that wrap at the stated
column count and keep a short last row aligned (`PanelGrid::measure` is
the width the shares are cut from, unset being the parent's — a grid in a
column that sizes itself from its content has none to divide, and its
cells would be dealt nothing and drawn over each other), and `kit::cells`, a run of
them along one axis with a hairline between neighbours, and
`kit::sheet`, the titled and footed page that rules its header and
footer off from the content between them, its three lines the parts
`Sheet::titleLine`, `Sheet::subtitleLine` and `Sheet::footerLine` over
the leaves `kit::sheetTitle`, `kit::sheetSubtitle` and `kit::sheetFooter`;
the props are the CONTENT and
the arrangement, and every face, size and colour is the cascade's — a
cell's label is set in the class `captionLabel` of the sheet in force
and its note in `captionNote`, a page's three lines in `title`,
`subtitle` and `footer` — so the kit decides no look and a text prop is a
`compose::Utf8`, which takes `"…"` and `u8"…"` alike;
`kit/Rows.h` — a name and the figure that answers it: `kit::reading`, one
row of a `kit::Reading` set as a `kit::Rows` says (the name at the left,
the figure at the far edge of `Rows::measure`, the swatch of a row that is
also a key before both), `kit::readout`, a stack of them ruled between
where `Rows::divider` names a fill, `kit::table`, N `kit::Column`s each at
its own width — one head cell per column in the class `section`, a figure
column's cells in `readout` and the rest in `captionNote`, one swatch and
one key per row, with the swatch column reserved in the head and every row
when any row has a mark. Every row is its own run of cells, so a short row stays
short and a surplus word takes the last column's class; its `cellLine`
names the ROW and that row's own CELLS as well as the column, which is
what lets a table light one row and what lets a cell be a DRAWING read off
the row it stands in — a bar, a swatch ramp, a sparkline — since a part
may answer any element and not only a line, and `Reading::ink` is the colour a row is set in over whatever
its lines' classes name, because WHICH rows are lit is the data's business
and a sheet cannot say it — and `kit::bars`,
one row per value against an extent DERIVED from the values
(`Bars::largest` states it instead; `Bars::inks` is one colour per row,
over the bar's paint and over the two lines' classes, because WHICH row
is lit is the data's business — the same door `Reading::ink` gave a
readout's rows), each bar in `Bars::bar` on the track
`Bars::rest` holds, with the figure after it as a function of the VALUE
because how a number reads is the data's business; a readout and a table
are different readings and neither is the other with a field set, and
`kit::figure` is the one leaf a measured figure is set by, in the class
`readout`, wherever it stands;
`kit/Ground.h`'s two dressings for a flat ground — `kit::vignette`, a
radial ramp measured to the CORNER so it meets all four at one value on
a surface that is not square, and `kit::grained`, value noise collapsed
to one channel and soft-lit so a coloured ground takes a grain as light
rather than as speckled hue, with mid grey soft light's identity and so
the strength linear and zero exact — the furniture a page of set text
carries in `kit/Typeset.h` (`kit::ruby` and `kit::kenten`, the two stock
`Annotation`s; `kit::bullets`, whose markers hang in the indent;
`kit::rules`, cut to the extent a block's lines occupy;
`kit::NestedStyle` with `kit::nestedRun`, where a block's opening words
stop; and `kit::textColumns` over a `kit::ColumnSet`, N frames of one story
threaded in order, with the `kit::Spanner`s that break the chain),
what stands BESIDE that text in `kit/Annotations.h` (`kit::annotate`
under `kit::Beside`, which does the arithmetic of the reading direction,
or `kit::Anchored`, which takes the offset the author states; the unit is
offered to the overlay it builds, so a mark that is the same at every unit
names nothing) — and,
shipped with the tiers whose
types they are spelled in, `kit/Strokes.h`'s finished lines, braid,
bracket spans, brush presets and `kit::groove` — the engraved cut across a disc's stroke, a
radial ramp concentric with the circle so it is dark on the inner wall
and lit on the outer, as the comparable `kit::grooveRamp` paint or the
`PathFormat` that wears it — with `kit/Plate.h`'s bordered feed plate and
`kit/Ornament.h` and `kit/Flourish.h`, the pieces a manuscript border is
made of (Brush), and
`kit/Legibility.h` (Typography). The kit
is a **separate CMake library** (`SigilComposeKit`) whose only include
path is compose's public headers, which is how the public/internal
boundary is proven rather than asserted. Note that `kit/Kit.h` does not
pull in the headers shipped with other tiers; include them directly.
