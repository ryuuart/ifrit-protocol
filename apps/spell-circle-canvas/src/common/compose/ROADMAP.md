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

**Status: R1, R2 and R3 SHIPPED (2026-07-26).** The parity table below is
CLOSED: every row reads CLOSED or CLOSED-BY-DESIGN. The R3 status note —
what died, what is condemned-but-alive, and the one-door judgement — is
at the END of this section, after the deletion list it executed.

**The first rulings, 2026-07-25.** The designer ruled:
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
**R1 and R2 have SHIPPED** (status notes below); R3's deletion list is
enumerated at the end of the R2 note, with three entries marked BLOCKED
on a designer's reading of a picture.

**THE PARITY GATE (designer, 2026-07-26) — binding on R2 and R3.**
*Expressiveness parity is the condition for deletion.* If anything the
old grammar can express has no new-grammar spelling, that is a BLOCKER
finding, reported as such, never papered over. A rename is not done
when it compiles; it is done when the capability table closes. The
method: enumerate the old surface's capabilities one row at a time,
show the new spelling WITH A TEST for each, and mark the rest NO
SPELLING. Where a gap is found, propose the additive term that closes
it — and nothing more; parity is the budget, not an invitation to
design.

**PHASE R1 SHIPPED 2026-07-26 — the ruled spellings, additively.** All
nine items landed; full Debug build, 14/14 ctest, and Release
byte-compare on ds2_bench / thaumonomicon / stroke_atlas.

1. **`animate(to(v), spec)`** — a `To<T>` builder beside `From`/`FromTo`
   and an `animate(To<T>, Transition)` overload that builds the same
   `Transitioned` `with(v, spec)` builds. The argument now says which
   kind of motion: `to()` alone is ramp-on-change, `from().to()` is a
   mount entrance. `with()` is documented as its legacy spelling.
2. **`animates()`** — the volatility concept split in two
   (`AnimatingDecoration` / `AnimatedDecoration`), duck-typing both
   words for the length of the transition, `animates()` winning where a
   scheme spells both. Every library scheme (16 of them) implements
   `animates()` as the primary with `animated()` forwarding; `Material`
   gained `animates()` beside `isLive()`; `Decoration` reads
   `animates()` and forwards `animated()`.
3. **`Bound::source(lo,hi)` / `target(lo,hi)`** — the stage names.
   `from`/`to` are one-line legacy delegates. `window()` is documented
   as `source()` that clamps.
4. **`Pool::commit()`** — `touch()` delegates. Recorded at the
   declaration that this is TASTE, not a bug fix: GRAMMAR_AUDIT M6's
   re-audit found 14 corpus sites and zero stale-lane renders.
5. **The `brushes::` fold** — `brush::` gained `Ribbon`, `taper`,
   `calligraphic`, `ribbon(profile, fill)`, `artAlong`, `Placement`,
   `StampMod`, `StampModFn`, `CornerArt`, `CornerAlign`, `Restyled`,
   `restyle`. **Ribbon's Profile migration landed too**: a `Profile
   width` member (comparable, required `max()`), `bleed()` reading the
   profile, and the profiled path built by the now-public
   `bandRegion()` — so the corner-join win falls out of `profileOffset`
   exactly as predicted. `widthFn`/`widthMax` stay, unchanged, and no
   existing ribbon's output moves.
6. **`derive::`** — `connector`, `rail`, `around` as using-declarations,
   `flowAround` as a free verb over the method; one API.md section
   ("The derive family") naming the six members and the four shared
   laws (silent unknown key, one cycle-guarded second pass, the
   one-frame lag, flat-not-recursive).
7. **`spans::wrap(begin, end)`** — see the parity table below.
8. **`cornerAlign` is a required constructor argument.** The break took
   the shape the §27 entry asked for and the type system can enforce:
   corner art and its alignment are ONE value, `brushes::CornerArt{art,
   align}`, with no default constructor — so "corner art with no stated
   alignment" is a state that cannot be described, and the paint-time
   warning is deleted. Five sketch consumers ported. It immediately
   found a sixth site that was setting `cornerAlign` on a brush with NO
   corner art (eva_magi_interior, twice): inert configuration that a
   warning could never have caught.
9. **LEFT-of-travel** noted at all five sign sites (two `lines::`
   members, three kernel sites): the convention dies in R3, left wins,
   and no sign flips now because a flip moves every caller's pixels —
   it rides the R2 port.

**THE TRIM PARITY TABLE (R1's gate deliverable).** `Element::trim` is
the surface R3 wants to delete, so every capability of it was
enumerated and tested against the span spelling
(`ComposeR1TrimParity.*` and `ComposeR1Wrap.*`, 12 tests):

| trim capability | spans spelling | status |
| --- | --- | --- |
| `trim(0, t)` reveal | `spans::upTo(t)` | closed (pre-existing test) |
| `trim(a, b)` window | `spans::range(a, b)` | closed |
| ends outside [0,1] pin (Clamp) | `range` normalises identically | closed |
| animated endpoints | `range`/`upTo` take any `Animatable` | closed |
| bound endpoints | same | closed |
| CONSTANT `offset` | addition at the call site | closed |
| BOUND `offset`, constant ends | `range(bind(&o), bind(&o).offset(w))` | closed |
| `TrimMode::Wrap`, static seam-crossing window | `spans::wrap(a, b)` | closed |
| Wrap marching ants (bound) | `wrap(bind(&p), bind(&p).offset(w))` | closed, 8 phases incl. mid-seam |
| Wrap marching ants (animated both ends) | `wrap(animate(...), animate(...))` | closed, 8 steps |
| Wrap degenerate: `end-begin <= 0` → nothing, `>= 1` → whole | same rule, from the RAW endpoints | closed |
| reveals ALL of a node's outline-followers at once | one pass with a COMPOSITE brush (the overlap diagnostic's own advice) | closed — and now for BOTH halves (see next row) |
| reveals BACKGROUND-slot followers (painted BELOW children) | `.background(Spans, Decoration[, name])` — stroke()'s twin in the other z-half | **CLOSED in R2** |
| BOUND `offset` AND bound endpoints together | `spans::range/wrap(a, b).offset(o)` — a third `Animatable<float>` on the term | **CLOSED in R2** |
| trim also trims the FILL surface | `wipe()` — a directional reveal of the SURFACE | **CLOSED BY DESIGN** (see below) |

**THE TABLE IS CLOSED (R2, 2026-07-26).** Every row now reads CLOSED or
CLOSED-BY-DESIGN. The two that closed by SPELLING did so additively, and
neither invented a kind:

- **Two live Outputs summed into one endpoint.** `trim(&start, &end,
  &offset)` adds two independently-driven values; a `BoundFloat` holds
  ONE source pointer, so endpoint arithmetic could not express it. Closed
  by the field the R1 note proposed and did not build: an
  `Animatable<float> offset` on `Spans::Term`, set by `Spans::offset(by)`,
  added to BOTH endpoints of every Range/Wrap term before the interval is
  read — which is exactly what `Paint.cpp`'s trim block does with its
  third argument (`s0 = start + off`, `e0 = end + off`). Still a closed
  comparable value, no new kind; `valueCount()` is 3 per term and the
  offset participates in equality the way the endpoints do (without that,
  a claim that only SLIDES would prune to its first frame — the same bug
  R1 fixed for Wrap's endpoints). Tests: `ComposeR2Offset.*`, three of
  them, Clamp and Wrap, with all three Outputs driving at once.
- **Background-slot followers.** `.background(Spans, Decoration[, name])`
  mirrors `.stroke(Spans, ...)` exactly and differs in ONE thing: where
  the mark lands. It is ONE ledger, two z-halves — `StrokePass` gained a
  `half` discriminator and nothing else moved, so claims, the no-overlap
  law, append order and `rest()` all read across both halves. That is not
  an implementation convenience, it is the law: a boundary does not have
  two of itself, so a background pass and a stroke pass claiming the same
  run is the same mistake it always was, and it is still loud. Tests:
  `ComposeR2Background.*`, five of them, including the z-order pin (the
  same brush in the two halves lands on opposite sides of a child) and
  `rest()` reading across the halves.
- **The fill under a trim — CLOSED BY DESIGN, not by spelling.** `trim()`
  also re-draws the node's FILL along the trimmed (open) path. That is
  not a capability to port: it is the documented misfeature `wipe()`
  exists to answer ("trim walks the PERIMETER, so on a filled shape it
  sweeps a wedge round the outline instead of extending the surface",
  three studies). The consequence for R2 is concrete and is recorded in
  the R2 status note below: 14 corpus trims sit on filled nodes, and each
  is a re-authoring decision (wedge sweep → directional wipe), not a
  rename. R3 cannot delete `trim()` until a designer has looked at those
  fourteen pictures.

**Why `spans::wrap` and not `range` learning to wrap.** Two reasons,
both load-bearing. (1) `range(0.9, 0.1)` compiles today and means the
reversed window `normalizeSpans` swaps — teaching `range` to wrap
changes what existing descriptions DRAW, which §27 forbids and R1 is
not the phase for. (2) The no-overlap law reads over RESOLVED runs, and
this is the only term that yields two runs from one pair of endpoints;
a reader auditing a claim conflict needs the call site to say the term
is cyclic. `wrap` names the intent, `range` stays the clamped interval.

**One real bug found by the parity test, and fixed.** `detail::spanPath`
claimed in its own comment that "a whole contour claimed whole stays
whole — closed stays closed, so joins and additive brushes behave as
they do untrimmed", and did not close it: `getSegment` returns an OPEN
run whose ends merely coincide, so the seam vertex got two butt caps
instead of a miter join. Two pixels at one corner of a rectangle — and a
visible notch under any wide or additive brush. It was invisible until a
full-cycle `spans::wrap` was compared byte-for-byte against an untrimmed
Wrap trim. The close() lands on ANY whole-contour claim — edges() on
a smooth shape, every(1), range(0,1), bare rest() against nothing —
not only full-cycle wrap; the corpus has zero spans:: sites at all, so
the byte-compares could not exercise this branch (two kernel tests
hit it and pass, seam pixels unpinned). R2 must pin: seam pixels on a
whole-contour claim, wrap under the overlap law, wrap + rest(), and a
LIVE-material animates() arm. Beyond that, nothing
downstream moved. **All four pinned in R2** —
`ComposeR2Seam.AWholeContourClaimKeepsItsCornerJoin` (the miter's own
square AT THE SEAM CORNER, five spellings against an untrimmed stroke:
`every(1)`, `range(0,1)`, full-cycle `wrap`, bare `rest()`),
`ComposeR2Wrap.WrapIsUnderTheOverlapLawLikeEveryOtherTerm` (a claim that
overlaps only the run on the FAR side of the seam is still loud, and one
that clears both runs is silent), `ComposeR2Wrap.RestIsTheComplement
OfBothOfWrapsRuns`, and `ComposeR2Volatility.ALiveMaterialOnASpanPass
DeclaresItself`. The last one found a real omission on the way: the
volatility scan read `pass.what.animated()`, the legacy word — correct
today because `animated()` forwards, and a landmine for the R3 deletion.
It reads `animates()` now.

A fact the seam test had to establish first, and which no doc stated:
**the seam (fraction 0) of an rrect outline is its BOTTOM-LEFT corner,
and the boundary runs UP the left edge from there** (`addRRect` start
index 3). Two of the four new tests were written against "top-left,
clockwise" and failed; the existing suite had never needed to say which
corner, because every earlier assertion was symmetric under the choice.
It is now written where an author will meet it — the `Spans` class doc in
`Compose.h` and API.md's stroke-slot section — and not only here.

**And the seam test had to be aimed at that corner to mean anything.**
Its first cut sampled the TOP-left, which is mid-run: every corner except
the seam joins correctly whether or not the contour was closed, so all
five assertions passed with R1's `close()` reverted. A test that cannot
fail is not a pin. Re-aimed at (20, 120) and VERIFIED by reverting
`close()` locally, watching this test and
`ComposeR1Wrap.DegenerateWindowsMatchTrimToo` both go red, and restoring
it. The general form is worth keeping: *when a test is written to pin a
fix, the acceptance step is watching it fail without the fix* — a
"passing" assertion aimed at the wrong pixel is indistinguishable from a
working one until the day it is needed.

**PHASE R2 SHIPPED 2026-07-26 — the parity table closed, and the corpus
ported.** Four work packages, in order; the gate throughout was that
every gallery plate renders BYTE-IDENTICAL in Release, which is the only
reason a sweep this size can be believed.

**WP1 — the two closable gaps.** Both closed additively; see the parity
table above for the design and the tests. `Spans::Term` gained
`Animatable<float> offset` (`valueCount()` 2 → 3, equality, volatility,
`spanEndpoints`, `resolveSpans` all follow it); `Element::background(Spans,
Decoration, name)` joined `stroke(Spans, ...)` over ONE `StrokePass` list
carrying a `half`. Eleven new tests (`ComposeR2*`) covering both, plus
the four obligations R1 pinned; five more in `compose_kit_test` for the
kit values WP2 and WP3 added. 400 cases in `compose_test`, 47 in
`compose_kit_test`, 14/14 suites.

**WP2 — the corpus port.** 68 files (35 studies, 18 gallery headers, the
kernel's own docs, the tests, the bench and compose_demo). The right-hand
column counts the NEW spelling in the ported corpus, which is the number
a grep test will read:

| old | new | sites |
| --- | --- | --- |
| `outline()` | `shape()` | 591 |
| `withFrom(a,b,s)` | `animate(from(a).to(b), s)` | 280 |
| `withKeyframes<T>(f,e)` | `animate(through(f), e)` | 19 |
| `with(v,s)` | `animate(to(v), s)` | 17 |
| `trim(...)` | `spans::upTo/range/wrap[.offset]` | 58 of 75 |
| `PropValue<T>` | `Animatable<T>` | 48 |
| `bind().from/to` | `.source()/.target()` | 65 |
| `pool->touch()` | `pool->commit()` | 17 |
| `brushes::X` | `brush::X` (or `kit::brush::presets::X`) | 181 |
| `animated()` | `animates()` | 29 |
| `.op(ops::X)` | `.shaped(kit::brush::shapers::Y)` | 21 |

(The 58 trims land as 56 `spans::upTo`, 2 `spans::range` and 2
`spans::wrap`, two of the latter carrying `.offset()` — R2's own new
term, in the corpus on the day it shipped.)

`linearUnit` untouched (ruled CLOSED). The mechanical renames were
scripted with a balanced-paren parser rather than a regex — `withFrom`
arguments span lines and nest — and only lines the port made longer than
84 columns were re-wrapped, so the diff is line-for-line and the corpus's
hand layout survives.

**The presets kept their old NAMES.** WP3 moved the four bodies out of
core, and the first cut deleted `brushes::filament` and its three
siblings outright — a phase early, and a straight breach of the
alias-first law the whole program runs on. They are back as
using-declarations at the bottom of `kit/Strokes.h` (not in core's
`Brushes.h`: the tier boundary is structural and a core header cannot
name a kit value), with a test that the legacy spelling still resolves
AND still means the same value through its default arguments.

**Three things the port found that a compile could not.**

1. **`trim()` ports for 58 of 75 sites, and the fill row is far smaller
   than it looked.** The first classification called every `.fill()` in
   the chain a blocker and counted 14. That was wrong: **12 of those 14
   are `Fill::none()`**, and `Paint.cpp` skips the fill block outright
   when `resolvedFill->kind == Fill::Kind::None` (and the echo block with
   it), so there is no surface to sweep and nothing to port around. The
   test is whether the fill PAINTS, not whether a `.fill()` call is
   present. Re-classified and ported, **17 remain**, in three groups:

   > **A PAINTING fill — 2, and these are the designer's two pictures.**
   > `chaucer_astrolabe` 971 (a 2 px meridian bar, filled, no stroke at
   > all — the reveal IS the fill sweep) and `sigillum_aemeth` 1225 (a
   > translucent brass wash under a `lines::rails` stroke, both revealed
   > together). Nothing else in the corpus depends on the wedge.
   >
   > **Built across statements — 11.** `astral_tome` 1364 ·
   > `chaucer_astrolabe` 1023, 1032, 1269 · `sigillum_aemeth` 672 ·
   > `thunder_fulu` 760 · `twoadvanced_v4` 1062 ·
   > `vagrant_story_target` 742, 1703 · `ScenesBeethoven.h` 132, 137.
   > The element is assembled in a helper or over several statements, so
   > the sweep cannot see the stroke and the trim together. A limit of
   > the PORT, not of the grammar — each is a hand edit.
   >
   > **More than one outline follower — 4.** `ds2_bench` 819 (two
   > unqualified strokes) · `ScenesNetwork.h` 303 (three) and 308 (a
   > `.style()` bundle, whose `under` layers land in the other z-half) ·
   > `chaucer_astrolabe` 1352 (**three `foreground()` calls and no
   > `stroke()` at all** — the milled-edge bar). Each is one pass with a
   > composite brush, which is a spelling, but it is also a picture the
   > author has to look at.

   So **R3's `trim()` deletion is 17 sites of work, of which exactly 2
   need a designer** — not 29 with 14 pictures, as the first pass
   reported. The lesson generalises past this port: *"the chain contains
   `.fill()`" is not the question; "does the fill PAINT" is.* A
   conservative classifier that reads syntax rather than behaviour
   over-reports blockers, and an over-reported blocker is indistinguishable
   from a real one in a handoff.

2. **`widthFn` → `Profile` is a RE-DRAW, not a rename — BLOCKER.**
   `Ribbon::paint` has two entirely different constructions: with a
   profile it calls `bandRegion()` (offset rails, real joins), without
   one it samples the contour and zips left/right point lists. So moving
   the corpus's 7 `widthFn` ribbons onto the profile lane changes their
   pixels by construction, and no amount of care makes it byte-identical.
   A second, independent reason: `Profile::across` is asked in FRACTIONS
   of arc length, and every corpus `widthFn` keys on `PathSample::
   distance` **on purpose** — `thunder_fulu` documents why (under a
   reveal the decoration is handed the REVEALED contour, so `fraction`
   slides). **Not ported.** R3's deletion of `widthFn`/`widthMax` needs
   either the profile lane reproducing the zip construction, or a
   designer looking at 7 pictures.
3. **The `brush::ops` demotion needed THREE kit twins, not two.** §33
   named `Rounded` and `Square`; both are now
   `kit::brush::shapers::Rounded/Square`. The third was
   `ops::Wave{.zigzag = true}`, which has a live corpus site (the
   gallery's pipeline trio) and no kit spelling — kit `Wave::shape()`
   hardcodes `zigzag = false`. Closed as its own value,
   `kit::brush::shapers::Zigzag`, rather than a flag on `Wave`, because
   `Wave` is ALSO read as a profile and a flag the profile reading
   ignored would be a silent asymmetry. **The demotion is now unblocked
   for the PIPELINE seam** (`.op(ops::X)` → `.shaped(kit…)`, all 21 sites
   ported) **and still blocked for the per-LEG suffix**:
   `Brush::leg(Decoration, std::vector<GeometryOp>)` has no shaper-typed
   spelling, and `{kit::brush::shapers::Offset{...}}` cannot convert
   implicitly (two user-defined conversions do not chain). The additive
   closure is a `leg(Decoration, std::vector<Shaper>)` overload. Not
   built: parity is the budget.

**WP3 — the four presets left the core.** `filament`, `circuit`, `rope`,
`pulse` moved from `brushes::` to **`kit::brush::presets::`**, unchanged —
same layers, same numbers, same REFERENCES citations. Placed under
`kit/Strokes.h` beside the shapers, and in a SEPARATE scope from them,
because they are peers in tier mechanics (free functions over the public
API) and not peers in kind: a shaper is vocabulary, a preset is a finished
drawing, and `kit::brush::shapers::wave` vs `kit::brush::presets::rope`
says so at every call site. §33's end state for presets is an EXTERNAL
loadable kit; no such mechanism is built, and these four had to leave
`brushes::` because that namespace dies with R3 — so this is one move now
and a change of HOME later, rather than two changes of name. 25 call
sites ported. The header's own "what is deliberately NOT here: PRESETS"
paragraph was rewritten to say all of that instead of contradicting the
file it sits in.

**THE PLATE LEDGER — 56 scenes, Release, `ComposeGallery --headless`,
hashed before and after.** **49 byte-identical. 7 differ. 0 reverted.**
And the seven split cleanly in two, because the run carried a CONTROL:

- **Four are not attributable to the port.** `chladni_tab1`,
  `hitman_verlet`, `ksp_mapview` and `slitscan_2001` differ **from
  themselves** — the PRISTINE binary, re-run on the same sources,
  produces a different plate. Each was checked that way individually.
  (`genesis_fire` and `slitscan_2001` were already on record as
  self-differing from the cornerAlign pass; `genesis_fire` came out
  IDENTICAL this time and `slitscan_2001` did not, which is what
  self-instability looks like from close up. The other three are new to
  the list, making five scenes now observed unstable.) Their deltas are
  ±3 LSB over antialiased curves (`chladni_tab1`, `ksp_mapview` — and
  bit-for-bit the same noise signature, same bounding box, before and
  after twelve trims were ported inside `chladni_tab1`, which is the
  strongest evidence available that the ports did not touch them) or a
  few dozen pixels of one live readout (`hitman_verlet`,
  `slitscan_2001`). The capture FRAME is deterministic (§31 fixed it);
  what is not is auto texture promotion, which is decided on measured
  milliseconds and therefore on machine load. `--no-promotion` exists for
  exactly this and is the flag any future ledger should carry.
- **Three are the port, and all three are CAPTION STRINGS** — the
  precedent the gate names. `stroke_atlas`, `thunder_fulu` and
  `minard_1869` are specimen and report plates that PRINT the API names
  they use, so `brushes::rope(...)` → `kit::brush::presets::rope(...)`
  and `Brush{}.op(ops::Square{5,26})` → `.shaped(shapers::Square{5,26})`
  change the drawing because the drawing is the text.

  **Proved to the byte, not argued.** Re-render `thunder_fulu` from a
  tree in which its ONE changed string is restored to the old spelling:
  the plate's SHA-256 is `b6618a16…`, which is the baseline's, exactly.
  Restore the ported spelling and it returns to `1ed74d90…`, which is the
  swept plate's. Both artifacts kept, in their own directories — the
  first attempt at this experiment overwrote its own evidence with an
  ordinary re-render and left a file that hashed as the PORTED plate,
  which is worse than no experiment.

  The differing region is ONE text line in `thunder_fulu` (13 rows) and
  one in `minard_1869` (9 rows). `stroke_atlas` is the specimen page and
  changes twelve caption lines, so its diff is **twelve bands**, each
  12–24 rows tall, totalling 0.35% of the plate — measured, not
  estimated.

The method was itself controlled: a per-scene `--scene` render reproduces
the sweep's plate byte-for-byte (checked on `ds2_bench`, `spacejam_1996`
and `thaumonomicon`), which is what makes every stability test above mean
anything.

`sizeof(ElementNode)` is unchanged and its guard stands — `StrokePass`
grew by one byte and lives in `Box<StrokeData>`, never inline.

**WP4 — this entry, and API.md.**

**THE R3 DELETION LIST, enumerated.** Everything below is legacy surface
kept alive only by the alias-first law, with its remaining in-repo count
after the R2 port. Anything marked BLOCKED cannot be deleted without a
designer's decision about a picture:

| surface | where | state |
| --- | --- | --- |
| `Element::outline()` | Compose.h | 0 corpus sites; 1 deliberate legacy-parity arm in the tests. NB `ksp::Conic::outline()` is a *different* member (7 uses) and stays — a grep for the deletion has to be `\.outline(` on an Element |
| `withFrom()` | Compose.h | 1 deliberate legacy-parity arm in the tests |
| `withKeyframes<T>()` | Compose.h | 1 deliberate legacy-parity arm |
| `with(v, spec)` | Compose.h | 0 corpus sites |
| `Element::trim()` + `TrimMode` + `FxData::trim*` | Compose.h, Paint.cpp | **CONDEMNED, not deleted — 17 sites, of which exactly 2 need a designed replacement interface** |
| `PropValue<T>` alias | Compose.h | 0 corpus sites; the LIBRARY still spells it internally (~130) and that is a rename, not a deletion |
| `Bound::from(lo,hi)` / `Bound::to(lo,hi)` | Compose.h | 0 corpus sites |
| `Pool::touch()` | Instances.h | 0 corpus sites |
| `namespace brushes` (the whole fold) | Brushes.h, and the four preset names in kit/Strokes.h | 0 corpus sites; tests only (deliberate `static_assert` identity arms, plus the preset-alias test) |
| `animated()` on every scheme + `Decoration` | ~14 headers | deliberate parity arms only |
| `Material::isLive()` | Material.h | 41 sites — **RULED in R3** (ruling 13): the fifth spelling of one idea, dead with the other four; `isAnimated()` is the word |
| `Ribbon::widthFn` / `widthMax` | Brushes.h | **CONDEMNED, not deleted — see WP2 finding 2 and the R3 note** |
| `Brush::op()` | Brushes.h | 0 corpus sites — every `.op()` is `.shaped()` |
| `ops::` PUBLIC (the structs) | Brushes.h | **DONE in R3** — `layer(Decoration, vector<Shaper>)` closed the per-layer gap; the structs died, `ops::PathOp`/`chain`/`debug` stayed as the one documented mechanism door |
| `lines::offsetAlong` / `Rail::offset` right-of-travel sign | Lines.h | **DONE in R3** — renamed `offsetAcross`/`across` and flipped, every call site negated, plates byte-identical |
| the lowercase retention docs ("Legacy spelling of …") | ~30 sites | delete with their subjects |

Two of those deserve saying plainly. **The `lines::` sign flip did not
happen in R2.** §33 rules left-of-travel everywhere and says the flip
"rides the R2 port"; it cannot, because R2's gate is byte-identity and a
sign flip moves every caller's pixels by construction — the same argument
that blocks `widthFn`. It needs its own pass with a designer reading the
diffs. And **`isLive()` was never in R2's map**: ruling 2 unified the
five *volatility* spellings on `animates()`, and `Material` gained
`animates()` beside `isLive()` in R1; whether the older word dies is a
call nobody has made.

**Namespace friction, FOURTH sighting** (the roadmap wanted a ruling
after three). `derive::` collided with `fallout2_charsheet.cpp`'s own
`fo::derive()` under `using namespace fo` — a hard error, fixed by
qualifying the call site. The pattern is unchanged: a short, good noun
at `sigil::compose` scope collides with corpus code, and every new
concept namespace adds a sighting.

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
  missing piece. **CLOSED in R1** by `spans::wrap(begin, end)` — a
  dedicated cyclic term, not `range` learning to wrap; see the R1 status
  note and the trim parity table above for why, and for the two rows
  that remain open.
- **`rest("unknown")` and `fit("unknown")` are silent** — they resolve to
  nothing, matching the `flowAround` precedent for an unresolved key.
  Now documented rather than changed; a diagnostic would have to be the
  whole family's at once. **R1 wrote that law down** as the first of the
  derive family's four (API.md, "The derive family").
- **NAMESPACE FRICTION, now FOUR sightings.** A short, good noun at
  `sigil::compose` scope collides with corpus code: (1) §32's `from`,
  (2) `band` shadowing locals at `Brushes.h:1138` and
  `LayerStyles.h:450`, and (3) stage two's `Weave`/`Strand`, which
  `sigillum_aemeth.cpp:442` already owns — that one was a HARD ERROR (9
  ambiguous references) and forced the composite type into
  `namespace brush`. The move then bit inside the kit, where `brush::`
  means `kit::brush`, so `kit::strands::braid` has to spell
  `sigil::compose::brush::Strand` and name its brush parameter `ink`.
  (4) R1's `derive::` against `fallout2_charsheet.cpp`'s own
  `fo::derive()` under `using namespace fo` — a HARD ERROR again, fixed
  by qualifying the call site.
  Three sightings is a pattern, not friction; four is the pattern
  repeating on schedule, and every new concept namespace adds one. It
  wants a ruling before any further nouns land.
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
and the `brush::ops` demotion (audit item 6, C-batch); the four pinned
passes, unchanged. Two entries CLOSED by phase R1 below: `Ribbon`'s
migration onto the profile seam (a `Profile width` member landed
additively — `widthFn`/`widthMax` still compile and still draw what they
drew, so the trap is closed on the new path and the old pair dies with
R3), and the perpendicular-sign reconciliation (RULED: left wins
everywhere, the `lines::` sign dies in R3, the flip rides R2 because it
moves every caller's pixels).

**R2 UPDATES BOTH.** (1) The `ops::` demotion is **half done**: R2 added
the `Rounded`, `Square` and — the gap nobody had spotted — `Zigzag` kit
shapers, and ported all 21 corpus `.op()` sites to `.shaped()`, so the
PIPELINE seam is clear. What still holds `ops::` public is
`Brush::leg(Decoration, std::vector<GeometryOp>)`: the per-leg suffix has
no shaper-typed spelling and `{kit::brush::shapers::Offset{…}}` cannot
convert implicitly (two user-defined conversions do not chain). A
`leg(Decoration, std::vector<Shaper>)` overload closes it. (2) The
`lines::` sign flip **did NOT ride R2**, and the entry above should be
read as superseded: a sign flip moves every caller's pixels by
construction, and R2's gate is byte-identity. It needs its own pass with
a designer reading the diffs — the same shape as the `widthFn` blocker.

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

---

## PHASE R3 SHIPPED 2026-07-26 — the deletion

The alias-first law (§27) is a bridge, not a destination, and R3 is the
far bank. Everything the R1/R2 ports left compiling under a one-line
"legacy spelling" doc is gone, along with the two mechanism words the
mid-flight rulings added to the list. **No surviving legacies**, with
three named exceptions that are CONDEMNED rather than deleted and say so
in their own doc comments.

**TWO RULINGS LANDED MID-PHASE and are folded in here.**

**Ruling 13 — the volatility predicate is `isAnimated()`, not
`animates()`.** R1 unified five spellings on `animates()`; a cold read of
it asks "animates *what*?", and the `is` prefix settles that it is a
QUERY. There is no setter to confuse it with — the answer is derived from
how the value was constructed — so the prefix costs nothing and buys the
reading. `animates()` (R1's word), `animated()` (the original), and
`Material::isLive()` all die together: **one word, 16 library schemes +
`Decoration` + `Material`, 100 sites ported.** The concept keeps the name
`AnimatedDecoration` — freed by the deletion of the legacy concept that
held it — because the concept and the predicate now share a word;
`AnimatingDecoration` is gone.

**Ruling 14 — the composite-brush unit is `layer()`, not `leg()`.**
`Brush{}.layer(casing).layer(face)` is symmetric with `brush::layers(…)`,
the fixed-order composite, the way a strand is the unit of a weave. `leg`
named a mechanism nothing else in the grammar used. The internals follow
the taught word: `Brush::Leg` → `Brush::Layer`, `legs` → `layers`,
`Leg::ops` → `Layer::shapers`. 68 sites.

### THE DELETION MANIFEST — name → what was removed

| deleted | declaration | definition | call sites ported |
| --- | --- | --- | --- |
| `with(v, spec)` | Compose.h | inlined into `animate(To<T>, spec)` | 1 test arm |
| `withFrom(a, b, spec)` | Compose.h | inlined into `animate(FromTo<T>, spec)` | 1 test arm |
| `withKeyframes<T>(f, e)` | Compose.h | inlined into `animate(Waypoints<T>, ease)` | 1 test arm |
| `Element::outline()` | Compose.h | forwarder | 1 test arm (`ksp::Conic::outline` is a different member and stays) |
| `PropValue<T>` alias | Compose.h | — | 97 internal spellings renamed to `Animatable<T>` |
| `Bound::from(lo,hi)` / `Bound::to(lo,hi)` | Compose.h | forwarders | 1 test arm |
| `Pool::touch()` | Instances.h | forwarder | 1 test arm |
| `namespace brushes` | Brushes.h | the namespace itself, folded into `brush::` | 28 test/kit-test sites; the four preset using-decls in kit/Strokes.h deleted with it |
| `PatternBrush` / `ScatterBrush` / `ArtBrush` | Brushes.h | renamed to `brush::Pattern`/`Scatter`/`Art` | the taught aliases became the definitions; ~45 comment/caption references swept |
| `animated()` on 16 schemes + `Decoration` | ~9 headers | 18 forwarders | — |
| `animates()` (R1's word) | everywhere | — | 100 sites → `isAnimated()` |
| `Material::isLive()` | Material.h + Material.cpp | renamed | 33 sites in Material.cpp/Paint.cpp/Reconcile.cpp/tests. **No consumer outside compose** — checked SigilScry, SigilImage and the whole of `src/`: the only other `animated()` in the repo is `sigilimage::ImageAsset::animated()`, an unrelated member, untouched |
| `AnimatingDecoration` concept | Compose.h | merged into `AnimatedDecoration` | 2 static_asserts |
| `Brush::op(GeometryOp)` | Brushes.h | forwarder | 1 kit-test arm |
| `Brush::leg(Decoration, vector<GeometryOp>)` | Brushes.h | replaced by `layer(Decoration, vector<Shaper>)` | 68 |
| `ops::Wave`/`Rounded`/`Sketchy`/`Square`/`Offset` | Brushes.h | bodies MOVED into their kit shaper twins | 27 |
| `GeometryScheme` concept (the `apply()` spelling) | Brushes.h | — | the seam word is `shape()` and only `shape()` |
| `lines::offsetAlong` | Lines.h | renamed `offsetAcross`, sign flipped | 8 |
| `lines::Rail::offset` | Lines.h | renamed `across`, sign flipped | 82 |
| `lines::Line::offset` | Lines.h | renamed `across`, sign flipped | 3 |
| `Pattern::CornerAlign` / `Pattern::CornerArt` nested aliases | Brushes.h | — | 0 |
| the lowercase "Legacy spelling of …" docs | ~30 sites | deleted with their subjects | — |

### THE PER-LEG SPELLING — `layer(Decoration, std::vector<Shaper>)`

Chosen over a variadic `layer(Decoration, Shaper...)`. The reason is that
the suffix reads beside `.shaped()`, and `.shaped()` takes ONE shaper per
call: a braced list is the only spelling that says "and these, in order,
for this layer only" without inventing a second way to mean the same
thing. It is also what a `Brush::Layer` literally holds, so the
declaration and the field agree — and a variadic would have made
`layer(dec)` and `layer(dec, a, b)` two different-looking calls to one
slot.

`Shaper`, not `GeometryOp`, and that is the whole unblocking: the R2 note
recorded that `{kit::brush::shapers::Offset{…}}` could not reach a
`vector<GeometryOp>` because two user-defined conversions do not chain.
A `vector<Shaper>` is one hop. `Brush::pipeline` moved to `vector<Shaper>`
with it, so a Brush is now Shaper-typed end to end and `GeometryOp`
survives in exactly one place.

### THE `ops::` JUDGEMENT — one door, kept deliberately

The comparable structs died: every one had a `kit::brush::shapers::` twin
after R2 (`Wave`, `Zigzag`, `Rounded`, `Jitter`, `Square`, `Offset`), and
their bodies moved into those twins unchanged — `Jitter` now holds
`SkDiscretePathEffect` with the hairline rec, `Rounded` holds
`SkCornerPathEffect`, `Square` calls `lines::displaceSquare` directly.
`Brush::op()` and the `GeometryScheme`/`apply()` concept went with them.

**`ops::PathOp`, `ops::chain()` and `ops::debug()` SURVIVE**, and this is
the recorded judgement rather than an oversight. A `Shaper` requires
`std::equality_comparable` **by design** — that is what makes a brush
prunable — so a one-off closure can never be one. Deleting `PathOp` would
have removed the raw-lambda escape hatch with nothing to say instead,
which is precisely the failure the parity gate exists to prevent. It is
now reachable through exactly one entry point, `brush::restyle(op, dec)`,
documented in Brushes.h under a heading that says it is a mechanism and
prices it (it never prunes). `GeometryOp` survives as the thing `restyle`
carries, with two constructors: a shaper value, or the lambda.

### THE SIGN-NEGATION PORT

**One convention: positive `across` is LEFT of travel** — outside a
clockwise path in screen space. Stated once, in DESIGN.md, and nowhere
else claims otherwise. The `lines::` family was the minority that meant
right-of-travel (Mapbox's line-offset sign) and it is gone.

The name is **`across`**, taken from the kernel's own vocabulary rather
than invented: `Profile::across(along)`, `bandPointAt(spine, along,
acrossPx)`, `Across`/`across(px)` and `strand::offset` already spell the
left-positive frame, and `across` is the half of the `(along, across)`
pair a displacement actually lives in. `offsetAlong` named the mechanism
(a walk down the path) and carried no sign at all. So:
`lines::offsetAlong` → **`lines::offsetAcross`**, `Rail::offset` →
**`Rail::across`**, `Line::offset` → **`Line::across`**.

**Renamed as well as flipped, on purpose.** A silent sign flip on a field
that kept its name would have made the port grep-verified across ninety
sites; renaming made it COMPILER-verified — every missed site is a hard
error, and every ported site is visible in the diff as `.offset = 12` →
`.across = -12`.

| site class | count | how |
| --- | --- | --- |
| `offsetAlong(p, x, s)` → `offsetAcross(p, -x, s)` | 8 | argument negated (kernel 3, tests 3, minard_1869 1, Compose.cpp 1 — the last DROPPED a negation that existed only to bridge the two conventions) |
| `Rail{.offset = X}` → `Rail{.across = -X}` | 82 | scripted, brace-scoped, expression-negating |
| `Line::offset` → `Line::across` | 3 | 2 kernel reads + stroke_atlas's specimen |
| `kit::brush::shapers::Offset::px` | 4 | the two pre-existing value sites (ScenesNetwork.h:200/205) flipped with the function the shaper wraps; the ~12 arriving via the ops:: port are counted in that row; its "OPPOSITE sign" doc paragraph is deleted |
| the five sign-note sites | 7 | `offsetAlong`, `Line::offset`, `Rail::offset`, `strand::Offset`, `profileOffset`, `bandPointAt`, `shapers::Offset` — all rewritten to point at DESIGN.md's single statement |

`lines::rails(count,…)`, `heavyHairHeavy()` and `dottedCore()` keep their
literals: each builds a SYMMETRIC set whose mirrored entries carry
identical width and fill, so flipping the frame permutes identical rails
and the pixels cannot move. Stated because it looks like a missed port.

`TextPath::offset` was ALREADY left-of-travel and is untouched — it is the
member that made the kernel right all along.

### CONDEMNED, NOT DELETED — three, each with a doc comment saying so

1. **`Element::trim()`** + `TrimMode` + `FxData::trim*`. 17 sites remain
   after R2, and 2 of them reveal a PAINTING FILL (`chaucer_astrolabe`'s
   meridian bar, `sigillum_aemeth`'s brass wash), which the span
   vocabulary has no spelling for. The replacement interface is a
   separate designed piece of work; the method dies with it. Doc now
   opens "CONDEMNED, and still here … Do not add call sites."
2. **`Ribbon::widthFn` / `widthMax`.** Moving the corpus's 7 sites to the
   profile lane is a RE-DRAW, not a rename (`bandRegion()` vs the
   sample-and-zip walk), so it moves pixels by construction and needs a
   designer reading 7 plates. Same doc treatment.
3. **`decorations::brackets` / `gappedRule`.** NOT on the enumerated R3
   list, with live corpus sites (astral_tome ×5, stroke_atlas ×2) and a
   different construction from `spans::corners`/`edges` — deleting them
   is an uncleared corpus sweep with pixel risk. Their docs said
   "retained until the R3 deletion", which would now be false, so they
   were re-worded to CONDEMNED on the same terms as the other two. This
   is the one place R3 refused an implied deletion, and it is recorded
   here rather than quietly done.

### THE GATE

Debug and Release both build clean. `ctest` green across all 14 suites —
**400 cases in `compose_test`, 47 in `compose_kit_test`**, which are R2's
counts exactly: the phase deleted test ARMS (the legacy-parity halves)
and replaced each with a test of the surviving spelling, so no coverage
left with the surface. `sizeof(ElementNode)` guard unchanged and still
asserting at 768 B.

**THE PLATE LEDGER — 56 scenes, Release, `ComposeGallery --headless`.**
One baseline sweep at HEAD, then **THREE** post-port sweeps (two on a
quiet machine, one under deliberate CPU load), hashed all four ways.

**47 of 56 are byte-identical across all four runs.** Nine move, and every
one of them is accounted for:

| scene | verdict |
| --- | --- |
| `chladni_tab1`, `genesis_fire`, `hitman_verlet`, `ksp_mapview`, `slitscan_2001` | **self-nondeterministic, the five already on record.** Each produces two or three distinct hashes across the three post-port sweeps, and `chladni_tab1`, `ksp_mapview` and `slitscan_2001` render the BASELINE hash again from the ported tree |
| `black_watch` | **self-nondeterministic, NEW to the list — and it is the phase's control.** Its `.cpp` is byte-identical to HEAD, so nothing in it could have changed; rendered six times UNDER 8-WAY CPU LOAD — load is part of the
control's protocol, a quiet machine renders one hash stably — it returns `4a93f9dc…` (the baseline) on runs 3, 4, 5 and `0e78e983…` (the sweep) on runs 1, 2, 6. A scene nobody touched, flipping between exactly the two hashes in question, is what "not attributable" looks like |
| `bg3_dice_roll`, `minard_1869` | **differ by ±1–2 LSB and stay there.** Their sources changed only by a comment and by sign-negated calls, and the negation is an identity by construction (`offsetAcross`'s body sets `offset = -across` and then runs the unchanged construction). Proved by the control that matters: a MIRRORED port — the only way the sign could be wrong — was built and rendered, and it moves 0.093% / 0.443% of the plate's own pixel count at **maxDelta 149 / 35**. The observed baseline difference is 0.047% / 0.066% (same denominator) at **maxDelta 2 / 1**. A wrong sign is structural; this is antialiasing noise, the same signature `black_watch` demonstrably flips on |
| `stroke_atlas` | **the caption plate — proved to the byte.** The specimen page PRINTS the API names it uses, and eleven of its captions changed. Restoring ONLY those eleven strings, with every code port left in place, renders `d2de7bde0718` — the baseline's hash, exactly. Restoring the new captions returns it to `193333121578`, the swept hash. The diff is ten text bands, 14–43 rows tall, `maxDelta` 205; nothing outside them moves |

A trap worth recording, because it nearly produced a false negative: the
first run of the caption experiment rendered from a binary the build had
not yet relinked and reported the PORTED hash, which reads exactly like
"the captions were not the cause". The experiment is only valid if the
link step is watched, not assumed — the same lesson as R2's overwritten
artifact, in a different disguise.

**Method note.** `--no-promotion` was used as an A/B and confirms the
mechanism: `slitscan_2001`, `minard_1869` and `ksp_mapview` all render a
THIRD hash with automatic texture promotion forced off, so their pixels
depend on a decision made from measured milliseconds — i.e. on machine
load. Any future ledger should carry the flag on both arms.

**WHAT R3 REFUSED, and why.**

- **`decorations::brackets` / `gappedRule`** — see the CONDEMNED list
  above. Not on the enumerated deletion list, live corpus sites, and a
  different construction from `spans::corners`/`edges`, so deleting them
  is an uncleared sweep with pixel risk. Re-doc'd, not removed.
- **The four preset using-declarations at the bottom of `kit/Strokes.h`**
  (`sigil::compose::brushes::{filament,circuit,rope,pulse}`). The work
  order said they "stay since they point at kit values"; they could not,
  because the namespace they lived in is the thing being deleted. The
  presets themselves stay — they were never the target — under their real
  and only name, `kit::brush::presets::`. Recorded as a reading of the
  order rather than done quietly.
- **`sketch/sketches/README.md`** still lists `brushes::` among the
  namespaces a study may spell. It is on the untouchable list for this
  phase (sketch READMEs), so the stale line stands and is flagged here.
- **`ScenesNetwork.h`'s legend caption** "offset legs: lane + curb" was
  left alone. It is plain English about two offset marks, not an API
  name, and changing it would have cost a plate diff to prove nothing.
- **ROADMAP.md and STRESS_TESTS.md keep their historical mentions** of
  the deleted names. They are records of what happened; API.md and the
  headers are the surface, and those are clean (`grep` is the audit).
