# The declared-volatility contract

A chapter of [SigilCompose's README](../README.md).

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
- A decoration that paints beyond the node's box declares `bleed()`, or
  `bleed(SkSize)` when its overflow depends on the resolved layout size.
  The latter is queried again after resize. Mark width is declared with
  `reach()` or `reach(SkSize)`. Composite brushes and outline adaptors
  forward the resolved size to their decorations. These
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

**A LIVE LAYER EFFECT OVER STATIC CONTENT IS APPLIED TO A BAKE.** A node
whose only volatility is its own `filter()`'s bound parameters — nothing
live in its children, its material, its scalars or its decorations —
still declares itself volatile, because the pixels it composites do
change. But the volatility is applied *outside* the content, so the
content is rasterized ONCE with the effect left out and the effect runs
over that one image at every blit. The image's identity holds, and the
bake is held across every value the parameter takes. `backdropFilter()` is the
exception and always paints live: it reads what is already on the canvas,
and a bake holds none of that. A masked node is refused too, since the
blit-side resolve hands the effect's child materials the node's box and
clock rather than a gated outline.

**A STATIC LAYER EFFECT OVER SETTLED CONTENT IS RUN OVER THE BAKE, NOT
INSIDE IT.** A node holding a `Cache::Texture` bake and wearing an
`filter()` that never changes takes its bake in two steps: the content
rasterizes into a surface with the effect left out, and the effect runs
over that image into the surface the node holds. What that replaces is
the layer the filter opened *inside* the content raster — allocated over
the node's whole paint bounds plus the filter's reach, cleared, drawn
into and composited back on every bake — and that layer, not the filter's
arithmetic, is most of what an effect over a large node costs: a small
sigma paid nearly what a large one did. The picture is the same one, and
the blit is untouched. The tier is the author's own bake only
(`Cache::Texture`; automatic promotion refuses a filtered node outright),
and it is refused to a masked node, a `backdropFilter()`, and a node whose
subtree composites against the canvas — the same list the live tier
refuses.

The effect stays on the *bake* and never moves onto the *blit*, which is
where a live effect goes. A filter on a blit's paint is evaluated per
draw and answered from Skia's own cache only while the mapping that draw
stands under holds still, so a node that turns — the whole population
that keeps a local bake rather than a device one — would pay the entire
filter every frame to save it once per bake.

**A DEFERRED EFFECT READS THE BAKE'S OWN MARGIN AND NOTHING ELSE.** Both
tiers above filter an image rather than a layer, and outside that image
there are no pixels: the effect's reach must be inside the node's paint
bounds, as the transparent band a declared `bleed()` (or a text leaf's
ink, or a child's overflow) puts there. A reach the bounds do not hold is
spread from a cut edge — which is what it was before, when the filter's
own layer was cut by the bake surface it composited into.

**AN EFFECT DECLARES ITS REACH, OR IT COSTS THE CANVAS.** A filter built
from a runtime shader may write any pixel, so Skia gives it a layer the
size of the whole clip and a small node's effect then evaluates over the
entire canvas — the same node twice as expensive on a canvas twice the
size. `Effect::blur(map, maxSigma)` declares its reach (the box the map
is defined over, grown by the range's Gaussian support) and costs its own
node. An author writing a runtime-shader effect of their own owes the
same declaration.

**A SCALE MOTION THAT NAMES ITS DESTINATION IS BAKED THERE, ONCE.** A
`Cache::Texture` bake taken while the node is moving is held in local
space at a coarse scale ladder, so a scale nobody declared — a resize, a
pinch zoom — reuses one bake per step. The rung is the other answer that
belongs to the scale it was taken at: a local bake is a texel grid the
blit stretches over the node's own units, and a node inside a held
recording is never asked for a new rung, so it narrows that recording's
scale window to the rung's own span. An entrance is the opposite case:
a `from(a).to(b)` on `scale`, `scaleX` or `scaleY` names where it is
going, so the bake is taken there once and the blit minifies through the
entrance, which is the sharp direction. A scale driven by a binding names
nothing and keeps the ladder.

**A DECLARED DENSITY MAKES A BAKE A PICTURE OF THE CANVAS.** The ladder
above is right for a host that draws its canvas at one scale and wants
the sharpest raster for it, and wrong for a host whose reader can zoom:
a wheel spin walks the rungs, and each one re-rasterizes every generated
material in the scene at a new resolution while the reader waits.
`Composer::setBakeDensity(devicePixelsPerUnit)` names the density every
pixel bake is taken at instead, whatever matrix the frame is drawn
under. The bake is then taken ONCE and blitted through the view's
transform ever after — exactly as an image node's pixels are: sharp at
the density it was baked for, magnified beyond it, and never re-taken
for a change of view scale. `Element::cacheScale` still multiplies it, so
a node that needs more resolution than the canvas carries asks for it and
gets it once. Content that changes still re-bakes, because that is a
change of what the picture IS, not of how big it is being shown.
A canvas kept under `compose::graphics` is formed no coarser than it
from its first frame, which is what a picture drawn once and photographed
finer than its host steps needs: pixels that accumulate cannot be
re-taken at the still, only magnified to it.
`Composer::bakeDensity()` reads it back, and zero — the default — is the
ladder.

**A LOCAL BAKE IS BLITTED AS ONE IMAGE.** The image follows the node's
transform and the layer's coordinate system. Transparent texels contribute
nothing. A device-space region cannot safely restrict this blit when a
capture changes scale or an effect evaluates the picture on an intermediate
surface, so local bakes carry no separately cached clipping grid.

**A PROMOTED NODE PAINTS THE PICTURE ITS LIVE PAINT PAINTS**, within one
code value per channel, and that is the whole of what automatic promotion
may cost. The one value is not slack for a bake to be approximately
right: what a bake holds is an image of eight bits a channel, premultiplied
by the coverage it was drawn with, and the blit composites from that — so
a shaded pixel may land one code value from the live paint and nothing may
land further. Every condition the promoter is held to follows from that —
the bake carries the canvas's own clip, so an edge that leaves the canvas
is cut the same way in both; it stands on the canvas's own grid, so the
matrix it inverts is the live paint's to the bit; the node must be
upright, since off-axis a blit at an absolute device rect is not the same
picture at all; and nothing live may be inside the bake. A scene whose
promoted frame differs from its unpromoted one by more than a value is a
defect in this library, never a plate to rebase.

**AND THE CLIP IS PART OF WHAT THE BAKE IS.** Because the bake carries
the canvas's clip in, what it holds is the node's paint AS THAT CLIP LEFT
IT — and a clip narrows and widens for reasons the node's own bounds
cannot see: a panel opening, a window growing, an ancestor's layer
standing over the box while the node fades in. Nothing else a bake is
compared against moves with it, since the paint bounds are the same
bounds whatever the clip did to them, so a bake held across the change
would blit the cut for as long as the node's content stood still and the
marks the clip removed would never come back. Every device bake is
therefore stamped with the clip it was taken under and remade when that
clip moves, exactly as a recording holding a device blit is.

**AND THE BAKE STANDS CLEAR OF WHAT IT PAINTS.** Skia decides whether a
path needs its clipped rasterisation from the path's control-point
bounds, and its clipped and unclipped routes do not answer the same
antialiased coverage. A curve's control points stand outside the ink it
draws — about a hundredth of the curve's own extent for the cubics a
stroker approximates an offset with — so a bake cut to exactly what the
node paints cuts inside them, and a stroked ring or arc baked flush moves
by tens of code values along its whole length. Every device bake is
therefore taken with a margin, a thirty-second of its own larger side and
never less than two pixels, so what bounds the drawing is the clip the
bake carries in and never the rect it was measured to. AND IT HOLDS THE
SHAPE THE NODE DECLARES. A `Shape` is a function of a size and nothing
holds what it returns inside the box that size came from — a generator
anchored on a centre of its own, a ring of rules drawn at radii the box
knows nothing about — and the node's surface is filled with that path
while every decoration dresses it, so the ink is where the path is. AND
THE REACH EVERY EFFECT UNDER IT FILTERS OVER. A layer effect paints
outside the box its node was measured to — a blur's skirt, a glow's halo,
a shadow's offset — and that ink is drawn by the child's own filtered
paint INTO whatever the node above was given room in, so the rect a bake
is measured to grows by what each filter below it answers for its own
input. A LAYER is not grown that way: Skia grows a filtered `saveLayer`
for its filter already, and where the effect is deferred to the blit
instead the rect is the frame the effect reads its own parameters in — a
sigma map's unit square is the box the layout decided, so growing it would
re-aim the effect rather than make room for it.

**A NODE IS SIZED IN ONE PLACE, AND THE DECLARED SHAPE IS PART OF THAT
SIZE.** Everything a node is given room in comes from its own paint
bounds: the recording cull and the subtree union over it, the BOUNDED
`saveLayer` a group opacity or blend composites through, the one a layer
effect is run over, the surface a lifted filter runs over, the local and
device texture bakes, the split bake's own half, and the alpha surface a
coverage boundary is traced off. So the declared shape
bounds every one of them, and a LAYER is the harsher case rather than the
lenient one: a `saveLayer`'s bounds are a clip, so a layer that misses the
node's ink DELETES it, where an allocation that misses it merely cuts what
falls outside. What bounds a node's drawing is the clip it carries in —
never the rect it was given room in. The ONE term a layer does not take is
the reach the effects below it filter over, for the reason above: a
filtered `saveLayer` grows itself, and a deferred effect reads the rect as
its own frame.

**AND THE BAKE STANDS ON THE CANVAS'S OWN GRID.** A bake taken on a
surface allocated at the node's own corner maps every point through the
live matrix with an integer subtracted from its translation, and the
integer is exact while the SUM it enters is not: the live paint rounds its
sum in the binade of the device coordinate and the offset one rounds its
own in the binade of the offset coordinate. Near the canvas origin the
integer cancels exactly and the two are the same pixels; far from it they
part by half a float step of the device coordinate — which is nothing
along an edge that meets the grid squarely, and a whole quantizer bucket
where a curve runs nearly TANGENT to one or a glyph mask is cached at a
quarter-pixel phase. It is not the rasterisation route: the same curve
into a layer and into a fresh offscreen is the same pixels, which
`ADeviceBakeRasterisesOnItsLivePaintsRoute` holds.

So no offset enters the layer's matrix at all. Every device bake — the
promotion tier's, the split's, the group's and the device-space texture
one — is painted under the node's own matrix on a surface standing at the
canvas's origin, and the node's rect is taken off it as the image the blit
replaces the paint with. The surface is SHARED, one per depth of nesting
and grown to the largest rect any bake has needed, so the grid costs a
canvas rather than a canvas per node; what a node keeps is the image, and
the rect it is measured to has one producer.

AND THE CONTRACT CARRIES A THIRD CLAUSE STILL, FOR WHAT A ROUNDING COSTS
AN EDGE. A PROMOTED MARK MAY STAND ONE STEP OF ITS OWN COVERAGE FROM ITS
LIVE PAINT WHERE A CURVE GRAZES THE GRID. What one step costs a pixel is
not a code value but whatever quantizer stands at that coordinate — a
supersample bucket on a nearly tangent edge, the phase bucket a glyph mask
is cached at — so the clause bounds the SHAPE of the difference rather
than its size: it may reach only pixels whose difference is confined to an
antialiased edge BOTH the promoted and the live picture draw, where the
picture varies by at least the difference within a pixel of that point in
each of them and so does every differing pixel beside it. That is one
pixel's coverage of an edge the two agree about. A picture that MOVED
shows the opposite — pixels taken off the edges, a mark that is gone, a
wash at another value — and no clause admits it. The lane below measures
the two apart and holds each to its own bar.

The one value is the bake **over transparent black**. A bake that lands
on CONTENT carries a second, and it is a rounding rather than a move: the
node's own coverage is composited twice where the live paint composited
once — into the bake, and again when the bake is blitted — and Skia's
blit of a raster image is not the arithmetic of its direct shader draw.
So a texel whose alpha is between none and all can settle one value
further out over a bright backdrop, and taking the bake at higher
precision does not remove it. It appears only where the node's own alpha
is partial; an opaque node over anything is exact.

**AND THE SECOND VALUE IS PER COMPOSITE.** A bake is one composite and a
pixel can stand under many: nothing nests, but independent nodes overlap,
and a stack of concentric rings each promoted on its own puts seven or
nine cached rasters over one pixel. Each of them rounds and the rounding
does not decay — a flat wash through N bakes stands N code values from
the same wash painted live, which
`ACachedRasterCostsThePixelItLandsOnOneCodeValue` measures over every
destination value, every source alpha and a spread of source colours. So
what bounds a PICTURE is the two above times how many composites its
pixels stood under, and a lane that judges a picture has to count them:
`Composer::setCompositeCounting` tallies every device blit into a plane
the size of the canvas, which is how the difference is priced. Anything
beyond that is a picture that changed.

**WHICH IS WHY A NODE INSIDE A BAKE IS NOT BAKED AGAIN.** Two composites
is what the second value bounds, and a bake standing inside another node's
bake makes three: the node's own coverage into its own image, that image
into the layer above, and the layer above onto the canvas. It buys nothing
either — the image of the bake above is what the blit lands, so the inner
one is consulted only on the frames that one is remade, which is the
measured cost being near zero and the cost rule never asking. Only the
eager policy ever reached for it, and refusing it is a condition on the
pixels rather than a change of policy, so `Eager` and `ByCost` refuse it
alike.

**AND SO DOES TYPE THAT ADDS RATHER THAN COVERS.** A glyph pass carries
an `SkPaint` of its own, so a phrase set additively — the blend on the
paint, which is where it belongs, since a blend on the NODE opens a layer
every frame — composites against what is under the node exactly as a
blended decoration does. Every paint a text node can carry is asked: the
style's foreground and its under- and overlays, its line decorations,
each run of a `weave::RichText` value, and each `spanPaint()` restyle. A
node carrying one, and every ancestor, is refused the automatic bake and
the memo hold, and the row says `ReadsBackdrop` — because a bake would
offer that light transparent black instead of the ground, and the light
comes back flat.

**A PAINT PROGRAM OF ONE'S OWN READS THE BACKDROP.** A `custom()` leaf is
handed the canvas and may draw with any blend mode — and a picture
recorded elsewhere and replayed through one may hold any blend inside it.
Nothing in this library can look inside a callable, so the refusal
analysis counts such a node as compositing with the canvas: it and every
ancestor are refused the automatic bake and the memo hold, and the row
says `ReadsBackdrop`. The cost of the other reading is not a rounding — a
plus-blended wash baked against transparent black lands a hundred code
values from its live paint. An author who knows their program only draws
over what it covers asks for the bake with `.cache(Cache::Texture)`.

**WHAT DECIDES A PROMOTION IS A POLICY, AND ONE OF ITS VALUES HAS NO
STOPWATCH IN IT.** `Composer::setAutoTexturePromotion` takes
`PromotionPolicy::Off`, `ByCost` or `Eager`, and the policy a composer
runs under reaches every program it paints as `PaintContext::promotion`,
so a program that keeps a composer of its own — a pen's retained guest,
a `TextureScene` — runs it under the same rule, and a capture that
pinned the session's promoter off pins every composer built inside its
paint. `ByCost` is the default and
the library's own judgement: a node is baked once its paint has measured
over the threshold for several consecutive frames, which means the set of
nodes promoted is a fact about how busy the machine was — on an idle one
it can be empty. `Eager` bakes every node the rules above admit, from its
first frame, whatever it costs. Not one eligibility rule moves: a node
whose bake would paint different pixels is refused under `Eager` exactly
as under `ByCost` and reports the same reason. It is not a performance
mode — a bake nobody needed costs the bake — it is how a run that means
to TEST the promoter gets the same node set on every machine, and gets
all of it rather than the few nodes that happened to be slow.

**THE LANE THAT HOLDS THE PROMOTER TO THAT.** Every scene in the sketch
registry is rendered twice on the CPU — once with the policy `Off` and
once `Eager`, everything else about the two runs pinned to the same
clock, the same fixed step and the same capture moment — and the two
pictures are differenced channel by channel. The pair is judged by the
three clauses above, each differing pixel against the bar for what it
stands on: one code value over transparent black, two over content per
composite the pixel stood under — counted into a plane the eager half is
asked for — and the grazing case under a bar of its own, which is the
quantizer's own step and not a scene's figure: Skia's supersampled scan
converter samples four rows of a pixel, so coverage moves in quarters,
and a quarter of the widest contrast an eight-bit plate can carry is
sixty-four code values. A scene past any of them is a promoted
node painting a different picture, and it is filed against this library. Because the on half is eager rather than measured, that sweep
covers every promotable node in the registry and reports the same numbers
on any machine. It is `sigil.py plates --tier promotion`, and it
is the only run in the repository that photographs this library with
promotion switched on.
