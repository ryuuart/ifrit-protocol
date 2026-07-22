# SigilCompose — what the studies asked for

This list is not speculation. Every entry below came out of *building
something real* and finding the library could not express it: seven
gallery scenes rebuilt on referenced geometry, and a set of study
sketches under `sketch/sketches/` each reconstructing a named artefact —
a Dead Space 2 bench screen, a hinoki asanoha ranma, Nightingale's 1858
mortality plate, 2Advanced Studios' v4 Prophecy interface, the DOOM
PlayStation title flame, the spirals John Whitney drew for *Vertigo* on a
repurposed anti-aircraft gun director.

The rule the program ran on: an author who hits a wall works around it
and **writes down what the natural API would have been**. Entries are
ordered by how many independent studies asked for the same thing, because
that is the only signal here worth trusting.

Numbers are STABLE, not compacted: agents in flight cite items by number,
and closed sections stay in place marked **CLOSED** rather than being
deleted and everything below renumbered. What each closed section keeps is
its citation count, which is the argument for why it was worth doing.

Companion documents: `DESIGN.md` (architecture), `API.md` (surface),
`STRESS_TESTS.md` (the acceptance catalog and the measured numbers),
`REVIEW.md` (the earlier first-principles pass this extends).

---

## What the list actually says

Read as a list this is thirty-odd unrelated papercuts. Read together it
is four arguments, and they are worth more than any single entry.

**1. The library computes the right thing and hands out only the finished
result.** Bindings resolve a value and drop the arithmetic (§1).
`brushes::Ribbon` works in (along, across) space to taper and never
exposes it, so a milled metal band is inexpressible (§8b). `Material`
compares structurally by recipe and `outline()` does not, so every shaped
node re-records (§3). `drawSpriteAtlas` was already decomposing to quads
and only ever needed a branch to read a size lane (§2, now closed). The
same shape recurs: an internal representation is richer than the public
one, and the fix is usually to expose what already exists rather than to
build something.

**2. Instancing promises a flyweight and delivers a sprite stamp.**
Eight studies, more than any other item. `Pool` covers "many copies of
one thing"; every real case is "many variations of one recipe" — labelled
nodes, staggered lattices, particles with lifetimes, rows with text,
hairlines that must not thicken with their circle. The non-uniform scale
lane has landed; what remains is a delay/progress lane, a short-string
lane, a stroke-width lane, and — separately, wearing the same clothes —
a **positioned leaf set** for generated geometry that wants no layout at
all. Three studies paid four-figure Yoga node counts for scenes with zero
layout in them.

**3. Volatility is declared per NODE, and authors think per PROPERTY.**
`trim` is a node property while a second window wants to be a stroke's
(§7 — which turned out to already work). `dashPhase` was a constant while
`trimPhase` was bindable (closed). `Effect` takes constant uniforms while
`Material` takes bound ones (§11). A `custom()` leaf is all-or-nothing
volatile. Each of these is the same asymmetry: the caching contract is
sound, and its granularity is one level coarser than the work.

**4. Four entries were WRONG, and that is the most important line here.**
`PathFormat` has always had its own trim window; there has always been a
bound `Fill`; the whole brush vocabulary has always worked on hand-built
geometry; and `onPath` walked every contour hours before a doc comment
said otherwise. In each case a capable author concluded "impossible",
built a workaround, and the workaround got recorded as a gap. An entry
that reads *impossible* outranks one that reads *awkward*, so a wrong
entry distorts everything below it — and three of the four were caught
only when researchers started reading the library's source instead of its
documentation.

The lesson is not "write better docs". It is that **this library's real
defect rate is lower than its perceived one**, and the difference is
discoverability at the call site. Four features existed, were correct, and
were worth nothing. That is a more actionable finding than any of the
missing ones.

---

## Closed during the program

| What | Why it mattered | Where |
|---|---|---|
| `Element::onPath` | Curved lettering cost one Element and one layout PER GLYPH — ~230 of each for one ring of labels, with no kerning | `Compose.h`, `Paint.cpp` |
| `Element::scaleX/scaleY` | Bars, wipes, meters and cooldown sweeps are the most common animated primitive in a UI and none of them are uniform | `Compose.h`, `Paint.cpp` |
| `ease::outBack/outElastic/outBounce` | choreograph's overshoot curves take a shape parameter, so `&easeOutBack` never converted; every entrance quietly settled for easeOutQuint | `Compose.h` |
| `Material::linearUnit/radialUnit` | `linear()` is in node-local pixels, which an author cannot know for a content-sized box; every scene that met one guessed and guessed wrong | `Material.h` |
| `shapes::sector` | `arc()` is open by contract, so every pie wedge, polar petal, cooldown pie and gauge fill was hand-built with `arcTo` | `Shapes.h` |
| `patterns::grain` | `noise()` is fractal RGB — overlaid on a coloured surface it hue-shifts rather than shades, turning porphyry into rainbow terrazzo | `Patterns.h` |
| `textFill()` dropping the style's other passes | A chrome wordmark silently lost its cast shadow and keyline | `Paint.cpp` |
| `brushes::rope(state, zoom)` | Widths tuned for a wide study swamped a real 43 px-spacing tree | `Brushes.h` |
| The split-Skia SkSL rule | Stock materials with helper functions or uniform-guarded breaks **segfaulted every sketch that painted them**, with no diagnostic | `Patterns.h`, `compose_sketch_stock` |
| `shapes::inset(px, Decoration)` | "The same bevel again, 6 px in" is the whole vocabulary of nested chrome and needed a second element every time | `Shapes.h` |
| `shapes::arrow`, `util::disc` | Every HUD, gizmo and diagram draws one; every inscribed-in-the-box polar shape wrote the same four lines | `Shapes.h`, `Util.h` |
| Two `onPath` bugs, found hours after it shipped | `autoFlip` turned each glyph over **in place**, mirroring the run; a centred run at `at = 0` silently ate every glyph before the seam | `Paint.cpp` |
| A third: `onPath` was never reconciled | `textEqual()` compared everything about a run except its baseline, so a new path or a moving `at` pruned and kept the OLD one. `TextPath`'s defaulted `operator==` was implicitly deleted and compiled quietly | `Reconcile.cpp` |
| **`bind()` — a binding you can shape** | The most-cited gap in the program: five studies, five directions, all keeping a second Output in pixels beside the [0,1] one | `Compose.h`, `Transitions.cpp` |
| An empty easing crashed instead of defaulting | `{360ms, {}, 220ms}` — the obvious spelling — aggregate-initialises an empty `std::function` and throws `bad_function_call` on frame one | `Compose.h` (`Transition::easing()`) |
| A guest crash was exit 139 and silence | Four agents spent most of a night localising ONE bad shader with no diagnostic at all | `sketch/SketchCrash.*` |
| `shapes::parametric` + `lissajous`/`harmonograph`/`rose`/`spiral`/`trochoid` | Nothing evaluated a caller's t → (x, y), so every curve DEFINED by a parameter was a hand-rolled `SkPathBuilder` loop | `Shapes.h` |
| Per-sprite blend on `instances()` | Nothing in the chain to `drawSpriteAtlas` carried a blend mode, and `Element::blend()` flattens the field into a layer — so an additive particle system could not accumulate at all | `Instances.h`, `GpuImage.h` |
| A fourth `onPath` bug: only the first contour | A trajectory clipped to the frame is several contours; its label vanished with no diagnostic | `Paint.cpp` |
| `shapes::circle`, `shapes::annulus` | Three places hand-wrote a circle OutlineFn; `util::disc` is the Element form, and onPath/trim/decorations take an OutlineFn | `Shapes.h` |
| `debug::coverage`, `debug::endpointDegrees` | A generated tiling's two CHEAP checks — area conservation and containment — both pass on a subdivision that overlaps in one place and gaps in another | `Debug.h` |
| `bind().quantize(n)` | Winamp's volume slider is literally `round(percent · 28)` — quantisation is the design, not an approximation of one | `Compose.h` |
| `dashPhaseBinding` on `PathFormat` and `lines::Line` | `trimPhase` took a bound Output and declared `animated()`; `dashPhase` was a plain float, so marching ants — the commonest animated-line idiom in map UI — meant re-describing every frame | `Decorations.h`, `Lines.h` |
| **`Pool::sizes()` — per-instance non-uniform scale** | The hard half of §2, eight studies deep: `SkRSXform` is uniform by construction, so a motion-blur streak whose aspect swings 2.4:1 → 1:1 could not be instanced at all | `Instances.h`, `GpuImage.h` |
| `addFixed`'s render interpolant | A fixed-rate sim drawn at an unrelated rate judders; the accumulator lived inside the steppable with no way to read it | `sigilmotion/Ticker.*` |
| `decorations::paintOn` | The brush vocabulary always worked on hand-built geometry — nobody could tell, and the roadmap said the opposite | `Decorations.h` |
| `TextPath::Orient::Radial` | `onPath` rotated to the tangent; a limb, a compass rose and a radial axis want type RADIATING, and each numeral was costing a rotated Element | `Compose.h`, `Paint.cpp` |
| `PathFormat::strokeMaterial` | `fill()` took a Material and a stroke took only the kernel `Fill`, so an object made of strokes wrote the same material twice, once per return type | `Decorations.h` |
| `debug::coverage(…, SkPath region)`, `VertexDegrees::components()` | An annulus cannot be tested against its bounds; and "is this one piece of metal?" needed hand-rolled union-find | `Debug.h` |
| `TextPath::Orient::Upright` | Neither Tangent nor Radial can leave a glyph level, which is what a calendar ring and a modern gauge use | `Compose.h` |
| **`Element::textStroke(width, Fill)`** | Three studies dropped to hand-built `PaintStyle` underlays; one spelled a 1 px outline as 117 re-draws of a paragraph | `Compose.h`, `Paint.cpp` |
| **`Element::wipe(angleDeg, fraction)`** | Three studies. `trim()` walks the perimeter and `scaleX` squashes; the last workaround left the retained tree entirely and forfeited decorations, hit-testing and pruning on twelve nodes | `Compose.h`, `Paint.cpp` |
| `textFill` + the `Unit` ramps | The metric band already maps the shader to a unit square, then `linearUnit`'s SkSL divided by the NODE size on top: t ≈ 0.003, every glyph flat on the first stop, silently — and `Material.h` advertised the two as the same trick | `Paint.cpp` |
| `Slice::filter` | Nine-slice is mostly used FOR pixel art, and was locked to linear | `Decorations.h` |
| `compose::metrics(style, fonts)` | A text node's top is the LINE BOX top and artefacts position type by the CAP TOP; ~134 runs were placed off an empirical guess at the slack | `Compose.h`, `Composer.cpp` |
| `PathFormat::cap` / `join` | ~30 open contours of line art all ended square and mitred because the paint was built and never asked | `Decorations.h` |
| `decorations::wash(Material, blend, amount)` | The decoration primitives were strokes, slices, contour walks and raw programs — none filled a shape with a Material, so a wash above the children was an incomparable lambda that never pruned | `Decorations.h` |
| `Pattern::offset` / `Pattern::sampling` | Pattern exposed two thirds of a matrix its own backend takes whole; and its tile was locked to linear sampling | `Pattern.h` |
| `bind().window(lo, hi)` | `from()` normalises and the curve runs after it, so a multi-beat binding fed easings values outside their domain — and none of `ease::` is total | `Compose.h` |
| The material cost model, documented | A static SkSL material's shader caches and its PIXELS do not; one full-canvas grain node was 480 ms of a 624 ms frame, and a texture bake took that frame to 28 ms | `API.md` |
| `Material::glowUnit()` | `radialUnit`'s radius is a fraction of the HALF-DIAGONAL, so "a soft glow filling this box" was still at ~10% alpha at the inscribed circle — two studies lost an iteration, one silently wrong on five cells | `Material.h` |
| `Ticker::addFixed(hz, fn)` | Every simulation-shaped study reinvented the accumulator AND its spiral-of-death clamp; the library had declared choppiness for shaders and nothing for logic | `sigilmotion/Ticker.*` |
| `Element::overlay()` | `background()` hides under the fill and `foreground()` paints above the children, so a textured button greyed out its own label — two studies worked around it with a sibling stack | `Compose.h`, `Paint.cpp` |
| `Element::sampling` | Every blessed image path hardcoded `kLinear`, so pixel art and tilemaps were silently blurred; `Material::image()` alone took a sampling parameter | `Compose.h`, `Paint.cpp` |
| `lines::radialHatch` / `concentric`, `shapes::star(…, waist)` | `hatch` is a parallel lattice, so an engraved radial FAN cost 120 sector nodes; and engraved star arms are concave, not straight-chorded | `Lines.h`, `Shapes.h` |
| §7 was WRONG: `PathFormat` has always had its own trim window | Two studies rebuilt a second trim as a duplicate node re-measuring the same path | `Decorations.h` (doc + test) |
| Four silent traps documented | `custom()` measures ZERO on the main axis and draws nothing; `grain`'s `stretch` multiplies the y frequency until it aliases; a `Pool` position is the cell's CENTRE; and there IS a bound `Fill` — a study concluded there was not and left the binding path over it | `Compose.h`, `Patterns.h`, `Instances.h`, `API.md` |

---

## 1. Bindings that cannot be shaped — *five studies* — **CLOSED**

> Shipped as `bind()` in `Compose.h`. The section stays, with its number,
> because agents in flight are citing these by number and because the
> five citations are the argument for why it was worth doing.
>
> ```cpp
> .translateX(bind(&phase).to(-70, 170))
> .opacity(bind(&progress).map(ease::outBack()).clamp(0, 1))
> .scaleX(bind(&hp).from(0, maxHp))
> ```
>
> `from()` normalises the source range onto [0,1]; `map()` shapes it with
> any choreograph easing; the affine chain composes in call order;
> `clamp()` lands last. `sizeof(PropValue)` is unchanged — the map shares
> the out-of-line block the transitioned form already allocates, so the
> 1288 B → 688 B `ElementNode` compaction still holds. It prunes properly
> (same Output, same affine, same curve), which matters because the map
> is read live: a pruned node would shape through the old one forever.

A bound `choreograph::Output<float>` lands on the property **raw**. There
is no scale, no offset, no curve at the binding site.

The consequences compound. A phase that lives in `[0,1]` — which is what
`trim()` and `opacity()` want — cannot drive a translation in pixels
without a **second** Output updated in the same steppable. One per-piece
progress driving both opacity and scale needs two Output vectors: the
kumiko study carries 1028 objects where 514 would do, with the easing
written in the tick loop, far from the properties it shapes. The Vertigo
study needs `growth − 0.008` for the pen-tip highlight and a curve of
`growth` for its trailing alpha, so it keeps eight scalars in sync by hand
across four cards — the fifth study to arrive here from an unrelated
direction.

```cpp
// wanted
.translateX(bind(&phase).scale(240).offset(-70))
.opacity(bind(&progress).map(ease::outBack()))
```

Natural API: `PropValue(const Output<float>*, std::function<float(float)>)`,
or a `.map()`/`.scale()`/`.offset()` chain on the binding. The paint path
already reads through a pointer; this is one call site.

## 2. Instancing covers "many copies of one thing", not "many variations of one recipe" — *eight studies*, and by a distance the most-cited item in the program

`Instances.h` names inventory cells and node-graph nodes as its cases.
Both are usually **labelled**, and a `Pool` carries only position,
rotation, uniform scale, tint and frame. So:

- a labelled node graph cannot be one atlas stamp (Dead Space);
- a lattice of 514 differently-mitred boards has no flyweight (kumiko);
- a staggered assembly has no per-instance progress or delay (kumiko, 2Advanced);
- strips of varying length are outside RSXform's uniform scale (kumiko);
- press-wire rows, chips and readout windows are all ineligible (2Advanced);
- a playlist's rows are the textbook instancing case and carry text (Winamp);
- a manoeuvre gizmo with two different arm lengths is out of reach even
  with six pre-coloured cells, because `Atlas::cell()` bakes ONE logical
  size and `Pool` carries ONE uniform scale (KSP);
- 549 rhombs, each needing its own outline and its own trim window, fall
  outside RSXform entirely — so the Penrose paving pays 1647 Yoga nodes
  for a scene with **zero layout in it**, every child `.absolute()` with a
  computed rect.

That last one is a different ask and worth separating: not "richer
instances" but **a positioned leaf set** — N children with caller-supplied
rects and no flex participation, skipping the Yoga pass. Generated
geometry (tilings, lattices, node graphs, particle fields drawn as real
elements) never wants layout, and today there is no way to say so.

**The blocker, finally named — it is not the transform, it is that the
STROKE WIDTH is baked into the atlas cell.** The astrolabe study has both
call sites in one file and they decide it. Its 360 limb ticks DID
instance: one cell, three lengths through `Pool::sizes()`, y-multiplier 1
so the mark width holds — because a tick is a FILLED RECT. Its 45
almucantars, 12 azimuths and 12 hour lines are literally one shape at 69
(centre, radius) pairs and cannot, because there the mark width IS the
stroke on the shape's outline, and RSXform's uniform scale would make a
bigger circle a thicker line. An engraved line does not thicken as its
circle grows. So the plate pays ~73 real Elements with zero layout in
them.

Wanted: a `Pool` **stroke-width lane**, or a `strokeInvariant` flag on
`Atlas` that re-strokes at stamp time rather than magnifying baked
pixels. That one change opens every dial, contour map, ripple field and
concentric-ring diagram — the whole class this library keeps being asked
to draw.

Two more details that the lane list does not cover, both from the Chladni
plate's 9,580 settling sand grains:

- **`tints()` is the only per-instance opacity lane**, so fading a subset
  means rewriting RGBA every frame when only alpha moves.
- ~~**The non-uniform-scale half**~~ — **CLOSED**: `Pool::sizes()` is an
  opt-in `SkSize` lane, and the fix was smaller than the gap looked
  because `drawSpriteAtlas` was already decomposing to quads for backend
  portability. It emitted them from `RSXform::toQuad`; with a size lane
  present it builds them directly, and everything downstream is
  byte-identical. What follows is what it cost before that landed:
- **The non-uniform-scale half had a measured price: 69 lines.** The
  Genesis study hand-built an 8-vertex flat-cored strip per particle plus
  the uint16 chunking `drawSpriteAtlas` already does internally, and with
  it went every decoration slot on the node and all picture caching. Its
  bench panel is the gap as a PICTURE — the same 700 particles through
  `instances()`+kSrcOver, `instances()`+kPlus and hand-built quads, where
  the instanced cells hold one baked aspect forever and keep slow
  particles as full-length streaks that the quad path correctly collapses
  to dots. Wanted: a `sizes()` `SkSize` lane, with the stamp falling back
  to the `drawVertices` quads it already builds when the lane is present.
- **One pool cannot be split across several clipped parents.** That study
  used a single canvas-wide leaf to keep one draw call, and therefore had
  NO per-figure clip — keeping sand inside twelve rims became hand-tuned
  radius arithmetic instead of `clip(true)`. A `clipPath` on the
  instancing leaf, or per-instance layer lanes, closes that half.

Two refinements arrived independently and both point away from "more Pool
columns":

**The atlas, not the pool.** KSP's starfield instanced perfectly; its
gizmo could not. The contract is right for masses and has no answer for
small HETEROGENEOUS sets. The shaped fix there is `(cell, variant)`
addressing on the `Atlas` — several bakes of one recipe — rather than
widening every instance.

**The schema already exists, and it is from 1983.** William Reeves' §2.2
attribute list for the Genesis Demo — the first particle system — *is*
the `Pool` the roadmap keeps asking for. Three of its seven attributes
are inexpressible today: `shape: streaked spherical` is per-instance
**non-uniform** scale (a quad `0.5·|v|` long by `size` wide, swinging
~3.5:1 → 1:1 across one particle's life), and `lifetime` is the delay /
progress lane. Non-uniform scale is the hard one: `SkRSXform` is uniform
by construction, so that half is a different draw path, not a new lane.

Also `place::repeat` writes lanes it does not own — it clobbers
`tints[i].fA` and cannot set `frame`, so every mixed-frame call site
re-walks the lanes by hand and calls `touch()` again.

Natural API: optional per-instance **alpha**, **delay** and **short
string** lanes; non-uniform per-instance size; generators that write only
their own lanes. Or, failing that, say plainly in the header that
labelled lists stay real elements — the doc currently reads as if they
are covered.

## 3. `outline()` can never prune, and parametric curves have no generator — *four studies*

It takes an incomparable `std::function<SkPath(SkSize)>`, so every shaped
node re-records on each `render()`: 514 in the kumiko lattice, every
chamfered panel at four nesting depths in the 2Advanced reconstruction.
A runtime-parameterised pattern re-records its whole panel per change.

`Material` already solved exactly this by comparing **structurally by
recipe** rather than by shader pointer, and `Brushes.h` solved it with the
`GeometryOp` value / `PathOp` lambda split. Shapes want the identical
move: a comparable `Outline` value (kind + params) covering the stock
generators, with the raw lambda as the escape hatch that never prunes.

Two more shapes of the same problem, both worth naming:

**Geometry that is BOUND cannot be a node shape at all.** `outline()`
resolves at LAYOUT, so a form that changes per frame — Winamp's EQ
response curve, a function of ten live Outputs — has to become
`custom()`, and gives up pruning with it. A `PropValue`-aware outline
would cover it.

This entry used to add "and it forfeits `trim()` and the decorations",
which was **wrong** — the fourth wrong entry this program has found, and
again caught by reading the source instead of the list. `PathFormat`,
`lines::Line` and `Brush` read only `PaintContext::outline`;
`Decoration::paint` is public; `PaintContext` is a plain aggregate. So
the entire brush vocabulary, trim window included, works on geometry you
computed yourself. `decorations::paintOn(canvas, ctx, path, decoration)`
is now the spelling, and a test paints a trimmed dashed head on a path
built inside the `custom()` program.

The Vertigo study says which generators are missing, and it is a whole
family. `Shapes.h` builds closed **shapes** from parameters; nothing
evaluates a caller's `t → (x, y)`. So every curve *defined* by a
parameter — Lissajous, harmonograph, rose, epitrochoid, spirograph, phase
portrait, orbit trace — is a hand-rolled `SkPathBuilder` loop inside
`outline()`, i.e. lands in the escape hatch by default rather than by
choice. `shapes::parametric(fn, t0, t1, samples)` plus named
`lissajous(a, b, delta)` / `harmonograph(...)` — **as comparable values** —
closes the two halves of this at once.

## 4. No `Material::buffer` — content that changes without re-describing

`Material`'s only volatility tier is `uTime` / bound-uniform: a pure
function of time, Shadertoy-shaped. Anything with **state** — a
simulation, a paint buffer, a decoded video frame, a scrollback — falls
back to `custom()` + `Cache::None`, forfeiting picture and texture
caching and every decoration slot on the node.

`Instances.h` already invented the seam this needs: a user-owned `Pool`, a
`revision()`, `touch()`, and `Mode::Data` pruning on
`(atlas, pool, revision)`. Natural API:
`Material::buffer(std::shared_ptr<PixelSource>)` — the raster analogue,
reusing that pruning rule verbatim.

## 5. A `Material::blend` layer has no amount — *two studies*

"Soft-light this noise at 30%" has no expression. The only route is
baking `0.5 + (v-0.5)*amp` into the noise's own SkSL, which means every
consumer forks the stock generator. (`grain` grew a `contrast` parameter
for exactly this reason; that fixes one generator, not the shape of the
problem.)

Natural API: `blend({{base, kSrcOver}, {tex, kSoftLight, 0.30f}})`, or
`Material::amount(float)` on the layer value.

## 6. No directional wipe — *three studies* — **CLOSED**

`Element::wipe(angleDeg, PropValue<float>)` reveals the fraction of a
node lying before a moving edge at any angle. Paint-only and bindable
like the transforms, and it covers the node's decorations too, because a
reveal reveals.

`trim()` could never express it — it walks the PERIMETER, so on a filled
shape 0→1 sweeps a wedge round the outline rather than extending the
surface — and `scaleX`/`scaleY` squash, which a striped or textured fill
shows immediately. The third study's workaround is the reason this got
built: it left the retained tree entirely, snapshotting each node at
setup and replaying it under a hand-written `clipRect` in a
`custom(Cache::None)` leaf, forfeiting decorations, hit-testing and
pruning on twelve nodes at once.

## 7. One trim window per node — **WRONG, and worth saying so loudly**

Filed twice, from two studies, and it is not true. `PathFormat` has
carried `trimStart` / `trimEnd` / `trimOffset` / `trimPhase` all along,
and the windows **compose**: a decoration receives the node's
already-trimmed outline, so its own window is a fraction of the revealed
part.

```cpp
PathFormat head = util::stroke(6, Fill::color(kBright));
head.trimStart = 0.90f; head.trimEnd = 1.0f;   // the last tenth of what
                                               // the node has revealed
box().outline(curve).trim(0, &growth)
     .foreground(util::stroke(3, Fill::color(kBody)))
     .foreground(head);                        // rides the drawn head
```

That is exactly the pen-tip-behind-the-head case the Vertigo study
rebuilt as a duplicate node re-measuring the same 2000-segment path,
four times over. Now pinned by a test
(`ComposeDecorations.EachStrokeCarriesItsOwnTrimWindow`).

What is genuinely missing here is smaller, and stated where it belongs:
the node-level `trim()` is the only one that reaches the FILL and the
content, and `dashPhase` has no bound form (§10b).

**The real defect was discoverability, and this is the second time this
week a study has worked around something that exists** — the other being
the bound `Fill`. A gap list is only worth its accuracy: an entry that
reads "impossible" outranks one that reads "awkward", so one wrong entry
distorts everything below it. Both were caught by checking the claim
against the source before ranking it, and both should have been caught by
the header saying so at the call site.

## 8. `routers::orthogonal()` is unusable for its most natural application

Found building a PCB-style node graph, three problems at once:

- it is a `Router` (rect, rect) and `rail()` takes a `RailRouter`
  (span of points). No adapter exists, so the obvious call site does not
  compile;
- it emits **zero-length segments** on axis-aligned pairs, and
  offset-contour brushes (`lines::cased`) flare visibly at both ends of
  every edge;
- it always bends at midX (a Z), never an L at the target column, with no
  knob — the wrong shape for any circuit graph.

Natural API: `routers::manhattan(...)` as a RailRouter,
`routers::fromPairwise(Router)`, collinear-point collapse, and
`Bend::MidX|HFirst|VFirst` plus a **chamfer** alternative to
`cornerRadius` (45° cut corners are the game-UI convention;
`SkCornerPathEffect` only rounds).

## 8b. No way to shape a stroke ACROSS its width

Named by the Penrose study as the single highest-value cue it could not
get cleanly. The Oxford paving's inlay is a **milled band**: a groove
shadow down one side, a specular ridge down the other — a cross-section,
not a colour. `PathFormat` and `Brush` give only concentric legs of
decreasing width, and `Fill` is evaluated in **node-local** space rather
than stroke-local, so there is no way to say "dark at the edges, bright
just off centre" for a stroke that curves.

The study only got a real cross-section because its strokes happen to be
circular arcs: a radial gradient centred on each arc's centre of curvature
is constant *along* the band and varies *across* it. On any other path
that trick evaporates.

Natural API: `lines::Profile` / `PathFormat::crossFill` — a Fill sampled
in `(t along arc length, u across half-width)`. That space is not new;
`brushes::Ribbon` already computes it internally to taper. It just isn't
exposed as a paint space.

This is the same shape of request as gap 3 and gap 5: the library computes
the right thing internally and hands out only the finished result.

## 9. Text: the missing spellings

- **Per-glyph animation and per-glyph *style* are mutually exclusive** —
  *two studies*. `paintKineticText()` reduces every glyph to
  `(font, colour, RSXform)` and drops the style's `SkPaint`;
  `paintTextOnPath()`, written this same night, inherited the shape. So a
  hollow display face, a gradient fill, an underlay or a `glyphFx` cannot
  ride a kinetic or curved run, and Vertigo's title rebuilds `pop()` one
  tier up out of seven letter nodes under `staggerChildren(30ms)`.

  The obvious fix is to bucket the batch on a resolved `SkPaint`, which
  covers both call sites. But the implementer's sharper point is worth
  pinning **before** that lands in SigilWeave: a glyph run with underlays
  and overlays is not one paint, it is an *ordered list* of them — that
  hollow face wants a blurred-stroke underlay under a stroked foreground.
  One pass per layer is the right answer, so the entry point wants the
  whole `PaintStyle`, not one `SkPaint`.
- **`onPath`'s `autoFlip` cannot upright a run that WRAPS past the
  crossover**, and by construction it never will: it is a decision about
  the RUN, because flipping glyphs one at a time turns them over in place
  and reverses reading order ("TECHNICOLOR" came out "ROLOCINHCET"). A
  long centred run on a full circle spans both halves, the majority reads
  upright, and it correctly does nothing — which looks like a dead flag
  and was reported as one.

  The missing feature is the engraver's own convention: a full-circle
  inscription is cut as TWO runs, top and bottom set separately with the
  bottom reversed. Wanted: split the run at the crossover automatically,
  or a `TextPath::Wrap::TwoRuns` mode. (The decision now samples across
  the whole run rather than reading one midpoint tangent, which is more
  robust but does not change this.)
- **No hollow-type preset.** Outline display type — a stroked face with
  the counters left open, the single most common title-card treatment
  there is — has no spelling at all. It is `PaintStyle` surgery: switch
  the foreground paint to `kStroke_Style`, then hand-build a blurred
  stroke underlay for the shadow, because `decorations::dropShadow`
  operates on the node and **fills the letterforms' interiors**.
- ~~**No glyph-level stroke.**~~ **CLOSED** — `Element::textStroke(width,
  Fill)` adds a stroke pass on the glyphs, under the fill, joining the
  style's own underlays rather than replacing them. Three studies had
  dropped to `PaintStyle::addUnderlay` by hand; one spelled "1 px outline
  plus offset shadow" as 117 full re-draws of a paragraph through
  `echo()`. It composes with `textFill()`, so engraved chrome type is now
  two calls. (The hollow-type case above is its sibling and still open:
  that one *hollows* a face, this one *thickens* one.)
- **No way to shape a run without building an Element.** `measure()` is
  per-Element, so hand-placing 230 glyphs costs 230 layouts. Wanted:
  `FontContext::measureRun(u8string, TextStyle) -> vector<float>`.
- **No tab leader — now the headline text gap, and priced.**
  `TabStopOptions` is `{positions, interval}` and nothing else, so a
  dot-leadered two-column row is two absolutely positioned leaves plus a
  sized box for the rule: **48 rows → 95 nodes and 18 setup-time
  `measure()` calls**, against 48 text leaves and zero measures for
  `TabStopOptions{positions, interval, leader, align}`. Worse than the
  count, the rule is registered to a BOX rather than to the run's
  baseline, so any face change silently drifts it.
- **`flowAround` splits the line; sometimes you need it to SHORTEN the
  measure.** `ExclusionFlow` yields an interval on each side, which is
  correct DTP behaviour — and wrong for reproducing a 1998 screen, whose
  "silhouette float" turned out to be a scan for one global-minimum inked
  column, i.e. an implicit rectangle. Wanted: `Exclusion::Outline` AND a
  side/measure-clip mode. "Narrow the measure" and "split the line" are
  different operations and only the second is spelled.
- **No bitmap-font path at all**, which is a large slice of game-UI
  history. Everything typographic in a period reconstruction becomes
  compensation: a body size derived from a measured advance, a face set
  BOLD because a 20 px outline regular cannot reach a 10 px bitmap's stem
  weight, condense factors fitted to ink boxes. `Element::sampling` does
  not help — the missing thing is a FACE, not a filter.
- **No binding path for text CONTENT.** `PropValue` covers floats, colors
  and fills but not `u8string`, so a counter or a timer must re-describe.
  `slot()`/`renderSlot()` is the right answer and is genuinely cheap —
  but the obvious first attempt is to hunt for `text(&output)`, and
  API.md never names slots as *the* counter idiom.

## 10. Decorations: adaptors and frames

- **Light angles are in the node's LOCAL frame.** `BevelEmboss`,
  `InnerShadow` and `util::Shadow` take an angle in node space, so on a
  rotated node you write `120 + angDeg` by hand or 514 boards light from
  514 directions. Correct at the low level, wrong as a default, subtle
  enough to ship. Wanted: a `worldLight` flag, or the node's accumulated
  rotation on `PaintContext`.
- **`lines::Line` with `parallels > 1` has no join control** — the offset
  contour rounds sharp corners, so 45° jogs come out as soft S-curves.
  The offset is built from a stroke outline anyway; expose
  `SkPaint::Join`.
- **`connector()` has no endpoint gap.** `Anchor` has one; `connector()`
  does not, so a route always runs to the node box's *centre* — and with
  `sdf::` chrome the box is far larger than the visible shape.

## 10b. Animated lines and paths: the parameter anchor

- **No angle or parameter anchor for text on a path.** `TextPath::at` is
  an ARC-LENGTH fraction. On a conic — or a dial, or any non-uniformly
  parameterised curve — the author knows the *parameter* (true anomaly,
  degrees) and arc length is wildly non-uniform, so placing three orbit
  labels cost three renders each of pure guessing. Wanted:
  `atPoint(SkPoint)`, or letting an outline generator publish its own
  parameterisation.

## 10c. Materials: a radius that means something, and a slot that is missing

- **`Material::radialUnit`'s radius is a fraction of the HALF-DIAGONAL.**
  Documented, and still a live trap: a planet terminator authored at 1.28
  puts the dark end of its ramp entirely outside an inscribed disc and the
  shading silently disappears. A min-side-relative variant, or a doc that
  reads as a warning.
- **No offset-focus radial.** `SkShaders::TwoPointConicalGradient` is the
  natural sphere-shading primitive; displacing the centre works but
  couples falloff to offset.
- **`Material` is node-local, with no world-space option.** 549 per-tile
  granite grains seeded off tile identity is correct for stone, but
  anything that must be continuous ACROSS tiles — the plaza's weathering —
  has to become a separate full-canvas multiply layer. Wanted:
  `Material::worldSpace()`, resolving the local matrix against the
  composer root instead of the node, so one material serves both.
- **A `Fill` cannot be DERIVED from a bound float at the binding site.**
  `fill(bind(&level).map(ramp))` — "this widget's colour IS its value" —
  has no spelling. Ranked honestly: `fill(&out)` with a `ch::Output<Fill>`
  DOES work live, so this is a convenience over a path that exists, not a
  missing capability. One study concluded otherwise and left the binding
  path entirely, which is why it is written down at all. Wanted:
  `PropValue<Fill>` from `(const Output<float>*, function<Fill(float)>)`,
  or a `Material::steps(colors, Bound)` value.
- **`patterns::grain` over a near-transparent base composites as its own
  luminance** rather than modulating what is beneath; the first nebula
  came back a white cloud at 15% alpha. The header warns about `noise` vs
  `grain` channels; it should also say grain wants an OPAQUE surface.

## 10d. Custom layout: a data channel, and a container that can size itself

First use of `LayoutScheme` anywhere in the repo — an HTML auto-table
layout reproducing Chrome's column widths to 0.11 px — turned up two
things at once.

- **`LayoutInput` has no per-child data channel.** A table scheme's
  span/align table ends up a member vector parallel to the `child()`
  calls, matched only by index, with nothing checking it: the study
  mis-placed a whole row once by inserting a cell in the wrong position.
  Wanted: `Element::layoutData(...)` surfacing as `LayoutInput::childData`.
- **A scheme cannot size its own container.** The concept requires only
  `place()`, so the container's height is authored beside the algorithm
  that computes it — `.height(870)` next to a `resolvedHeight()` that
  knows the answer. **API.md advertised an optional
  `measure(in) -> SkSize` and the header neither required nor called
  one**; the doc has been corrected rather than the promise quietly kept.
  Wanted: an optional `measure()` consulted by the container's Yoga
  measure func.

## 10e. Colour has no space, and gradients have six stops

Sixteen studies in, the first one whose entire content is a PALETTE — a
reconstruction of Chevreul's 1864 chromatic circle as a measuring
instrument — and it lands on something nothing else could have found.

- **`Fill` and `Material` carry no colour space, and `Composer` has no
  input space.** For a study of measured pigment values that is not a
  nicety: "I deliberately did not set a view transform" and "nobody
  thought about colour at all" produce IDENTICAL trees. There is an OCIO
  seam (`Ocio.h`, `setView`) and it is opt-in and invisible, so a value
  described as sRGB and a value described as anything else are the same
  bytes with the same behaviour. Wanted, minimally: a declared input
  space on the Composer so a mismatch is a question the library can ask,
  rather than one nobody knows to.
- **Gradients cap at six stops** — *two studies*, from opposite
  directions. `Material::linearUnit`/`radialUnit` compile to one SkSL
  pass with six chained mixes, which is a 24-run tartan sett or a 72-step
  chromatic sweep short. Both fell back to hand-written
  `PatternProgram`s. Wanted: a stop COUNT that scales the way
  `patterns::grain` scales its octaves — bake the count into the source
  and cache one effect per count, which is the rule that file already
  follows.
- **`TextPath` has no `operator==`**, deliberately (its baseline is a
  `std::function`), so a node carrying one never prunes — 72 radial
  labels re-record on every `render()`. The comparable-`Outline` fix in
  §3 covers this too if it carries a key.

## 11. `Effect` has no live uniforms

`Material` solved this with `uniform(name, &output)` and a volatility
contract. `Effect::shader(fx, uniforms)` takes constants only, so
animating a ripple phase or a bloom threshold requires a full re-describe
per frame.

## 12. `Ticker` has no fixed-timestep helper — **CLOSED**

`ticker.addFixed(hz, fn, maxCatchUp = 8)` calls `fn()` zero or more times
per frame so it advances at exactly `hz`, whatever the host draws at.

Two studies had reinvented it — a cellular automaton at 27 Hz behind the
DOOM PlayStation titles, particles at 24 — and both had to reinvent the
spiral-of-death clamp with it. The clamp's contract is now stated rather
than rediscovered: beyond `maxCatchUp` steps the backlog is **discarded**,
not carried, because carrying it makes the next frame longer, which grows
the backlog. The sim running slow for one frame is the correct failure.

## 13. Sampling, and the pixel-art path — **element leaf CLOSED**

`Element::sampling(SkSamplingOptions)` now reaches the `image()` leaf, so
pixel art, tilemaps and simulation buffers stop being silently blurred.

`Slice::filter` and `Pattern::sampling` have followed it — nine-slice is
mostly used FOR pixel art (a window chrome, a dialog border, a button
from a tile sheet) and a woven or dithered tile is the same case.

Still hardcoded to `kLinear`: `Instances.h`, `Brushes.h` and `Web.h`.
Wanted: a `sampling` field on `Atlas` and on the brush stamp. `Material::image()` always took one, which is exactly why this
was hard to find — the fix was discoverable only by diffing two
signatures.

## 14. Smaller, but each cost someone an iteration

- **`sdf::` glow eats the shape silently.** `pad()` is reserved *inside*
  the node's box, so a 300×300 box with `glowRadius: 54` renders a 0.5 px
  disc and says nothing. `sdf::minBoxFor()` is the answer and nothing
  points at it from the call site. Warn when `pad ≥ half-size`, or add
  `Style::glowOutside` that bleeds past the bounds the way `OuterGlow`
  already does.
- **`Material` has no `bleed()`.** `DecorationScheme` can declare one so
  the recording cull grows; a Material cannot, so anything painting
  outside its box needs arithmetic the caller does.
- **`Pattern` cannot pan LIVE** — the describe-time half is closed
  (`Pattern::offset(SkPoint)`, which turned out to be plumbing that
  already existed: `bake()` hands its matrix to `Material::image`, whose
  `localMatrix` always took a translation). What remains is
  `offset(PropValue<SkPoint>)` under the paint-only volatility contract
  bound transforms already have, so a conveyor or a marching weave
  animates without re-describing.
- **`patterns::stripes` is single-colour and un-phased.** A coloured
  sequence of runs — what a tartan, an awning, a ribbon or a chart axis
  actually wants — is a hand-written `PatternProgram` every time, and
  `Material::linearUnit` cannot substitute (six stops against a 24-run
  sett). Wanted: `patterns::sequence(span<pair<float, SkColor4f>>, phase)`.
- **`HyphenationOptions` has no hyphenation in it.** `enabled` and
  `penalty` read like a hyphenator; the engine breaks solely at U+00AD
  discretionaries the author typed. A legitimate contract, badly named.
- **`console::console()` admits no entrance choreography.** It builds its
  line Elements internally, so `staggerChildren()` on the returned panel
  is a no-op and "the console types out on mount" is inexpressible.
  Wanted: `console::Style::entrance`, or expose `console::line(...)`.
- **`custom()` re-records on every `render()`** — its program is an
  incomparable callable. Wanted: a `custom(key, program)` overload, or let
  `Cache::None` imply "nothing to invalidate".
- **`layouts::Radial` has one radius for all children.** A data-driven
  ring — the idiom the header claims as native — needs `radiusAt(i)`.
- **`decorations::ContourWalk` is one field from being the text-on-path
  answer for *sequences*.** It already samples the tangent and rotates to
  it; it just replays one stamp. A
  `stampAt(const PathSample&, size_t) -> optional<Element>` callback turns
  it into ruler ticks with numbers, ribbon menus, chained ornament.
- **`echo()` takes a single stamp.** Registration doubling on a light
  display face wants one each side of the glyph run, not one behind it.
- **A fixed `width()` flex child still shrinks**, and the failure is
  silent overlap. Faithful Yoga semantics, so not a bug — but `width(150)`
  reads as "this is 150" at the call site. One sentence in API.md's layout
  section (pair with `.shrink(0)` when you mean it) would pay for itself.

---

## Host and tooling

- ~~**A guest crash surfaces only as exit 139.**~~ **CLOSED** — handlers
  on SEGV/BUS/ILL/FPE/ABRT now name the sketch, the phase (setup / update
  / draw / capture), the frame, a stack, and the two causes that account
  for nearly all of them, then re-raise so the shell and any debugger
  still see the real signal.
- **A material that fails to build should be loud.** `MakeForShader`
  returning a valid effect and an empty error string, then dying at draw,
  is the worst possible failure mode.
- **The ABI skew guard has no override and no protocol.** One library
  header touch blocks every sketch until someone rebuilds the host. That
  is the right default; what is missing is a documented "who rebuilds"
  convention for concurrent work.
- **`SketchContext` dangles if captured by reference in a steppable.** It
  is a per-frame value the host rebuilds. Now documented in
  `sketch/README.md`; making it non-copyable would be better.
