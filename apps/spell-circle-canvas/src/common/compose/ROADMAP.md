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

## READ THE SOURCE BEFORE BUILDING ON ANY ENTRY, INCLUDING A POPULAR ONE

**Nine entries across two runs described a wall that was not there.** Run 1
filed four things the library already did. Run 2 filed five more, and they
are kept below marked **NEVER REAL** with the evidence rather than deleted,
because the record of why a gap was filed and why it was not real is worth
more than a clean list.

The pattern is not carelessness. **An entry is written at the moment of
hitting a wall** — when the author knows the symptom exactly and is
guessing at the cause. The symptom is nearly always real; the mechanism is
a hypothesis, and it is written down in the same confident voice as the
measurement beside it.

Three properties make a wrong entry more dangerous than no entry:

- **Citation count does not validate a mechanism.** NR-4 was promoted out
  of a footnote on *two independent citations* — a film UI and an anime
  UI, consecutive briefs, agreeing on the remedy. Both authors hit a real
  wall; both were wrong about which one. One of them found a workaround
  and wrote it up as a half-refutation without noticing that the simple
  spelling worked all along. Agreement between authors is evidence about
  the SYMPTOM and none at all about the CAUSE.
- **A wrong mechanism sends the fix to the wrong place.** NR-4 would have
  bought `Material::worldSpace()`; what is actually missing is one
  injected uniform. The entry did not merely overstate the gap, it
  described a different one.
- **Wrong entries propagate into study headers**, where they are read as
  house knowledge. Five such claims are live in shipped sketches.

So: reproduce the wall before building the fix, and prefer a probe that
reads pixels back to an argument that sounds right. Every NEVER REAL below
was settled in minutes by a twenty-line sketch.

Companion documents: `DESIGN.md` (the design canon — where the rules
live), `API.md` (surface), `STRESS_TESTS.md` (the acceptance catalog and
the measured numbers), `archive/REVIEW.md` (the earlier first-principles
pass this extends). Closed entries live in `archive/ROADMAP_CLOSED.md`
(stable numbers preserved as stubs here).

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

**3. Volatility is declared per NODE and BINARY, and authors think per
PROPERTY and per RATE.** The CDE study priced this and the number is
uncomfortable: forty bound `Fill` Outputs driving a whole desktop's
theme cost **0.33 ms/frame steady** against **0.033 ms** for the same
forty as plain values — ten times the steady-state paint to save about
1 ms on one frame in three hundred, i.e. **5.6× more total work per
palette change**. Nothing is wrong with the binding; the PRICING is.
`animated()` is per node and binary, so "repaints when the theme changes"
and "repaints at 60 Hz" are one declaration, and 80% of a desktop
inherits it through its ancestors. Wanted: a change-rate hint, or
property-granular volatility.

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

The full table — what shipped during the program, why each mattered,
and where — is in `archive/ROADMAP_CLOSED.md § Closed during the program`.

---

## 1. Bindings that cannot be shaped — *five studies* — CLOSED
A bound `Output` landed on a property raw — no scale, no offset, no
curve at the binding site; closed by `bind()` in `Compose.h`, which
normalises, maps and composes affinely and still prunes. Full record:
archive/ROADMAP_CLOSED.md §1.

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

**And a second thing the cell bakes: its SHADE.** The X-COM study
measured it — block 3 at shade 8 needs per-channel multipliers R 0.17 /
G 0.54 / B 0.42, and the best single scalar renders red 2.4× too bright,
so `tints()` cannot shade a tile at all. The faithful flyweight is
`frames = types × shades`. Together with KSP's gizmo (one cell, two arm
lengths) and the astrolabe's hairlines, three studies now point at the
same answer: **atlas VARIANTS** — several bakes of one recipe, addressed
as `(cell, variant)` — rather than more `Pool` columns.

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
- **`hitTest` cannot see a pool instance.** `instances()` is one
  `custom()` leaf, so picking a stamped cell means writing your own
  inverse projection beside the one that placed it. Wanted:
  `instancing::pick(pool, atlas, point)`. Confirmed at a second call site
  by the X-COM study, which was free to work around it only because the
  reference publishes its own inverse.
- **`tints()` MULTIPLIES, which is a trap for exact-palette work.** A
  mask cell filled with a palette's own white (#FCFCFC) scaled every
  tinted glyph by 252/255 and put seven off-palette colours on screen,
  each exactly two units low — invisible to the eye and caught only by a
  colour census. Another argument for atlas variants over more lanes.
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

**§3 IS the cost model, and this roadmap had them as separate items.**
The Chevreul study measured it. Re-describing every frame: paint 43.5 ms.
Removing ONE node — a 584×584 `patterns::grain` wash under
`.cache(Cache::Texture)` whose shape is `.outline(shapes::circle())` —
takes the same frame to **0.10 ms**. That is 43.4 of 43.5 ms spent
throwing away one texture bake, every frame, because its outline callable
cannot compare. The *same material* on a node with no `outline()` (an
1800×1200 paper grain in the same tree) prunes and keeps its bake.

So the "static SkSL pixels do not cache" finding in the closed table and
this section are one problem seen twice: a generated material's bake is
only as durable as the node's ability to prune, and an `outline()` makes
that impossible. Fixing §3 fixes the cost model for every shaped node at
once — which makes it, by measured impact, the highest-value open item in
this document.

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

## 6. No directional wipe — *three studies* — CLOSED
Three studies wanted a reveal at an angle, which `trim()` (perimeter) and
`scaleX` (squash) cannot express; closed by `Element::wipe(angleDeg,
PropValue<float>)`, paint-only and bindable. Full record:
archive/ROADMAP_CLOSED.md §6.

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
- **`Material` is node-local, with no world-space option** — *narrowed by
  a half-citation against it.* 549 per-tile granite grains seeded off tile
  identity is correct for stone, but anything that must be continuous
  ACROSS tiles — the plaza's weathering — has to become a separate
  full-canvas multiply layer. Wanted: `Material::worldSpace()`, resolving
  the local matrix against the composer root instead of the node.

  **Refuted for one artefact, and the reasoning narrows the item.**
  `eva_magi_defense` needed one continuous field across sixteen ribbons
  and got it without any new API: make the funnel ONE canvas-sized node
  whose `outline()` is the union of every ribbon. Node space then IS
  canvas space, so a single `Material::linear` serves all sixteen in one
  draw. That works because the field is axis-aligned and the geometry
  absolutely placed — which is most of the cases that reach for world
  space, and it is faster than the feature would have been.

  So the item survives only for what escapes that: a field that is NOT
  axis-aligned (the union outline still works, but the ramp has to be
  authored in the rotated frame), and geometry that is LAID OUT rather
  than placed (you cannot take the union of rects you do not know). Try
  the union-outline workaround first; cite this section only if your case
  is one of those two.
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
- **A scheme returns rects, and real placement is (rect, rotation,
  phase).** The corpus's figures that wanted per-instance rotation or
  entrance delay hand-placed instead of writing a scheme — which is why
  `Radial` succeeds at exactly its stated case and nothing reaches for
  it beyond that. (Filed from the corpus audit, archive/EXTRACT.md §1.1.)

### The two entries filed beside this one, MEASURED — one is wrong and the other is not what it said

Both were carried as separate open items. Measured with `Composer::bounds()`
on a 200×200 canvas:

```
flex-embedded stack, 60x40 child             200.0 x   0.0
absolute stack, 60x40 child, no size           0.0 x   0.0
absolute stack, children out to (70,80)        0.0 x   0.0
absolute BOX, the same ABSOLUTE children       0.0 x   0.0   <- control
absolute BOX, FLOW children                   50.0 x  50.0   <- control
slot(), 60x40 content root                   200.0 x  40.0   <- control
plain box, the same 60x40 child              200.0 x  40.0   <- control
```

**`slot()` lays out W×0 — REFUTED.** It sizes exactly like the plain box
beside it. The symptom is real and the cause is elsewhere, and my own
probe reproduced it before I found out why: **`slot(name)` stores the
name in `key`**, so `slot("panel").key("s")` silently RENAMES the slot,
`renderSlot("panel", …)` then finds nothing, and an empty container
stretched by flex is W×0. That is the same shared-namespace seam commit
`4cc6c06` fixed between `byKey` and `bySlot`; this is its remaining
authoring-side edge, and it is worth a warning at minimum — a `.key()`
on a `Kind::Slot` node is almost certainly a mistake.

**`stack()` measures 0×0 always — TWO corrections.** It measures **W×0**
when flex-embedded, which is the common case; 0×0 only when it is itself
absolute. And **it is not a `stack()` defect at all**: a plain `box()`
with absolute children measures 0×0 identically. `stack()`'s only
contribution is that it makes its children absolute for you
(`Reconcile.cpp:574`), so it always lands in a case a box lands in only
when the author opts in. The one true statement under all of it is
Yoga's: **an absolutely-positioned child does not contribute to its
parent's intrinsic size.**

So this is ONE item, not three, and it belongs in this section rather
than beside it: *a container should be able to size itself to what it
contains, including absolutely-placed content.* `applyCustomLayouts`
already implements exactly that rule for `placeFn` containers — union
the placed rects, apply per axis, only where the author left the axis
Auto and un-pinned. Generalising it is a small change and a **large blast
radius**: every flex-embedded `stack()` in the corpus goes from W×0 to
W×H, which moves its siblings in any flex column. It should land with a
full 35-study pixel sweep behind it, and probably opt-in, which is why it
is filed rather than done.

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
- ~~**Gradients cap at six stops**~~ — *two studies* — **CLOSED**. The
  count is baked into the source with one effect cached per count, which
  is the rule `Patterns.h` already followed for grain's octaves and for
  the same two reasons (a uniform-guarded loop faults across the
  split-Skia boundary; `main()` must stay monolithic). A 24-run tartan
  sett and a 72-step chromatic sweep had both fallen back to hand-written
  `PatternProgram`s for want of stops.
- **`TextPath` has no `operator==`**, deliberately (its baseline is a
  `std::function`), so a node carrying one never prunes — 72 radial
  labels re-record on every `render()`. The comparable-`Outline` fix in
  §3 covers this too if it carries a key.

## 10f. `Material::sksl()` has no child shader — but a LUT is NOT unreachable

**Corrected before anyone built on it.** This entry said "a palette LUT
is unreachable", which is too strong, and a researcher caught it by
reading `Effect`'s doc: *the layer arrives as the child shader named
`content`*, and `Element::effect()` / `backdrop()` take it. So a LUT, a
transfer curve or a posterising quantiser over **already-painted
content** works today. Verified with a probe that posterises a smooth
ramp into four bands per channel.

What is genuinely missing is a **two-source Material** — an index texture
sampled against a palette texture — because `Material::sksl()` has no
child slot even though `Material.cpp` already builds an
`SkRuntimeShaderBuilder` internally. That is a smaller and more precise
ask than the one this entry started as.

*(Seventh claim in this program that would have been recorded as
impossible and was not.)*

Related and smaller, from the same study: X-COM's shading is
`(src & 0xF0) | min(15, (src & 0x0F) + shade)` — index arithmetic with no
multiplication anywhere in the renderer, and overflow snapping to
absolute black rather than to the ramp's darkest entry. That is
expressible as SkSL over a LUT and not otherwise.

## 10g. No way to propagate a THEME down a tree — *new, and structural*

Sixteen studies drew one picture each. The first one that draws a
**system** — CDE 1.0, where the entire desktop's appearance is the output
of a published function over eight background colours — needs this in its
first hour, and there is nothing: no context, no environment, no
provide/inherit.

Components are free functions over plain data, so passing a `const Theme&`
is idiomatic and cheap *for your own components*. What has no answer is
the library's own: a `console::`, a `styles::` bundle, a decoration
nested four levels down. Each has to be handed its colours explicitly by
whoever composes it, so a theme change is a mechanical edit at every call
site rather than one value changing.

Worth noting that CDE's own answer to the identical problem was the X
resource database, and Motif's `XmGetColors` derives foreground, top
shadow, bottom shadow and select from ONE background at runtime — so the
artefact this program picked to expose the gap is itself a study in
solving it.

Wanted: an inherited value on the element tree — SwiftUI's `Environment`,
React's context — resolved during reconcile so a subtree reads it without
being handed it. This is a kernel change and deserves a designed pass,
not a bolt-on.

## 10h. Bound colours on decorations: PARTIALLY closed, and worth stating

Filed as "`Decorations.h`, `Lines.h` and `Brushes.h` contain zero
`PropValue`s, so no stock decoration's colour can be bound". Half true,
and the wrong half matters: **`PathFormat::strokeMaterial` takes a
`Material`, which can be LIVE** — a bound uniform, resolved per paint,
with `animated()` already declaring the volatility. Verified with a probe
sketch driving a stroke's colour from an `Output` with no re-describe.
A Motif bevel is 100% `PathFormat`, so the artefact that raised this is
covered.

Still true for `Slice`, `ContourWalk`, `lines::Line` and the `brushes::`
family, none of which has a Material lane. That is the entry, and it is a
smaller one than it was filed as.

*(This is the sixth claim in this program that would have been recorded
as impossible and was not, because it got checked first. The habit is now
worth more than any single feature it has produced.)*

## 10i. A shared axis has no spelling, and a drawn width cannot be audited

Both from the first study to put two panels over one abscissa — Minard's
1869 sheet, whose map's longitude axis and temperature panel's longitude
axis are the same axis.

- **Two panels sharing one scale has no expression.** A scale is not a
  layout: the panels are siblings with independent boxes, and keeping
  their x mapping identical is a discipline the author enforces by hand
  at every call site. Every small-multiple, every stacked chart with a
  shared time axis, every before/after pair has this shape.
- **`debug::widthAlong`.** `debug::coverage` answers "does this tiling
  tile"; the flow-map question is "is this band the width it claims", and
  the auditor is a min-chord measurement perpendicular to the spine. The
  Minard study wrote one to measure the 1869 engraving and then needed
  the identical function to check its own output — which is the argument:
  a study that measures its reference should be able to measure itself
  with the same call.
- **A `widthFn` Ribbon can never prune** — its `operator==` ends
  `&& !widthFn && !o.widthFn`, so the whole band re-records per
  `render()`. Same shape as §3, and the same fix would cover it: a
  comparable value, or a key that participates in equality.
- **`brushes::Ribbon` has no corner joins**, so a 14-corner route shows
  its facets.

## 10j. Winding, boxes, and three traps a colour study walked into

- **`shapes::circle()` has no winding direction, and the winding IS the
  historical convention.** On a text baseline, path direction decides
  whether glyph-up points radially IN (Chevreul's limb) or OUT
  (Nightingale's ring) — one uniform engraver's convention each way, and
  opposite in sign. `circle()` is `addOval(kCW)` from 12 o'clock, so half
  of all ring inscriptions need a hand-rolled `OutlineFn`. Wanted:
  `shapes::circle(SkPathDirection, float startDeg)`, or
  `Orient::RadialIn/Out`.
- **`TextPath`'s baseline resolves against the TEXT NODE's own box**, so
  the obvious `disc(c, R).child(text(...).onPath(...))` collapses every
  label into a blob at the text's intrinsic size. The working spelling is
  that the text leaf IS the disc. Undocumented; one sentence pays for
  itself.
- **`snapshot()` sizes by the root's CHILDREN, not the root's own dims** —
  stated only in a comment inside `Instances.h`. A probe of
  `box().width(32).fill(…).effect(…)` read back a colour implying an
  exponent of 1.82; the same content wrapped in a shell reads 2.20. For
  the read-your-own-output-back pattern that verification depends on,
  that is a silent wrong answer.
- **`Material::sweep` clamps outside `[startDeg, endDeg]` instead of
  wrapping**, so `sweep(c, stops, 90, 450)` — the obvious way to start a
  hue wheel at red — draws a quarter of the ring in the first stop's
  colour with no diagnostic.
- **`lines::concentric` cannot place a ring at a stated radius.** Radii
  are `inner + (reach − inner)·k/rings` with `reach` the bbox
  HALF-DIAGONAL, so on a `circle()` node the outermost ring always lands
  at R√2 — outside the shape, clipped away. Same class as §10c's
  `radialUnit` trap, and a two-circle limb is unspellable.

## 10k. Themed textures, and text that cannot be asked for

- **`Pattern` bakes its colours**, so a themed texture cannot be bound.
  `Pattern::tile` memoizes an `SkImage` on shared state and the colours
  live inside the `PatternProgram`, so a themed backdrop needs one
  `Pattern` per palette AND a re-describe to swap them — the only reason
  the CDE study re-describes at all. §10f is the same wall from the other
  side: two-colour mask × palette LUT is the natural spelling and wants a
  child shader. Wanted: `Pattern::recolor(span<Fill>)`.
- **Text COLOUR has no `PropValue`** — third route to §10c.
  `TextStyle::paint.foreground` is an `SkPaint`, and `textFill(Material)`
  CAN be live (checked before filing) but only as float uniforms plus
  hand-written SkSL for what is a solid colour. That is why a theme
  switch still repatches every label even in the bound build. Wanted:
  `text()` taking a `PropValue<Fill>` for the glyph pass.
- ~~**Text edging cannot be asked for**~~ — **CLOSED** as
  `ShapingStyle::aliased`. Skia takes edging from the FONT, never the
  paint, so `setAntiAlias(false)` was silently ignored and a period
  reconstruction left this engine entirely for its labels. One field, not
  a face.

## 11. `Effect` has no live uniforms

`Material` solved this with `uniform(name, &output)` and a volatility
contract. `Effect::shader(fx, uniforms)` takes constants only, so
animating a ripple phase or a bloom threshold requires a full re-describe
per frame.

## 12. `Ticker` has no fixed-timestep helper — CLOSED
Two studies had each reinvented a fixed-rate accumulator and its
spiral-of-death clamp; closed by `Ticker::addFixed(hz, fn, maxCatchUp)`
with the discard-the-backlog contract stated. Full record:
archive/ROADMAP_CLOSED.md §12.

## 13. Sampling, and the pixel-art path — **element leaf CLOSED**

`Element::sampling(SkSamplingOptions)` now reaches the `image()` leaf, so
pixel art, tilemaps and simulation buffers stop being silently blurred.

`Slice::filter` and `Pattern::sampling` have followed it — nine-slice is
mostly used FOR pixel art (a window chrome, a dialog border, a button
from a tile sheet) and a woven or dithered tile is the same case.

`Atlas::filter()` has followed — instancing's biggest real use is
tilemaps and sprite sheets, which are pixel grids. **All five blessed
image paths now have a knob except two:** `Brushes.h`'s stamp and
`Web.h`'s frame, neither of which has yet had a study ask. `Material::image()` always took one, which is exactly why this
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

## 15. A node's OWN paint cannot be cached apart from its volatile children — CLOSED
A node's expensive static own paint shared its volatile children's
cacheability verdict and lost; closed by the split bake in `Paint.cpp`
(`genesis_fire` p50 74.16 → 23.60 ms, 35 of 35 studies
pixel-identical). Full record:
archive/ROADMAP_CLOSED.md §15.

## 16. Stamped-brush bakes are cached in the VALUE, so rebuilding the value re-bakes everything

`PatternBrush`, `ScatterBrush` **and `ArtBrush`** hold their `snapshot()` of the tile art
in a `shared_ptr<Cache>` that is a member of the brush. Copying the brush
shares the cache; CONSTRUCTING one gets an empty cache. So a brush built
inside a per-frame describe re-bakes every tile every frame, and each bake
is a full reconcile + layout + record pass through `snapshot()` — one
study measured **eighteen of those per frame** from a brush constructed
inside the function passed to `renderSlot()`. `ArtBrush` was missing from
this entry's first draft and has the most expensive bake of the three —
it bakes at 2x and builds a triangle-strip ribbon per contour.

Documented in the header now, which stops it being silent but does not
stop it being a trap. The real fix is a bake cache that does not live in
the value. The obvious spelling — a process-wide map keyed on the art
Element's node pointer, which is already the cache key — is unsafe as
written, because a freed Element's address can be reused and the next
brush would silently inherit the wrong art. It needs either a weak handle
to the node or a generation counter. Worth doing: this is the only place
in the library where re-describing costs raster work rather than a diff.

## 17. `withKeyframes` is live volatility even where its value is constant — CLOSED
Keyframe holds and settled easings repainted every frame while provably
constant — 29 ms of a 38 ms frame in one study; closed by
`Instance::scalarMemo` in `Paint.cpp`. Full record:
archive/ROADMAP_CLOSED.md §17.

## 18. A `Cache::Texture` node with a blend allocates a saveLayer to composite ONE blit

Measured by a study: **3.45 → 0.24 ms** on one node, by removing the need
for `kPlus` rather than by any library change.

`leafDirectBlend` — the carve-out that routes a leaf's blend and opacity
straight onto its own paint instead of a device-clip-sized `saveLayer` —
explicitly excludes `cacheMode == Cache::Texture`. **That exclusion is
load-bearing as written**, and not an oversight: the bake is taken with
plain srcOver into a transparent layer ("bakes isolate"), so if
`leafDirectBlend` were true the node's blend would never be applied to
anything at all and would be silently dropped. `needsLayer` is currently
the only thing applying it.

But the layer is the wrong mechanism for this case. A texture-cached node
composites exactly ONE draw — its blit. Compositing one source image with
alpha `a` and blend `B` through a saveLayer is the same operation as
drawing that image with a paint carrying `a` and `B`, minus a full-canvas
intermediate buffer and one extra rounding of every pixel. The direct
route is both cheaper and slightly *more* accurate.

The fix is therefore not "relax `leafDirectBlend`" but "hoist the
decision": determine before the layer whether this node will actually
take the texture branch, and if so skip `needsLayer` and pass the node's
opacity and blend into the blit's paint instead. The predicate must be
exact — a node that fails the texture branch and falls through to the
picture or live path with `needsLayer` already suppressed would lose its
blend entirely, which is the failure the current exclusion prevents.

Not attempted yet because the predicate has to be exact and the function
it lives in has had three defects this run that each looked like one.

## 19. Materials and effects have no spatially-varying parameter channel

**Two independent citations, from a film UI and an anime UI**, for the
same shape of gap — which is what promotes it out of a note under §10c.

- `Material` is node-local with no world-space option (§10c): a field
  that must be continuous ACROSS separately-laid-out nodes has to become
  one canvas-sized node whose `outline()` is the union of its parts. That
  workaround is real and often better than the feature (see §10c), but it
  requires the geometry to be absolutely placed and axis-aligned.
- `Effect` has a single scalar parameter for the whole node. A blur whose
  sigma varies with node-local position — a depth-of-field falloff, a
  tube's curvature, a lens edge — has no spelling. The workaround route
  IS reachable (the node's own layer arrives at a runtime effect as the
  child shader `"content"`), but SkSL has no cheap dynamic loop bound, so
  the kernel must be sized for the worst sigma anywhere in the node and
  that cost is paid at every pixel.

Wanted, and the two citations agree on the shape: a parameter that is a
FUNCTION OF POSITION rather than a constant, resolved by the library into
something with the right cost model — for the blur case, a 2–3 level
pyramid blended by the parameter, which is O(1) in the sigma range
instead of O(sigma²) per pixel.

## 20. A bound property that has FINISHED is live volatility forever

Filed by `dunhuang_star_chart`. Every `window()` on a master clock stays
live after its value has been pinned at 1.0 for ten seconds; the node is
`subtreeVolatile` for the rest of the run, and the only cure available
was a hand-rolled `settled` flag plus a full `render()`, which costs a
22 ms frame to save a smaller one.

**Check this against `Instance::scalarMemo` before building anything**,
and the answer is that it is NOT the same mechanism, though it is the
same family. §17 memoises the animated CONTENT SCALARS — trim, wipe,
glyph progress — by comparing the numbers a recording was baked with
against the numbers this frame resolves to. It is a per-slot compare over
five floats with a fixed schema. A bound property is a `choreograph::Output`
whose value can be anything the author maps it to, read through
`resolveFloat` at paint time, and there is no equivalent of "the numbers
the recording was baked with" for the general case.

Two candidate shapes, and the second is better:

- `bind(&out).window(a, b).settleAt(b)` — past `b` the property becomes a
  constant and the node re-caches. Explicit, and a new knob for something
  the library could work out.
- **Extend the measured-stability rule that temporal promotion already
  uses for live materials.** `liveStableRate` is an EMA of "did this
  resolve to what was already baked" and promotion is gated on it. The
  same question asked of a bound property's VALUE gets a settled reveal
  re-cached with no new API and no author knowledge — and, exactly as
  with the material, it also handles the case the knob cannot: a binding
  that is *nominally* live and happens to be holding still.

Note this shares a slogan with §15 and §17 — "provably not changing,
believed to be changing" — and the family has now three times shared a
slogan and not a mechanism. Read the source before merging any two of
them.

## 21. `console::Style::visibleLines` gives no height

Filed by `dunhuang_star_chart`: fitting three `LineRing`s in one panel
meant hand-tuning panel height against font size × line count.
`compose::measure()` answering for a console element would close it.
`Console.h` is the extraction layer's file; coordinate before touching.

## 22. Two names for one identity, and it contaminates the guard

CLOSED 2026-07-27 (9339996): headless captures are written under the
REGISTRY name via `registryName()` — the selector spelling is the
capture spelling by construction. Blast radius: all 33 study plates
renamed to their stems, plus the three catalog scenes whose display
name had drifted (tilemap, nineslice, ui_particles). Nothing in-repo
consumed the old spellings.

Five instances in one program of a change landing on one path and not its
sibling — four corner scanners, `Promotion::Filtered`'s four causes,
the GPU `showStats`, a sweep tool whose model of the API predated
`rect()`/`at()` becoming edge setters, and §15's split bake missing the
`upright` gate its own neighbour had.

The gallery adds the sharpest one, and a corollary. A scene has a
SELECTING name and a `Scene::name()`, and the capture guard written
specifically to catch that — `[ -f "$OUT/$s/gallery_$s.png" ]` — was
itself written assuming there is one spelling. It reported three misses
on captures that had succeeded.

> **A two-name identity contaminates the code written to defend against
> it.** The guard is written by the same person holding the same wrong
> model, so it fails in the same direction and returns a confident answer.

The cheap general fix is to write the PNG under the REGISTRY name, or to
expose the mapping so that nothing downstream has to guess.

## 23. `SkRect::join` early-outs on an empty rect, and a single point IS an empty rect — CLOSED

CLOSED 2026-07-27, confirm-and-close: the corpus sweep found exactly
one join-accumulation site (minard_1869.cpp:2641 — seed-with-first,
one of this entry's own two remedies, safe), dunhuang's original
pattern removed entirely, penrose's MakeEmpty values dead
(setBounds replaces). No live bugs, no latent traps.

Not a compose gap, and it cost `dunhuang_star_chart` a full render.
Accumulating a bounding box point by point —
`bounds.join(SkRect::MakeXYWH(x, y, 0, 0))` — leaves the rect inverted
and every node inside it silently draws nothing. `SkRect::join` returns
early when the ARGUMENT is empty, and a zero-extent rect is empty by that
test even though the point is real.

Use `MakeXYWH(x, y, 1, 1)`, or seed the accumulator with the first point
and `joinPossiblyEmptyRect`. Documented in `sketch/README.md` beside the
bounds discussion.

## 24. `layouts::stickerScatter` is DELETED, and the record is the point — CLOSED
A scheme that parameterised a design judgement, with zero users and a
written refusal from the one scene that wanted it; deleted, with the rule
about what belongs in `Layouts.h` kept where the code was. Full record:
archive/ROADMAP_CLOSED.md §24.

## 25. Ten documentation defects, and what they say about doc tests

A documentation site built against these sources found ten defects in
`API.md`, `EXTRACT.md` and `STRESS_TESTS.md`. Two of them were **code
that does not compile**: `PathFormat{.effects = …, .paint = …}` at two
sites, where the header has `effect` (singular, one `SkPathEffect`, not
a chain) and `strokeFill`. That is the primitive an author meets on page
one.

**The lesson is about the guard, not the defects.**
`ComposeDocs.EverySignatureInTheLineAndBorderDocsCompiles` exists
precisely to make a non-compiling documented call impossible, and it did
not catch these — because `PathFormat` is documented in a section that
test does not cover.

> **A doc test that covers one section proves the mechanism works and
> leaves every other section exactly as wrong as before.** It is worse
> than no test in one specific way: the suite now contains something
> named as though documentation is checked.

The same shape as the four corner scanners, `Promotion::Filtered`'s four
causes, the sweep tool that did not know `rect()`/`at()` had become edge
setters, and §15's split bake missing the `upright` gate its neighbour
had. Sixth instance. `ComposeDocs` now covers the caching/promotion,
material, decoration, shape, layout, text-path and pattern surfaces, and
**its own first draft did not compile in six places** — `Material::color`
(it is `solid`), `Material::Stop` (a free struct), `glowUnit`'s two
colours (it takes a stop vector), `util::shadow`'s argument order,
`PathSample::pos` (it is `position`) and `t()` for `text()`.

The rest, fixed: `PathSample::fraction` is per CONTOUR (the same trap
`Ribbon::widthFn` documents under `trim()`); `patterns::grain` has five
parameters and `stretch` aliases past ~8; `Layouts.h` ships seven
schemes and `API.md` listed three, omitting `BaselineGrid`, the only
consumer of `LayoutInput::childBaselines`; `TextPath::Orient` has three
values and `API.md` had two; a caching paragraph was spliced mid-sentence
across four paragraphs; and `<sigilcompose/compose.h>` / `util.h` were
lowercase at four sites — **fine on macOS, broken on Linux**, which is
the kind of defect that cannot be found by anyone able to find it.

`TextPath::atDeg` was struck rather than shipped: `kit::Frame::fraction()`
answers the same need from a better place, since θ→arc-length-fraction is
a property of the FRAME and not of text, and every consumer of a circular
contour wanted it. A field on `TextPath` would have been the fifth copy
of the arithmetic wearing the name of its first caller.

## 26. Two studies are not reproducible captures, and every pixel sweep will blame the wrong change — CLOSED
Two studies drew their own measured timings into their own plates, so any
pixel sweep reported them as changed by a patch that changed nothing;
closed by `ctx.measured()` / `--deterministic` (33 and 20 differing
pixels → 0). Full record:
archive/ROADMAP_CLOSED.md §26.

## 26b. `renderSlot()` on a name that does not exist was SILENT — CLOSED
An unknown slot name failed silently and presented as a W × 0 layout bug;
closed by a one-time warning that lists the slot names that do exist and
names the `slot()`/`.key()` rename trap with the caller's own
string. Full record:
archive/ROADMAP_CLOSED.md §26b.

## 27. A default that encodes a judgement about the caller's art cannot be changed compatibly

`f706f5d` (2026-07-22 12:03) taught `PatternBrush`'s corner scanner to
bisect its own bracket **and** added `cornerAlign`, defaulting to
`Bisector`. **Both halves are right.** The combination silently rewrote
every corner stamp in the corpus.

Before that commit the bisector was computed by re-probing at `d±2` from
a point already past the vertex, so both probes landed on the **same leg
twice** and every corner in every study behaved as `Outgoing` — not as a
choice, as a bug. Art authored against it is authored in the outgoing
frame. After 12:03 that art is stamped on the true bisector, off by half
the turn angle.

Source-compatible. No warning. No diagnostic. It edited no file its
victims owned. And the commit's own subject line is about caching, so
`git log --oneline` gives no hint that corner geometry moved in it.

### Two casualties, both found by looking rather than by any tool

- **`thaumonomicon`** (`6962173`) — elbows of pipe 45° off, and whole
  edges unrecognisable rather than merely tilted, because a 2×2-cell
  route is *all corner*: `cornerRoom = 2 × 1.5 × kCell = 216` against a
  trimmed path of exactly 216, so both side runs come out to zero tiles.
  Fixed `aa11a6b`.
- **`sigillum_aemeth`** (`0ea8aa3`) — Dee's "little Crosses" at the
  corners of the seven angle plates, and the worst case of the class for
  a structural reason: **every corner of an annular sector is a right
  angle**, so the bisector sits 45° from *both* legs — and a Greek cross
  has 90° symmetry, which makes `Outgoing` corner-agnostic and `Bisector`
  uniformly, maximally wrong. Twenty-eight crosses had been twenty-eight
  saltires, through a review. Fixed `0c2c36e`.

**Cleared by the same method:** `thunder_fulu` (`a932bd1`, 12:36 — the
commit that ADDED the file also edits the post-change `Brushes.h`, so its
art was drawn against current behaviour). Forcing `Outgoing` rotates six
stubby lozenges 13–35° and nothing snaps.

### The recipe — thirty seconds, and the only thing that settles this class

> Set the other value in a scratch copy, re-render the same `--at`, and
> diff. **If nothing moves, the art is rotationally forgiving and the
> study is *proved* clean; if corners snap, it was broken.** Render a
> third variant with `corner` unset to mask the stamps, and you can
> **measure** the rotation instead of judging it.

Judging by eye failed twice on the Sigillum plate, by the person who
went looking for exactly this.

### What shipped

1. **The changelog line, with the SHA and an imperative.** The header
   already said "Until the tangent-break search learned to bisect its own
   bracket, every corner silently got `Outgoing`" — the fact, in the past
   tense, with no date and no instruction. **Neither author read that
   sentence as being about their file.** It now names `f706f5d`, its
   date, and what to do: *if your art predates it, it is aligned wrong
   and you must ask for `Outgoing` explicitly.* Both diagnoses become a
   header read. The audit recipe is in the header too.
2. **The default now announces itself.** `cornerAlign` is a
   `std::optional<CornerAlign>`; unset still behaves as `Bisector` and
   still compiles at every existing designated-initializer call site, but
   a brush with corner art and no explicit alignment prints a one-time
   warning naming both cases. **The diagnostic is the change; the
   behaviour is untouched** — verified below. A required constructor
   argument would be better and costs a source break at all five
   consumers; this is the version that could land today, and after the
   two fixes every corpus consumer sets the field explicitly, so the
   warning fires only for authors who have not yet thought about it.
3. **The rule, stated where authors meet it.** `Bisector` is for an
   ORNAMENT — art symmetric about its own bisector, one drawing serving
   four corners. `Outgoing` is for anything with a distinguishable entry
   and exit: an elbow of pipe, a flow tick, an arrow turning a corner, a
   cross whose arms are meant to lie along the edges. **That second class
   is not exotic — it is two of the five corner consumers in this corpus,
   and both shipped broken.** With the corollary from `aa11a6b`, because
   it is the reason people reach for `Bisector` in the first place:
   bisector alignment does **not** buy you one art instead of two. The
   arms sit at `(turn/2, 180 − turn/2)` off the bisector and mirror with
   the sign of the turn, so handedness costs an art either way.

Verified pixel-neutral the way §26 says to: same sketch sources, the only
variable being `Brushes.h` with and without the change. **33 of 35
studies byte-identical; the 2 that differ are `genesis_fire` and
`slitscan_2001`, which differ from themselves.**

### The general lesson

> **A default that encodes a judgement about the caller's art cannot be
> changed compatibly, and "source-compatible" is not the test. The test
> is whether any existing caller's OUTPUT changes.**

`cornerAlign` cannot have a correct default because the correct value
depends on what the art looks like, and the library cannot see the art.
Any default is a guess made on the author's behalf, silently, in a field
they never typed.

This is the eighth instance of the session's recurring shape — a change
that landed correctly in one place and silently altered its siblings —
and the first where **both halves were individually correct**. That is
what makes it the hardest one to catch: there is no wrong line to find.

## 28. A documented limit is a CLAIM, and claims in this codebase have a poor record

`Console.h` stated that a `thunder_fulu`-style column plate needed a
third sizing mode, because `ringExtent = 0` gives each ring `grow(1)`
where the hand-built plate lets them take content height. Measured:
**zero differing pixels at four phases, twelve lines shorter.** The
mechanism is `shrink`, defaulting to 1 — a console plate sizes
`visibleLines` to fill its panel, so the rings always OVERFLOW the
interior, and shrink distributes the deficit to exactly the sizes grow(1)
distributes the surplus to. They converge for every plate in the corpus.

Its author's framing is the entry:

> Not a change that landed on one path and not its sibling, but **a limit
> asserted from reasoning and never tested** — which is worse in one
> specific way: **it reads as a finding, so nobody re-runs it.**

That is the distinguishing feature and it is worth separating from the
sibling-path family. A sibling-path defect is latent and someone
eventually trips over it. A false documented limit is *actively load
bearing*: it is cited, planned around, and used to justify writing the
workaround again. It compounds.

The record it belongs to, which is now long enough to be a rule:

- Four run-1 roadmap entries described capabilities the library already
  had.
- Four more in run 2 described things that were not what they said.
- `Material::worldSpace` was cited independently by two studies, and
  measurement refuted it.
- A study refuted its own brief by finding `brushes::Ribbon::widthFn`
  does what the brief called impossible.
- §7 was wrong: `PathFormat` always had its own trim window, and two
  studies rebuilt one.
- `slot()` "lays out W × 0" (§10d) — refuted.
- `stack()` "measures 0 × 0 always" (§10d) — twice wrong.
- The device bake "is exact at any angle" (§25/§26 era) — argued by me,
  accepted by the manager, refuted at 5 pixels and then at 1157.
- And this one, which had shipped in a header as guidance.

> **A documented limit is a claim. Before building on one — especially a
> popular one, especially your own — spend the thirty seconds it costs to
> try the thing it says is impossible.**

This is why the top of this file says READ THE SOURCE BEFORE BUILDING ON
ANY ENTRY. The instruction is not scepticism about the authors; every one
of these was written by someone who had just been in the code. It is that
a limit is the one kind of claim nobody re-tests, because re-testing it
means attempting something you have been told will not work.

## 29. The profiler is blind on GPU, and every caching decision rode on it

Every number this program measured — the 60 FPS gate, the 28×, the corpus
table, the promotion thresholds — was `SkSurfaces::Raster`. The product
runs Graphite/Metal. Six scenes measured on GPU: five fine (`chladni tab1`
3.4 ms, `daemon console` 1.0), one catastrophic — `kumiko asanoha`
**113 ms, 9 fps**.

The root is structural and it is the sentence that ties the whole session
together. `Composer::profile()`'s `selfMs` is a wall-clock bracket around
`draw`. Under Graphite, `draw` RECORDS commands; the GPU executes them
asynchronously later. **So selfMs measures op-recording time, not GPU
execution** — a node can read 0.1 ms here and cost 20 ms on the GPU. And
every caching decision is built on that number: the 1 ms promotion
threshold, the stability EMA, the temporal re-bake gate. They describe the
raster machine, not the one the product ships.

This is the same failure as the leaf-measurement bug (§ "Promotion could
not SEE a leaf"), the vacuous tests, and the y2k picture-replay bug, in
one sentence: **the instrument measured a different machine than the one
that pays.**

### Promotion is INERT on GPU, not harmful — measured, against a shared prior

The natural fear was that bakes REGRESS GPU. They do not. Same binary,
`--gpu`, promotion on vs off:

```
kumiko GPU, work ms:   ON 112.01  144.17  125.11
                       OFF 111.27  123.28  124.09
```

The run-to-run variance dwarfs any on/off delta. Promotion barely fires on
GPU — it rarely crosses the recording-time threshold — so ON ≈ OFF. When
it would fire, the bake+sync+upload costs more than the ~0.8 ms recording
it replaces. Dead weight, not a regression.

### What shipped

- **Backend-aware default: automatic promotion OFF on Graphite/GPU**
  surfaces, unless the host calls `setAutoTexturePromotion` explicitly
  (`Composer::draw` detects the backend via `canvas.recorder()` /
  `recordingContext()`; the reason is in `ComposeRuntime.h` beside
  `autoPromoteEffective`). The global switch overrides both ways, so a
  future GPU-timestamp cost model can re-enable it with evidence.
- **`ComposeGallery --no-promotion`** forces it off explicitly on either
  backend, so the ledger A/B is reproducible on a real binary.
- **The GPU headless header now says the profiler is blind**: "per-node
  profile times are RECORDING time on GPU — trust the work-ms column."
  One line, where the next person would otherwise trust `selfMs`.

Still open: a GPU counterpart to the 60 FPS gate in the ledger, and a
per-node GPU cost measurement (timestamp queries) if promotion is ever to
be justified on GPU.

## 30. kumiko's 113 ms GPU is static shader ALU — routes to an EXPLICIT bake — **SHIPPED as `Cache::Group`, PIXEL-VERIFIED, NOT YET TIMED**

> Shipped as `Cache::Group` in `Compose.h` / `Paint.cpp`, with the subtree
> value memo that holds it. **The performance claim below (115.69 → ~4.95 ms)
> is still a TARGET, not a result** — the session ended with the machine
> under a corpus sweep and no reading was taken. Do not quote a number for
> this until someone takes the before/after pair back to back on a clear
> machine. What IS established is correctness, and the shape of the win:
> kumiko's lattice reads `[group]` at **0.49 ms of recording time on a
> 1400x1000 node that was 523 live pictures**, with `0 cache writes` in
> steady state.
>
> ### What it is
>
> `Cache::Group` on a container bakes the container AND its children into
> one unrotated device-space layer and blits it at an integer offset — the
> construction promotion and the split bake already use. The children's
> rotations, bevels and mutual compositing all resolve INSIDE the bake at
> full precision, which is exactly why it is pixel-safe where the per-strip
> `Cache::Texture` this section proposed was not (that isolated each piece;
> 34% of pixels moved).
>
> **The bake was the easy half. The invalidation is the feature.** kumiko's
> strips are `subtreeVolatile` forever — not because their content changes
> but because each carries a live opacity/scale BINDING that never
> disconnects. So the group is held by a SUBTREE VALUE MEMO: every frame,
> gather every animated scalar below the node (transform slots, opacity,
> trim/wipe/glyph, the fill lerp), compare with last frame's vector, and on
> any difference DROP the bake and paint live. §17's `scalarMemo`
> generalised from one node's content scalars to a whole subtree's bound
> transforms. Exact comparison, not a hash: a 64-bit digest of 2000 floats
> is a small chance of blitting last second's picture forever.
>
> Refusals are computed in `computeVolatile` (`groupSafe` / `groupRootOK`)
> and printed once per node: a live material, an animated decoration, an
> animated image, a bound `fill()`, a variable-font drive, a `Cache::None`
> descendant or a non-srcOver blend below the root all refuse the bake
> outright, because a float comparison cannot see any of them.
>
> ### THE FINDING, and it is bigger than this feature
>
> **Every pixel-identity test in `ComposeTest.cpp` composites over the
> host's opaque BLACK clear — and premultiplied srcOver over opaque black
> is `result.rgb = src.rgb`, with the destination term multiplied away. So
> none of them can see the one error an isolating bake actually makes.**
> Measured on the same 24-board rotated lattice:
>
> | reference | differing pixels | peak |
> |---|---|---|
> | over black | **0** | 0 |
> | over a lit ground | 1847 / 57600 | 2/255 |
> | over a lit ground, reference ALSO isolated in a layer | **0** | 0 |
>
> The residual is one extra 8-bit requantisation: an antialiased edge lands
> in the bake as premultiplied coverage rounded to 8 bits and composites
> from there, where live paint composites the same edge in one step from
> full-precision coverage. It is **not** the rotation (a flat-colour fill
> shows it just as strongly, 1474), **not** the shader's inverted CTM, and
> **not** the integer device offset (snapping the offset to zero changed
> nothing). It applies to `Cache::Texture` and to automatic promotion
> exactly as much as to `Cache::Group`; it has simply never been visible.
>
> So the claim the tests make is the exact one: **a group bake is byte
> identical to compositing the same subtree through a layer**, which is a
> thing an author can already ask for by hand. Both spellings are asserted.
>
> ### One real bug the tests caught, which nothing else would have
>
> The first bake rect was the node's paint bounds, unclipped. A lattice of
> rotated boards with bevel bleed overruns its own canvas on all four
> sides, and a bake rect LARGER than the device clip hands Skia a different
> clip to rasterize antialiased edges against — §25 measured this on the
> promoter and it is worth tens of levels. Here: **peak channel delta 12
> before intersecting the bake rect with `getDeviceClipBounds()`, 2 after.**
> Nothing visible is lost, since content outside the device clip never
> reaches the canvas either way.
>
> ### The positive control, run
>
> The drop-on-tick was deliberately defeated in `Paint.cpp` (the memo still
> computed, never acted on — the most plausible way this breaks), rebuilt,
> and the suite re-run. Three tests failed:
> `GroupDropsTheBakeOnTheFrameABindingTicks` directly, and both pixel tests
> at **238 of 241 frames, worst frame 17815 pixels at peak 217/255** —
> against the honest residual of 1847 at peak 2. Two orders of magnitude in
> both count and peak. Restored; 321/321 green.
>
> ### On kumiko itself
>
> `.cache(Cache::Group)` on `lattice()` (523 strips) and on `frame()` (4
> mitred keyaki members, the next two cost centres once the lattice stopped
> being one). Both read `[group]`. **Byte-identical at seven phases across
> the 6.4 s loop** — 0.6 / 1.9 / 2.74 / 3.5 / 4.2 / 5.0 / 6.3 s, covering
> the entrance, the seating beat, the hero and the wrap — 0 differing
> pixels, peak 0, at 1400x1000. The remaining top cost is the backlight
> (990x630 radial, 3.20 ms of picture replay); it is static and wants
> `Cache::Texture`, and is untouched.
>
> ### Generalises to
>
> "Many small rotated/blended pieces forming one static assembly."
> `sigillum_aemeth`'s plates, `thunder_fulu`'s stations and
> `thaumonomicon`'s frame all rhyme with kumiko and were not tried.

Characterised, because §29's default turns promotion off on GPU and
therefore does NOT fix this — kumiko needs a different remedy.

`kumiko_asanoha` is a hinoki asanoha ranma: a lattice of ~dozens of
strips, each a box carrying a wood-grain **SkSL** fill plus a
`BevelEmboss` image-filter arris. The opacity/scale of each strip is
bound to a 6.4 s entrance loop, so the strips are `Volatile` and never
auto-promote — on ANY backend. They cache as PICTURES, and a picture
replay re-runs its shaders every frame. On raster that was 331 ms until
the CPU promotion pass; on GPU the shaders re-run on the GPU every frame
and it is 113 ms.

**The experiment that settles the category** (manager's hypothesis):
`.cache(Cache::Texture)` on the strip node, GPU-measured:

```
kumiko GPU:   baseline 115.69 ms  →  strips Cache::Texture 4.95 ms   (23x)
```

So it is **static shader ALU** — not a software fallback (a texture blit
avoids the shader, and the cost vanishes) and not overdraw (the lattice
barely overlaps). It is the "a picture is not a pixel cache" finding,
alive on GPU, on content auto-promotion cannot reach because the binding
keeps it Volatile.

**Route: perf-pass, under its existing bar** (static-only, explicit,
steady-state-writes-verified, pixel-checked) — with one caveat that the
bar will catch: the naive per-strip `Cache::Texture` above is **not
pixel-safe**. It changed 34% of pixels (peak 0.57) because the bake
ISOLATES — the `BevelEmboss` arris and the compositing where strips abut
resolve differently baked-in-a-layer than composited-live. The fix is to
bake at a granularity that does not split the bevel or the joints (the
settled lattice as one static layer is the likely shape), which is
exactly the judgement the pixel-check bar exists to force.

The general lesson is §29's: this was invisible for the whole program
because the gate was raster, and the one scene where a picture's shaders
dominate is the one that a raster gate flatters and a GPU punishes.

### Handoff record (2026-07-22; full session detail in archive/HANDOFF.md)

Shipped and pixel-verified; **NOT TIMED — no performance number exists
for `Cache::Group`.** The 23× above is the per-strip experiment's
hypothesis (itself not pixel-safe), never a Group result. The one
baseline taken (kumiko 111.88 GPU) is QUARANTINED — contended machine;
do not use it, or use it as half of a pair. Verified: the drop-on-tick
memo (asserted with a positive control that defeats the drop and
requires the test to FAIL), pixel identity on kumiko at seven phases
across the full loop against a stripped copy of the same sketch.
Outstanding measurements: before/after back-to-back on a quiet machine,
same commit and thermal state; the **feature-present-but-not-opted-in**
middle point (separates "helps kumiko" from "taxes everyone"); CPU
raster (the bandwidth risk — a large bake is SAMPLED every frame
whether or not it re-bakes; the 0-writes proxy proves nothing about
bandwidth); the untried stretch scenes (sigillum_aemeth, thunder_fulu,
thaumonomicon).

What the next person must not assume:

1. **"Byte-identical" is not achievable in general** — every pixel test
   over opaque BLACK is blind to isolation error (srcOver's destination
   term vanishes). Over a lit ground the honest residual is 1847/57600
   px at peak 2/255, and exactly 0 when the reference is itself
   isolated in a layer. That is the standard Group holds — and
   `Cache::Texture` and promotion always had the same property.
2. **Do not remove the bake-rect clip** in the group branch of
   `Paint.cpp` (peak 12 → 2 on content that overruns its canvas).
3. **`groupRootOK` stays out of `memoized`** in `computeVolatile` — a
   volatile group root must fall through to LIVE paint, not replay a
   stale picture no over-black test would catch.
4. **The refusal list in the header is tested** — any new limit added
   to that doc comment needs a case in the refusal tests (§28).

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
- Small open items (2026-07-22 handoff): **persona menu**'s selection
  wedge occludes the "EQUIP" label — a spacing decision wanting a P3R
  reference (the real menu pushes neighbours clear), not a paint bug;
  **stroke_atlas** still carries ~40 dead `.absolute()` calls (1,327
  removed corpus-wide; a bare `.absolute()` with NO adjacent edge setter
  is load-bearing — check before deleting); the residual 1-LSB
  background diff on y2k/aero is flagged benign, untreated.

## 31. A still is a CLAIM about an animation — and the harness was choosing it

**Status: closed for the mechanism (61c8963, ffc8b04), open for the audit.**

§26 made gallery captures reproducible: two consecutive sweeps had differed
on 15 of 45 scenes, so no plate had ever been verifiable frame to frame.
The fix derived the capture frame from the probe/warm/sample caps, and it
was correct. It also answered only half the question.

The frame it derives is a fixed **t = 6.0 s** — 360 frames at dt = 1/60,
which is `kProbeFrames + kMaxWarmFrames + kMaxSampleFrames` and nothing
else. No scene was authored to look best at an arithmetic identity, and
scenes whose loops run longer than 6 s are photographed wherever that
lands.

**How it surfaced.** `black watch` weaves a tartan, proves its arithmetic,
then turns five *registered* shade families over one another — Modern,
Ancient, Muted, Weathered, Reproduction — before returning to Modern for
the hold. Its cycle is 8 s, so t = 6.0 s is loom 0.75: dead centre of the
WEATHERED card. Every still ever taken of that plate showed brown and
olive cloth under a title reading BLACK WATCH (GOVERNMENT), beside its own
shade cards showing the navy and green everyone knows. It was reported as
an incorrect blending layer on top. Sampling settled it: the panel is
`#3D2A20` against the weathered card's `#4C3428` — exactly itself under
the multiply grain. **Right frame of the wrong beat.**

**Why it survived review.** From the still alone, a wrong-looking frame
and a wrongly-*chosen* frame are indistinguishable. It went through several
review passes, including one where a person looked straight at it and
correctly said something was wrong — and the wrongness was unattributable
without reading the loop and recompiling, so it stayed.

This is the second instance this session of the same shape: **a visual
report named a mechanism, and the mechanism was innocent.** `aero desktop`
was reported as opaque glass with a suspected broken/inert backdrop; the
backdrop had been rendering correctly the whole time and a 0.54 tint alpha
was the entire story (12b15e9). The rule that falls out is cheap and would
have saved both: *when a report names a cause, verify the cause is even
involved before fixing it* — one A/B, one sample, before any edit.

**The mechanism.** `Scene::captureSeconds()` sits alongside `canvasSize()`
and `background()`, the two other harness assumptions scenes had already
had to take back; sketches declare it `ctx.captureAt(seconds)` the way they
declare their canvas. Reaching a declared time rebuilds and steps exactly,
from zero — the benchmark frames cannot be reused, because their count is
machine-dependent (the §26 bug) and a declared time may be *earlier* than
them, which there is no rewind for. Proven to be the same machinery, not a
second path: `--capture-at 6.0` reproduces the default at **0 differing
pixels** on `y2k chrome`, `aero desktop`, `persona menu`.

**Open: the audit.** `--capture-at` exists so the corpus question is
askable — sweep at two scene times and diff, and whatever differs was
moving under the shutter. Those scenes are not necessarily wrong; each
needs its author to say whether the moment it was caught at is the one
worth showing. Only `black watch` has been given a declaration so far
(7.2 s, loom 0.90, inside the Modern hold).

Determinism made the captures reproducible. This makes them
*representative*. They are not the same property, and the corpus spent its
whole life with only the first one.

**Audit result (2026-07-22, complete — all 56 pairs; full percentage
table in archive/HANDOFF.md §2).** Sweeps at t=6.0 s and t=7.0 s,
diffed pairwise: **52 of 56 scenes are in motion when the gallery
photographs them.** Only four are settled: beethoven, fallout2
charsheet, penrose paving, stock materials. Range: daemon console 98.7%
of pixels differing down to stroke atlas 0.03%. Differing does NOT mean
wrong — the per-scene judgement is the remaining task, by taxonomy:

- **Continuous ambient motion** (daemon console, ui_particles,
  flourish) — any frame is representative; leave alone.
- **Discrete named states** (black_watch's five registered shade
  families) — exactly one is canonical; capturing another makes the
  plate assert something false. These need `ctx.captureAt()`.
- **Entrances that settle into a hold** — capture in the hold. Check
  the 5–35% band first; that range is where a large but non-ambient
  change lives (thaumonomicon 58.9, ds2 bench 66.8, xcom 49.3, and
  eva magi defense 50.1 are the discrete-state suspects above it).

Only black_watch declares its beat so far (7.2 s — loom 0.90, inside
the Modern hold; it is also the audit's control, straddling the
Weathered/Modern boundary at 33.1%).

## 32. The animation grammar names the MECHANISM, not the intent

**Status: open — the alias-first spelling SHIPPED 2026-07-26; the
taste calls below remain.** Filed 2026-07-25 from the consolidation pass, not
from a study — the wall-hitter is the library's own designer, and the
independent code-only review corroborates: one animation engine,
roughly five authoring grammars, and "the map of which grammar owns
which use case exists only in comment folklore."

The evidence, concretely: `PropValue` reads as "property", which is
what it is — an internals name that leaked into the authoring
vocabulary. The most load-bearing word in the surface is a preposition
(`with()`), and its extensions modify the preposition (`withFrom`,
`withKeyframes`) rather than any animation idea. And mode switches are
implicit: `fill(&out)` vs `fill(v)` silently changes write paths;
declaring `uTime` in SkSL text flips a node's volatility as a side
effect of a shader string.

The natural API (sketch, unverified): one principle — a value over
time on a property — with one verb per door: it CHANGES (today
`with()`/`transition()`), it is DRIVEN (one explicit spelling; retire
the bare-pointer overloads so every driven call site announces
itself), it RUNS ITSELF (an explicit spelling beside the uTime
inference). Entrances are change-at-mount; staggers are per-child
delays on change; kinetic is one driven progress fanned per glyph.
Acceptance test: grep a sketch for the door verbs and you have found
every animation in it.

Constraints: the two-write-paths physics, declared volatility, and
the price tags are not in question — this is grammar over the same
machine. Per-property transition scoping must survive (properties are
named once, by their setters — value-site specs like
`opacity(v, ease)` qualify; a property enum does not). The rename is
exactly the churn §27 prices, so it lands behind a probe — port two
or three sketches to the target grammar (one transition-heavy, one
binding-heavy, one self-running) and read them — never as a sweep.

SHIPPED alias-first 2026-07-26: `animate(from(a).to(b), spec)` and
`animate(through({...}))` are the authored-motion spelling, delegating
to unchanged machinery, and `through()` carries a concrete float
overload so the waypoint path finally deduces — `withKeyframes<float>`
was paying for a nested braced list being a non-deduced context.
`with`/`withFrom`/`withKeyframes` compile forever with the one-line
legacy doc. The probe read three sketches, ported whole:
chaucer_astrolabe (48 sites), twoadvanced_v4 (27 + 5 keyframe paths),
sigillum_aemeth (25). The port's one friction, recorded as probe
data: a free function named `from` collides with the commonest local
name in path code — sigillum declares `std::vector<SkPoint> from,
to`, so 2 of its 25 sites qualify `sigil::compose::from(...)`. Where
this entry's body disagrees with §33's refined ruling, the ruling
wins: the bare-pointer overloads are RETAINED, and the acceptance
grep splits into two searches (authored vs data-driven). Still open,
all designer taste calls: the per-property change override
(`with(v, spec)` today), the scheme-declaration word, and — filed by
the confirming review, straight from the meta-rule — the second-word
collision between authored `from(a).to(b)` (entrance endpoints) and
driven `bind().from(lo, hi)`/`.to(lo, hi)` (range normalisation):
same two words, unrelated meanings, one authoring line apart.

## 33. The grammar audit — §32 generalised over the whole surface

**Status: open, first rulings landed 2026-07-25.** The designer ruled:
(1) `PropValue` → **`Animatable<T>` — SHIPPED**, alias-first, tests
green; (2+3) the door verbs take a different shape than first sketched
— **one overarching `animate()` verb** as the umbrella, with `bind`
KEPT inside that context (the umbrella disambiguates it) and the
change forms qualified under it; the exact qualified spellings are
what §32's three-sketch probe now explores; (4) the self-run
declaration: `runsItself()` rejected as unclear; `isLive()` preferred
but flagged as confusing live DATA with animated GRAPHICS — the probe
should weigh `animates()`, which shares the umbrella's root; (5)
`Material::linear`/`linearUnit` stay as they are — CLOSED; (6)
`Pattern` keeps its name ("tiling is a kind of pattern") — CLOSED; the
false Patterns.h file doc ("Each returns a Pattern") is FIXED.

REFINED RULING (2026-07-25, after reading snippets): `animate()` is
NOT an umbrella over all three doors — it is the verb for
COMPOSER-MANUFACTURED motion only. The driven forms KEEP today's data
spelling (`&out`, `bind(&out).window(...)`, bare-pointer overloads
retained by design): the designer's model is that a driven property is
DATA UPDATING, and animation is a side effect of the update — wrapping
it in `animate()` would misstate that. `.transition(spec)` is KEPT as
the node-level change policy — it reads clearly against `animate()`.
The grammar thus names the OWNER of the motion at the first word:

  opacity(0.5f)                              // constant
  opacity(animate(from(0.f).to(1.f), {400ms}))  // authored motion (composer runs it)
  scale(animate(through({...})))             // keyframes (deduces; withKeyframes<float> dies)
  opacity(&phase)                            // data-driven (you run it)
  trim(0, bind(&demo).window(.14, .20))      // shaped data
  .transition({.duration = 200ms})           // change policy on reconcile

Consequences accepted: the grep test splits into two honest searches
(`animate(` = authored motion; `bind(`/bound fields = data-driven);
the old `fill(&out)` discoverability gap is now a DOC fix, not a
spelling fix. `Animatable<T>` stays the slot noun (capability), never
folded under the verb. Still open for the probe: the per-property
change override (today `with(v, spec)` — `animate(to(v), spec)` vs a
value-site spec), and the scheme declaration word (`animates()` vs
`animated()`/`isLive()`). Only the `withFrom`/`withKeyframes` family
actually renames, which shrinks the probe.

RULINGS SESSION (2026-07-27, designer — twelve calls, closing the
taste agenda; the standing doctrine change first): **REPLACE, not
converge** — alias-first was the migration mechanism, never the end
state; after the corpus ports to the new grammar, the legacy
spellings are DELETED. The rulings: (1) per-property change ramp is
`animate(to(v), spec)` — to() alone means ramp-on-change, from().to()
means entrance; with() dies in the port. (2) The scheme
self-declaration word is `animates()` — unifies the five volatility
spellings; animated()/isLive() die. (3) Bound's stages rename
`from/to` → `source(lo,hi)/target(lo,hi)` — the authored from/to
keeps its words. (4) Namespace convention RATIFIED as shipped: value
types live in concept namespaces, free verbs stay top-level and
qualify at colliding sites. (5) The across sign: LEFT of travel wins
everywhere; lines::' right-of-travel members die in the port. (6) The
decoration cache home: an Instance-side slot map handed through
PaintContext — serves §16 and the crossing cache in one design. (7)
The weave's two limits (translucent double-cover, close-knot
territory) are ACCEPTED; the crossover pass owns them. (8)
cornerAlign becomes a REQUIRED argument — the break rides the port.
(9) Pool::touch() → commit(). (10) brushes:: folds into brush:: and
dies (Ribbon→profile seam, restyle→.shaped()). (11) The derive family
gathers under derive:: (the phase name is canon vocabulary). (12) §8b
closes: a milled groove is band+fill; no crossFill lane.

THE REPLACEMENT PROGRAM, gated in order: phase R1 — land the ruled
spellings additively (animate(to), animates(), source/target,
commit(), derive::, the brushes:: fold) plus the wrapping span (N7,
trim's last job); phase R2 — port the corpus sweep-style with pixel
verification (503 outline(, 128 withFrom(, 81 trim(, 39 PropValue<,
the ops::/linearUnit/widthFn stragglers); phase R3 — DELETE the
legacy spellings and the aliases, one loud commit. §31's beat
judgements and the measurement campaign remain separate tracks.

BRUSH & STROKE GRAMMAR — consolidated 2026-07-26 after six sample
rounds with the designer. Supersedes the 2026-07-25 interim record
(which had corners-as-kinds and weave-as-container; both were walked
back during the rounds). This is the settled model:

**The words.**
- SHAPE = the region an element occupies, set by `.shape()` —
  **`outline()` RENAMES to `shape()`** (the old name read as a drawn
  line, i.e. as stroke). LINE = an element whose geometry is an open
  path. BAND = a derived shape around a spine — `band(spine,
  across(px))` or `band(around(key), across(px))`, formation explicit:
  `.centered()` (default) / `.outward()` / `.inward()` (offset-path
  lineage) — owning an (along, across) space, hosting ordinary
  content, and strokable/fillable like any shape. "Frame"/"border"
  are not concepts (they are strokes of a boundary); "bounding box"
  is QUERY-side vocabulary only (`bounds()`), never a shape.
- STROKE = the slot: `.stroke(where, what[, name])`; repeated calls
  APPEND (the existing decoration law). WHERE = `spans::` factories
  — corners/edges/every/range/at/upTo/`fit(key, margin)` (a gap sized
  from keyed content via the derive pass, the flowAround pattern) —
  composed with `|` (union). Claims must not overlap (LOUD error),
  except: bare `rest()` fills the gaps up to the next claimed span,
  and `rest("name")` is the complement of that named pass and may
  intentionally overlay others. REVEALS are span animation
  (`spans::upTo(animate(...))`) — uniform across every brush kind;
  **`Element::trim()` is REMOVED** (overloaded word; its jobs move to
  spans) — *read, per the stage-one note below, as REMOVED FROM THE
  TAUGHT SURFACE: alias-first (§27) keeps the method and its machinery
  compiling indefinitely under a legacy one-line doc, and an actual
  deletion is a corpus sweep the designer has not cleared.* Stroke pass names are LOCAL to their element (inspection +
  intra-element reference) — never a global query key; the
  second-identity-system law holds.
- BRUSH = what paints. KINDS are the leaf tools: `brush::solid`
  (PathFormat's successor; `pen` rejected — implies calligraphy),
  `Pattern` (built from cells), `Scatter`, `Art`. COMPOSITES combine
  any brushes: `layers` (fixed order, bottom-up) and `weave`
  (per-crossing order) — any brush can participate in either, and
  composites NEST (a strand painted by layers; a braid as one pass).
  REPAIRED 2026-07-26: a weave strand is a PAIR
  `{.path = strand::self()/offset(px)/from(key), .brush = ...}` —
  the earlier two-parallel-lists-matched-by-index shape reproduced
  §10d's defect and is rejected; `strands::parallel(n, spacing)`
  survives only as sugar for n offsets sharing one brush. Crossings
  are DISCOVERED (path intersection, indexed along the boundary),
  never authored; the rule ladder: `alternate()` ==
  `sequence({Over, Under})` / `sequence({...})` generic repeating
  patterns / `pairs(...)` strand dominance incl. cyclic (Penrose) /
  `at(index, ...)` pins layered over any rule / a comparable user
  value `decide(const Crossing&)`. Index pins are POSITIONAL —
  stable rules survive geometry change, pins are for settled
  compositions; the field's doc must say so. Refinements
  (2026-07-26b): pins compose onto a base rule via `.except(idx, ...)`
  — one `.crossing` field, never stacked entries. Strand sources are
  TWO FAMILIES: RELATIVE — displacements of the stroked boundary in
  its (along, across) frame, the same frame the band owns —
  `strand::self()` (across≡0), `offset(px)` (parallel — NEVER
  crosses), `wave(amp, wavelength, phase)` (oscillates — THE braid
  primitive; crossings exist where strands trade sides); and
  ABSOLUTE — `from(key)` (derive-phase path of another element),
  `path(SkPath)` (authored; SkPath is comparable — prunes). With
  only absolute strands the boundary is an unpainted host.
  `strands::parallel` is REMOVED (parallels are rails — already
  layers + offset shapers — and cannot braid); the weave sugar is
  `strands::braid(n, amp, wavelength)` = n waves at phase k/n,
  crossings by construction. (2026-07-26c) The relative family's
  SEAM is the PROFILE value: comparable, `float across(float along)`
  + `float max()` — max REQUIRED so reach/bleed is decidable, which
  structurally kills the Ribbon widthFn/widthMax silent-clip trap
  (§25/audit I9); `self`/`offset` are the CORE presets; `wave` (and
  `braid`, which is built on it) live in the KIT per the tier rule
  (2026-07-26d, designer); custom profile values are accepted
  directly as `.path`. The profile is
  SHARED vocabulary: a band's taper and the future ribbon width ride
  the same value. LAW, now explicit: paths are DATA, only elements
  render — a path participates as an element's shape, as borrowed
  geometry (`from(key)`, derive), or as pure guide data in no tree
  (strand::path, band spines, TextPath, AlongPath). And FORMALLY `layers == weave` with
  coincident self-strands (no crossings → list order everywhere):
  one machine, two author intents, both words kept (the
  alternate==sequence precedent). Seam-value convention: one named
  required member per seam (`shape()` for shapers, `decide()` for
  crossing rules), comparable values throughout. Only the rule
  VALUES are shared with the pinned element-level crossover — its
  API stays undecided. Double/triple lines are
  `layers` + offset shapers (or a kit preset) — NEVER element
  duplication; under/over relative to content uses the existing
  background/foreground slots.
- `.shaped(value)` is the ONE geometry-deviation seam: comparable
  values with `SkPath shape(const SkPath&) const` (SkPath is proved
  right because dash/width are path operations). No sugar methods
  (`jittered()` etc. die); stock shapers are kit values, peers of
  user-written ones. `brush::ops` is INTERNAL-ONLY — authors never
  spell it.
- The two mechanisms, named: a SHAPER bends the one continuous mark
  (wave, zigzag — no tile exists); a PATTERN builds the mark from
  cells (the cell is an element — anything paints it).

**Tiers.** Core = seams, kinds, composites, span/strand factories.
KIT = convenient values under concept scopes (`kit::brush::shapers::
jitter/wave/offset`, `kit::shapes::ring` — "annulus" rejected as
jargon — `kit::spans::brackets`), and the kit becomes a SEPARATE
CMake library that links only compose's public headers (structural
enforcement of the tier boundary). PRESETS (cased→outlined, railway,
rope, GlossContour...) live in EXTERNAL loadable kits, never core;
`stroke_atlas` stays the in-repo specimen page. Standing check: a
preset whose name is craft jargon over a plain composition gets
demoted (the `cased` treatment).

**Pinned as separate passes** (named problems, deliberately not
solved here):
1. Inter-element crossing order ("crossover"): leading candidate is
   the PATCH model — a derive-phase relationship (edge-store family,
   like connector/rail) that replays the winner's cached picture
   clipped to each intersection region; z-law preserved, elements
   unsplit. Shares weave's crossing vocabulary BY DESIGN so
   brush::weave can lift to it later. Known hard cases that make
   this its own feature: translucent strands (patch double-cover)
   and multi-crossing over the same region. Touches perceived
   z-index — deferred on the designer's call.
2. The masking family — `wipe()` is one member (a paint-only
   directional mask, fraction Animatable), shape/alpha masks and the
   kit's alphaMask bake are others; the family was never designed as
   one. Own review pass.
3. Material's own interface interrogation.
4. Hit-testing organic shapes for text-flow / fill-pattern exclusion
   — rides the derive-export arc.

**Before any code**: the paper probe — rewrite ds2_bench and
thaumonomicon's stroke code in this grammar and read it. Then
alias-first migration, per §27.

**STAGE ONE SHIPPED 2026-07-26 — the structural core.** The paper probe
ran first and found no defect in the ruling; it found two large wins
(`spans::corners` deletes ds2_bench's 11-line corner-bracket generator;
`spans::edges` deletes thaumonomicon's 10-line "rect minus corners"
generator — both also restore a node's REAL box to `bounds()`/`hitTest`,
which was being corrupted to buy a stroke shape), one clean 39-site
rename, and ONE constraint the ruling had not spelled out:

> **An unqualified `.stroke(what)` overlays and does not CLAIM.** Every
> `.stroke()` call in both probe files is whole-boundary, and four
> elements stack two of them; under a literal reading of "claims must not
> overlap" every stacked-stroke element in the corpus becomes an error,
> which §27 forbids. So claims (and the no-overlap law) belong to
> span-QUALIFIED passes; the unqualified form stays first-class and
> non-claiming. Resolved toward alias-first, as the law requires.

Landed: `outline()` → **`shape()`** (alias-first, `outline()` compiles
forever); the **stroke slot** `.stroke(where, what[, name])` with the
`spans::` factories range/upTo/corners/edges/every/at/fit/rest, `|`
union, append order, LOUD overlap diagnostic (it names both passes, the
shared run, AND the composite-brush fix — the probe found that message
is the only place an author learns the N-pass reveal rule), bare
`rest()` and `rest("name")`; **reveals as span animation**
(`spans::upTo(Animatable<float>)`, pixel-parity with `trim()`, whose doc
is now the legacy one-liner — the method and its machinery stay);
`spans::fit(key, margin)` through the existing derive pass (the
flowAround pattern, same flat edge-store walk, no new phase); **`band()`**
with `across()`, `.centered()/.outward()/.inward()`, `around(key)`
spines, and `bandPointAt()` as the (along, across) space; the **profile
seam** (`float across(float along)` + REQUIRED `float max()`, comparable,
type-erased) with `strand::self()`/`strand::offset(px)` as the core
presets and `max()` wired into the paint cull — which makes the
silent-clip trap (§25/audit I9) structurally impossible **for band and
profile geometry**. `Ribbon` itself still carries its own
`widthFn`/`widthMax` pair and is unmigrated; moving it onto the profile
seam is stage-two work, and until then the trap is closed on the new
path only.

One judgement recorded: `Spans` is a CLOSED comparable value (a Rule +
term list), not an open seam. The seam convention governs shapers,
profiles and crossing rules — values whose point is that users write new
ones; a span is an interval set, so kit values (`kit::spans::brackets`)
are compositions of core terms. Widening it later is additive.

**Review residue (stage two)** — the confirming review's non-blocking
findings, recorded so they are not rediscovered:

- **The perpendicular-sign split is older than this work.** The band's
  `across` is positive LEFT of travel (outward on a clockwise path),
  matching `TextPath::offset`, which has always read that way; but
  `lines::offsetAlong` and `lines::Rail::offset` are positive RIGHT of
  travel. So the KERNEL says left and the LINES extension says right, and
  has since before the profile seam existed. All five band/profile doc
  sites now state the sign and name the conflict. Stage two shares the
  `Profile` value between bands and strands and must pick one.
- **No per-instance span cache.** `resolveSpans` re-walks the boundary
  three or four times per paint (measure, corner scan, extract) with
  nothing held between frames. Fine at the corpus's pass counts; the
  place to fix it is an `Instance`-side cache keyed on (outline identity,
  resolved endpoints), i.e. the same shape as `outlineCache`.
- **`Spans` equality is term-ORDER-sensitive; `resolve()` is not.**
  `corners(8) | at(0,4)` and `at(0,4) | corners(8)` claim the same runs
  but compare unequal, so a describe that reorders terms produces a
  spurious patch. Never a wrong picture — only a lost prune.
- **There is no seam-crossing span.** `spans::range` clamps, so
  Wrap-mode marching ants and the orbiting comet remain `trim()`'s job.
  If spans are ever to retire trim outright, a wrapping range is the
  missing piece.
- **`rest("unknown")` and `fit("unknown")` are silent** — they resolve to
  nothing, matching the `flowAround` precedent for an unresolved key.
  Now documented rather than changed; a diagnostic would have to be the
  whole family's at once.
- **NAMESPACE FRICTION, now three sightings.** A short, good noun at
  `sigil::compose` scope collides with corpus code: (1) §32's `from`,
  (2) `band` shadowing locals at `Brushes.h:1138` and
  `LayerStyles.h:450`, and (3) stage two's `Weave`/`Strand`, which
  `sigillum_aemeth.cpp:442` already owns — that one was a HARD ERROR (9
  ambiguous references) and forced the composite type into
  `namespace brush`. The move then bit inside the kit, where `brush::`
  means `kit::brush`, so `kit::strands::braid` has to spell
  `sigil::compose::brush::Strand` and name its brush parameter `ink`.
  Three sightings is a pattern, not friction; it wants a ruling before
  stage three adds more nouns.
- **`discoverCrossings` is uncached — O(P^2 M^2) per paint, and now
  MEASURED.** Every paint of a weave re-flattens its strands and re-tests
  every segment pair. The braid regression test (two strands over a
  1000 px spine, ~500 flatten samples each, 50 and 66 knots) takes
  **33 s in Debug** — four discovery passes. That is unoptimised-build
  cost and Release will be far cheaper, but the shape is quadratic in
  both strand count and sample count and it is the first thing any real
  weave will hit. Caching per Instance was evaluated and REFUSED as
  not-small: a `Weave` is a `Decoration`, so it has no Instance; caching
  would mean plumbing a mutable cache handle through the const
  `PaintContext`, which is a design change, not an optimisation. **There
  is still no `compose_bench` weave arm** — add it before choosing a fix.
- **`Decoration` is 136 B** (104 before stage two): +24 for the borrows
  vector, +8 for `reach` and padding. `ElementNode` is unchanged at 744 —
  decorations live in vectors, never inline.** Storing the keys on demand instead was evaluated and
  REFUSED: the only cheaper store is a thunk over `m_scheme`, and
  `m_scheme` is populated only for equality-comparable schemes — a
  non-comparable borrowing scheme would silently lose its keys, trading a
  size win for a correctness hole. The per-frame cost of the extra 32 B is
  unmeasured for the same reason as above (no weave bench arm).
- **Two of the new tests assert less than their names claim.**
  `AnimatedRevealDrawsOnAndDeclaresVolatility` proves the reveal advances
  but never checks the volatility half, and
  `FitSizesAGapFromKeyedContent` needs a second `frame()` before the
  derive answer lands — whether that is correct derive timing or a
  one-frame lag worth closing is unresolved.

**STAGE TWO SHIPPED 2026-07-26 — kinds, composites, strands, crossings,
the shaper seam, the kit library.** Landed:

- **KINDS under `brush::`** — `solid` (= `PathFormat`, whose name was the
  mechanism), `Pattern`/`Scatter`/`Art` (= the `brushes::*Brush` types).
  Aliases, not new types: no behaviour change, every legacy spelling
  compiles, `brush::solid(w, fill)` is the one-line form.
- **COMPOSITES** `brush::layers(...)` / `brush::weave(...)` over ONE
  `Weave` value. `layers == weave` with coincident self-strands is
  literal, not an analogy — there is no special case in the code, and the
  test asserts identical pixels from both spellings.
- **Strand PAIR** `{.path, .brush}`, with `StrandPath` covering the
  relative family (any `Profile`, in the band's left-of-travel frame via
  the new public `profileOffset()`) and the absolute family
  (`strand::from(key)` through the derive pass, `strand::path(SkPath)`).
  Absolute-only leaves the boundary an unpainted host — tested.
- **Crossings DISCOVERED** by `discoverCrossings()`, numbered along the
  boundary, with the rule ladder as one comparable `CrossingRule`
  (`alternate()` == `sequence({Over, Under})`, generic `sequence`,
  `pairs()` with cycles, and a user value whose one named member is
  `decide(const Crossing&)`), pins composing via `.except(i, order)` onto
  a single `.crossing` field. Pins are positional and the field's doc says
  so.
- **`.shaped(value)`** as the one geometry-deviation seam (`SkPath
  shape(const SkPath&) const` + equality); `Brush::op`/`GeometryOp`/`ops::`
  retained as the legacy spelling, with a `Shaper`→`GeometryOp` adaptor so
  both share ONE pipeline rather than two.
- **`SigilComposeKit` as a separate CMake library** whose only include
  path is compose's public headers — `kit::brush::shapers::wave/jitter/
  offset`, `kit::profile::wave`, `kit::strands::braid`,
  `kit::spans::brackets`, `kit::shapes::ring`. Presets stay out.
- **Audit item 10 closed** as the doc-and-sugar move: all four corner
  spellings keep compiling and now carry legacy one-liners pointing at
  `spans::corners`/`spans::edges`.

Two implementation findings worth keeping, both from tests that failed
first:

1. **Strict-interior intersection was wrong.** Symmetric geometry — two
   diagonals of a square, a horizontal met by verticals — puts a genuine
   crossing EXACTLY on a flattening sample boundary, and a strict test
   discarded every one. The discriminator that actually separates a
   crossing from a MEETING is transversality (does the other strand pass
   through, or only touch), and it rejects a shared polygon vertex for the
   right reason instead of by accident.
2. **A constant profile must delegate to `lines::offsetAlong`.** The naive
   sample-and-displace walk offsets a corner point along ONE edge's normal
   and leaves a spur on the inside of every rectangle; `offsetAlong`
   already finds real vertices and joins them properly. `profileOffset`
   detects constancy by sampling and delegates, negating the sign at that
   one seam — flipping `offsetAlong` itself would be a §27 breach.

**NOT DONE, and API.md was wrong about it (now corrected):** the
`brush::ops` internal-only demotion did NOT happen. `ops::` remains the
pre-existing PUBLIC escape hatch; demoting it is coupled to deleting its
lowercase incomparable lambda family (audit item 6), and both are deferred
to the C-batch. `.shaped()` is the taught spelling and `ops::`/`GeometryOp`
are documented as the legacy one, which is all stage two actually
delivered here.

**Stage three / open**: the lowercase incomparable `ops::` lambda family
and the `brush::ops` demotion (audit item 6, C-batch); `Ribbon`'s migration onto the profile seam (its
`widthFn`/`widthMax` pair is the last silent-clip trap); the
perpendicular-sign reconciliation (kernel says left, `Lines` says right);
and the four pinned passes, unchanged.

A full-surface discovery pass (2026-07-25) swept
every authoring header for names that say mechanism instead of
intent. The catalog with counts and candidates is
`archive/GRAMMAR_AUDIT.md`; two hazards were independently verified
before filing (`PaintContext::animating` is dead — declared false,
never assigned, copied faithfully forward at Brushes.h:459/482 — and
`Placement::interval == 24.0f` is a live sentinel that silently
overrides an author's explicit 24).

**The meta-rule the audit produced, now canon** (DESIGN.md §Growth
rules): when a doc comment's job is to distinguish two names, that is
the rename ticket — every multi-page disambiguation essay in
Brushes.h/Lines.h/Decorations.h was written after a study shipped
wrong.

The ten that matter, by confusion-evidence × usage ÷ churn (all
alias-first or additive; details and candidate spellings in the
audit file):

1. `PropValue<T>` → an intent name (`Animatable<T>` class) — 12/35
   sketches spell the type; a `using` alias costs zero call sites.
2. Complete the Fill→Material seam — five peer types expose five
   different positions; purely additive overloads (decision C work).
3. The three door verbs (`with`/`withFrom`/`withKeyframes`, the drive
   spelling, the self-run spelling) — new evidence: the family is
   inverted (`with(` 17 sites, `withFrom(` 294) and `fill(&out)` is
   so invisible a study concluded it did not exist (1 corpus site
   ever). Behind §32's probe.
4. `Material::linear` vs `linearUnit` — corpus wants unit-space 8:1;
   alias in, never redefine (§27).
5. One word for declared volatility (five spellings today). (The
   "dead `PaintContext::animating`" half is REFUTED 2026-07-27 — the
   field is assigned `ticker.active()` at both constructions; see the
   audit file's M4 correction. The word question stands.)
6. Delete the lowercase `ops::` lambda family — EXECUTED 2026-07-27:
   wave/zigzag/rounded/sketchy deleted, 8 sites ported; `PathOp`/
   `chain`/`debug` kept as the documented escape hatch. The
   `brush::ops` demotion stays REFUSED until `Rounded`/`Square` have
   kit-shaper twins (§27).
7. The corner family — `Corner::All == 15` and `Corners{15}` both
   compile, one letter apart, meaning opposite things.
8. Name the derive phase as one family (flowAround/connector/rail/
   routers:: share nothing; ~55 total uses — the low churn IS the
   symptom). Rides the derive-export seam work.
9. `Pattern`/`patterns::`/`PatternBrush` — Patterns.h's headline
   claim is false for its three most-used entries (they return
   Material); the doc fix is free.
10. `decorations::brackets`/`gappedRule` vs `lines::cornerBrackets`/
    `cornerGaps` — one capability, two names, corpus found one
    (41 vs 3). §26-family sibling failure in naming form.

Also filed from the audit: the missing boolean-shape vocabulary — a
sketch reached for `clipOut()` and `shapes::subtract` by name
(chaucer_astrolabe.cpp:972) and neither exists. New surface, not a
rename; lands as comparable `ops::` values per decision C, never an
eighth vocabulary.
