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
lane has landed, and so has the **positioned leaf set** for generated
geometry that wants no layout at all (`positioned()`, 2026-07-27); the
delay/progress lane is REFUSED (owner ruling 2026-08-03, §2 — schedule
is SigilMotion's word, not a `Pool` column); what remains is a
short-string lane, per-leaf clip, and the stroke-width question. Three
studies paid four-figure Yoga node counts for scenes with zero layout
in them.

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

~~That last one is a different ask and worth separating: not "richer
instances" but **a positioned leaf set** — N children with caller-supplied
rects and no flex participation, skipping the Yoga pass. Generated
geometry (tilings, lattices, node graphs, particle fields drawn as real
elements) never wants layout, and today there is no way to say so.~~
**CLOSED 2026-07-27 — SHIPPED as `positioned()`.** The child spelling is
the one the corpus already wrote (`.left/.top/.width/.height`, px or pct,
an open dim with an opposing inset pinning the far edge, text measuring
against its resolved width); the container is the one new word. Children
of a `positioned()` container — and everything below them — mount with NO
Yoga node; `instanceRect()` resolves their rects straight from the
description, and every downstream consumer (paint, bounds, hitTest,
derive, syncLayoutRects) already read through that chokepoint or was
converted to. Penrose is the acceptance case: **1,647 Yoga nodes → 1**
(`stats().yogaNodes`, the counter added with the feature), both the field
and the deflation vignette — and the Release plate is BYTE-IDENTICAL to
the R4 baseline (`834bf1d613ae`, quiet machine, one render). Not supported inside by
contract (documented in API.md): flex props, `centerAt`, `layout()`
schemes, `flowAround`. The remainder of this entry — the instancing lanes
proper — stays open below.

**The byte-identity clause above — CORRECTED 2026-08-03, owner ruling;
the node count stands.** The §3 plate ledger caught it: HEAD stably
renders `ccd7c2aa…`, not `834bf1d613ae` — verified on two independently
built binaries, ~89% of pixels differing at maxDelta ≤ 139, the
sub-pixel edge-shift signature of antialiasing moving, not of geometry
changing. The claim predates the Skia m151 toolchain bump that landed
in the working tree between the claim and the check; the attribution is
TOOLCHAIN, not compose, and the ruling is to record measured reality
rather than bisect a delta no compose change owns. `834bf1d613ae` is
superseded as a gate — the plate baseline rebased 2026-08-03 carries
the post-m151 lineage, so the ledger now gates against what HEAD
actually renders.

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
  *(Taken 2026-08-04: the warning now sits on `Pool::tints()` itself in
  Instances.h — the multiply, the #FCFCFC case, and atlas variants as
  the exact-palette remedy. Doc-only, no pin.)*
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

> **SHIPPED 2026-07-27: `Atlas::variants(count, size, make)`** (and the
> general form where each variant brings its own logical size — KSP's
> two arm lengths). `make(v)` is baked per variant as consecutive
> frames; variant v is frame `first + v` in `Pool::frames()`. The
> re-render cases (X-COM's types × shades, re-stroked rings) are the
> point; crop-variants stay `texWindows()`. Pinned by
> `ComposeInstances.VariantsAreConsecutiveBakesOfOneRecipe`.
> **Also shipped the same day: the ALPHA lane and the `place::repeat`
> hygiene repair.** `Pool::alphas()` is opt-in like `sizes()`,
> multiplied into the tint's alpha at stamp time, so fading a subset is
> one float lane and the authored colour stays authored. `repeat()`
> writes the lerp to THAT lane (only when its opacity parameters say
> something), no longer clobbers `tints[].fA`, and takes an optional
> `frame` (< 0 leaves the lane untouched) — a generator writes the
> lanes its parameters use, and no others. The old contract's pin was
> re-pointed at the new one
> (`RepeaterLawExponentialScaleLinearEverythingElse` now asserts the
> tint is UNTOUCHED). Pinned by
> `ComposeInstances.TheAlphaLaneFadesWithoutTouchingTheTint`.
> **And `instancing::pick(pool, atlas, point)` — SHIPPED 2026-07-27**:
> the inverse projection against the same lanes the stamp reads
> (rotation, scale, sizes(), texWindows(), frame size), topmost-first;
> pinned by `ComposeInstances.PickInvertsTheStampTopmostFirst`.
> Still open in this entry: ~~the delay/progress lane (leaning REFUSE
> as library surface — progress semantics live in the author's update
> loop, and the Pool doc already rules user data stays user-side; a
> ruling should say so or name the consumer)~~, the short-string lane,
> per-leaf clip, and the stroke-width / `strokeInvariant` question —
> which `variants()` now partly answers (a re-stroke per size IS a
> variant set).
> **The delay/progress lane — REFUSED, owner ruling 2026-08-03.** The
> lean above asked for a ruling; this is it, and it is the lean's
> argument sharpened. Schedule semantics belong to SigilMotion:
> `derive()` (Ticker.h) and `bind()` are the timing vocabulary, and a
> timing lane inside the instancing pool would spell SCHEDULE a second
> time in a library whose job is STAMPING — exactly the two-spellings
> class §32/§43 refuse. And the stagger the studies wanted is not
> stranded: the frame lane plus `alphas()` plus authoring math already
> express it — per-instance index arithmetic at describe time, which
> is where a describe-time library keeps its authoring decisions
> anyway. A consumer this cannot serve is a SigilMotion consumer, not
> a new `Pool` column.
> **Plate ledger for the whole 2026-07-27 instancing stretch
> (variants + alphas + repeat hygiene + pick): 54 of 56
> byte-identical** against the §16 arm; the two movers are
> `genesis_fire` and `slitscan_2001`, the documented flappers. Zero
> corpus sites use the new lanes and `place::repeat` has zero corpus
> call sites, so identity was the pre-registered expectation, and it
> held.

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
are covered. *(Status against that list, 2026-08-03: the alpha lane,
the size lane and generator lane-hygiene shipped 2026-07-27 — the
blockquote above; the delay lane is REFUSED by the 2026-08-03 ruling
there; the short-string lane is the part of this paragraph still
standing.)*

## 3. `outline()` can never prune, and parametric curves have no generator — *four studies* — **CLOSED 2026-07-27 (comparability + generators); one named remainder stays open**

**SHIPPED as the `Shape` value + `ShapeScheme` seam** (`Compose.h`), the
identical move `Material` made and this entry asked for. `Element::shape()`,
a `TextPath` baseline and a `band()` spine all hold a `Shape`: every
`Shapes.h` generator is now a comparable value (params + `path(SkSize)` +
`==`, still callable so `OutlineFn` consumers keep compiling), the raw
callable stays accepted as the escape hatch that never compares, and
`propsEqual`'s blanket refusal of shaped nodes is gone. Copies of ONE
Shape compare equal (shared state), which upgrades the old
"pointer-stable" advice into a real prune for raw callables too. The
parametric family landed as values (`lissajous`/`harmonograph`/`rose`/
`spiral`/`trochoid` compare by parameters; raw `parametric(fn, …)` takes
an optional KEY — `parametric("orbit-a", fn, …)` — whose contract is that
one key names one curve). `svg()` turned out to be comparable for free
(the parsed SkPath has structural equality) and its "incomparable like
every outline()" doc was the stale claim. Kit riders: `kit::shapes::ring`,
`kit::ticks(Ticks, Frame)` and `kit::chords(Chords, Frame)` are values
(`TicksShape`/`ChordsShape`; a `Ticks::classify` callable is the one
member equality cannot see, so classified ladders stay conservative).
`TextPath` gained the `operator==` its own comment said could not exist,
which closes §10e's third bullet — and the equality is honest: a moved
`at` now patches, where the original omission silently kept the old
placement. Pinned by `ComposeShapeValues.*` (10 cases): the prune
observed via `patchedNodes`/`picturesRecorded`, the Chevreul bake-survival
scenario, honest inequality on changed parameters, the escape hatch's
conservatism, and a positive control (defeating the propsEqual compare
turns the two prune pins red). One pre-existing pin strengthened:
`ComposeWidthProfile.TheLastNeverPruneRibbonsCanPruneNow` asserted "no
MORE recordings than the first draw" because its tree carried
`.shape(circle())` and could do no better; it asserts zero now.

**THE PLATE LEDGER — 56 scenes, Release, `--no-promotion` both arms,
quiet machine.** Baseline arm: the pre-change binary. Change arm: the
same tree rebuilt with the seam. **53 of 56 byte-identical.**
`genesis_fire` and `hitman_verlet` are the two scenes already on record
as self-nondeterministic on a quiet machine. `penrose_paving` differed —
and was proved NOT to be this change: a clean worktree at HEAD with none
of these edits renders the identical new hash (`ccd7c2aa…`, stable
across four renders and two independently built binaries), so the delta
sits between the baseline binary (built before the `positioned()`
commit was cut) and HEAD itself. Two findings ride on that, filed here
because this ledger surfaced them: **(1) §2's positioned() acceptance
claim ("Release plate BYTE-IDENTICAL to `834bf1d613ae`") does not hold
at HEAD** — HEAD renders `ccd7c2aa…`, 88.97% of pixels at maxDelta 139,
the full-plate low-amplitude signature of every antialiased edge moving
sub-pixel (instanceRect() resolves rects through different rounding
than the Yoga path it replaced, and the grain overlay amplifies it);
whatever state that acceptance measured, it is not the committed one.
**(2) The stored R4 baseline hashes are stale as a gate** — only 19 of
56 scenes still match `/tmp/r4_q1.sha256` at HEAD (the Skia m151 bump
moved antialiasing corpus-wide, plus the approved widthFn/positioned
deltas), so any future ledger must bake its own baseline arm rather
than quote those files.

**Both findings DISPOSED — owner ruling 2026-08-03: amend the claim, do
not bisect.** Finding (2) already named finding (1)'s mechanism: the
m151 bump moved antialiasing corpus-wide, and the penrose delta wears
exactly that signature — ~89% of pixels at maxDelta ≤ 139, every edge
shifting sub-pixel. The byte-identity claim simply predates the
toolchain bump that landed in the working tree between claim and check,
and `ccd7c2aa…` is stable across four renders and two independently
built binaries, so the new number is reality, not flake. Attribution is
TOOLCHAIN, not compose — the instanceRect()-rounding hypothesis in (1)
is retired with it — and a bisect would spend a day re-deriving what
the m151 record already states. §2's acceptance text now carries the
correction inline. The gate is repaired the way (2) asked: the plate
baseline was rebased 2026-08-03 onto the post-m151 lineage, so future
ledgers gate against reality rather than against a pre-bump hash. And
the "must bake its own baseline arm" rule itself is retired as
methodology, not just satisfied once: `ComposeGallery --ledger` +
`scripts/plate_ledger.py` (the §20 note records their maiden run) made
the baking one command and ~8 minutes, and the 2026-08-03 baseline is
what the tool's manifest-compare now gates against — the rule's
spirit, never quote a stale baseline, lives on inside the tool.

**What stays open, and it is one sentence of this entry:** geometry that
is BOUND (an outline as a function of live Outputs — Winamp's EQ curve)
still has no spelling and still drops to `custom()`; an
`Animatable`-aware shape is its own designed pass, not a comparability
fix. The original entry follows, unedited:

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

**CLOSED 2026-07-27 — SHIPPED as exactly that:**
`Material::buffer(std::shared_ptr<PixelBuffer>, …)` over a concrete
`PixelBuffer` (N32 bitmap + `canvas()` writer + `commit()`; snapshot
copied once per revision, so a pruned describe never touches a pixel).
The recipe compares by (source, revision) — the Instances rule verbatim
— so identical re-describes prune between commits and one commit
patches exactly once. Rides the ordinary static-material fill path:
picture caching, decorations and every slot stay intact, which is the
whole entry. Pinned by
`ComposeMaterial.ABufferPrunesBetweenCommitsAndPatchesOnCommit`;
451/451; ledger byte-neutral (56/58 identical, movers = two flappers).

## 5. A `Material::blend` layer has no amount — *two studies* — **CLOSED 2026-07-27**

"Soft-light this noise at 30%" has no expression. The only route is
baking `0.5 + (v-0.5)*amp` into the noise's own SkSL, which means every
consumer forks the stock generator. (`grain` grew a `contrast` parameter
for exactly this reason; that fixes one generator, not the shape of the
problem.)

Natural API: `blend({{base, kSrcOver}, {tex, kSoftLight, 0.30f}})`, or
`Material::amount(float)` on the layer value.

**SHIPPED as the second spelling** — `Material::amount(a01)` on the
layer value (the triple-braced overload would have been ambiguous
against the existing pair vector at every current call site). Photoshop
layer-opacity semantics, stated in the header because the two readings
differ on every non-Porter-Duff mode: the layer composites with its
mode IN FULL, then the result mixes back toward the accumulation — not
src-alpha thinning. Implemented as one process-wide mix() runtime
effect (the Patterns.h one-effect rule) applied in both the eager
flatten and the deferred per-resolve path, so live and
geometry-dependent layers carry their amounts too. The amount is
recipe: it participates in `Material::operator==`, so equal amounts
prune and a changed amount patches. Pinned by
`ComposeMaterial.ABlendLayerCompositesAtItsAmount`.

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

*(Correction 2026-08-03: both halves of that residual are resolved, and
the §10b pointer dangles — §10b carries no dashPhase bullet. The
node-level `trim()` no longer exists to be the only anything: §33 R4
deleted `Element::trim` when `mask(parts::…, by::…)` took its jobs. And
`dashPhase` HAS a bound form — `PathFormat::dashPhaseBinding`
(Decorations.h), "Bind it and the dashes march" (Lines.h). Nothing in
this residual is missing any more.)*

**The real defect was discoverability, and this is the second time this
week a study has worked around something that exists** — the other being
the bound `Fill`. A gap list is only worth its accuracy: an entry that
reads "impossible" outranks one that reads "awkward", so one wrong entry
distorts everything below it. Both were caught by checking the claim
against the source before ranking it, and both should have been caught by
the header saying so at the call site.

## 8. `routers::orthogonal()` is unusable for its most natural application — **CLOSED 2026-07-27**

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

**CLOSED 2026-07-27 — SHIPPED as the natural API, with one sub-claim
overturned by the header's own law.** Verified against source first:

- *No rail spelling / no adapter* — **real**. Routers.h had only
  polyline/octilinear/orbit as RailRouters; the `rail(anchors,
  routers::manhattan())` call site did not exist in any spelling.
- *Always bends at midX, corners only round* — **real**, by
  construction (`midX = (fx+tx)/2` hard-coded; SkCornerPathEffect).
- *Zero-length segments* — **real** (verb-dumped: an fy==ty pair emits
  M + three lines with a zero-length V leg; fx==tx degenerates both H
  legs). *The flare they were blamed for* — **not real in today's
  code**: the exact scene (axis-aligned connector, `lines::cased`, the
  offset-shaper Brush, with and without cornerRadius) renders
  BYTE-IDENTICAL to hand-authored clean geometry (PIL diff: 0 changed
  px, maxDelta 0) — Skia's stroker skips exactly-degenerate segments
  and every contour-measure construction drops them. The symptom the
  study saw is real somewhere; this mechanism is not it. (The one
  artifact that does reproduce is different: a NEAR-aligned pair — a
  1 px jog — under `cornerRadius` knots at midX, because
  SkCornerPathEffect halves a segment shorter than 2r. A chamfer or a
  clean bend policy avoids it.)

Shipped, all additive (`orthogonal()`'s zero-arg output frozen verbs
and all, per §27 — corpus callers pass floats, which cannot resolve to
the new overload):

- `routers::manhattan(Bend, cornerRadius, chamferCut)` — the
  RailRouter; `routers::orthogonal(Bend, ...)` — the same family as a
  pairwise overload; both collapse consecutive-duplicate and
  forward-collinear waypoints (reversal spikes kept: real geometry).
- `routers::fromPairwise(Router)` — any pairwise router rides
  `rail()`: legs stitch into ONE contour (terminal caps fire once),
  junction moves drop, zero-length segments collapse, exactly-collinear
  line runs merge, curve legs (`arc`) ride through.
- `Bend::MidX | HFirst | VFirst` — the Z and both Ls (bend at the
  target column / source column).
- `routers::chamfer(path, cut)` — the 45° cut as a function (clamped to
  half-legs, closed polylines cut their closing vertex, curved contours
  pass through), wired as `chamferCut` on the manhattan family and as
  `kit::brush::shapers::chamfered(cut)`, Rounded's machined sibling for
  any brush pipeline.

Pinned in ComposeTestLines.cpp: `ComposeRouters.
ManhattanIsARailRouterAndCollapsesCollinearRuns` (also freezes the old
spelling's degenerate verbs as the §27 contrast),
`BendPoliciesTakeTheNamedColumns`, `ChamferCutsTheCornerRoundingCannot`,
`FromPairwiseStitchesOneContourAndKeepsCurves`, and the cased-brush
pixel guard `ManhattanCasedRailMatchesCleanGeometry`. Positive controls
run: disabling collapse, bend, chamfer and the adapter's merge each
failed exactly its own pin (the pixel guard stayed green with collapse
disabled — which is the flare verdict above, demonstrated a third way).
Suite 457 → 462. Ledger (Release, --stability 2): 54/58 byte-identical,
movers = the three documented flappers plus easel_playground, which
moved SELF-STABLY with zero references to any router — its own .cpp and
the shape/ library under it carry the concurrent shape-session's
uncommitted edits, so it is attributed there; byte-neutral for this
entry's delta (corpus callers all pass floats, which cannot resolve to
the new Bend overload).

## 8b. No way to shape a stroke ACROSS its width — **CLOSED 2026-07-27 by §33 ruling 12: a milled groove is band+fill; no crossFill lane**

*(Header reconciled 2026-08-03: the ruling had closed this entry
without the header ever yielding. The record is §33's 2026-07-27
rulings session, ruling 12; the entry below stands as filed.)*

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
- ~~**No way to shape a run without building an Element.**~~ **CLOSED
  2026-08-04.** `measure()` is
  per-Element, so hand-placing 230 glyphs cost 230 layouts. Wanted:
  `FontContext::measureRun(u8string, TextStyle) -> vector<float>`.
  Landed as the compose free function `measureRun(u8string_view,
  TextStyle, FontContext&) -> vector<float>` beside `metrics()` — a
  method on weave's FontContext would put a compose convenience inside
  the layout engine, and metrics() already set the free-function
  precedent for exactly this tier. One Paragraph + one unconstrained
  single-line `layoutParagraph` (the same machinery `text()` runs),
  advances collected via `forEachPlacedGlyph`. Pin:
  `ComposeText.MeasureRunShapesOnceAndMatchesTheLaidOutElement` — the
  advance sum reproduces `measure(text(...))`'s width through the full
  Element path (the independent arm), a doubled face doubles the run,
  and an empty run shapes to nothing.
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
  API.md never names slots as *the* counter idiom. *(Doc half taken
  2026-08-04: API.md's Slots tier now opens "Slots are THE text-content
  idiom", says outright that no text-Output overload exists, and names
  the counter/timer/ticker cases. The `PropValue<u8string>` question
  itself stays open — this note is about discoverability, not a new
  lane.)*

## 10. Decorations: adaptors and frames

- **Light angles are in the node's LOCAL frame.** `BevelEmboss`,
  `InnerShadow` and `util::Shadow` take an angle in node space, so on a
  rotated node you write `120 + angDeg` by hand or 514 boards light from
  514 directions. Correct at the low level, wrong as a default, subtle
  enough to ship. Wanted: a `worldLight` flag, or the node's accumulated
  rotation on `PaintContext`.
- ~~**`lines::Line` with `parallels > 1` has no join control**~~ —
  **CLOSED 2026-08-04.** The offset
  contour rounded sharp corners, so 45° jogs came out as soft S-curves.
  The offset is built from a stroke outline anyway, so the entry's route
  was taken literally: `Line::join` (`SkPaint::Join`, default stays the
  grounded round) is applied to the drawn strokes AND to the spread
  paint that builds the parallel rails. Plain data, so it rides the
  defaulted equality. Pin:
  `ComposeLines.ParallelJoinControlKeepsACornerSharp` — miter reaches
  the outer corner's diagonal point, round provably does not, and two
  same-join renders are byte-identical (the control).
- ~~**`connector()` has no endpoint gap.**~~ **CLOSED 2026-08-04.**
  `Anchor` has one; `connector()`
  did not, so a route always ran to the node box's *centre* — and with
  `sdf::` chrome the box is far larger than the visible shape. Now
  `connector(from, to, router, gap)`: Anchor::gap's spelling and clamp
  (≤45% of the route per end), applied to the ROUTED PATH's open-contour
  terminals so it works under any router. `DeriveData::connectorGap`
  joins `deriveEqual` and the 14→15 field pin; the existing re-described-
  route invalidation covers a gap-only change. Pin:
  `ComposeDerive.ConnectorGapPullsTheWireOffTheEndpoints` — gap 0 (the
  control) pierces both terminals exactly as before; gap 30 starts and
  stops where it says and leaves the middle intact.

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
  reads as a warning. *(Taken 2026-08-04, the doc route — because the
  min-side-relative variant ALREADY EXISTS: `glowUnit()` is radius-as-
  fraction-of-the-shorter-side, radius 1 = the inscribed circle, and has
  been the documented remedy since §14. What was missing was this entry's
  direction of the trap: radialUnit's doc warned about the hard rim at
  radius 1, not about a ramp authored PAST 1 disappearing outside the
  disc. The doc now states both, and names glowUnit as the min-side
  variant. Doc-only, no pin.)*
- ~~**No offset-focus radial.**~~ **CLOSED 2026-08-04.**
  `SkShaders::TwoPointConicalGradient` is the
  natural sphere-shading primitive; displacing the centre works but
  couples falloff to offset. Now `Material::conical(focus, focusRadius,
  center, radius, stops, tile)` — the Skia primitive exposed under the
  factory tier, `Recipe::Kind::Conical` reusing the gradient arm's
  p0/p1/f0/f1 fields (no new Recipe fields, so Material's 7-member field
  pin stands). Pin:
  `ComposeMaterial.ConicalMovesTheHighlightWithoutMovingTheFalloff` —
  the highlight pixel visibly follows the focus against the centered
  control arm, identical recipes compare equal, a moved focus does not,
  and a degenerate conical never aliases a radial.
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
  *(Taken 2026-08-04: grain's doc block now opens with "GRAIN WANTS AN
  OPAQUE SURFACE", the nebula case, and the multiply-over-solid-ground
  remedy. Doc-only, no pin.)*

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
- ~~**`TextPath` has no `operator==`**, deliberately (its baseline is a
  `std::function`), so a node carrying one never prunes — 72 radial
  labels re-record on every `render()`.~~ **CLOSED with §3** — the
  baseline is a `Shape`, `TextPath` compares structurally, and a run on
  a comparable generator prunes
  (`ComposeShapeValues.TextOnAComparableBaselinePrunes`).

## 10f. `Material::sksl()` has no child shader — but a LUT is NOT unreachable — **CLOSED 2026-07-28**

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

### CLOSED 2026-07-28 — the child slot

**The surface is one verb**, beside `uniform()` and obeying its rules:

```cpp
Material::sksl(paletteFx, {{"uShade", 1.0f}})   // uniform shader uIndex;
    .child("uIndex",   Material::image(indexTex, …, kNearest))
    .child("uPalette", Material::image(lut, …, kNearest));
```

Any Material is a legal child, sksl ones included; the tree still compiles
to ONE shader. `Material::Live` grew a `children` vector (name → Material)
and nothing else moved: `build()` fills each declared slot from
`child.resolve(ctx)` on the paint path and `child.asShader()` on the static
snapshot, so a child sees the SAME PaintContext — the parent node's box, the
parent's clock — because there is one node.

**Three things were load-bearing, and each has a pin whose positive control
was run** (break the mechanism, watch the named test fail, restore):

1. **Tier inheritance.** `isAnimated()` and `geometryDependent()` recurse
   into the children. Without it a live child is resolved once and frozen
   into the parent's cache. `ALiveChildMakesTheParentLive`,
   `AGeometryChildPropagatesTheGeometryTier`.
2. **The resolve memo's blind spot.** `Live::lastInputs` is a digest of the
   parent's OWN varying inputs and cannot see a child's, so a material with
   a context-needing child skips the memo entirely. Left in, the second
   frame returns the shader built on the first and the child never ticks
   again — the control fails exactly there.
3. **The prune signature.** Children compare recursively inside
   `operator==`. A child read live that did not participate in reconciler
   equality would leave a pruned node sampling last frame's palette forever
   (DESIGN.md's rule, written for this case). `TheChildRidesThePruneSignature`
   pins both halves: identical children prune (`patchedNodes == 0`,
   `picturesRecorded == 0`), a swapped LUT patches and repaints.

**The driving case is pinned end to end at pixels**
(`AChildSlotSamplesAnIndexTextureThroughAPalette`): a 4-cell index texture,
a 4-entry palette, one shader — the picture re-colours when the LUT is
swapped with the index texture untouched, and `min(i + uShade, 3)` moves
every cell one entry down the ramp and clamps at the end. `kNearest` is
carried in the test's own comment because an index sampled at `kLinear` is
a blend of two unrelated palette entries.

§27 holds: the spelling is NEW and nothing existing calls it. For an
existing material the whole change is an empty vector — `build()`'s
`childNeedsCtx` scan is over nothing, equality compares two empty lists,
and both tier queries loop zero times.

**THE GATE, 2026-07-28.** Debug and Release both build clean and
warning-free. `ctest -C Debug` **16/16, 430.13 s**. `compose_test`
**461 cases / 83 suites** (462 before: −6 from the §34 audit ruling, +5
here), 460 passed and 1 skipped (§34's variation-drive skip);
`compose_kit_test` 47, unchanged. **PLATE LEDGER** (Release,
`scripts/plate_ledger.py`, 58 scenes, `--no-promotion`): **54
byte-identical, 3 attributed flappers** (`genesis_fire`,
`hitman_verlet`, `slitscan_2001` — the documented quiet-machine list),
**1 mover: `easel_playground`, attributed to SigilShape, NOT to this
change.** That scene is one of the two that link SigilShape, whose
sources were being edited in parallel; the Release `libSigilShape.a`
rebuilt underneath the sweep (12:01) while the baseline predates those
edits (Jul 27 19:24), and `easel_playground.cpp` itself is untouched.
No rebase was taken.

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

### DESIGNED AND LANDED 2026-07-29 — `env::`, and why it costs the prune nothing

**The tension, stated first, because resolving it is the whole entry.**
Pruning is this library's central performance property: a re-described
node that compares equal to its previous self does not re-layout,
re-record or re-paint (§3 measured 43.4 of 43.5 ms lost on ONE node whose
outline callable could not compare). An inherited value looks, by
construction, like the thing that breaks it — an input a node reads
without it appearing in that node's own props. A naive context
invalidates everything below the provider and destroys pruning for the
whole subtree.

**It is a false dilemma, and one fact dissolves it: DESCRIBE IN THIS
LIBRARY IS EAGER, TOTAL, AND OUTSIDE THE KERNEL.** `box().child(panel())`
calls `panel()` before the box exists; components are plain functions and
their arguments are evaluated inside the enclosing scope. The kernel
never calls a component — it is handed a finished tree. So the
describe-time CALL STACK is the element tree, and the C++ answer to
"inherit down a call stack" is dynamic scope. A value read there lands in
the reading node's own props before the Composer sees anything, which
means:

- **`propsEqual` is already the exact dependency tracker.** A node whose
  props came out identical prunes, whether or not it read the
  environment. A node whose colour actually moved re-patches. Nobody has
  to record a read set, and the granularity is per-property, not
  per-subtree.
- **The element tree is environment-INDEPENDENT.** By reconcile time the
  theme is baked in; no kernel phase learns a new concept.
- **"Which nodes must re-describe" is nearly moot.** Every `render()`
  already re-executes every component. The question that costs money is
  which nodes PATCH, and that answer was already correct.

The shipped shape is therefore **dynamic scope over the describe stack**:

```cpp
env::Provide<Palette> theme(dark);      // binds for this scope
return box().child(panel());            // panel() -> … -> a chip four
                                        // levels down reads it
const Palette *p = env::inherited<Palette>();
```

**THE ONE PLACE THE KERNEL HAD TO LEARN IT is `memo`** — the only site in
the library where a component function runs after the author's scope has
ended (`resolveMemo`'s `invoke`, and grep confirms it is the only deferred
describe: `Instances::variants` calls `make()` eagerly, `snapshot()` and
`measure()` take a built tree). Left alone, a memo would hit on props
alone and serve the theme it first described under forever. So a memo now
captures the ambient stack at construction, compares it *before* its
props, and re-establishes it around the deferred call — `memo` is a pure
function of **(props, environment)**, which is the same purity contract it
always claimed, honestly stated. That is the kernel change this entry
predicted, and it is ~35 lines.

Everything else that takes a callable — a `ContourWalk` stamp, a
`custom()` program — runs at derive or paint time with NO scope, and
`inherited<T>()` there returns null deterministically rather than
something stale. The rule: such a lambda captures what it needs BY VALUE
at the call site, which is where the scope still exists.

**THE THREE REJECTED SHAPES.**

- **(b) a provider NODE with subtree invalidation keyed on its value.**
  Rejected: it buys nothing `propsEqual` does not already do better, and
  it pays for it by re-recording subtrees whose props did not move. The
  measurement is in this file already — the CDE study's palette switch,
  with the colours as plain values, cost **237 re-records over a
  1270-node desktop** on the switch frame and nothing on the other 299.
  A provider node would have invalidated the provider's whole subtree,
  which on that artefact is the desktop. It also puts a second identity
  system in the kernel for a value the kernel never needs to see.
- **(c) resolution at PAINT time.** This one deserved the serious weight
  the brief gave it, and it is *already shipped* — a bound `Fill`, a live
  `Material` uniform, resolved per paint with `animated()` declaring the
  volatility (that is also §10h's half-closure). It was measured, on
  exactly this artefact, and it lost: **40 bound `Fill` Outputs cost
  0.33 ms/frame steady against 0.033 ms for the same 40 as plain values —
  ten times the steady paint to save ~1.1 ms on one frame in three
  hundred, 5.6x more total work per palette change** (`cde_motif.cpp`
  header; captures pixel-identical both ways). It is also partial: it
  serves colours and cannot serve a padding, a face, a type size — a
  theme that changes text metrics must reach LAYOUT, and paint is
  downstream of layout. Kept as the per-property escape for values that
  genuinely move at 60 Hz; refused as the mechanism. (The real defect it
  exposes is §Argument 3's, not this entry's: `isAnimated()` is per-node
  and binary, so "repaints when the theme changes" and "repaints at 60 Hz"
  are one declaration.)
- **(a) a comparable `Theme` threaded through describe with reads
  recorded.** This is what shipped, minus the read recording. Read sets
  would buy exactly one thing: a memo that does NOT read the environment
  could keep hitting through a theme change. Scoped, not built, because
  the cost it avoids is a describe call whose result then prunes anyway,
  and a thread-local read flag is only sound under memo's existing purity
  assumption. Measure before building it.

**(d) "stays out of the kernel entirely"** is what landed, to within the
one memo seam — and the seam is not optional. Note also that this entry
is NOT the palette/theme layer archive/EXTRACT.md §4.7 refused: bindings
are keyed by C++ TYPE and the key a library component uses is its own
existing props type (`console::Style`, which is now comparable and has an
env-reading `console(ring)` overload). There is no `compose::Theme`, no
role names, no scale — the library ships the CHANNEL, the author owns the
value.

**WHAT A THEME'S EQUALITY MEANS.** Two inherited values are equal exactly
when describing anything under them yields props that compare equal under
`propsEqual`. So the comparison is structural and exact — `SkColor4f`
bitwise, typefaces by pointer identity — never perceptual, never
epsilon'd, because the consumer of the answer is the prune and the prune's
contract is "provably identical". One consequence is load-bearing:
**materialise derivations INTO the value.** A theme carrying a
`std::function` derivation rule is incomparable and turns every memo below
it into a permanent miss, which is §3's wall in new clothes. Motif's
`XmGetColors` is the model — run the function, store the eight-by-five
colours it produced.

**SURFACE** (kernel, `Compose.h`; API.md §env): `env::Provide<T>` (RAII,
LIFO, an inner one shadows, other types unaffected), `env::inherited<T>()
-> const T*` (null = use your own default, like a React context default),
`env::inheritedOr<T>(fallback)`, `env::bound<T>()`. Free when unused: the
snapshot on a memo is an empty vector, so no allocation and no compare.

**PINS AND CONTROLS** (`ComposeTestKernel.cpp`, `ComposeEnv.*`, 6 cases).
Every control was run — break the mechanism, confirm the NAMED test fails,
restore:

| Pin | Control | Result |
| --- | --- | --- |
| `UnchangedEnvironmentStillPrunes` — patchedNodes 0, `dirty()` false, picturesRecorded 0 | second render with a MOVED palette | fails: patchedNodes 1, dirty true, **picturesRecorded 4** |
| same | `inherited<T>()` -> nullptr | fails (its `ASSERT_EQ` on the inherited colour) |
| `ThemeChangeRepatchesOnlyTheNodesThatMoved` — patchedNodes **exactly 1** through four container levels and a plain sibling | `inherited<T>()` -> nullptr | fails |
| `MemoIsAPureFunctionOfPropsAndEnvironment` | `envEqual` -> always true | fails: memo serves the stale colour |
| same | `envEqual` -> always false | fails: memoHits 0 where 1, describeCalls 2 where 1 |
| same | `EnvRestore` deleted | fails: the deferred describe reads the fallback |
| `ALibraryComponentReadsTheEnvironmentByItsOwnPropsType` | overload ignores the binding | fails |

**One control was VACUOUS on the first attempt and the pin was rewritten
because of it.** Deleting `EnvRestore` did not fail anything, because the
first draft called `render()` *inside* the `Provide` scope — the ambient
stack was still live during reconcile and the restore had nothing to do.
The pins now DESCRIBE inside the scope and RECONCILE after it ends, with
an `ASSERT_FALSE(env::bound<T>())` between the two statements, which is
also the honest model of the two phases. Separately,
`UnchangedEnvironmentStillPrunes` was vacuous in a second way — with
`inherited` broken it read the fallback both renders and pruned happily —
so it now asserts the pixel IS the inherited colour before asserting the
prune. Both are recorded here because the program's standing lesson is
that a passing pin is not evidence until its control has failed.

**THE GATE, 2026-07-29.** Debug and Release both build clean and
warning-free. `ctest -C Debug` **16/16, 434.53 s** (both configs rebuilt
in full first — the sketch host's header-skew guard hashes headers, and
this change touches two). `compose_test` **477 cases / 85 suites** in
BOTH configs (471 before: +6, the `ComposeEnv` slice), 476 passed and 1
skipped (§34's variation-drive skip); `compose_kit_test` 47,
`shape_test` 83, `world_test` 45, `motion_test` 11 — all unchanged.
**PLATE LEDGER** (Release, `scripts/plate_ledger.py`, 58 scenes):
**54 byte-identical, 3 attributed flappers** (`genesis_fire`,
`hitman_verlet`, `slitscan_2001`), **1 mover: `easel_playground`
187686f0e651 -> 39528e682c55**, which is the SigilShape-attributed hash
this sweep was told to expect and not this change. Byte-neutral, as an
additive feature should be. No rebase was taken.

**SCOPED, NOT BUILT.** (1) Read tracking — a thread-local flag set by
`inherited<T>()` so a memo that never read the environment keeps hitting
through a theme change; sound under memo's existing purity assumption,
and worth exactly one describe call that would prune anyway, so it wants
a measurement first. (2) A second library consumer: `console::` is the
worked example; `Debug.h`'s plates and the `kit::` frames are the obvious
next keys, each on its own props type. (3) The `styles::` bundle the entry
names does not exist yet — when it does it inherits by the same rule with
no new mechanism. (4) Nothing was done about §Argument 3's real defect,
which this entry keeps running into: `isAnimated()` is per-node and
binary, so a property that changes every three seconds and one that
changes at 60 Hz make the same declaration.

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
- ~~**A `widthFn` Ribbon can never prune**~~ — CLOSED by the
  widthFn→Profile migration (§33's dated note): every corpus law is a
  comparable value now, keying is part of the type, and the prune is
  pinned by test.
- **`brushes::Ribbon` has no corner joins**, so a 14-corner route shows
  its facets.

## 10j. Winding, boxes, and three traps a colour study walked into

- ~~**`shapes::circle()` has no winding direction, and the winding IS the
  historical convention.**~~ **CLOSED (found already shipped) 2026-08-04.**
  On a text baseline, path direction decides
  whether glyph-up points radially IN (Chevreul's limb) or OUT
  (Nightingale's ring) — one uniform engraver's convention each way, and
  opposite in sign. `circle()` is `addOval(kCW)` from 12 o'clock, so half
  of all ring inscriptions need a hand-rolled `OutlineFn`. Wanted:
  `shapes::circle(SkPathDirection, float startDeg)`, or
  `Orient::RadialIn/Out`.
  *The papercut pass came to build this and found it built — the shapes
  wave (fcbdfa1) had already landed
  `shapes::circle(SkPathDirection, unsigned startIndex = 1)`, spelled
  with `addOval`'s startIndex rather than the entry's `startDeg` so the
  oriented overload stays byte-for-byte `addOval` conics (its own doc
  records why, including the startIndex=0 near-miss a test caught).
  Pinned by `ComposeText.RingWindingDecidesWhichWayTheGlyphsFace`
  (onPath text observably reverses; the directed default IS the
  undirected path — the control) and the `KitFrame` baseline-direction
  suite. An arbitrary startDeg remains unspelled; nothing has asked for
  a start point off the oval's four extremes.*
- **`TextPath`'s baseline resolves against the TEXT NODE's own box**, so
  the obvious `disc(c, R).child(text(...).onPath(...))` collapses every
  label into a blob at the text's intrinsic size. The working spelling is
  that the text leaf IS the disc. Undocumented; one sentence pays for
  itself. *(Taken 2026-08-04: the sentence is on `TextPath::path`'s doc
  in Compose.h, where the author meets the field. Doc-only, no pin.)*
- **`snapshot()` sizes by the root's CHILDREN, not the root's own dims** —
  stated only in a comment inside `Instances.h`. A probe of
  `box().width(32).fill(…).effect(…)` read back a colour implying an
  exponent of 1.82; the same content wrapped in a shell reads 2.20. For
  the read-your-own-output-back pattern that verification depends on,
  that is a silent wrong answer. *(Taken 2026-08-04: stated on
  `snapshot()`'s own doc in Compose.h — including the shell-wrap remedy
  the Instances.h comment carried. Doc-only, no pin.)*
- ~~**`Material::sweep` clamps outside `[startDeg, endDeg]` instead of
  wrapping**~~ — **CLOSED 2026-08-04 (the diagnostic, not a wrap):**
  `sweep(c, stops, 90, 450)` — the obvious way to start a
  hue wheel at red — drew a quarter of the ring in the first stop's
  colour with no diagnostic. The factory now warns once per process when
  a window leaves [0, 360] (the sdf-pad warn-once pattern), and the
  header doc says to rotate the STOPS instead. Clamping itself is
  unchanged — partial sweeps inside the circle are legitimate and stay
  silent. Pin: `ComposeMaterial.SweepWarnsWhenTheWindowLeavesTheCircle`
  (in-circle windows stay silent — the control — then the trap window
  names itself).
- ~~**`lines::concentric` cannot place a ring at a stated radius.**~~
  **CLOSED 2026-08-04.** Radii
  were `inner + (reach − inner)·k/rings` with `reach` the bbox
  HALF-DIAGONAL, so on a `circle()` node the outermost ring always landed
  at R√2 — outside the shape, clipped away. Same class as §10c's
  `radialUnit` trap, and a two-circle limb was unspellable. Now:
  `RadialHatch::radiiPx` (joins the hand-written equality) and the
  `lines::concentric(fill, std::vector<float> radiiPx, …)` overload —
  stated px radii, one circle per entry; the spacing form is untouched.
  Pin: `ComposeLines.ConcentricPlacesARingAtAStatedRadius`, whose
  control demonstrates the trap verbatim (one evenly-spaced ring on a
  circle() node draws NOTHING).

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

## 11. `Effect` has no live uniforms — **CLOSED 2026-07-27**

`Material` solved this with `uniform(name, &output)` and a volatility
contract. `Effect::shader(fx, uniforms)` takes constants only, so
animating a ripple phase or a bloom threshold requires a full re-describe
per frame.

**SHIPPED as exactly that spelling**: `Effect::uniform(name, &output)` —
resolved per paint (`resolvedImageFilter()`), declaring content
volatility AND group-memo opacity the way a live material does (the
filter is captured by recordings, so a bound uniform is content, not
paint). `then()` chains with a live side re-compose per paint; static
chains precompose once as before. And the seam got the same equality
upgrade on the way: a STATIC shader effect now compares by RECIPE
(runtime-effect pointer + constant uniforms) instead of by filter
pointer, so the sharedHeavyEffect pattern — one process-wide
SkRuntimeEffect, re-described each frame — prunes instead of re-patching
(its fixture comment in the cache tests documents the old failure).
Pinned by `ComposeEffects.ALiveUniformAnimatesWithoutRedescribe`,
`AStaticShaderEffectPrunesByRecipe` (honest inequality included) and
`LiveChainsRecomposeAndStaticChainsStayCheap`. Scope note: the
Composer-level `setView()` effect stays static — a live VIEW transform
has no redraw contract and was not asked for.

**Plate ledger (shared with §5's closure — one sweep gated both):
54 of 56 byte-identical** against the §3-closure arm, Release,
`--no-promotion`, quiet machine. The two movers are `genesis_fire` and
`slitscan_2001`, both on the documented self-nondeterministic list —
and slitscan's attribution is hash-exact: the two pre-change arms both
render `c6975b5f3273…`, the same value the R4 ledger records as this
scene's recurring hash, and the post-change arm rendered the scene's
other face. No corpus site spells the new APIs, so identity was the
prediction, and it held.

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

**SWEPT 2026-07-29.** Eleven sub-items; each checked against the SOURCE
rather than against its own text. Result: **six were already closed** and
verified still present (`minBoxFor` warning in `Material.cpp`,
`Material::bleed`, `patterns::sequence`, `custom(key, program)`,
`Radial::radiusAt`, `ContourWalk::stampAt`); **two closed today**
(`console::line`, the `width()`/shrink documentation); **one was NEVER
REAL** (`echo()` — it appends, so registration doubling is two calls, and
that is now pinned); **two stay open and are marked with why**. Status is
inline per bullet below.

- ~~**`sdf::` glow eats the shape silently.** `pad()` is reserved *inside*
  the node's box, so a 300×300 box with `glowRadius: 54` renders a 0.5 px
  disc and says nothing. `sdf::minBoxFor()` is the answer and nothing
  points at it from the call site. Warn when `pad ≥ half-size`, or add
  `Style::glowOutside` that bleeds past the bounds the way `OuterGlow`
  already does.~~ **CLOSED 2026-07-27 — the warning half shipped** (the
  entry offered either/or; `Style::glowOutside` remains unbuilt): a
  once-per-process SkDebugf in `Material::build` (Material.cpp), where
  the numbers finally meet — uPad is a style constant, uResolution the
  laid-out size — scoped to the sdf prelude's signature (uPad + uGlowR
  + uResolution) and firing when pad >= half of the box's min
  dimension; the message states the pad, the box, the ~px the shape
  shrank to, and names `sdf::minBoxFor(style, contentPx)` as the fix.
  *(Re-verified 2026-07-29: the warning is live in `Material.cpp`;
  `Style::glowOutside` is still unbuilt and still unrequested.)*
  Stderr only, zero pixels moved. Pinned by
  `ComposeSdf.PadSwallowingTheBoxWarnsOnceNamingMinBoxFor` (fires once,
  silent on the second offender; control run: fails with the warning
  disabled). 457/457; ledger 55/58 byte-identical (flappers + the
  shape-attributed easel_playground).
- ~~**`Material` has no `bleed()`.** `DecorationScheme` can declare one so
  the recording cull grows; a Material cannot, so anything painting
  outside its box needs arithmetic the caller does.~~ **CLOSED
  2026-07-27 — `Material::bleed(px)` shipped, the same number on the
  same word**: a builder + getter participating in `operator==` (a
  changed reserve must re-record), read by `ownPaintBounds` /
  `recordBounds` from BOTH carriers `Element::fill(Material)` stores —
  the live/geometry slot and the static recipe — and max-accumulated
  with the decorations' bleeds, so a fill on a `shape()` outline that
  escapes the box survives the picture cull and the Cache::Texture bake
  alike. Default 0 changes nothing (§27). *(Re-verified 2026-07-29:
  `Material::bleed` at Material.h:314.)* Pinned by
  `ComposeMaterial.DeclaredBleedGrowsTheRecordingCull` (both carriers,
  under Cache::Texture where truncation is hard; control run: pixels go
  black with the recordBounds hookup disabled). 457/457; ledger 55/58
  byte-identical (flappers + the shape-attributed easel_playground).
- **`Pattern` cannot pan LIVE** — the describe-time half is closed
  (`Pattern::offset(SkPoint)`, which turned out to be plumbing that
  already existed: `bake()` hands its matrix to `Material::image`, whose
  `localMatrix` always took a translation). What remains is
  `offset(PropValue<SkPoint>)` under the paint-only volatility contract
  bound transforms already have, so a conveyor or a marching weave
  animates without re-describing.
  **STILL OPEN 2026-07-29 — LEFT, needs a ruling.** Verified in source:
  `Pattern::offset(SkPoint)` is describe-time, folded into the
  `SkMatrix` handed to `Material::image`, which stores it in the recipe
  and compares it — so moving it re-records. Making it live is not a
  `Pattern.h` change: `Material`'s only live channel is `sksl()`
  uniforms bound to `ch::Output`, so this wants a NEW bound-matrix
  channel on `Material` plus a decision about what volatility a panning
  pattern declares (paint-only, or does the node go live?). That is a
  Material/Paint change with a contract attached, not a papercut.
- ~~**`patterns::stripes` is single-colour and un-phased.**~~ **CLOSED
  2026-07-27 — `patterns::sequence(runs, phase)` shipped**: {width px,
  color} runs along +x, period = their sum, wrapped phase, seam covered
  by painting two periods; rotate the Pattern for diagonals. Pinned by
  `ComposePatterns.SequencePaintsColouredRunsAndPhaseSlides`.
  *(Re-verified 2026-07-29: `patterns::sequence` at Patterns.h:73.)* (The stop
  model was never the tool: a 24-run sett is runs, not stops.) 453/453.
- **`HyphenationOptions` has no hyphenation in it.** `enabled` and
  `penalty` read like a hyphenator; the engine breaks solely at U+00AD
  discretionaries the author typed. A legitimate contract, badly named.
  **STILL OPEN 2026-07-29 — LEFT, needs a ruling, and it is not a
  compose item.** The type lives in
  `src/sigilweave/include/sigilweave/ParagraphLayout.h:47`, and its doc
  comment there already reads "Controls soft-hyphen handling
  independently from the break strategy" — so the contract is stated and
  only the NAME misleads. Renaming it (`SoftHyphenOptions`,
  `DiscretionaryOptions`) is a SigilWeave API break and the owner's call;
  the alternative — a doc sharpen saying outright that there is no
  hyphenation dictionary — is a comment in a header that rebuilds
  SigilWeave, every weave target, SpellCircle and all of compose in both
  configs, which is not a trade an overnight batch should make on its
  own authority. Bundle it with the next SigilWeave change.
- ~~**`console::console()` admits no entrance choreography.** It builds its
  line Elements internally, so `staggerChildren()` on the returned panel
  is a no-op and "the console types out on mount" is inexpressible.
  Wanted: `console::Style::entrance`, or expose `console::line(...)`.~~
  **CLOSED 2026-07-29 — `console::line(const Line&, const Style&)`
  shipped, the entry's own second option and the smaller one.** The
  entry's mechanism was RIGHT, which is worth recording in a list with
  this one's error rate: `staggerChildren()` delays the `animate()` mount
  transitions a child DECLARES, and `console()`'s rows are plain `text()`
  nodes that declare none — so the no-op was real and had that cause.
  `line()` is the row `console()` builds (it now calls it), key and
  palette resolution included, so an author who wants an entrance owns
  the loop and the three-line window and nothing else:
  `p.staggerChildren(40ms)` over `console::line(l, st).opacity(animate(…))`.
  `Style::entrance` stays unbuilt on purpose — it would be a decision
  about what an entrance IS, and §27 says a default that encodes a
  judgement about the caller's art cannot be changed compatibly. Pinned
  by `ComposeConsole.LineIsTheRowTheComponentBuildsAndCanBeGivenAnEntrance`
  (controls: the palette lookup dropped → the palette assertion fires;
  the `con#<seq>` key renamed → the key assertion fires; the panel's own
  spelling changed → the hand-rebuild prune fires; the stagger removed →
  the delayed-row assertion fires). Noted in the test itself: the prune
  assertion CANNOT falsify `line()`, because `console()` delegates to it
  and a break breaks both sides equally — what it pins is the panel
  spelling the doc comment tells authors to reproduce.
- ~~**`custom()` re-records on every `render()`** — its program is an
  incomparable callable. Wanted: a `custom(key, program)` overload, or let
  `Cache::None` imply "nothing to invalidate".~~ **CLOSED 2026-07-27 —
  `custom(key, program)` shipped**: the key declares the program's
  identity on the keyed-parametric contract (one key = one drawing; fold
  what varies into the key); equal keys prune, the unkeyed form stays
  the escape hatch. *(Re-verified 2026-07-29: the keyed overload at
  Compose.h:2397.)* Pinned by
  `ComposeContent.AKeyedCustomPrunesAndTheKeyIsHonest`. 452/452.
- ~~**`layouts::Radial` has one radius for all children.**~~ **CLOSED
  2026-07-27 — `Radial::radiusAt`**: a per-index fraction vector
  overriding `radiusFraction`, tail falling back to it. *(Re-verified
  2026-07-29: `Radial::radiusAt` at Layouts.h:48.)* Pinned by
  `ComposeLayouts.RadialRadiusAtGivesEachChildItsOwnRing`. 454/454;
  rode the sequence arm's ledger (byte-neutral, movers = flappers + the
  shape/-attributed easel_playground at its unchanged attributed hash).
- ~~**`decorations::ContourWalk` is one field from being the text-on-path
  answer for *sequences*.** It already samples the tangent and rotates to
  it; it just replays one stamp. A
  `stampAt(const PathSample&, size_t) -> optional<Element>` callback turns
  it into ruler ticks with numbers, ribbon menus, chained ornament.~~
  **CLOSED 2026-07-27 — `ContourWalk::stampAt` shipped, exactly the
  field the entry named**: called per sample with a running index
  (across contours); a returned Element is baked via `snapshot()` at
  intrinsic size and replayed centered like `stamp`; `nullopt` falls
  back to `stamp`, so a numbered major tick rides over plain minors in
  ONE walk. The bakes are per call, per record, UNCACHED by choice —
  each returned Element is a fresh node, so the §16 instance-side
  StampCache has nothing stable to key them on (per-index entries would
  churn its slots and evict the node's real brush bakes); the callable
  keeps the decoration conservatively unequal, which every ContourWalk
  already was (no operator== — the raw `draw` callable decided that
  long ago). *(Re-verified 2026-07-29: `stampAt` documented and live in
  Decorations.h.)* Pinned by
  `ComposeDecorations.ContourWalkStampAtSequencesPerSampleArt`
  (control run: test fails with the mechanism disabled). 457/457;
  ledger 55/58 byte-identical — movers were the two documented flappers
  plus the shape-attributed `easel_playground` (concurrent shape/
  session).
- ~~**`echo()` takes a single stamp.** Registration doubling on a light
  display face wants one each side of the glyph run, not one behind it.~~
  **NEVER REAL — settled 2026-07-29, and it is the preamble's pattern
  exactly.** `Echo` is stored in a `std::vector` (`ComposeInternal.h:219`),
  `Element::echo()` PUSHES BACK (`Compose.cpp:472`), and the paint pass
  replays all of them bottom-first (`Paint.cpp:1566`, `:1633` — shape and
  text). One call each side of the glyph run has always been the whole
  feature; three calls is three stamps, ordered. The symptom was real (the
  author wanted two) and the mechanism was a guess written in the same
  confident voice. What made it survive is that the only two echo tests in
  the suite each used ONE echo, so nothing in the corpus contradicted it.
  Now pinned by `ComposePaint.EchoesAppendSoRegistrationDoublingIsTwoCalls`
  (three stamps, one up-left and two down-right, plus the declaration
  order where the last two overlap; control: make `echo()` clear before
  pushing → two of the three stamp assertions fire).
- ~~**A fixed `width()` flex child still shrinks**, and the failure is
  silent overlap. Faithful Yoga semantics, so not a bug — but `width(150)`
  reads as "this is 150" at the call site. One sentence in API.md's layout
  section (pair with `.shrink(0)` when you mean it) would pay for itself.~~
  **CLOSED 2026-07-29 — documented in BOTH places, header first.**
  `Element::width()`/`height()` had no doc comment at all; they now carry
  the one the entry asked for (the flex BASIS, not a guarantee; `shrink`
  defaults to 1; pair with `.shrink(0)` when you mean it), and API.md's
  layout block repeats it where a reader scanning the surface will hit it.
  No test: there is no behaviour here to pin — the semantics are Yoga's
  and correct, and the defect was that nothing said so at the call site.

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

**CLOSED 2026-07-27 — SHIPPED as the Instance-side `StampCache`**
(ruling 6's home: a slot map handed through `PaintContext::stamps`).
All three kinds consult it before any raster work — Scatter and
Pattern's four slots publish pictures, Art publishes its 2x image +
logical size — keyed on the art's node WITH THE WEAK HANDLE the entry
demanded: an entry whose weak guard no longer locks to its key is a
recycled address and re-bakes, so inheriting the wrong art is
structurally impossible. The member cache in the value survives as the
standalone-paint fallback (no composer, no stamps pointer). What still
re-bakes is a NEW art node per describe — the key is the art Element's
node, so pointer-stable art is still the author's half of the contract,
and the header now says exactly that. Pinned by
`ComposeBrushes.AStampBakeSurvivesABrushRebuiltEveryDescribe`: the
renderSlot() trap reproduced (fresh Scatter value each describe,
Cache::None node repainting every frame, stable art) — one bake in five
frames where the old world took five. The crossing-cache half of
ruling 6 (weave's `discoverCrossings`) still wants its bench arm before
it rides the same store — unchanged, see the stage-two residue.
**Plate ledger: 53 of 56 byte-identical** against the §11/§5 arm
(Release, `--no-promotion`, quiet machine); the three movers are
exactly the documented self-nondeterministic list — `genesis_fire`,
`hitman_verlet`, `slitscan_2001` — and nothing else.

**RECONCILED 2026-07-30 — the closure stands; two headers and one
citation had not caught up.** Three later entries (§41 reason 2, §19's
pyramid-cache note, §43's motion-blur reopening condition) cite §16 as
if open. The audit: what closed is the fresh-VALUE case (a brush
rebuilt every describe finds a pointer-stable art's bake in the
instance StampCache — the pin holds). What the citing entries lean on
is the residual this closure EXPLICITLY declared out of contract: a
fresh art NODE per describe has no stable key, so identity beyond the
node pointer — the "describe-keyed bake cache", i.e. content identity
— is a distinct, never-designed mechanism, not a shipped one. The
citations' dependency is real; their attribution was stale. Corrected:
`Scatter` and `Art`'s headers still said "THE BAKE LIVES IN THE BRUSH
VALUE … ROADMAP §16 is the open fix" (Pattern's was updated at
closure; its two siblings were missed — the §§22/25/27 sibling-path
family again), now all three carry the closed wording; §41 reason 2
re-worded to the fresh-NODE mechanism (its force is unchanged);
§19/§43's reopening conditions now name the content-identity cache as
beyond this entry rather than as this entry. Measured before deciding
not to build it: every brush user in the corpus (nine sketches)
either describes once at setup or holds art as members — thunder_fulu
:731 "held for pointer stability", vagrant_story_target:1076,
eva_magi_interior:1238 hold theirs across a per-frame renderSlot —
so the uncovered case is paid ZERO times per frame in the corpus, and
the content-identity key it would need lands exactly on §41's frozen
class (a recipe key that ignores sampled bindings serves stale art;
one that includes them is a per-paint subtree walk). No number
motivates that trade today. The boundary is now pinned from BOTH
sides: `AStampBakeSurvivesABrushRebuiltEveryDescribe` (the hit — 1
bake in 5 frames) and NEW
`ComposeBrushes.AFreshArtNodePerDescribeRebakesByContract` (the miss —
4 fresh nodes, 4 bakes, and a content change reaching pixels with no
stale red; a future content-identity cache must revisit that pin
deliberately, which is the point of it).

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

**CLOSED 2026-07-27 — SHIPPED as the entry prescribed.** The predicate
is exact BY CONSTRUCTION: `deferBlendToBlit` is the texture branch's
own entry condition (the memo probes were hoisted above the layer
decision so `cacheHolds` is known there), and every exit of that branch
ends in a single image draw — the device blit, or the quantized-local
blit it falls back to — each now carrying the node's opacity/blend on
its paint when deferred. A node that fails the entry keeps its layer,
so the silent-blend-loss failure the old exclusion guarded against is
unreachable. Pinned by
`ComposeCaching.ATextureBlendCompositesOnTheBlitNotALayer` (deferred
blit vs a hand-built layer composite within §30's 8-bit residual, plus
a live kPlus accumulation check). **Plate ledger (via plate_ledger.py):
52/58 byte-identical; the three findings are the sanctioned residual,
measured** — fallout2_charsheet 0.78% of px, thaumonomicon 5.89%,
thunder_fulu 0.48%, all at **maxDelta ≤ 2**, the one-less-requantisation
direction the entry called "slightly more accurate"; the other three
movers are the documented flappers. Baseline rebased. 450/450.

## 19. Materials and effects have no spatially-varying parameter channel — **EFFECT HALF CLOSED 2026-07-30, MEASURED; the world-space material half stays open**

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

**DESIGNED 2026-07-30 (effect half) — design only, deliberately not
built.** Four questions were put to the entry; the answers, with what
was checked:

1. **The carrier is a `Material`.** It is already compose's comparable,
   animatable, position-varying value: it rides the prune signature
   (a raw callable never compares equal — the `Shape` escape-hatch
   lesson, restated at §3), it animates through the one uniform
   channel, its unit-space ramps (`linearUnit` et al.) solve
   "authored against a box the layout decides" for free, and §10f
   already defines its tier inheritance (a live child makes the parent
   live). §41 built luma coverage FROM one, which is precedent for
   "a Material as a per-pixel PARAMETER rather than paint". A raw
   callable never prunes; a new value type re-invents all of the above.
2. **Where it plugs in — CHECKED, and §10f does NOT already reach the
   effect seam.** `Material::child` fills `uniform shader` slots of an
   sksl() *material*; `Effect::shader` fills exactly one child,
   `"content"` (`Compose.cpp`: `RuntimeShader(builder, "content",
   nullptr)`) — a second declared `uniform shader param` is left
   unbound. The general case IS reachable today, statically, with no
   new machinery: build the filter yourself —
   `SkRuntimeShaderBuilder b(fx); b.child("param") = shader;
   Effect::filter(SkImageFilters::RuntimeShader(b, "content", …))` —
   but that spelling loses everything the carrier ruling wanted:
   pointer-only equality (never prunes), a raw SkShader param (no
   unit-space, no bound uniforms), and NO volatility declaration when
   the param changes (Q4's failure class). The designed door is
   **`Effect::child(name, Material)`, mirroring `Material::child`
   verbatim** — same warn-and-ignore guardrails, same tier
   inheritance, same prune-signature participation, the material
   resolved against the node's box where `resolvedImageFilter()` runs
   — a SLOT on the existing Effect, not a new Effect kind. For the
   blur cost model specifically, one more named factory on the chassis
   `directionalBlur` just proved out (comparable recipe + bound
   parameters rebuilding a filter DAG per paint): e.g.
   `Effect::blur(Material sigmaMap, float maxSigma)`, the pyramid
   below as its recipe.
3. **Cost model (ESTIMATES, labelled as such).** An author-written
   variable-sigma SkSL kernel pays worst-case sigma at every pixel —
   the entry's own point stands, and separability breaks under
   spatially-varying sigma, so it is O(sigma_max²) per pixel. The
   library pyramid: 2–3 fixed-sigma `SkImageFilters::Blur` levels of
   the layer plus one small mix pass reading the levels and the
   parameter as children — estimated 3–5 full-resolution passes,
   O(1) in sigma range. The intermediates are per-draw image-filter
   DAG surfaces inside the effect's one `saveLayer`
   (Paint.cpp:~1441) — the SAME place every effect intermediate
   already lives, OUTSIDE the §38 promotion/caching machinery, so
   nothing new to invalidate. Static content pays once under
   `Cache::Texture` (the existing effect/bake contract); a LIVE
   parameter re-renders the pyramid per frame and carries §38's
   measured ancestor-volatility tax (19.6× there; extrapolation
   here). Caching the blurred levels across frames while only the
   parameter moves would need a describe-keyed (content-identity) bake
   cache — the mechanism BEYOND §16's closure (its StampCache keys on
   node pointers; see §16's 2026-07-30 reconciliation) — the same
   reopening condition two other entries already name.
4. **The failure mode is stated, never silent.** The §41 frozen-matte
   class is a param that silently degrades to a constant; three rules
   forbid it: an undeclared child name warns-and-ignores (Material's
   guardrail, visible); a live param material makes the effect
   `isAnimated()` by tier inheritance, so the bake invalidates and
   volatility is declared — sampling the param once at bake time is
   forbidden by design; and a param on a `filter()` effect has nothing
   to fill — warn-and-ignore, exactly as `uniform()` on `filter()`
   behaves today.

**VERDICT: real machinery — `Effect::child` plus node-box material
resolution at the effect seam plus the pyramid recipe — so the design
is the deliverable and nothing was built overnight.** What DID ship the
same day is the chassis evidence: `Effect::directionalBlur` (§43.7)
carries a comparable parameter recipe with §11-bound uniforms
rebuilding a filter DAG per paint, which is exactly the shape
`Effect::blur(Material, maxSigma)` needs with a Material child added.
The entry stays OPEN, narrowed to that build.

### CLOSED 2026-07-30 (effect half) — built to the design, and MEASURED

**What unblocked it:** the design named one dependency — "caching the
blurred levels across frames would need §16's bake cache" — and §16 turned
out to have been closed already, the citation was reading a stale header
comment (see §16's 2026-07-30 reconciliation). Nothing else in the design
was waiting on anything, so this is the build.

**The surface is two calls, and the first is a mirror.**

```cpp
Effect &child(std::string name, Material source);   // Material::child, here
static Effect blur(Material sigmaMap, float maxSigma);
```

```cpp
// a depth of field, over a box the layout decides:
.effect(Effect::blur(Material::linearUnit({0,0}, {0,1},
                        {{0, white}, {0.42f, black}, {1, white}}), 14))
// a lens edge:      .effect(Effect::blur(Material::glowUnit(
//                       {0.5f,0.5f}, 1, {{0, black}, {1, white}}), 14))
// a rack focus:     .effect(Effect::blur(map, 0).uniform("maxSigma", &focus))
// the general door:  Effect::shader(fx).child("param", anyMaterial)
```

`Effect::child` mirrors `Material::child` in name, parameter list, return
type, last-write-wins slot semantics, warn-and-ignore guardrails, tier
inheritance and prune participation. Two divergences, both forced and both
invisible at the surface: (1) the storage is
`shared_ptr<const Material>` because `Material.h` includes `Compose.h`, so
`Effect` can only forward-declare `Material` — the surface still takes one
BY VALUE, and a copied Effect replaces the pointer rather than mutating a
shared child; (2) `"content"` is refused by name on an effect (it is the
node's own layer, filled by the library), which has no analogue on a
material because a material has no layer. Everything else is the same code
or the same words: `detail::childShader` (the Material→SkShader conversion)
and `detail::declaresShaderChild` (the validation) are now ONE definition
each with three and two callers — §10f's build loop and blend fold route
through the first, both `child()` doors through the second.

**Tier inheritance composes rather than repeating**: `Effect::isAnimated()`
calls `Material::isAnimated()`, which already recurses through material
children — so a live parameter lifts the effect, the node is declared
volatile at the existing `Paint.cpp` seam, and a bake CANNOT sample the map
once and freeze it. `Effect::blur` rides `directionalBlur`'s chassis: a
comparable recipe (`ParamBlur{maxSigma}`) plus the map in the same child
vector, so equality, the tier walk and the resolve loop are each one loop
over one vector, and `child("sigma", other)` re-aims the map for free.

**THE MEASUREMENT — the design's O(1) claim was an estimate; here are the
numbers.** `compose_bench`, Release, quiet machine, `VaryingBlur` arms.
Three arms on the same node, same map: the library pyramid; the NAIVE
kernel that produces the same picture by hand (one SkSL pass, loop bound
sized for the worst sigma in the node, `R = 3σ`, non-separable because
sigma varies per pixel); and a constant-sigma `Blur(σ,σ)` as the honest
FLOOR — it does not produce the picture, but it is what an author reaches
for when they give up. Graphite arms submit with `SyncToCpu::kYes` (the
first draft did not, and the most expensive shader looked like the
cheapest because its queue never drained — noted in the bench).

| arm                     | CPU raster 96² | Graphite 256² (median of 3) |
|-------------------------|---------------:|----------------------------:|
| pyramid, σmax 6         |        0.80 ms |                     0.66 ms |
| naive kernel, σmax 6    |         167 ms |                     1.95 ms |
| pyramid, σmax 24        |        1.04 ms |                     0.84 ms |
| naive kernel, σmax 24   |        2521 ms |                     17.1 ms |
| constant σ 24, no vary  |        0.45 ms |                     0.60 ms |

- **The pyramid wins by 208× (CPU) / 3.0× (GPU) at σmax 6, and by 2424×
  (CPU) / 20× (GPU) at σmax 24.**
- **The SCALING claim survives, restated honestly.** Quadrupling sigma
  costs the naive kernel 15.1× (CPU) / 8.8× (GPU) — the CPU figure is the
  tap ratio 21025/1369 = 15.4× almost exactly, so that arm is doing what
  the entry said it does. It costs the pyramid 1.3× (CPU) / 1.26× (GPU).
  That is not literally O(1): two of the three levels ARE sigma-dependent
  Skia blurs, and Skia's own Gaussian is sublinear in sigma (it downsamples
  for wide radii), not free. "O(1) in the sigma range" was too strong; the
  measured statement is *the pyramid costs what two Skia blurs and a mix
  cost, which grows ~1.3× per 4× of sigma, against ~σ² for the kernel.*
- **The overhead against giving up** is 2.3× (CPU) / 1.4× (GPU) at σmax 24.
  The GPU numbers include a ~0.6 ms submit-and-wait floor common to every
  arm; net of it the pyramid's own GPU work at σmax 24 is ~0.24 ms against
  the kernel's ~16.5 ms.

**PINS, each with its positive control run and the file restored (mtime
stamped).** Debug, the named test fails exactly as stated:

1. **FIELD PIN** — `Effect` gained two members (`m_paramBlur`,
   `m_children`); the in-class `fieldPin` decomposition made that a BUILD
   FAILURE until ruled on in `operator==`. Control: a 10th member added →
   `error: type 'Effect' decomposes into 10 elements, but only 9 names were
   provided`.
2. **Tier inheritance** — drop the child loop from `Effect::isAnimated()` →
   `ALiveSigmaMapMakesTheWholeEffectLive` fails (the parameter freezes) and
   `AnUndeclaredEffectChildIsIgnoredNotBound`'s controls fail with it.
3. **The prune signature** — drop `childrenEqual` from the blur branch of
   `operator==` → `AStaticParamBlurPrunesByRecipeAndByItsMap` fails on
   "the sigma map must ride the prune signature".
4. **The node-box context** — resolve the effect with a null PaintContext
   instead of the node's → all three pixel tests fail, because a
   unit-space map with no box is not the map that was authored. This is
   also the ALIGNMENT pin: the fixture node sits at (40, 40), so a map
   reading layer or canvas coordinates would shift its falloff by a third
   of the box and the assertions would not hold.
5. **The guardrail** — remove `declaresShaderChild`'s check from
   `Effect::child` → `AnUndeclaredEffectChildIsIgnoredNotBound` fails (an
   ignored child that silently declares volatility is the failure).
6. **The store-time snapshot** — a BUG FOUND BY REVIEW, not by a test, and
   then pinned. `child()` first refreshed `m_filter` through
   `resolvedImageFilter(nullptr)`, which early-outs by returning the very
   snapshot it was meant to replace — so a STATIC child on a `shader()`
   effect (one the paint path has no reason to re-resolve) never reached
   the filter at all. Every test in the entry used a unit ramp, which is
   geometry-tier and therefore rebuilt per paint, so all of them passed
   over it. The fix is one construction — `buildFilter(ctx)`, called
   unconditionally by `child()`/`blur()` and behind the early-out by
   `resolvedImageFilter()`, exactly `Material::build(live, ctx)`'s shape —
   and the pin is a solid-material child arm in
   `AnEffectChildFillsASecondDeclaredShaderSlot`; the control restores the
   old line and it fails.

**THE PIXEL PIN** is `AParameterMapVariesTheBlurAcrossTheNode`: 8px stripes
in a 120² node, stripe contrast measured at three points across the
falloff — 190 sharp / 122 mid / 9 soft (monotonic), a picture no constant
sigma can produce. Both controls run: a constant blur at the same sigma
washes the sharp end too (contrast 12), and no blur at all leaves the soft
end sharp (255). Six tests total, all in `ComposeEffects`.

**Tests: `compose_test` 518 (517 passed, 1 skipped — the expected §34
variation-drive skip), +6 from this entry; `compose_kit_test` 47,
`motion_test` 21, `shape_test` 83, `world_test` 65 — all in BOTH configs —
and `ctest` 17/17 in both (442 s Debug, 34 s Release). Both configs build
clean; the one warning in the tree (`chladni_tab1.cpp:530`, an ignored
nodiscard) predates this and is untouched by it.

**PLATE LEDGER** (Release, `scripts/plate_ledger.py`, 65 scenes):
**55 byte-identical, 0 unexplained movers — byte-neutral.** The three
movers are all documented: `easel_playground` 187686f0e651 →
**39528e682c55**, the SigilShape-attributed value this session already
recorded, and `hitman_verlet` + `slitscan_2001`, auto-attributed flappers
(`genesis_fire` happened to agree this run). Seven scenes report "not in
baseline": the six unadopted API sketches plus `blur_falloff`, which is
new here. **No rebase taken.** Render temps deleted immediately after
hashing (~170 MB).

**The sketch**: `sketch/sketches/blur_falloff.cpp` (gallery scene
`blur falloff`, `Kit · API`) — four panels, same content and same maxSigma,
only the map differing: constant (what it replaced), depth of field, a lens
edge, and a rack focus with `maxSigma` bound. It reports "not in baseline"
in the ledger, as every new scene does.

**What is NOT closed.** The first bullet — a Material with no world-space
option, so a field continuous ACROSS separately-laid-out nodes still has to
become one canvas-sized node — is untouched by this and stays open under
this entry. And the cross-frame level cache (blur the levels once, move
only the parameter) is still the describe-keyed content-identity bake two
other entries name as their reopening condition; the pyramid is cheap
enough per frame that nothing here forced it.

## 20. A settled bound property never releases its volatility flag — **SHIPPED as the measured-stability RELEASE, CLOSED 2026-07-27**

*(Header restored 2026-08-03: this entry's body was always here — nine
other entries cite §20 by number — but the `## 20.` heading itself had
gone missing between §19 and §21. Nothing below is moved or reworded;
the title and the CLOSED date are taken from the body's own SHIPPED
record.)*

**SHIPPED as the measured-stability RELEASE** — the second candidate
shape below, exactly as the entry preferred: no new API, no author
knowledge. The §17/§3.6 memos already kept a settled node's own
recording; what never released was the volatility FLAG, so the node
replayed live every frame and ancestors could not cache across it
(measured by the probe: 0 re-records, 5/5 live paints). The release:
the paint side counts consecutive stable paints
(`Instance::kScalarSettleFrames = 8`, promotion's own bar) and crossing
it requests ONE volatility recompute; the walk honours the warmed-up
release and registers the instance; and a per-draw MOVEMENT SCAN
(`scanReleasedScalars`) re-checks released nodes so an
externally-driven Output that moves re-declares volatility — staling
every ancestor recording — in the same frame, before anything paints.
Nothing stale can replay. The one-time cost is the settling frame's
re-record (ancestors caching for the first time), now documented in the
two held-keyframe pins that measured it. Covers gate scalars and glyph
progress (the memoized scalars); bound TRANSFORM/OPACITY slots remain
paint-only volatility as designed — they never blocked the content
cache. Tests: `ComposeR4Mask.ASettledBoundGateRecachesWithoutAnyNewApi`
(the acceptance test written red before the mechanism, now green,
including the stale-replay control: the frame the binding moves again,
ink changes) and the two §17 pins re-aimed at post-release steady
state. 449/449. **Plate ledger: byte-neutral — 53/56 identical vs the
instancing arm, movers = exactly the three documented flappers,
auto-attributed.** That verdict was also the maiden run of
`scripts/plate_ledger.py` + `ComposeGallery --ledger` (benchmark-free
exact-stepped captures, parallel, manifest-compared, flappers
attributed, `--stability N` self-attribution for new movers): the
whole byte-identity ritual is ONE command and ~8 minutes now, against
the ~45 serial minutes every prior arm cost — which retires the
methodology debt the §33 R4 note filed ("any future ledger must bake
its own baseline") by making the baking cheap.

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

**PROBED 2026-07-27, and the entry narrows by half.** The filed case (a
settled bound GATE) was measured with the R4 machinery in place:
`settledRecords == 0` — the gate/scalar memo already keeps the
recording — but `settledPaints == 5 of 5` — the volatility FLAG never
releases, so the node replays live every frame and its ancestors cannot
cache across it. So the remaining §20 work is exactly the second
candidate shape above: a measured-stability RELEASE of the volatile
flag (with the drop-on-tick discipline Cache::Group proved, inverted —
the frame the value moves must re-declare before anything stale
replays). The acceptance test exists and is deliberately red:
`ComposeR4Mask.DISABLED_ASettledBoundGateRecachesWithoutAnyNewApi` —
enable it when the release lands.

## 21. `console::Style::visibleLines` gives no height — **CLOSED 2026-07-29**

Filed by `dunhuang_star_chart`: fitting three `LineRing`s in one panel
meant hand-tuning panel height against font size × line count.
`compose::measure()` answering for a console element would close it.
`Console.h` is the extraction layer's file; coordinate before touching.

**CLOSED 2026-07-29 — `console::height(style, lines, fonts)` and
`console::height(style, fonts)`, entirely inside `Console.h`, no kernel
change.** The panel the study hand-tuned is now arithmetic over an
answer:

```cpp
const float rows = console::height(logStyle(), fonts);
const float h = 2 * padY + 3 * rows + 4 * gap + 2 * dividerWidth;
```

Three things worth keeping:

- **It MEASURES, it does not compute.** A probe ring of `lines` rows runs
  through the real `console()` and the real `compose::measure()`. The
  obvious arithmetic — `metrics(style.text, fonts).lineHeight * lines +
  gap * (lines − 1)` — is wrong by most of a row: at the study's 9.2 px
  mono × 12 it answers **121.4 where the laid-out console is 131**,
  because each row is `ceil()`ed onto Yoga's pixel grid before the gaps
  are added. That number is a measurement, taken as this function's own
  positive control (control 2 below).
- **It clamps to the window**, because the probe goes through the
  component: `height(style, 400, fonts)` on a 12-line window is the
  12-line height. Virtualization is the whole point of `visibleLines`,
  and its height is bounded by it.
- **The children-not-root rule was demonstrated, not assumed.**
  `snapshot()`/`measure()` size by the root's CHILDREN, and the pin
  asserts that the shelled spelling `measure(box().child(console(…)))`
  and the bare `measure(console(…))` return the SAME number — true
  because `console()` returns a panel that sets neither width nor
  height. The shell stays in the implementation so it remains true if
  that ever changes.

Pinned by `ComposeConsole.VisibleLinesHasAHeightAndThreeRingsFitOnePanel`,
which is the study's shape: three rings, two hairline dividers, one
column panel sized from the answer with no room to spare. A wrong height
there is SILENT — flex `shrink` (default 1) absorbs the deficit and every
ring quietly loses rows, which is exactly how the study lost its
iteration — so the test asserts each ring's laid-out height EQUALS the
answer. Three positive controls, each failing the named test: (1) drop
`Style::gap` from the measurement → the shelled/bare equality fires
(131 vs 120); (2) the metrics arithmetic instead of the measurement →
the ring-height assertion fires (121.4 vs a laid-out 121, against a true
131); (3) drop the window clamp → the clamp assertion fires (4399 vs
131).

Not built, deliberately: `panelHeight(Panel, fonts)` for `console::panel`
— one line of arithmetic at the call site, and the corpus's plates are
hand-built rather than `panel()`-built. The `env::`-reading overload
(`height(fonts)` off `env::inheritedOr(Style{})`) is also unbuilt; ask
for it when a component needs its own inherited height.

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

### 2026-07-29 — the guard is GENERATED now, and it found four more

The entry's own lesson was about the guard, so the guard is what changed.
`test/docs/api_doc_probes.py` reads `API.md`, `DESIGN.md` and
`STRESS_TESTS.md`, extracts every documented name, and emits
`ComposeApiDocProbes.cpp` — a TU in `compose_test` that only builds if
the headers still spell those names that way. CMake regenerates it
whenever a doc or a public header changes, so a section written tomorrow
is covered tomorrow. **Nothing is registered per section**, which is the
one property the hand-transcribed guards did not have.

**Coverage, before and after.** Across `API.md`'s 42 `cpp` blocks there
are 217 qualified-name instances. The four `ComposeDocs.EverySignature…`
tests together name **60 of them, touching 18 of the 37 blocks that carry
names at all** — and nothing outside the blocks. The generated TU checks
**207 of 217, in all 37**, plus every name in the prose's inline spans,
plus both other documents: **167 using-probes + 52 member-probes + 25
designated-initialiser probes + 34 header-index checks = 278 checked
names, 23 excluded with a written reason, 0 unresolved**.

**Why generated names and not compiled blocks.** The obvious mechanical
route — compile each ```cpp block — was measured and rejected: **5 of 42
blocks compile verbatim**, and the 5 are the WORST case, not the best.
`struct PaintContext {…}` and `class Composer {…}` "compile" inside a
function body by declaring a LOCAL type that touches no header at all, so
block compilation would report success on precisely the header
recitations it is supposed to check. The other 37 are pedagogical by
design: dangling `.stroke(…)` fragments with no receiver, menus with no
semicolons, `{...}` and `…` elisions. Making them compile means rewriting
the document into a literate program, and any transformation clever
enough to fix them (inserting receivers, semicolons, a placeholder
vocabulary) is magic that hides errors. So the mechanical thing that IS
extractable is the NAMES, and they are extracted exhaustively. The four
hand-written tests stay: they check argument ORDER (which is what caught
`brush::restyle`), and no name probe can.

**Three probe forms**, because one does not fit. A namespace-scope entity
is a `using`-declaration — the only spelling that works uniformly for
overload sets, types, variables and enumerators. A class member is a
`requires` disjunction. A designated initialiser gets its own form,
`T{.field = Any{}}`, for two reasons: `PathFormat{.effects = …}` never
spells `PathFormat::effects`, which is exactly how the §25 defect
survived; and it asks a STRICTER question than existence, because
`PathFormat{.paint = …}` names the real member function `paint` and every
existence form answers yes while the initialiser still does not compile.

**The positive control found a hole in the guard's own first draft.**
Reintroducing `PathFormat{.effects = …, .paint = …}` verbatim, the guard
PASSED. Its exclusion table had entries for those two spellings, added on
the theory that API.md names them in prose to warn against them — an
exclusion that disarmed the guard for the one defect it exists to
prevent. (API.md spells that warning as bare `paint`/`effects`, which is
not a probed form; no exclusion was ever needed.) Removed, and the table
now says in writing that nothing may name a defect spelling. With that
fixed the control fails the BUILD, both designators named, and two more
controls — a renamed `shapes::chamfered` and an invented `ClipMode` —
fail the compile and the generator respectively.

**What the widened guard caught**, beyond the ten:

1. **`API.md` documented `Element &trim(...)` and `TrimMode` as live
   API.** R4 deleted both with `wipe()`. The document contradicted
   ITSELF — its own masking fold table says "`trim()` and `wipe()` are
   **deleted**" 2000 lines later. The signature block is replaced by the
   fold: `spans::upTo` / `by::spans` for the reveal, `spans::wrap` for
   `TrimMode::Wrap`, `.offset(o)` for the third argument.
2. **`STRESS_TESTS.md` named a layout scheme that does not exist** —
   `Grid{.columns, .gap}`; the header ships `layouts::ModularGrid` with
   `columns`/`rows`/`gutter`, no `gap`.
3. **`API.md` illustrated designated-initialiser syntax with `Grid{.columns
   = 3, .gap = 12}`** — a made-up example name that collides with the real
   `kit::Grid`, so the doc reads as documenting a type it is not. Now
   `RowData{…}`, the fiction the same bullet already introduces.
4. **A stale header comment, REPORTED not fixed** (headers win, and this
   is a behaviour question): `Decorations.h:578` still calls `gappedRule`
   "one of the two legacies R3 did not delete (with `Element::trim`…)".
   R4 deleted `Element::trim`, so the sentence counts a legacy that is
   gone. Same for `Compose.h:2457`, which lists `trim()` among the things
   that dress a rail.

**23 exclusions, each with a reason**, are the cost of probing prose as
well as blocks: `API.md` deliberately names deleted spellings
(`Rail::offset`, `Brush::op`, `Ribbon::widthFn`, the R3 rename tables),
worked-example fictions (`Palette`, `RowData`), and one name a study
reached for and did not find (`shapes::subtract`). An exclusion is keyed
to the exact spelling so it can never widen to a sibling.

Doc and test changes only — no compose behaviour moved, so no plate
ledger. `compose_test` is 484 (483 + the 1 expected skip); the new case
is `ComposeDocs.EveryNameInTheDocsResolvesAgainstTheHeaders`, which
asserts the extractor still matches something, because a guard whose
extractor silently matches nothing compiles perfectly and proves nothing
— the §25 failure one level up.

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
  is the worst possible failure mode. *(Taken in the buildable half
  2026-08-04: `Material::sksl(nullptr)` — the classic MakeForShader-
  returned-null-and-nobody-looked route — now warns once at BUILD that
  the material is NONE and its node will paint nothing, instead of
  silently drawing nothing. Pin:
  `ComposeMaterial.ANullSkslEffectIsLoudAtBuild`, valid-effect control
  arm included. The valid-effect-then-faults case named above is the
  split-Skia pointer-auth fault (Patterns.h documents the two rules that
  prevent it; `stock_materials` is its ctest) — nothing observable
  exists at build time to warn on there, so that half stays with the
  crash reporter.)*
- **The ABI skew guard has no override and no protocol.** One library
  header touch blocks every sketch until someone rebuilds the host. That
  is the right default; what is missing is a documented "who rebuilds"
  convention for concurrent work.
- ~~**`SketchContext` dangles if captured by reference in a steppable.**~~
  **CLOSED 2026-08-04.** It
  is a per-frame value the host rebuilds. Was documented in
  `sketch/README.md`; making it non-copyable would be better — and now it
  is: copy ctor/assignment deleted (an explicit constructor keeps the
  hosts' braced construction; `makeContext()` returns a prvalue under
  guaranteed elision). A steppable can no longer hold one by value
  either, which would have dangled its spec/size pointers just as
  silently. Layout unchanged, kAbiVersion stays 4; every static sketch
  and the gallery compile clean, so no sketch was copying it.
  Compile-time change — no runtime pin.
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

**Status: ~~open — the alias-first spelling SHIPPED 2026-07-26; the
taste calls below remain~~ one taste call left (corrected 2026-08-03 —
the line had outlived its own rulings). The per-property change
override was RULED `animate(to(v), spec)` by the 2026-07-27 rulings
session (§33, ruling 1); the scheme-declaration word was RULED
`animates()` there (ruling 2) and finished by R3's ruling 13, which
settled it as `isAnimated()`. The one call still live is the
second-word collision filed by the confirming review — authored
`from(a).to(b)` against the driven stages, now spelled
`source(lo,hi)`/`target(lo,hi)` per §33 ruling 3 — recorded at the
end of this entry.** Filed 2026-07-25 from the consolidation pass, not
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
| `Ribbon::widthFn` / `widthMax` | Brushes.h | ~~CONDEMNED~~ **DELETED 2026-07-26 — see the widthFn→Profile note at the end of this section** |
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
call nobody has made. *(Superseded — noted 2026-08-03: R3's ruling 13
made the call. `Material::isLive()` died with `animates()` and
`animated()`; `isAnimated()` is the one word, 33 sites ported — see the
deletion manifest below.)*

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
2. ~~The masking family — `wipe()` is one member (a paint-only
   directional mask, fraction Animatable), shape/alpha masks and the
   kit's alphaMask bake are others; the family was never designed as
   one. Own review pass.~~ **CLOSED 2026-07-27 — SHIPPED as R4.** The
   review pass ran (four candidate shapes against one fixed eight-sample
   set), the designer ratified candidate 1 with amendments, and
   `mask(parts::…, by::…)` landed. `trim()` and `wipe()` are DELETED.
   The note is at the end of this section.
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
profile geometry**. `Ribbon` rode the profile seam in the widthFn→Profile migration
(dated note below) — `widthFn`/`widthMax` are deleted and the trap is
closed on every path.

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
- ~~**`Spans` equality is term-ORDER-sensitive; `resolve()` is not.**~~
  **CLOSED 2026-08-04.**
  `corners(8) | at(0,4)` and `at(0,4) | corners(8)` claim the same runs
  but compared unequal, so a describe that reorders terms produced a
  spurious patch. Never a wrong picture — only a lost prune.
  `Spans::operator==` is a multiset match now (greedy-with-used-flags
  over the SAME term comparison — exact because term equality is an
  equivalence; identical-order describes keep a one-pass fast path).
  Safe because a pruned node keeps ITS OWN term order and the resolved
  values array paired with it. Pin:
  `ComposeSpans.ReorderedTermsPruneBecauseResolveNeverReadsOrder` —
  reorder patches 0 nodes, a genuinely different claim still patches
  (the in-test control), duplicates count; the reverted comparator
  fails the pin (run 2026-08-04). Prune-only: the ledger stayed
  byte-neutral.
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
  `PaintContext`, which is a design change, not an optimisation. The missing
  `compose_bench` weave arm is now `BM_Draw_BrushWeave_Live/{2,4,8}`; keep
  it as the decision gate before choosing a cache or algorithmic fix.
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
passes, unchanged. *(Settled since — noted 2026-08-03: audit item 6
records the lambda family EXECUTED 2026-07-27 — wave/zigzag/rounded/
sketchy deleted, 8 sites ported — and R3's `ops::` JUDGEMENT below
records the deliberate end state of the demotion: `ops::PathOp`,
`ops::chain()` and `ops::debug()` survive behind `brush::restyle` as
the one documented mechanism door. Nothing of stage three remains
open.)* Two entries CLOSED by phase R1 below: `Ribbon`'s
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

### CONDEMNED, NOT DELETED — three at the time; one has since closed

1. ~~**`Element::trim()`** + `TrimMode` + `FxData::trim*`.~~ **CLOSED
   2026-07-27 — DELETED**, together with `Element::wipe()`, by the
   masking family (R4, the note at the end of this section). The
   replacement interface it was waiting for is `mask(by::spans(…))`;
   the two painting-fill sites were the two the designer had to look
   at, and both were ruled on.
2. ~~**`Ribbon::widthFn` / `widthMax`.**~~ **CLOSED 2026-07-26 — DELETED.**
   The designer cleared the re-draw and read the plates; the migration,
   the bridge decision and the per-scene verdicts are the
   *widthFn→Profile* note at the end of this section. It was 8 sites in 4
   sketches, not 7 in 5.
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

---

## widthFn → Profile — SHIPPED 2026-07-26, the last condemned pair is gone

R3 left `Ribbon::widthFn`/`widthMax` alive because the port is a RE-DRAW:
the profile lane calls `bandRegion()` and the callable lane sampled the
contour and zipped two point lists, so no amount of care makes it
byte-identical. The designer cleared exactly that — **the gate was their
eye on before/after plates, not byte identity** — and the pair is now
DELETED, along with the sample-and-zip `widthFn` branch inside
`Ribbon::paint`. `widthStart`/`widthEnd` and the nib are untouched.

### The enumeration, fresh

The R2 note's "19 sites in 5 sketches" and the R3 list's "7 ribbons" were
both wrong. Counted by `grep`: **8 `widthFn` assignments + 8 `widthMax`
assignments = 16 live sites in 4 sketches**, plus 2 deliberate test arms.
The fifth "sketch" was `eva_magi_interior`, whose only mention is a header
line saying its ribbons are widthFn-FREE.

| site | law keyed on | under a reveal? | ported to |
| --- | --- | --- | --- |
| `astral_tome` 652 (bloom), 662 (body) | `fraction` | no | `at::LinkTaper` — fraction |
| `dunhuang_star_chart` 2073 | `distance / authoredLen` | YES (`spans::upTo`) | `BonePress` — **px** |
| `thunder_fulu` 697 | `distance / authoredLen` | YES (`trim`) | `StrokePress` — **px** |
| `thunder_fulu` 704 | absolute `distance`, 75-span table | YES (`trim`) | `FootPress` — **px** |
| `thunder_fulu` 1656, 1762 | `fraction` | no | `LawBand` — fraction |
| `minard_1869` 1247 | absolute `distance` + a LIVE `ch::Output` | YES (`spans::upTo`) | `FlowWidth` — **px** |

### THE BRIDGE: one adapter on the seam, not per site

The blocker's second half was real. `Profile::across` is asked in
FRACTIONS of arc length; four of the eight laws key on
`PathSample::distance` **on purpose**, because a decoration under a reveal
is handed the REVEALED contour and a fraction is a fraction of what has
been drawn so far. `thunder_fulu` documents why: keyed to a fraction, the
頓 press slides down the stroke as it writes.

Three shapes were considered. A per-site "divide by the length I
authored" adapter **cannot work** — under a reveal the length being
sampled is not the length authored, which is the bug itself. Teaching
`Ribbon` to hold a second px-keyed field is `widthFn` again. So the
conversion went on the SEAM, where the measured length actually lives:

```cpp
struct MyLaw {
  static constexpr bool alongIsPx = true;   // optional, one line
  float across(float px) const;             // arc-length px from the start
  float max() const;
};
```

`Profile` reads the flag through a `PxKeyedProfileScheme` concept and
exposes `acrossAt(along, lengthPx)` — the one call a consumer that has
measured its spine makes. `profileOffset` (both the constancy sampler and
the varying walk) and `BandRail` (which gained the whole spine's length)
now go through it; `across(along)` still means "the law at its own key".
**One adapter, because all four px sites want the identical thing** —
absolute arc distance from the spine's start — and because the number
needed to compute it is knowable only at paint. The four fraction-keyed
sites take nothing: `across(along)` is literally their old
`widthFn(s.fraction)`.

The key is part of the scheme's TYPE, so `Profile` equality can never
confuse two laws that differ only in how they are keyed.

### THE PLATE LEDGER — 4 scenes, Release, `--no-promotion`

All four are **self-stable**: re-rendered from the pristine binary they
hash identically, so every difference below is attributable to the port.
(This is the control the R3 ledger had to fight for; here it came free.)
Baseline moment-plates were captured by materialising the HEAD versions of
the seven touched files, building, rendering, and restoring — verified by
SHA-256 on the way back.

| scene | verdict |
| --- | --- |
| `dunhuang_star_chart` | **default plate BYTE-IDENTICAL.** The archer enters at t=26.0 and the default capture is t=6.0, so the port is invisible there — which is itself the evidence that nothing else moved. At its own moment (t=27.5, figure complete) **1648 px / 0.040%, maxDelta 83, 3 px over 64**, all of it edge antialiasing on the fifteen bones. The figure's shape, weight and press are unchanged |
| `minard_1869` | **2053 px / 0.050%, maxDelta 15** on the default plate (the Hannibal band), **5266 px / 0.129%, maxDelta 189** at t=9.5 with the retreat band drawn. The high maxDelta is a black band on a light ground: any sub-pixel edge shift reads as ~190. Shape, risers and city registration are unchanged, and the sharp bends are clean in BOTH — the sketch's documented corner defect is a VARYING-width property and survives the port (`profileOffset` delegates to `lines::offsetAcross` only when the law is constant). Recorded in the sketch's header so nobody re-tests it by accident |
| `thunder_fulu` | **14118 px / 0.382% (default), 24187 px / 0.654% at t=22.0** with the whole talisman written. The ink itself is edge antialiasing only — 起行收 lands where it did, the 頓 press does not move, the foot's 75-span table reads the same. Two of the row bands are the **caption strings**, the documented precedent: the plate PRINTS the API names it uses, so `Ribbon::widthFn` → `Ribbon::width` and "key the law to distance/fullLength" → "the profile is keyed in PX" change the drawing because the drawing is the text |
| `astral_tome` | **THE ONE THAT NEEDS A LOOK: 234357 px / 6.510%, maxDelta 81.** Not a re-draw artefact — a DEFECT REPAIR the port uncovered, described below |

### The astral_tome finding — a NaN was deleting a band

`astral_tome`'s link law is `0.40 + 0.60·sqrt(sin(π·along))`. The literal
`3.14159265f` rounds UP to 3.1415927, so `sin(π·1.0f)` is **-8.74e-08**,
negative, and `sqrt` of it is **NaN**. Every construction samples the law
at exactly `along == 1` (the zip walk's last sample is at `d == len`; the
rail walk's is at `k == steps`), so one non-finite vertex entered the band
path — and Skia draws **none** of a path that contains one. The bloom and
the body were silently absent for the sketch's whole life, leaving only
the rails; the plate's own header text describes a band nobody had seen.

The port did not cause it and does not fix it by itself: it moved the
sample positions, so **two** links started drawing while the rest stayed
dark, which is how it was noticed. Isolated with a controlled probe (the
same links on the surviving zip lane at CONSTANT width: all present), so
the variable is the law, not the lane. Fixed in the law
(`sqrt(std::max(k, 0.0f))`), and the plate now draws every link's bloom.
**6.5% of the plate changes and all of it is the band appearing.**

Filed as a hazard in API.md's traps list, because the failure mode
generalises: a profile that returns a non-finite width does not pinch the
band to nothing, it deletes the whole band, and nothing says why. The seam
is ~~NOT guarded~~ — one line in `profileOffset` would turn this into a local
pinch instead of a silent deletion, and that is a policy call for whoever
owns the next robustness pass, not something to slip into a migration.
**GUARDED 2026-08-04 (the papercut pass took the policy call as written):
the one line in `profileOffset`'s varying walk resolves a non-finite
sample to width 0 — a local pinch to the spine — so the rest of the band
draws. The constant-detection path needs no guard (NaN ≠ NaN already
routes a poisoned law to the varying walk). Pin:
`ComposeWidthProfile.ANonFiniteSamplePinchesInsteadOfDeletingTheBand`;
control run with the guard reverted — the band vanishes outright and the
pin fails. API.md's trap 2b updated to say guarded-but-still-clamp.**

### Tests

Three new cases in `ComposeWidthProfile`, plus two rewritten:

- **`StraightRunsAgreeWithTheLaneTheyReplaced`** — the equivalence claim,
  quantified. The same taper (30→10) spelled both ways over a straight
  spine agrees to **≤1 px of band thickness and ≤0.6 px of centreline** at
  five stations. The surviving `widthStart`/`widthEnd` lane IS the zip
  construction, so the comparison is live rather than historical.
- **`APxKeyedLawStaysPutUnderAReveal`** — a 12 px pulse 40 px along a
  160 px run, rendered at reveal 1.0 and 0.55. The px-keyed law's pulse
  moves ≤2 px; the fraction-keyed twin moves >8 px, which is the trap
  demonstrated rather than asserted.
- **`TheLastNeverPruneRibbonsCanPruneNow`** — the comparability win
  pinned: identical laws compare EQUAL (a `widthFn` ribbon's `operator==`
  ended `&& !widthFn`, so it was unequal to itself and its whole band
  re-recorded per describe), different laws compare unequal, the px key is
  part of the type, `max()` is honoured, and `acrossAt` converts.
- `ARibbonsReachIsDERIVEDFromItsProfile` (was
  `ARibbonWithAWidthFnMustDeclareItsReach`) and
  `ProfileIsComparableAndBoundsItsOwnReach` lost their legacy arms.

### The gate

Debug and Release both build clean; `ctest` green across all 14 suites;
`sizeof(ElementNode)` guard unchanged. `Profile` grew one `bool` into
existing padding — it is held in `Across`/`Box<>`, never inline in
`ElementNode`.

---

## §33 R4 — THE MASKING FAMILY (2026-07-27)

Pinned pass 2 closes here. `trim()` and `wipe()` are **deleted**; one verb
replaced both, and it is the first appearance-gating surface in the library
that was designed as a family rather than assembled from whatever each study
needed next.

### The finding the design rested on

Seven mechanisms existed that made some of an element's paint not appear —
`wipe`, `trim`, `stroke(Spans,…)`, `clip`, `PathFormat::trimStart/End`, the
kit's A8 bake, and hand-rolled `kSrcIn`. **No two gated the same set and no
two were the same value kind.** Read as a table it is not seven features, it
is a scattering of PRE-MULTIPLIED CONSTANTS from a product of two
vocabularies:

- **SELECTION** — which of this element's paint outputs does the gate apply
  to?
- **GATE** — by what rule is that output cut?

Every combination is a picture someone wants; the API offered four of the
~24 cells, chosen by history. `clip()` and `trim()` were exact complements
on the decorations/children axis and nobody chose that. Three of the four
things an author would call a mask did not exist at all —
`chaucer_astrolabe.cpp:976` reaches for `clipOut()` and `shapes::subtract`
BY NAME, finds neither, and drops below the Compose seam to a raw
`SkPathOp`; `Console.h:19` prescribes `Material` + `kDstIn` as an idiom in a
SHIPPED HEADER because it is not a feature; two more studies hand-roll
`kSrcIn`. And the family already failed DESIGN's own rename test: two of the
seven were documented BY COMPARISON TO EACH OTHER.

### The surface

```cpp
Element &mask(Gate with);              // taught default == parts::all()
Element &mask(Parts what, Gate with);  // the granular form

namespace parts { Parts all(), marks(), surface(), content(), children();
                  Parts named(std::string_view); }
Parts operator|(Parts, Parts);

namespace by { Gate spans(Spans), edge(float, Animatable<float>),
                    shape(Region), outside(Region), alpha(Material); }

class Region { static Region own(), rect(SkRect), oval(SkRect), path(SkPath); };
```

plus an optional local `name` on the four unqualified mark slots
(`foreground`/`overlay`/`background`/`stroke(Decoration)`), which is what
`parts::named()` addresses. They are the SAME local names
`stroke(Spans, what, name)` already carried — one element's own labels for
its own marks, never a query key, no second identity system.

### The rulings, and why each went the way it did

**`by::`, not `gate::`** — the designer's amendment, and it earns itself at
the call site: *mask by edge*, *mask by spans*, *mask by shape* is English,
and `gate` was rejected as lock-implying. The one-argument `mask(by::…)` is
the TAUGHT DEFAULT; the two-argument form exists so the family is closed
rather than merely tidy, and 24 of the 25 corpus substitutions use the
short one.

**A gate is a SHOW set; the complement is a term, never a mode flag.**
`by::outside(r)` is the word for the outside of a region, with
`spans::rest()` as the precedent. Same argument that made `wrap` a term
rather than a flag on `range`: a reader auditing a picture needs the call
site to say which way round it is.

**Stacked masks INTERSECT where their selections overlap.** Both must pass.
Nesting already meant that everywhere else, and union is spelled inside one
gate value (`Spans::operator|`), never across masks. **Each mask carries its
OWN Animatable slots** — separately indexed, so three masks at three rates
on one node is a picture rather than a race, and a retarget on the second
retargets the second. That was a design requirement the designer asked for
by name, and it is pinned at pixels
(`ComposeR4Mask.S8PlusThreeMasksAtThreeRatesIntersectPerFrame`).

**The claim ledger reads the UNMASKED boundary.** Span-pass claims resolve
against the uncut outline and the gate intersects afterwards, so the
no-overlap diagnostic is a statement about the description and never blinks
in and out between 0.3 and 0.7 of a transition.

**`stroke(Spans, d, name)` is STATED AS LAW to be sugar** for
`stroke(d, name).mask(parts::named(name), by::spans(where))` — pixel-exact,
pinned on a multi-run claim. The one thing the pass form does that the sugar
does not is CLAIM its run and join the ledger; that is written down at both
call sites. `clip()` likewise stays as sugar for the shape gate, on cost
grounds (a `clipRRect` is much cheaper than a general path).

**The shape gate takes a COMPARABLE `Region` from day one.** The obvious
signature takes `shapes::OutlineFn` — an incomparable `std::function`, which
never participates in reconciler equality and therefore never prunes. That
is §3, the highest measured-impact item on this roadmap. A `Region` is a
closed value (`SkPath` has structural equality, so `Region::path()` is a
general escape hatch that still prunes), which is why the shape member could
ship WITH the family instead of queued behind comparable outlines.

### The cache repair — the part that is not a rename

`Instance::ContentScalars` was a **fixed five-float struct**
(`trimStart/trimEnd/trimOffset/wipe/glyph`), and that fixed size was the
whole obstacle: it is why `spanVolatile` is excluded from the §17 scalar
memo by a written decision in `Paint.cpp`, and therefore why **every one of
R2's 58 `trim()` → `stroke(spans::…)` ports moved its node from the scalar
memo to per-frame content volatility and out of `Cache::Group` eligibility.
The plate ledger was byte-identical, so nothing caught it — byte-identity is
a pixel gate, not a cost gate.**

A mask's gate scalars are a bounded, per-node, resolvable-to-floats list, so
`ContentScalars` now holds `{float glyph; std::vector<float> gates;}` and an
element-level gate KEEPS the memo. A held keyframe on a masked node repaints
nothing, verified by the same §17 probe that shipped with the memo
(`ComposeR4Mask.AGatedNodeKeepsTheScalarMemoAndPrunes`). This is the
structural reason selection lives in an ARGUMENT and not in the slot call or
in the mark value: per-pass gate scalars land in the open `spanAnims` vector
the memo cannot see. (Per-PASS span endpoints are still excluded — hoisting
those is separate work, and this note is where it is filed.)

An `alpha` gate on a LIVE material declares volatility and refuses both
memos, exactly as a live material fill does.

### Storage — ElementNode did not grow

The masks live in `FxData`, the block whose two departed tenants they
replace: `hasTrim/trimStart/trimEnd/trimOffset/trimMode` and
`hasWipe/wipeAngleDeg/wipeFraction` are gone and one `std::vector<Mask>`
took their place, so the rare-fields rule is honoured with a NET SHRINK of
the block and no new `Box<>` pointer. `Instance::Slot` lost four entries
(`kTrimStart`, `kTrimEnd`, `kTrimOffset`, `kWipe`) and gained none — the
per-mask motions live in `maskAnims`, sized by the description like
`spanAnims`. The `sizeof(ElementNode)` guard is unchanged and still
asserting at 768 B.

### STANDING NOTE — the namespace reservation

The family lands **two** new concept namespaces (`parts::`, `by::`) into a
codebase whose namespace-friction log in this section already stood at four
sightings, two of them hard errors, and which says in as many words that it
"wants a ruling before any further nouns land."

The ruling handed down with the ratification was a NAMING ruling, recorded
verbatim: *"the gate namespace is `by::` not `gate::` ('mask by edge'
English; gate rejected as lock-implying)"*. The designer's
namespace-REORGANIZATION reservation is recorded as standing and open: the
LAYOUT of the concept namespaces (`parts::`/`by::` at `sigil::compose`
scope, versus nesting them under a `mask::` or moving to free factories) is
not settled by this pass and may be reorganised later. Both are closed
vocabularies of factory functions with no call-site state, so a
reorganisation is a mechanical spelling change across 25 corpus sites and
one header — it must not be re-litigated as a design question when it comes
up. `Region`'s factories deliberately avoided the problem entirely by being
STATIC MEMBERS (`Region::own()`, `Region::path()`), the house pattern
`Fill::color` and `Material::radial` already use; that is the shape to reach
for if the ruling goes against new nouns.

### The corpus port

**17 `trim()` sites and 8 `wipe()` sites, plus the tests.** Two of the trim
sites reveal a PAINTING FILL and were the two the designer had to look at:

- **`chaucer_astrolabe.cpp:971` — the meridian, ported to `by::edge(90°)`,
  and it is the ONE port in the corpus that moves pixels on purpose.** The
  bar is a filled 2 × 2R rect with no stroke at all. An arc-length window
  walks its PERIMETER, so the first half of the 520 ms ramp crawled up a
  2 px-wide left edge enclosing no area — a ~260 ms dead beat and then a
  snap, plus an invisible hairline taper. A meridian draws downward. This is
  the measured bug the proposal predicted, repaired by the gate kind that
  exists for it.
- **`sigillum_aemeth.cpp:1226` — the pentagram, ported to
  `parts::marks()`** per the amendment: the rails draw themselves, the
  18%-alpha wash is simply there. The proposal measured the wash sweep at
  ≤ 4 LSB over its own ground before the port.

Every other site is a one-line substitution. `astral_tome`'s three-stroke
`linkPass()` helper is the sample that decided the shape: the caller gates
all three of a helper's marks with a property, and never has to change
someone else's signature to do it.

### Tests

**20 new cases in `ComposeR4Mask`**, and the phase's ported arms.

The eight design samples S1–S8 are tests, because a family designed against
eight pictures should draw all eight and the shapes that were rejected were
rejected for failing one: S1 a helper's three marks gated from outside it,
S2 a decoration receiving the already-gated run (the wet nib rides the
head), S3 the retarget across an if/else, S4 a gate applied to an
already-built element (the still-frame conditional), S5 a claim under a gate
resolving to the intersection — with the claim-ledger law checked in the
same test, S6 the edge gate reaching a lattice of children, S7 the seal
(region, its complement, and the two-mask SET DIFFERENCE the raw `SkPathOp`
was written for) plus S7b the alpha gate, S8 one mark gated with its sibling
untouched, and **S8+ the composition the designer asked about**: three masks
at three rates, intersection pinned at pixels including the disjoint case
that no single-gate implementation can produce.

Then: the intersection law as pixels, the sugar law pixel-exact on a
multi-run claim, the silent-no-op law for an unmatched name, the memo repair
(the §17 held-keyframe probe) and its prune half, `Region`/`Gate`/`Parts`
comparability, the fold behaviour (a spans gate reaches surface and marks
and NOT the children), the edge gate as wipe's half-plane, and —

**`TheGateGeometryIsTrimsGeometry`, the trim-parity witness that survives
the deletion.** The expected geometry is built in the test by
`SkTrimPathEffect` itself, exactly as `trim()` built it, and drawn through a
`custom()` leaf the masking family never touches. Three windows, boundary
ring compared. It is the one parity test that does not depend on `trim()`
still existing, and it is what the 17-site port's byte-identity rests on.

The historical parity suites (`ComposeR1TrimParity`, `ComposeR1Wrap`,
`ComposeR2Offset`, `ComposeR2Background`) kept their names and their pixel
expectations — those expectations were pinned against `trim()` while it
existed — and their "legacy" arm is now the node-level gate that inherited
its geometry. They therefore keep earning their keep as the sugar law's
proof: the pass door and the node door describe one run.

### THE GATE

**Debug and Release both build clean**, warning-free, on a from-scratch pass
with no edits in flight.

**`ctest` green across all 14 suites in BOTH configurations** — Debug
492.43 s, Release 33.80 s, 14/14 each. **423 cases in `compose_test`** (403
before, +20 in `ComposeR4Mask`) across 81 suites; **47 in
`compose_kit_test`**, unchanged. No test arm was deleted without a
replacement: the phase RENAMED the suite that named a dead verb
(`ComposeTrim` → `ComposeMask`, `ComposeFx.Wipe*` → `ComposeFx.EdgeGate*`)
and re-pointed the legacy arm of every historical parity row at the gate
that inherited trim()'s geometry, so the rows still compare two independent
paths through the library.

`sizeof(ElementNode)` is **744 B**, unchanged by this phase and still
asserting under the 768 B guard — the masks replaced two field groups
inside `FxData`, which is a `Box<>`, so the base struct never saw them.

**THE PLATE LEDGER — 56 scenes, Release, `ComposeGallery --headless`,
run against the combined commit.** The prediction below was pre-registered
BEFORE the sweep, and is left standing with the outcome beside it. Baseline:
R3's three post-port sweep hashes (`/tmp/r3_new{,2,3}.sha256`), which ARE
35cf6b7; the four widthFn scenes are read against the designer-approved
after-plates instead.

What it is expected to show, written down BEFORE it runs so the prediction
is falsifiable:

| set | expectation |
| --- | --- |
| 49 of 56 | **byte-identical.** The port is a spelling change: a `spans` gate cuts the outline through the same `spanPath`/full-coverage short-circuit trim used, and a whole-node `edge` gate is wipe's half-plane under wipe's own save/clip in wipe's own position |
| `chladni_tab1`, `genesis_fire`, `hitman_verlet`, `ksp_mapview`, `slitscan_2001`, `black_watch` | **self-nondeterministic, the six already on record.** `black_watch` keeps its role as the control: its `.cpp` is untouched by this phase — including a now-stale comment ("there is no `.wipe()`") left in place ON PURPOSE so the control stays byte-identical to HEAD |
| `chaucer_astrolabe` | **MOVES, predicted and bounded.** The meridian's `by::edge(90°)` repair. The bar reveals downward instead of crawling its own perimeter; the change is confined to one 2 × 2R node and is visible only while `tAzim + 820 ms` is ramping |
| `sigillum_aemeth` | **may move by ≤ 4 LSB.** The pentagram's wash is no longer swept (`parts::marks()`), and the proposal measured that sweep at ≤ 4 LSB over its own ground. If the default capture is past `tInner + 1300 ms` the gate is settled and the plate is byte-identical; if it is mid-ramp, expect a bounded low-amplitude delta on the star only |

Any scene outside those four rows moving is a defect, not a re-draw, and
should be treated as one.

### The outcome — prediction vs actual

TWO full sweeps plus a 29-scene third on a QUIET machine (the load
conditions matter — see the methodology finding below), read against R3's
three-sweep baseline set. The hashes are kept at `/tmp/r4_q{1,2,3}.sha256`
and ARE the dd94846 baseline for the next phase, exactly as R3's were for
this one.

**51 of 56 reproduce a reference hash. ZERO unpredicted movers.** Every
scene outside the predicted set is byte-identical, including all 23 other
port sites. And the tree is deterministic where the corpus is:
**53 of 56 scenes are byte-identical to THEMSELVES across the two sweeps** —
the three that are not are the three that never repeat in the R3 baseline
either. The partial third sweep agrees with the first on all 29 scenes it
reached, 29 for 29.

| scene | predicted | ACTUAL |
| --- | --- | --- |
| the 49 untouched | byte-identical | **byte-identical** ✓ |
| `chaucer_astrolabe` | **MOVES** (the `by::edge` repair) | **BYTE-IDENTICAL — the prediction was WRONG** (below) |
| `sigillum_aemeth` | may move ≤ 4 LSB | **byte-identical** — the "maybe" resolved to zero |
| `astral_tome` | = approved after-plate | **= approved after-plate, hash-exact** (`9b870f08be38`) ✓ |
| `dunhuang_star_chart` | = baseline (capture predates the archer) | **= baseline = after-plate** ✓ |
| `minard_1869` | = approved widthFn delta | **2053 px / 0.0501% / maxDelta 15** vs the designer's documented *2053 px / 0.050% / maxDelta 15* ✓ |
| `thunder_fulu` | = approved widthFn delta | **14121 px / 0.3818% / maxDelta 158** vs the documented *14118 px / 0.382%* — 3 px apart ✓ |
| the six nondeterministic | flap | **only THREE actually flap on a quiet machine.** `black_watch` (the untouched control), `chladni_tab1` and `ksp_mapview` were STABLE across both sweeps AND reproduced known baseline hashes. `genesis_fire` and `hitman_verlet` produced two distinct hashes in two runs (they produced three in three under R3). `slitscan_2001` produced two — **one of which is the baseline, exactly** |

**`slitscan_2001` deserves its own line, because it is the only scene that
is BOTH a port site and nondeterministic** — the one place a real
regression could have hidden behind noise. It carries two
`wipe()` → `mask(by::edge(...))` ports. Its second sweep reproduces the
35cf6b7 baseline hash `c6975b5f3273` **exactly**, and its own run-to-run
variation between the two sweeps is **47 pixels / 0.0013%, confined to a
single 6 × 9 px box** at (332–337, 1245–1253) — the shutter bar's leading
edge, one sub-pixel step. A port that had changed the picture could not
reproduce the baseline bit-for-bit on one run and differ by a 6 × 9 px box
on the other. Attributed to the scene.

**WHERE THE PREDICTION WAS WRONG, and why it matters.** `chaucer_astrolabe`
does not move, and the reason is not that the repair is absent — it is that
**the plate is taken 20 ms before the animation starts.** The default
capture frame is `kProbeFrames + kMaxWarmFrames + kMaxSampleFrames` = 360 at
60 fps = **t = 6.000 s**; the meridian's ramp is
`ramp(tAzim·1000 + 820, 520)` with `tAzim = 5.20`, so it begins at
**t = 6.020 s**. At capture the reveal fraction is 0, and 0 draws nothing
under EITHER spelling — the old perimeter walk enclosed no area, the new
half-plane clips everything away. The two agree exactly at the one moment
the ledger looks.

This is the same shape as `dunhuang_star_chart`'s row in the widthFn note
("the archer enters at t=26.0 and the default capture is t=6.0, so the port
is invisible there"), and it earns the same conclusion: **the ledger proves
no REGRESSION, and for these two scenes it cannot prove the REPAIR.** The
repair's evidence is the design analysis (a 2 px-wide × 2R-tall filled rect
whose first ~50% of perimeter is a degenerate zero-area sliver) and
`ComposeR4Mask.TheEdgeGateIsWipesHalfPlaneToTheBit`, not this plate. Filed
plainly rather than quietly re-predicted: a pre-registered prediction that
comes out wrong is worth more than one edited afterwards.

**The cheap follow-up, for whoever wants the picture:** render
`--scene chaucer --capture-at 6.30` (mid-ramp) against the same at HEAD~1.
It is the only view in which the two spellings differ, and it costs one
scene render rather than a sweep. Left undone here rather than half-done:
single-scene renders still pay the full benchmark warm-up, so it is ~6
minutes a side, and nothing in the ledger's verdict waits on it.

### A METHODOLOGY FINDING — the ledger's determinism is load-dependent, and the knob already exists

Two things were found while taking this ledger, and they change how the
next one should be run.

**(1) Eight orphaned `yes` load generators had been running since 21:38:52**
— parent PID 1, abandoned from R3's own 8-way under-load protocol, still
burning eight cores hours later. Every plate rendered in that window was
rendered under load. The widthFn comparison sweep in the evidence dir
(`widthfn/sweep_after`, 22:40–22:50) shows **13 of 31 scenes differing from
the 35cf6b7 baseline** — including `botanical`, `cosmati`, `aero desktop`,
`y2k chrome`, `passive tree`, `fallout2_charsheet`, scenes with no widthFn,
no trim and no wipe in them. On a quiet machine every one of those comes
back **byte-identical**. They were the load, not the code.

**(1b) And the quiet numbers say the flaky list is too long.** Of the six
scenes on record as self-nondeterministic, only **three** reproduce that
verdict on a quiet machine: `genesis_fire`, `hitman_verlet` and
`slitscan_2001`. `chladni_tab1`, `ksp_mapview` and `black_watch` were
byte-identical across both sweeps here and each rendered a hash already in
the baseline set. Three of the six may be load artefacts rather than
properties of the scenes.

**(2) The mechanism is auto texture promotion, and it is timing-driven.**
`Paint.cpp`'s promoter bakes a node once `replayMs > kPromoteMs` (1.0 ms)
for `kPromoteFrames` (8) consecutive frames — a MEASURED WALL-CLOCK cost.
Under load those measurements move, different nodes promote, and the plate
changes. Headless runs leave promotion ON by default; `--no-promotion`
exists precisely as the A/B control. That is very likely the whole
explanation for the "self-nondeterministic" set: they are not random, they
are timing-sensitive at a promotion threshold.

Two consequences worth acting on, neither of them this phase's work:

- **A ledger must state its load conditions**, and a sweep taken beside a
  running build is not evidence. The six-scene nondeterministic list should
  be re-derived on a quiet machine before it is trusted as a property of the
  scenes rather than of the afternoon.
- **`thunder_fulu`'s after-plate PNG in the evidence dir is not the plate
  the designer measured.** It differs from the 35cf6b7 baseline by
  **24.89% / maxDelta 158**, where the approved note records *0.382%*. The
  plate rendered here reproduces the note's number to within 3 pixels, so
  the CODE is right and that one PNG is a load-perturbed render. Recorded so
  nobody re-derives a verdict from it.

## 34. THE TEST-AUDIT RULING — executed 2026-07-28

The five-way audit of 2026-07-27 (commit ea70d78) read every case and left
**23 `AUDIT-FLAG` comments** in place rather than touching a test, pending a
ruling. The ruling: **true redundancy and vacuity are deleted; a name that
claims more than its body proves is RENAMED, never deleted.** This is the
manifest, and there are now zero `AUDIT-FLAG` comments in the tree.

**Counts.** `compose_test` **462 → 461** cases (82 suites): −6 deleted, +5
added by §10f. `compose_kit_test` 47, unchanged. The drop is exactly the six
rows below.

### DELETED — six

| test | class | why |
| --- | --- | --- |
| `ComposeMotion.WithFromPlaysEntranceOnMount` | redundant (high) | strict subset of `AnimatePlaysEntranceOnMount`, which adds the mid-ramp pin. The R2 grammar port added the twin and never removed this one |
| `ComposeMotion.WithFromColorSweepsOnMount` | redundant (high) | byte-duplicate of `AnimateColorSweepsOnMount`, same cause |
| `ComposeR4Mask.StackedSpanGatesIntersectRatherThanUnion` | redundant (high) | same fixture and same mask pair as `TheIntersectionIsExactIntervalArithmetic`, which pins the intersection at PIXELS and refutes union with a disjoint pair |
| `ComposeR1Ribbon.ProfileIsComparableAndBoundsItsOwnReach` | redundant (medium) | comparability, non-equality and `bleed()` are all remade with more in `ComposeWidthProfile.TheLastNeverPruneRibbonsCanPruneNow`; reflexivity also in `ComposeBand.ProfilesAreComparableAndReflexive`. Its `LinearTaper` fixture went with it |
| `ComposeBrushes.PatternClosedSeamCornerUsesWrappedBisector` | redundant (medium) | one-pixel probe subsumed by `PatternCornerLandsOnTheVertexAndFacesTheBisector` (all four vertices including the seam, plus orientation) |
| `ComposeMask.WrapSeamIsOneContour` | **vacuous (high)** | rendered a node and then asserted on a path it stitched ITSELF with `SkContourMeasure`, never reading the render: it measured Skia's `getSegment`, and a total regression in `spans::wrap` would have left it green |

Every deletion names its survivor in a comment at the deletion site, so the
next reader meets the reason where the test used to be.

### RENAMED — ten, all kept

| was | is | the overclaim |
| --- | --- | --- |
| `ComposePattern.ARepeatCanBePannedAndItsSamplingChosen` | `ARepeatCanBePanned` | both arms set kNearest; nothing contrasts kLinear |
| `ComposeFx.EdgeGateIsBindableAndPaintOnly` | `EdgeGateIsBindableWithoutARedescribe` | no `bounds()`-unchanged check; paint-only is unasserted |
| `ComposeText.OnPathWrapsTheSeamAndFlipsWithoutMirroring` | `…AndTheFlippedRunKeepsItsHalf` | the first-third/last-third comparison its comment sets up is never made |
| `ComposeCache.AutoPromotionIsPixelIdentical` | `TheAutoPromotionSwitchChangesNoPixels` | nothing establishes that any node promoted |
| `ComposeCache.PromotionIsVisibleInTheProfile` | `NoRowReportsPromotedWhilePromotionIsOff` | promotion is OFF for the whole test |
| `ComposeR4Mask.S3TheGateRetargetsAcrossAnIfElseInsteadOfMounting` | `S3TheGateRampsAcrossAnIfElse` | phase 0 parks at 0.0001, so retarget and fresh mount are numerically identical |
| `ComposeR4Mask.ANamedMaskLeavesTheUnnamedMarksAlone` | `AnUnmatchedMaskNameIsASilentNoOp` | the fixture has one mark and no unnamed sibling |
| `ComposeShapeRename.ShapeOverridesTheBoxAndOutlineIsGone` | `ShapeOverridesTheBox` | the OutlineIsGone half asserted nothing |
| `ComposeBand.ConstructionIsLinearInSpineLength` | `ConstructionStaysUnderTheQuadraticCeiling` | one radius cannot show linearity; it is a wall-clock ceiling against the measured 700 ms regression |
| `ComposeComposites.TheRepairCoversShallowCrossingsAndInnerStrokes` | `TheRepairCoversShallowCrossings` | the Inner half is conceded untested in the test's own closing comment; the dead `align` parameter went with the name |

### STRENGTHENED — six, where the flag's remedy was an assertion, not a knife

Each of these was flagged VACUOUS or LIVENESS with "add the bound" as its
stated remedy; deleting them would have removed real coverage, so the bound
was added and **each addition's positive control was run**.

- `ComposeEffects.TextureBakesEffectOnce` — the word ONCE is now asserted by
  `stats().texturesBaked` (1 on the baking frame, 0 after). It also moved
  under `profiledUnder()`: under a cacheable parent the second frame replays
  the PARENT's picture and never visits the node, so "0 bakes" was true of a
  node that re-bakes every time it is asked. *Control: force the
  `Cache::Texture` bake condition true → fails on the second frame.*
- `ComposeR1TrimParity.ClampWindowOutsideZeroToOnePins` — two agreeing arms
  could not tell a pin from a wrap and two BLANK arms agree perfectly. Now:
  an ink bound, plus named pixels (fraction 0 is the bottom-left corner
  running UP the left edge, so the clamped `[0, 0.6]` leaves the BOTTOM edge
  dark, which is exactly the piece a wrap would add). **An inked-fraction
  bound cannot state this** — `boundaryRing` samples points outside the
  stroke, so "not all of the ring" is true of every window; that first
  attempt passed its own control and was replaced. *Control: swap
  `spans::range` for `spans::wrap` → the bottom-edge assertion fires.*
- `ComposeR1TrimParity.BoundEndpointsScrubTheSameWindow`,
  `AnimatedEndpointsRampTheSameWindow`, `ComposeR1Derive.FlowAroundAsAFreeVerbIsTheMethod`
  — two-arm `EXPECT_EQ` with no liveness guard; `inkedCount` bounds added.
  The flowAround one was drawing **black text on a black ground** and
  comparing two blank grids — the guard caught it on its first run, and the
  fixture now uses `whiteStyle`. *Controls: black-on-black strokes / the old
  default style → all three fail.*
- `ComposeVariationDrive.AdvanceVariantAxisIsRefused` — refused for the
  wrong reason: `axisIsAdvanceInvariant` answers FALSE both for "the axis
  moves advances" and for "there is no such axis", and on a face without
  wght the pixels hold however the drive behaves. It now skips unless the
  face DECLARES wght. On this machine's default face it **skips** — which is
  the honest state, and is filed as §35's third remainder.

### The remainder this ruling creates

The closed-contour seam law (`spans::wrap` must produce ONE contour, or
round caps and additive brushes double-hit at the joint) is now **asserted
nowhere**. It wants a pixel test shaped like
`ComposeMask.OpenContourWrapKeepsTwoPieces`. Deleting the test that only
appeared to cover it is what makes that visible.

**CLOSED 2026-07-28 — `ComposeMask.ClosedContourWrapSeamIsOnePiece`**, the
twin, one test above its open sibling in `ComposeTestBrushes.cpp`. It reads
only rendered pixels, which is the whole difference from the test it
replaces.

*The law HOLDS* — `spanPath`'s `seamStraddled` branch stitches correctly;
this closes a coverage hole, not a bug.

**The fixture parks the seam on a CORNER**, which is what makes one-vs-two
pieces visible in pixels at all. A closed 160×160 rect whose `moveTo` is its
top-left corner, `wrap(0.9, 1.2)` on a perimeter of 640: the window is 64 px
UP the left edge into the seam plus 128 px right along the top edge. Stitched,
the seam vertex carries a **miter join** and the outer corner square
(canvas [17,20]²) is covered; as two runs it carries two butt caps and that
square is empty — the "visible notch" `spanPath`'s own comment names. Probe
`(18, 18)`. Three further probes keep the claim honest: one pixel on each
piece (it really drew both) and one on the unclaimed right edge (the mask
really masked), so a total `spans::wrap` regression cannot pass.

*Positive control: `seamStraddled` forced false in `spanPath` → `(18, 18)`
goes opaque black and ONLY that assertion fires; the other three hold, and
the open sibling still passes (it never stitched).*

## 35. Three things found while executing §34 and §10f — filed, not fixed

1. **`Material::asShader()` can dereference a null `m_live`.** It reads `if
   (isAnimated()) return build(*m_live, nullptr);` — and a `blend()` whose
   LAYER is live has `isAnimated() == true` with `m_live == nullptr`
   (liveness is inherited through `m_recipe->layers`). The corpus does not
   hit it because `blend()` flattens through `resolve()`, and a blend nested
   inside another blend's layer list is the shape that would. One-line
   guard; left out of the child-slot change on purpose, because it is a
   different claim and wants its own pin.

   **CLOSED 2026-07-28 — and the one-line guard was the WRONG fix.**
   *Repro:* two nested `blend()`s, the inner one carrying a `uniform("uK",
   &output)` layer. Constructing the OUTER one segfaults — `blend()` calls
   `asShader()` on every layer, so the crash is at describe time, not paint
   time (`compose_test` exit 139, confirmed before any fix).

   *Why not the guard.* `if (isAnimated() && m_live)` falls through to
   `m_shader`, which for a blend is the snapshot `blend()` flattened at
   CONSTRUCTION — the exact stale answer the live branch at `:715` exists to
   prevent. Safe, and wrong: it converts a crash into a material that reports
   `isAnimated()` and then answers with a frozen value forever. What a
   live-layer blend's `asShader()` should return is the same fold `resolve()`
   already does, minus the context. So the blend fold moved into one private
   `Material::foldBlend(const PaintContext *ctx)` — `ctx` non-null is
   `resolve()`'s per-frame form, null is `asShader()`'s context-free one —
   and both call sites now go through it, which is also why they can no
   longer drift apart. `resolve()` loses its inline copy; past the new blend
   branch, `isAnimated()` implies `m_live`, since the other two liveness
   sources both read it.

   *Pin:* `ComposeMaterial.NestedBlendAsShaderFoldsItsLiveLayersPerCall`
   (`ComposeTestKernel.cpp`, beside `BlendWithLiveLayerTracksOutputs`). It
   draws the shader `asShader()` HANDS BACK into a raster surface and samples
   it twice across a change to the bound Output.

   *Positive controls, both run: (1) unfixed → SIGSEGV at the nested
   `blend()` call; (2) the guard-only fix → the second sample still answers
   0.8·255 and the test FAILS.* That second control is why the pin samples
   twice: one sample cannot tell a fresh fold from a stale snapshot.

   *Blast radius: none.* The only behavior that changed is a path that
   previously crashed, so nothing in the corpus could have depended on it —
   and the plate ledger agrees (byte-neutral).
2. **The `S3` mask fixture cannot discriminate a retarget from a mount.**
   §34 renamed it rather than re-fixturing it. Parking phase 0 ABOVE the
   target (0.8 → 0.5) would make the two answers numerically different and
   restore the stronger claim the old name made.

   **CLOSED 2026-07-29 — re-fixtured, and the original name is back.**
   Phase 0 now parks at **0.8** against a target of **0.5**, which makes
   the two hypotheses point in OPPOSITE DIRECTIONS: a retarget descends
   through ~0.65 at the midpoint, a mount from zero ascends through
   ~0.25. The new assertion is `half > settled`, which only the retarget
   can satisfy, and `ComposeR4Mask.S3TheGateRetargetsAcrossAnIfElse
   InsteadOfMounting` is the name §34 took away.

   *Positive control — the one that matters, and it is why the entry was
   worth closing properly.* Breaking the slot reuse in `Transitions.cpp`
   so the gate MOUNTS FROM ZERO (`transitionFloatAt` handed a zero
   previous value instead of `*prevGates[i]`): the new fixture FAILS,
   `half` **53** against `settled` **106** — and the OLD fixture, run
   side by side under the identical break, **PASSES**. That is §35.2's
   claim demonstrated rather than asserted: the old test could not see
   this failure at all.

   *A second control was run and is recorded as the weaker one:* clearing
   `maskAnims` unconditionally makes the gate JUMP to its target rather
   than mount from zero, and both fixtures fail — the old one by a
   zero-pixel margin (104 vs 104, failing only because `>` is strict),
   the new one by the directional assertion. A tie is not a
   discrimination, which is the other half of why the re-fixture was
   worth doing.
3. **The VariationDrive refusal path has no coverage on this machine.** The
   system UI face declares no wght axis, so `AdvanceVariantAxisIsRefused`
   skips. The refusal is real (`Paint.cpp` gates on
   `axisIsAdvanceInvariant`), but proving it needs a variable face with an
   advance-VARIANT axis in the test assets — the same asset question
   `GradDrivesPaintOnlyWhenAdvanceInvariant` already skips on.

**THE GATE for §35.1 + §34's remainder, 2026-07-28.** Debug builds clean.
`compose_test` **463 cases / 82 suites** (461 before, +2: one per item), 462
passed and 1 skipped — the same `ComposeVariationDrive.AdvanceVariantAxisIsRefused`
skip §35.3 files. `ctest -C Debug` **16/16, 430.24 s**. **PLATE LEDGER**
(Release, 58 scenes): **55 byte-identical, 2 attributed flappers**
(`hitman_verlet`, `slitscan_2001`; `genesis_fire` held still this sweep),
**1 mover: `easel_playground`**, unchanged from the sweep above it — still
SigilShape's uncommitted work, not this change. Re-rendered on its own it
reproduces the SAME new hash (`39528e682c55`), so it is a deterministic
consequence of that library's edits rather than a flapper, and the sketch
calls `easel::blend` — SigilShape's blend TOOL — with no `Material::` term
anywhere in it. No rebase was taken.

## 36. The windowed/tiled bake — MEASURED AND CLOSED, 2026-07-28: the mechanism has a ceiling of zero

Filed by SigilWorld's marquee. It authors ONE element tree ~33,000 px
along (display type, tick ruler, waveform bars, swatch runs, `.grow()`
spacers, absolute full-length rails), `snapshot()`s it to a vector
picture — no texture-size limit at author time — and slices that picture
into 8–10 GPU tiles of 324 x 4096, because no single texture could hold
it. The gap as filed: *"snapshot() bakes whole-tree only; a native
windowed/tiled bake (render picture region → texture set) is a
candidate."*

**The perf question was the whole load-bearing claim, and it is dead.**
`BM_Bake_TiledStrip_*` (`bench/ComposeCoreBench.cpp`) bakes a strip of the
marquee's own shape and density and slices it four ways. Release, this
machine, milliseconds, at 2 / 10 / 40 tiles of 324 x 4096:

| arm | 2 | 10 | 40 |
|---|---|---|---|
| `Snapshot` (the bake itself) | 0.533 | 2.93 | 12.7 |
| `FullReplay` (status quo: every tile replays the whole picture) | 0.640 | 4.66 | 37.8 |
| `RTreeReplay` (same, picture recorded behind a BBH) | 0.584 | **3.11** | **12.5** |
| `PerTilePicture` (FLOOR: each tile's ops extracted in advance) | 0.584 | **3.13** | **12.5** |
| `SurfacesOnly` (clear the tiles, draw nothing) | 0.167 | 0.836 | 3.34 |

Read the middle two rows together. **A bounding-box hierarchy — one extra
argument to `beginRecording` — lands EXACTLY on the extraction floor**,
3.11 vs 3.13 at ten tiles and 12.5 vs 12.5 at forty. There is no residue
for a bespoke region bake to collect. And the residue it would collect if
the BBH did not exist is 1.55 ms at the marquee's actual size, ONCE, next
to a 2.93 ms bake and a 0.836 ms floor of merely clearing the surfaces.

Two things the table also settles. The quadratic is real but small:
un-culled, every tile walks every tile's ops, so per-tile replay goes
0.320 → 0.466 → 0.945 ms across the sweep while the culled floor stays
flat at 0.292 / 0.313 / 0.313. And the BBH is not free — building it costs
0.091 / 0.494 / 1.96 ms against savings of 0.056 / 1.55 / 25.3 — so it
LOSES at two tiles and pays from about four on, which is why it is opt-in
and not welded into `snapshot()`, whose overwhelming caller (brush and
stamp bakes) replays whole and would only pay the record.

**A region bake would also have been SLOWER, not faster.** The only way
compose could serve a region natively is to keep a Composer alive and run
`paint` once per region — and the paint traversal has no node-level
quick-reject at all (`Paint.cpp` walks the whole instance tree and lets
`SkCanvas` reject), so it would walk the same ops as the replay while
doing strictly more per node. The candidate was structurally backwards.

### The verdict: the door is the ORIENTATION, and it is 25 lines

What actually cost time on the marquee was never the throughput. It was
the transform: the slice math was re-derived wrong at least twice, always
at the mirror, because a transpose has determinant -1 and composes with
the ribbon wall's own mirrored sampling — so an unmirrored slice needs a
flip that an along-oriented one does not. The settled marquee finally
avoided the transpose entirely by authoring the strip as a COLUMN and
mirroring in x. That lesson is the deliverable.

Shipped in `Compose.h` beside `snapshot()`, implemented in `Composer.cpp`:

```cpp
namespace tiles {
enum class Flow { Down, Across };
enum class Facing { Forward, Mirrored };
SkMatrix window(SkISize tile, int index, Flow = Flow::Down,
                Facing = Facing::Forward);
sk_sp<SkPicture> sliceable(const sk_sp<SkPicture> &art);
}
```

`Flow` offers **no transposing slice on purpose** — the header says
author the strip in the tiles' orientation and says why. `Facing` is
documented as a statement about the CONSUMER, not the picture: `Mirrored`
pre-flips ACROSS the strip so a surface that samples backwards reads it
the right way round. `sliceable()` is the BBH row of the table above,
made a verb because the one-liner has its own trap — `drawPicture()` into
a recorder stores a nested reference the hierarchy cannot index into,
leaving the tree empty and the cost unchanged. That trap was hit once
while building this bench, which is the evidence for spending a name on
it.

**Rejected, with reasons.** *(c) a `snapshot()` region overload*: ceiling
of zero by the table, and structurally slower per the paint-traversal
argument; it would also have to re-run reconcile+layout per region or
retain a Composer, which is a lifecycle for no gain. *(a) documentation
only*: necessary but not sufficient — the manager's bar was that a caller
must not be able to get the mirror wrong, and prose cannot enforce a
handedness. Both halves of (a) survive inside the shipped door as its
header comment and the API.md section. *A tile helper that returns
surfaces or images*: refused — raster vs GPU, colour type and alpha are
consumer policy, and compose must know nothing about GPU tiles or worlds.
It speaks pictures and transforms, which is exactly what `SkMatrix` and
`SkPicture` are.

### Pins (`ComposeTestContent.cpp`, suite `ComposeStripTiles`)

Five, all in PIXELS, over a strip whose every tile carries one small mark
near its top-left in a per-tile colour — so a tile reports its index by
colour and its handedness by which side the mark landed on, and the mark
is small enough that its own mirror image never overlaps it on either
axis:

- `ForwardWindowSlicesInOrderAndDoesNotMirror`
- `MirroredWindowFlipsAcrossTheStripNotAlongIt`
- `MirroredTileReadsForwardUnderMirroredSampling` — the contract itself:
  bake mirrored, sample mirrored, get the forward bake back
- `AcrossFlowStepsRightwardAndMirrorsInY`
- `SliceableFlattensTheOpsAndChangesNoPixel` — counts NON-nested ops,
  which is the only thing that tells `playback()` from `drawPicture()`

**Positive controls, all four run, each rebuilt and restored.** (1) Mirror
dropped from `Flow::Down` → `MirroredWindowFlipsAcrossTheStripNotAlongIt`
and `MirroredTileReadsForwardUnderMirroredSampling` FAIL. (2) Mirror moved
ALONG the flow instead of across → the same two FAIL, which is what makes
the second pin's name true rather than decorative. (3) `sliceable()`
nesting via `drawPicture()` → `SliceableFlattensTheOpsAndChangesNoPixel`
FAILS on the op count while every pixel still matches, which is precisely
the silent-cost shape the verb exists to prevent. (4) Tile step sign
reversed → the two ordering pins and the mirror pin FAIL. No control was
vacuous.

**THE GATE, 2026-07-28.** Debug builds clean. `compose_test` **468 cases /
83 suites** (463 / 82 before, +5 / +1: this entry's pins), 467 passed and 1
skipped — the same `ComposeVariationDrive.AdvanceVariantAxisIsRefused`
§35.3 files. `ctest -C Debug` **16/16, 429.43 s**. **PLATE LEDGER**
(Release, 58 scenes): **55 byte-identical**, 2 attributed flappers
(`genesis_fire`, `hitman_verlet`; `slitscan_2001` held still this sweep),
**1 mover: `easel_playground`**, `187686f0e651 -> 39528e682c55` — the SAME
hash §35 already attributed to SigilShape's uncommitted work, reproduced
exactly, so it is that library's deterministic consequence and not this
change. Byte-neutral, as a purely additive door must be. No rebase was
taken.

## 37. The animation VALUES were marooned inside a drawing library — MOVED to SigilMotion, 2026-07-29

Filed by a cross-library sharing sweep. `Transition`, the `ease::` house
curves, `Transitioned<T>` with the `animate()`/`from()`/`to()`/`through()`
builders, and `Bound`/`BoundFloat`/`bind()` sat at the top of
`<sigilcompose/Compose.h>` — lines 75–416, **342 lines** — and every one of
them is choreograph, `<chrono>` and float math. Nothing else. They were
reachable only by linking SigilCompose, which means Skia, Yoga, SigilWeave,
SigilImage and the reconciler, for the privilege of writing
`bind(&phase).target(-70, 170)`. SigilWorld and SigilShape both want shaped
bindings; neither can afford that price. SigilCompose has linked SigilMotion
since day one (`compose/CMakeLists.txt`), so the move cost compose nothing.

**Verification first, because the whole thing turns on it.** The region was
extracted and swept for `Sk*`, `YG`/Yoga, `weave::`, `image::`, and every
kernel type (`Element`, `Instance`, `Composer`, `Material`, `Fill`). **Four
hits, all four inside doc comments** — a cross-reference to the §8 stagger
law, the `compose::from(...)` shadowing note, `through<SkColor4f>` as an
example spelling, and the `ElementNode` block-split analogy in a comment
that stayed behind anyway. **Zero code-level dependencies.** The corpus
blast radius is 29 files under `compose/` touching the names, and — the
number that actually mattered — **every internal use is unqualified inside
`namespace sigil::compose`**, which is why the ruling below costs no edits.
Outside `compose/` there are no users at all yet: that is the gap this
closes, not a regression risk.

**What moved:** all 342 lines, plus `Animatable<T>` itself (87 more —
see the section below, which corrects the premise this entry started
from), into `<sigilmotion/Animation.h>` — **478 lines** with its own file
header and includes. The `animate()` family moved WITH `Transitioned<T>`
deliberately: leaving the verbs behind would have given SigilMotion a value
type with no ergonomic way to build one, which is most of what
"independently usable" means.

### The premise that was wrong — `Animatable<T>` moved too

**This entry originally stopped at the values and left `Animatable<T>`
behind, on a citation that does not survive contact with the source.** The
claim was: its `index()` is documented as "the old variant's index order,
**for the reconciler's compare**", `Reconcile.cpp` reads exactly that
ordering, therefore `Animatable` encodes kernel semantics and can never
leave. Two things are wrong with it.

**The citation was wrong.** It pointed at `Compose.h:461`, which is the
`Shape`/`ShapeScheme` seam. `Animatable` was at line 122.

**The reading was wrong, and this is the part worth remembering.** That
sentence is a COMPACTION note. It explains why the enum order was preserved
when `Animatable` stopped being a `std::variant` — so a shaped binding
sorts after a bare one rather than replacing it — and it names
`propEqual` as the code that would notice if the order moved. A stable
discriminant is what ANY consumer diffing two animatable values wants;
compose's reconciler is the first such consumer, not the definition of the
type. "The reconciler reads this" is not the same claim as "this belongs to
the reconciler", and the entire no-move argument rested on eliding them.
The comment has been rewritten in place so the next reader cannot make the
same inference, and it now says so explicitly.

**The proof, done the same way as for the values.** `Animatable<T>`'s four
forms are a `T`, a `Transitioned<T>`, a `choreograph::Output<T>*` and a
`BoundFloat` — every one of the last three already moved. Its privates are
`Kind` (a `uint8_t` enum), `T m_plain`, the Output pointer and a
`unique_ptr<Extra>`. No Skia type, no Yoga type, no kernel type, in the
class or in any signature. Swept for friends and reach-ins: `Animatable`
has **no friend declarations** and **nothing outside `Compose.h` touches
`m_kind`/`m_extra`/`m_bound`/`m_plain`** (the greps that look like hits are
`Shape::m_kind` in `Compose.cpp` and `Material::m_bound`, unrelated types).
`propEqual` (`Reconcile.cpp`) goes through `index()`, `plain()`,
`transitioned()`, `boundMap()` and `binding()` — all public — and **did not
change by a character**; it now compares a `motion::Animatable<T>` and is
none the wiser.

**MEMORY LAYOUT PRESERVED, measured not asserted.** The out-of-line `Extra`
block, the `Kind` enum and the mutually-exclusive-fat-forms trick are
hard-won and were moved byte-for-byte. Compiled with SigilCompose's own
Debug flags, before and after the move, arm64-osx:
`sizeof(Animatable<float>)` **24 B, align 8** → **24 B, align 8**;
`sizeof(Animatable<SkColor4f>)` **40 → 40**; `Transitioned<float>` 88;
`BoundFloat` 80. Identical. The existing
`static_assert(sizeof(ElementNode) <= 768)` in `Composer.cpp` is a
compile-time pin on the downstream consequence and it holds in both configs
— the documented `ElementNode 1288 B → 688 B` and `PaintProps 856 B →
~250 B` results are untouched. Those numbers are the EVIDENCE for the
layout, not a dependency on compose, and the doc comment now says that.

**What stayed, and it is the right seam:** RESOLUTION. An `Animatable` is
resolved against a `PaintContext` — node-level transition policy, stagger,
mount entrances, per-frame Composer state (`Transitions.cpp`,
`resolveFloat` and friends). That is compose deciding what a described
change MEANS to a node, and it stays in compose. **SigilMotion ships no
resolve surface at all**: the deliverable was the value type being
reachable, and inventing a clock-only `resolve()` nobody has asked for
would be exactly the speculative API this move is supposed to make
unnecessary. A consumer that wants a naive read writes five lines — the
pin below does, and that is the demonstration.

**THE NAMESPACE RULING: compose re-exports, and the re-export is PERMANENT.**
Fifteen `using motion::…` declarations (`Animatable` among them, by the
same rule) plus `namespace ease = motion::ease;`
in `Compose.h`, so `compose::Transition`, `compose::Animatable<float>`,
bare `animate(...)` under a
`using namespace sigil::compose;`, and the two qualified
`sigil::compose::from(...)` sites in `sigillum_aemeth` all keep compiling
untouched. No call site changed. **Does §32's "REPLACE, not converge" reach
this? No, and the reason is not convenience.** That ruling governed
GRAMMAR: two different WORDS for one intent, where keeping both left the
library unable to say which one an author should reach for, and the fix was
to delete the worse word. Here there is no second word. `compose::Transition`
and `motion::Transition` are *the same entity* — a using-declaration, not an
alias to a parallel definition — and the qualifier expresses which library
DEFINES the concept, not which spelling an author prefers. There is nothing
worse to delete and no author decision to disambiguate. Positively: these
types appear in compose's OWN signatures (`Element::transition()`,
`animate()`'s spec argument, every `Animatable` property), and a library
names the vocabulary it speaks — the same reason an API taking a
`std::filesystem::path` does not make its callers stop saying `path`.
Deleting the compose spelling would break every sketch's
`using namespace sigil::compose;` for zero naming gain, which is the exact
inverse of what §32 was for. Extraction is not convergence.

**The ruling was tested, not assumed.** A probe TU compiled against
SigilCompose's own flags exercises every moved name in both the qualified
(`sigil::compose::animate(sigil::compose::from(0.f).to(1.f), {520ms,
sigil::compose::ease::outBack()})`) and the unqualified sketch spelling,
plus `studio::ramp()` returning a `Transition`, an `Animatable<SkColor4f>`
colour sweep, and a real `box().opacity(animate(...)).translateX(bind(...)
.window(...).map(ease::outBack()).target(...)).transition({250ms})` chain.
It compiles clean. Three `static_assert(std::is_same_v<...>)` pin that
`compose::Transition` / `compose::Animatable<float>` / `compose::BoundFloat`
ARE `motion::` ones — same entity, not parallel definitions — and the
linker agrees out loud: unresolved symbols now read
`sigil::compose::Element::opacity(sigil::motion::Animatable<float>)`.

**THE PIN, and it is the entire point of the move.** `motion_test` links
`SigilMotion` and gtest — nothing else — and now carries six
`AnimationValues` cases: the empty-`ease` aggregate trap through
`easing()`; the three `animate()` shapes (entrance / ramp-on-change /
waypoint path, including the value-initialized degenerate `through({})`);
the `Bound` chain composing in call order with `window`/`quantize`/`invert`/
`clamp`; `TickerDrivesABoundChainWithNoRenderer`, which runs a
choreograph Output through `ticker.timeline()` under `ease::outBack()` and
reads it out as pixels through `bind(&phase).target(-70, 170)`; and two
`Animatable` cases — `AnimatableHoldsAllFourFormsWithNoKernel` (all four
forms, the discriminant order, `binding()` answering for both bound forms
while `boundMap()` tells them apart, and the `Extra` block DEEP-copying
rather than aliasing) and `AnimatableDrivenByTheTickerWithNoKernel`, which
writes a five-line naive resolve and drives a health-bar width from 0 to
240 px through the Ticker. The clock half and the value half of SigilMotion
working together, property slot included, with no drawing library linked.

**POSITIVE CONTROL for the pin.** A test asserting "usable without compose"
is worthless if compose is on the include path anyway, so `MotionTest.cpp`
opens with `#if __has_include(<sigilcompose/Compose.h>) #error`. Verified
non-vacuous by compiling the same guard twice: silent with SigilMotion's
include paths, and firing (`"GUARD FIRES"`) the moment
`-Isrc/common/compose/include` is added. If SigilMotion ever grows a link
edge that drags compose's headers in, the build stops instead of the pin
quietly hollowing out.

**SigilMotion's dependency set is UNCHANGED**: still
`target_link_libraries(SigilMotion PUBLIC choreograph::choreograph)` and
nothing else. That was the go/no-go condition and it held — the moved code
needed `<chrono>`, `<cmath>`, `<functional>`, `<initializer_list>`,
`<optional>`, `<utility>`, `<vector>` and choreograph, all of which it
already had. The library goes **292 → 770 lines** and its CMake header now
names both halves (the CLOCK and the VALUES) with the rule that anything
dragging Skia, Yoga or a kernel type in does not belong there.

**Header hygiene:** the moved region was the FIRST thing in `Compose.h`
after the forward declarations (`Animatable` immediately after it, still
ahead of `Fill` and every paint value), depended on no compose declaration,
and nothing later depends on it being defined at that exact point — each
re-export block sits exactly where its code was, so declaration order in
`Compose.h` is unchanged.
`<sigilmotion/Animation.h>` joins the existing `FrameClock.h`/`Ticker.h`
includes at the top.

**THE GATE, 2026-07-29.** Debug and Release both build clean (only the
pre-existing `chladni_tab1` `nodiscard` warning). `compose_test` **468 / 83
suites — UNCHANGED**, 467 passed, 1 skipped (`AdvanceVariantAxisIsRefused`,
the expected §35.3 skip); a move must not move the count and it did not.
`shape_test` **76**, `world_test` **34** — the latter matters because it
links SigilCompose test-only, so a broken compose API would surface there.
`motion_test` **5 → 11**, the +6 being this entry's pins — the ONLY
suite-count change in the tree, and it is the pin. `ctest` **16/16 both
configs** (Debug 430.48 s, Release 31.95 s).

*Process note for the next person:* the first `ctest -C Debug` after
editing only a COMMENT in `Animation.h` failed `compose_sketch_smoke` /
`compose_sketch_stock` with *"A sketch compiled against skewed headers
would corrupt the host ABI, so this build is refused rather than risked."*
That guard hashes headers, not semantics — a comment is a skew. Rebuild
BOTH configs before reading any sketch-host ctest result; after rebuilding,
16/16 both configs with nothing else changed.

**PLATE LEDGER** (Release, 58 scenes): **54 byte-identical**, 3 attributed
flappers (`genesis_fire`, `hitman_verlet`, `slitscan_2001`), **1 mover:
`easel_playground` `187686f0e651 -> 39528e682c55`** — the same hash §36
already reproduced and attributed to SigilShape's uncommitted work, not to
this change. Run twice, before and after `Animatable` joined the move, with
**the same four lines both times**. **No other mover. Byte-neutral, as a
move must be.** No rebase was taken.

## 38. §Argument 3's binary volatility declaration — MEASURED, and the measurement found a staleness bug instead

§10g's closing item (4) and §Argument 3 both name the same defect:
`isAnimated()` is per-node and binary, so "changes every three seconds"
and "changes at 60 Hz" make one declaration. Two waves filed it without
measuring it. This is the measurement.

### THE MEASUREMENT — the cost is real, and it is 19.6×

`compose_bench`, Release, quiet machine (load 1.8, no other build
running), four new arms in `bench/ComposeCoreBench.cpp`. The tree is a
panel — root → frame → wrapping row → N stroked, shaped cells, each of
which records its own picture — plus ONE accent cell in the same row.
The arms differ in exactly one thing: whether the accent's colour is
spelled `fill(&output)` or `fill(Fill::color(...))`.

**Pair one, the property never moves at all** (`BM_Draw_StillAccent_*`,
µs/frame, 5 repetitions, cv ≤ 0.6 %):

| cells (nodes) | bound | plain | delta | textures live |
| --- | --- | --- | --- | --- |
| 32 (35) | 344 | 343 | **none** | 0 / 0 |
| 128 (131) | 1337 | 254 | **+1.08 ms, 5.3×** | 0 / 1 |
| 512 (515) | 5284 | 269 | **+5.02 ms, 19.6×** | 0 / 1 |

**Pair two, the colour actually moves once every 180 frames** — three
seconds at 60 Hz, the entry's own example — each arm doing the minimum
its spelling requires (the bound arm assigns the Output and never
re-describes; the plain arm re-describes only on the frame it changes):
512 cells, bound **5326 µs**, plain **289 µs**. Paying the full
re-describe, re-record and re-bake, amortized, costs the plain spelling
20 µs/frame and it still wins by **5.04 ms/frame**.

**THE CAUSAL PROOF, from the profiler §29 built** (`COMPOSE_BENCH_WHY=1`,
depth ≤ 3):

```
bound:  root/frame/row/accent   cache=Live      promotion=Volatile
        c0…c511                 cache=Picture   promotion=Cheap   ~0.011 ms each
plain:  root                    cache=Promoted  promotion=Promoted  0.263 ms, one blit
```

So the chain is exact and has nothing to do with traversal overhead:
one bound `fill()` on one leaf sets `ownContent`, `computeVolatile`
carries it up, the root is `subtreeVolatile`, `contentStable` is false,
promotion is refused with `Promotion::Volatile`, and 512 cells replay
512 pictures at ~11 µs each forever. **Picture caching does not save
this** — a picture records the DRAW CALLS and re-runs every one. Texture
promotion is the only thing that saves it, and the binary declaration is
what denies it.

**Where the cost is ZERO, stated because it bounds the claim:** at 35
nodes the plain arm is not promoted either (`Promotion::Cheap`), and the
two arms are within 1 µs. The defect costs exactly what promotion would
have been worth, and nothing where promotion would not have fired.

**VERDICT: the cost justifies a mechanism.** 5 ms/frame on a 515-node
panel, for a property holding perfectly still, is the §3 wall in new
clothes.

### WHAT THE MEASUREMENT FOUND ON THE WAY — three silent staleness bugs

Reading `computeVolatile` to design the extension turned up that the
content-volatility enumeration is written **four times**: once for
`ownContent`, once for §30's `opaqueToTheMemo`, and once inside each of
the two memo carve-outs (`liveMatOnly`, `scalarMemo`). Three of the four
had drifted. Neither carve-out mentioned a **bound fill** or a **live
effect**, both of which the recording bakes in — so a node carrying one
of those AND an animated gate took a memo it had no right to, kept a
recording made with the old value, and replayed it for as long as the
gate held still.

Reproduced before anything was changed, three red tests, all three
`8100 vs 100` red pixels — the colour simply never changed:

| Pin (`ComposeCache`, `ComposeTestMask.cpp`) | the combination |
| --- | --- |
| `ABoundFillMovingUnderAHeldGateRepaints` | `fill(&out)` + `by::spans` |
| `ALiveEffectMovingUnderAHeldGateRepaints` | `effect(…uniform(&out))` + `by::spans` |
| `ALiveEffectMovingOverAHeldMaterialRepaints` | live `Material` + live `effect` |

**THE FIX is the root cause, not the three symptoms.** The terms are
named ONCE (`fillLerp`, `boundFill`, `liveMat`, `metricLive`,
`cacheNone`, `decorLive`, `imageLive`, `driveLive`, `liveEffect`,
`spanVolatile`, `maskOpaque`, `scalarContent`) and every consumer is a
SUBTRACTION from that one list — `otherThanScalar`, `otherThanLiveMat` —
so `ownContent == scalarContent | otherThanScalar == liveMat |
otherThanLiveMat` holds by construction rather than by review. The two
carve-outs are now one line each. 121 lines of hand-maintained
enumeration became 55.

The change is **monotonically more conservative**: every consumer gains
terms and loses none, so caching can only decrease. That is why it is
safe, and it is also why `maskOpaque` — added to `scalarMemo` by the
same construction and not separately reproduced — needs no pin of its
own: it is a tightening derived from the list, not a new claim.

**CONTROLS, one per term, each run and each restored.** Every control
removes exactly one term from exactly one list:

| Control | Result |
| --- | --- |
| `boundFill` out of `otherThanScalar` | `ABoundFillMovingUnderAHeldGateRepaints` FAILS; other two OK |
| `liveEffect` out of `otherThanScalar` | `ALiveEffectMovingUnderAHeldGateRepaints` FAILS; other two OK |
| `liveEffect` out of `otherThanLiveMat` | `ALiveEffectMovingOverAHeldMaterialRepaints` FAILS; other two OK |

A first attempt at these controls was **VACUOUS in the other
direction** and is recorded because it would have proved too much:
`opaqueToTheMemo && !term` does not remove one term from a disjunction,
it nulls the whole thing whenever that term is true, so it failed two
tests at once and said nothing about either. The controls above edit the
term list itself.

**THE STATS PIN**, in numbers, because this is a caching change: with the
memo correctly refused, four held frames give `picturesRecorded == 0`
and `nodesPainted == 8` (the node and its parent, live, four frames
each). Before the fix both were 0 — the memo held and nothing painted,
which is the bug stated arithmetically. The control on `boundFill` fails
on this assertion before it reaches the pixels.

**PLATE LEDGER** (Release, 58 scenes): **55 byte-identical, 2 attributed
flappers** (`genesis_fire`, `hitman_verlet`; `slitscan_2001` held still),
**1 mover: `easel_playground` `187686f0e651 -> 39528e682c55`** — the
same SigilShape-attributed hash §35 and §37 both already recorded, not
this change. Byte-neutral, which is the expected result of a change that
can only remove caching: no corpus scene was hitting the bug. No rebase.

### THE DESIGN FOR THE PERF DEFECT — §20 IS the mechanism, and it is short of coverage

Weighed against §10g's list:

- **Per-PROPERTY volatility rather than per-node** — already shipped, and
  re-filing it would be the fourth near-rebuild this session.
  `computeVolatile` splits per property four ways today: `ownPaint`
  (transforms/opacity, outside the content cache), `ownContent`,
  `scalarContent` (memo-visible floats), `opaqueToTheMemo` (what a group
  bake cannot see). What is binary is not the PROPERTY axis. It is the
  TIME axis.
- **A rate or an epoch, so "changed since last frame" is answerable** —
  this is what §20 already implements, and it does it with values rather
  than epochs: `settledScalars` is the snapshot, `scanReleasedScalars()`
  re-resolves it once per draw and re-declares volatility the frame it
  moves, before anything paints. An epoch would be cheaper only for
  values too expensive to compare, and none of these are.
- **A settle mechanism** — §20 shipped exactly this: `kScalarSettleFrames
  = 8`, a paint-side counter, a walk-side release, a per-draw movement
  scan. **It is most of the answer, and the remaining gap is purely its
  COVERAGE.** `scalarContent` admits two lanes — mask gates and glyph
  progress — because `ContentScalars` is `{float glyph;
  std::vector<float> gates;}`. A bound `Fill` is not in it, and §20's own
  text says why: *"there is no equivalent of 'the numbers the recording
  was baked with' for the general case."*

**THAT SENTENCE IS FALSE FOR A BOUND FILL, AND THAT IS THE WHOLE
DESIGN.** `Fill::operator==` (`Compose.h:146`) is kind + `SkColor4f`
bitwise + shader POINTER identity — structurally exact, never
perceptual, never epsilon'd. It is the same equality `bakedLiveShader`
already uses to hold the live-material memo, and the same rule §10g
wrote down for an inherited theme's equality. Resolving one costs a
pointer dereference (`binding()->value()`, `Paint.cpp:1579`). So the
extension is: `ContentScalars` gains a `Fill` lane, `boundFill` moves
from the unconditional `ownContent` terms into `scalarContent`,
`resolveGateValues()` gains a sibling, and `scanReleasedScalars()`
compares it. No new concept, no new API, no author knowledge — §20's
preferred shape, on one more lane.

**SCOPED, NOT BUILT, and the reason is the direction of the change.**
Tonight's fix only ever REMOVES caching, which is why it could be landed
on a measurement and a ledger at the end of one night. The extension only
ever ADDS it, which is the direction that produces stale pixels — the
exact failure mode the three bugs above were. It needs its own pass:
the release must be proven against `Promotion::Volatile`'s
`contentStable` as well as against the recording (promotion is the whole
5 ms, and it is a SEPARATE consumer of `subtreeVolatile` from the memo),
the `opaqueToTheMemo` group lane must be decided separately rather than
carried along, and it wants the ledger run against a corpus that
actually contains a slow bound fill — which, per this sweep, none of the
58 scenes does. The measurement above is the argument for doing it; it
is not the licence to do it at five in the morning.

**Two further lanes the same extension would want, named so the next
pass does not re-derive them:** a bound TRANSFORM (`ownPaint`/`moving`,
which does not block the node's own content cache but does block every
ancestor and every promotion, so a slow-moving position costs the same
5 ms), and a live `Material` whose `liveStableRate` already proves it is
holding still but which never releases the FLAG — so it buys promotion
for ITSELF (`temporallyStable`) and nothing for its ancestors. That
asymmetry between the two existing stability machineries is worth
closing in the same pass.

---

## 39. The MOTION PATH — After Effects' spatial/temporal split, ported from the 3D camera — LANDED 2026-07-29

`translateX`/`translateY` were two independent `Animatable<float>` lanes,
which is exactly where SigilWorld's camera was two days ago. Two lanes
describe a POINT. They cannot describe a TRAJECTORY, and driving a curve
through them means the author computing two numbers a frame — the
imperative door wearing the declarative one's clothes.

`Element::travel(MotionPath)` closes it, and the pattern is not invented
here: `world::CameraPath` (world/README.md, 2026-07-29) is the proven
shape and four of its rulings port unchanged — the lane is `t` (position
ALONG the curve, so the whole `bind()` chain applies to the SCHEDULE
rather than the geometry), whatever the path drives it drives outright,
closed curves WRAP and open ones CLAMP, and the arc-length table is
cached against the INPUT that determines it rather than behind a dirty
flag.

```cpp
box().key("comet").rect({0, 0, 14, 14}).fill(gradient(...))
    .travel({.path = shapes::circle(),                    // the shape
             .t = bind(&phase).map(&choreograph::easeInOutQuad).target(0, 2),
             .lookAhead = 0.02f})                         // auto-orient
    .rotate(bind(&phase).target(0, 360));   // …and it still spins on top
```

### WHAT DID NOT PORT, AND WHY — the complication is real but not fatal

SigilCompose must not depend on SigilShape, so `shape::Spline3` is
unavailable: the path currency is Compose's own `Shape` value. **A
`Shape` is a function of a SIZE.** Three consequences, all ruled and
pinned (the arguments are in DESIGN.md § The motion path):

- **It resolves against the PARENT's box.** `shapes::circle()` on a
  14 px comet inside a 400 px card must be a 400 px orbit, not a 14 px
  twitch — and the parent's space is also, exactly, the space paint
  applies the translate in. Rejected: the node's own box (useless), and
  the canvas (wrong under any nesting).
- **A relayout re-shapes the curve and leaves `t` alone.** The node
  slides to the same fraction of the NEW curve. `t` is the schedule; a
  layout change is not a schedule change. Rejected: freezing the path at
  first resolve (a wrong curve forever, and it contradicts the
  compare-against-the-destination rule), and holding constant px arc
  length (a shrink pushes `t` past 1 and the node off its own path).
  The table is cached against BOTH inputs — the `Shape` value AND the
  size — with no dirty flag.
- **Pruning is the shape seam's contract, inherited not re-invented.**
  A comparable scheme prunes; a raw callable never compares equal and
  keeps the node conservatively un-pruned.

### THE ONE DELIBERATE DEPARTURE — auto-orient ADDS to `rotate()`

`CameraPath` takes the eye outright. Position here does too, for the same
reason (a lane that half-contradicts a curve can only place the node off
it). Orientation does NOT: `lookAhead != 0` ADDS the chord angle to
whatever `rotate()` says. That is not a weakening of the rule, it is the
OTHER rule in the same header — `AnimatedCamera::rollDeg` composes with
the flight — and unlike position, tangent-plus-spin is geometrically
well-defined. It is also what AE does. So a comet can bank along its
orbit and spin on its own axis at once, which the outright reading
forbids for nothing.

The chord (not the exact tangent) is used so a negative `lookAhead` reads
back down the curve and a hard corner is smoothed rather than snapped;
at the end of an open curve the last good chord is held.

### ARC LENGTH CAME FREE, AND TOOK THE FLAG WITH IT

`SkContourMeasure` — already in `Brushes.h` for exactly this — is
arc-length parameterised by construction, so there is no table to build,
no `samples` knob, and **no `arcLength = false`**: `CameraPath` needs the
opt-out because a `Spline3` has a native parameter to opt back to, and an
`SkPath` has none. `t` is a fraction of the TOTAL length across every
contour, which is the coordinate `bandPointAt`, `spans::` and
`SkTrimPathEffect` already speak.

### FOUND ON THE WAY — `scaleX`/`scaleY` were never in `propsEqual`

The equality audit the brief demanded for `MotionPath` found that the
per-axis scale lanes have been MISSING from `Reconcile.cpp`'s
`propsEqual` since they landed. Two descriptions differing only in
`scaleX` compared EQUAL, so the patch pruned, `markPaintDirtyUp()` never
ran, and a bar re-described at a new width replayed the old picture — and
because `applyTransitions()` only runs inside the `own` branch, an
`animate()` on `scaleX` never ramped either. Fixed in the same wave and
pinned with its own control per axis
(`ComposeTravel.PerAxisScaleParticipatesInReconcilerEquality`).

**Also corrected:** DESIGN.md § Element memory quoted `sizeof(ElementNode)`
as 744. Measured on the way in: it was **728** before this change and is
**736** after (one `Box`, which is the rule working). The stale figure is
replaced with the measured pair and the date.

**Filed, not fixed:** `recordBounds()`'s child-transform gate reads
`rot != 0 || scl != 1 || skx != 0 || sky != 0` and its scale step reads
`if (scl != 1) m.preScale(scl * sx, scl * sy)` — both omit `sx`/`sy`, so
a child whose only transform is a per-axis scale contributes UNSCALED
bounds to its parent's record bounds. Left exactly as it was because
changing paint bounds moves cull and bake rectangles across the whole
corpus and that wants its own ledger run; the same wave's shared
`transformOf()` resolver makes it a two-line fix when it is taken.

### Pins (`ComposeTestKernel.cpp`, suite `ComposeTravel` — ten)

`PlacesTheTransformOriginOnTheParentSizedCurve` (the PIXEL pin: four
quadrant points of the parent-sized circle scanned out of the frame, plus
the origin ruling), `TIsAFractionOfTotalArcLengthAcrossEveryContour`,
`WrapsOnAClosedCurveAndClampsOnAnOpenOne`,
`OutranksTheTranslateLanesAndHandsThemBack`,
`AutoOrientAddsToRotateAndHoldsTheLastGoodChord`,
`PrunesOnlyWhenEveryFieldOfThePathMatches` (the PRUNE pin: one control
per field, plus the raw-callable escape hatch),
`IsPaintOnlyAndAResizedFrameKeepsT` (the size ruling and the layout
refusal in one), `TheHitTestUndoesTheSameMatrixPaintApplied`,
`APathWithNoMeasurableLengthLeavesTheLanesStanding`,
`PerAxisScaleParticipatesInReconcilerEquality`.

Sixteen positive controls run (mutate → named pin fails → restore, mtime
stamped); every one failed the pin it was aimed at. Additive: no corpus
scene uses `travel()` and the plate ledger is byte-neutral.

## 40. THE EQUALITY AUDIT — every field of every hand-compared struct, and the pin that makes the next miss a build failure (2026-07-29)

`propsEqual()` is what lets a re-described node PRUNE, and **a field
missing from it is silently broken**: the node compares equal when it is
not, the patch prunes, `markPaintDirtyUp()` never runs, a stale picture
replays, and `applyTransitions()` — which only runs inside the `own`
branch — never ramps an `animate()` on that property. Nothing errors. No
test fails.

That happened TWICE on one feature. `scaleX`/`scaleY` were missing from
`propsEqual` from the day they landed until §39 (e37d58d), and the same
pair was missing from `recordBounds()`'s transform gate, filed there and
taken here. Two data points on one feature is a pattern, so this wave
audited the whole surface and then closed the class.

### THE AUDIT — 161 fields across 23 hand-compared structs

Every count below is now a `static_assert` in the source, so the compiler
verified them and will keep doing so. **152 participate; 9 are excluded,
each with a stated reason.** No further miss was found — which is the
finding: `scaleX`/`scaleY` was a slip, not a habit.

**Tier 1 — `ElementNode`, 23 fields, 21 participate.** `kind`, `key`,
`layout` (defaulted `==`), `paint`, `corners` (defaulted), `shapeFn`
(the Shape seam), `clipContent`, `hitTestable`, `cacheMode`, `bakeScale`,
`nodeTransition`, `backgrounds`, `foregrounds`, `textData`, `imageData`,
`customData`, `deriveData`, `fxData`, `materialData`, `strokeData`,
`motionData`. **Excluded:** `memoData` — compared EARLIER and more
strictly by `resolveMemo()` (env snapshot, then the author's own props
comparator) and never lands in `inst.desc`, which holds the memo's
PRODUCED payload; `children` — reconciled BY KEY, never compared, which
is the whole point of the structural prune.

**Tier 2 — the blocks, compared by hand.** `PaintProps` 15/15 (the
`scaleX` hole, now closed and walked); `TextData` 12 (11 full, plus
`layoutOptions` compared only on `alignment` — legitimate, because the
only builder that can set the rest is `text(paragraph, options)`, which
also sets `paragraphOverride`, and a present override is unconditionally
conservative); `ImageData` 3/3; `CustomData` 2 (`program` excluded — an
incomparable callable, with equal non-empty `key` as the author's
contract); `DeriveData` 14/14 (its three callables participate by FORCING
inequality, which is participation); `FxData` 8/8; `StrokeData` 1/1 +
`StrokePass` 4/4; `MaterialData` 2/2 (`recipe` is ruled on in
`propsEqual` itself, beside the fill); `MemoData` 4 (`props` via the
author's comparator, `env` via `envEqual`; `equal` and `invoke` excluded
— they ARE the comparator and the deferred describe); `MotionPath` 3/3.

**Tier 3 — the values compared by hand in `Reconcile.cpp`.** `BoundFloat`
16/16; `Transition` 3/3; `Transitioned` 4/4; `Fill` 3/3; `Mask` 2/2;
`Spans::Term` 11/11, with `begin`/`end`/`offset` scoped to Range and Wrap
— sound, because `Spans::resolve` reads `values[3i..3i+2]` under those
two rules and nowhere else; `Gate` 7/7, kind-scoped by the class's own
"only the members its Kind reads are meaningful" contract. `Animatable`
is NOT decomposed and needs no pin: `propEqual` reaches it only through
the five public accessors, so a member no accessor exposes is invisible
to everyone, not just to the reconciler.

**Tier 4 — the delegated equalities.** `Material` 7/7; `Region` 3/3;
`Effect` 6 (`m_effect` + `m_uniforms` compared; `m_filter` is DERIVED
from them on the shader path and IS the comparison on the filter path;
`m_chainA`/`m_chainB` are retained only on a LIVE chain, which
`isAnimated()` refuses outright). `Shape`, `Decoration` and `Profile` are
complete by construction — a type-erased `std::any` plus the held
scheme's own comparator. Everything with a defaulted `operator==`
(`LayoutProps`, `Corners`, `MarkLabel`, `Echo`, `Anchor`, `Across`,
`Parts`, `Span`, `TextPath`, `ContentScalars`) is exhaustive by compiler
and needs nothing.

**Also audited, same hazard shape, no miss:** the four hand-written lists
over `Instance::Slot` — `collectGroupScalars`, `computeVolatile`,
`applyMountTransitions` and `applyTransitions` — each covers every one of
the twelve slots that applies to it (`kFillLerp` and `kGlyphProgress` are
CONTENT scalars and are pushed outside the transform block, not omitted).

### FOUND AND FIXED — `recordBounds()`, the second site

`NodeTransform::pivoted()` already spells the transform gate, and
`paint()` and `hitInstance()` both ask it. `recordBounds()` was the one
consumer of the three that hand-rolled the condition, and it left `sx`/`sy`
out — against the resolver's own stated invariant that "the three must
describe the same matrix or a node draws where it cannot be hit". A child
whose ONLY transform was a per-axis scale therefore handed its parent
UNSCALED bounds, and every consumer sized off them (the effect layer, the
opacity layer, the texture bake) truncated the overflow.

The fix is the gate ASKING `pivoted()` rather than copying it, plus
`scl != 1 || sx != 1 || sy != 1` on the `preScale` step. Pinned by
`ComposePaintBounds.PerAxisScaleReachesTheParentsChildBoundsUnion` (a
per-axis-scaled bar under an identity `offset()` filter — the bounded
`saveLayer` clips, so the scaled-out half is simply gone when the bounds
are wrong).

**Also corrected:** `boundMapEqual`'s invariant comment named
`BoundMapEqualitySeesEveryField` as its positive control. No such test
exists anywhere in the repo; the real one is
`ComposeReconcile.WiggledBindingsPruneOnlyWhenEveryParameterMatches`, and
it covers five of `BoundFloat`'s sixteen fields. The comment now names the
tests that exist, and the other eleven fields have a control for the first
time.

### THE MECHANISM — a structured binding is this file's `variant_size_v`

The pop wave's precedent is `kPopOpPso[]` under
`static_assert(std::size(...) == std::variant_size_v<pop::Op>)`: appending
an alternative without ruling on its row is a BUILD FAILURE. The
equivalent for a STRUCT's fields is a **structured binding**, which names
every direct non-static data member and stops compiling the moment the
count changes:

    error: type 'PaintProps' decomposes into 16 elements,
           but only 15 names were provided

It is EXACT where the obvious alternatives are not, and both alternatives
were tried on paper and rejected. A `static_assert(sizeof(T) == N)` is
walked straight past by a `bool` dropped into tail padding — which is
precisely the shape of field `originPx` already sitting next to
`originX`/`originY` in `PaintProps`. An aggregate-arity probe
(`T{Any{}...}`, maximise N) counts the BRACE-ELIDED flattening of nested
aggregates, so `LayoutProps` would report its `EdgeValues` as four floats
and the number would move for reasons that are not fields. A byte-wise
mutation walk is not available at all: these structs hold `std::string`,
`std::function`, `std::any` and `sk_sp`, and flipping bytes in them is
undefined behaviour, not a test.

So: `ComposeInternal.h` gains a `fields()` decomposition per hand-compared
struct and a `kFieldCount<T>`; `Reconcile.cpp` gains a `static_assert`
beside each comparator naming what the author must rule on; and
`Material`, `Effect`, `Region` and `NodeTransform` — whose state is
private — carry the same pin as a private static member function, which
is legal because a member function body is an access context for the
decomposition.

**And a third gate, which is the part that makes a ruling mechanical
rather than a rubber stamp.** `test/ComposeTestFieldPins.cpp` walks the
tied fields of `PaintProps`, `BoundFloat` and `ElementNode`, perturbs each
in turn, and demands the comparator notice — so a new field is COVERED
the moment it is named in `fields()`, with no second list to remember, and
a field whose type has no perturbation does not compile either. The two
legitimate `ElementNode` exclusions are asserted INERT there rather than
merely described, so an exclusion that quietly starts mattering also
fails. Comparing the values directly rather than counting
`stats().patchedNodes` is deliberate: a prune is a statement about the
SAME node across two describes, and a render-two-trees harness can pass
while the comparator is broken, because keyed siblings never prune into
one another. `propsEqual` and `boundMapEqual` moved out of the anonymous
namespace into `detail::` for that reason and no other.

### Pins and controls

New: `ComposeReconcile.EveryPaintPropsFieldParticipatesInEquality`,
`ComposeReconcile.EveryBoundFloatFieldParticipatesInEquality`,
`ComposeReconcile.EveryElementNodeFieldParticipatesInEquality`,
`ComposePaintBounds.PerAxisScaleReachesTheParentsChildBoundsUnion`.

**Fifteen positive controls, all fired** (mutate → the NAMED gate fails →
restore, mtime stamped): `scaleX`, `blendMode`, `bakeScale` and
`hitTestable` each pulled out of `propsEqual`; `wiggleSeed` and `inOffset`
pulled out of `boundMapEqual`; `memoData` wrongly ADDED to `propsEqual`
(the exclusion direction); a new field added to each of `PaintProps`,
`BoundFloat`, `Region`, `Effect`, `Material` and `NodeTransform` (six
build failures, each at its own decomposition); and both halves of the
`recordBounds` gate reverted.

**Ledger: byte-neutral, and run TWICE for attribution.** Run 1 (pins
only): 55/58 byte-identical — `easel_playground` at the documented
`39528e682c55`, plus `genesis_fire` and `hitman_verlet`, all three on the
known list. Run 2 (pins + the `recordBounds` fix): 54/58 — the same three
plus `slitscan_2001`, also a documented flapper. **The bounds fix moved
nothing**: no corpus scene has a per-axis-scaled child inside a
layer-forming parent whose overflow was being clipped. That is the
separate ledger run §39's filing asked for, and it came back empty.

Suites: `compose_test` 500 (499 passed, 1 skipped — the expected
`AdvanceVariantAxisIsRefused`), `compose_kit_test` 47, `motion_test` 15,
`shape_test` 83, `world_test` 64, `ctest` 16/16 — both configs.

## 41. TRACK MATTES — the luma half, both complements, and the element-matte REFUSED (2026-07-29)

After Effects' matte model is a compositing fundamental and compose had a
quarter of it: `by::alpha(Material)`, one channel, one direction. The wave
closed the cheap symmetric half and **declined the expensive half with an
argument**, which is the more useful of the two results.

### What shipped

```cpp
namespace by { Gate alpha(Material), alphaOut(Material),
                    luma(Material),  lumaOut(Material); }
```

`Gate::Kind::Alpha` is renamed `Kind::Coverage` — the kind is the
MECHANISM (a `saveLayer` and one compositing pass) and `alpha` was never
the name of the mechanism, only of one reading of it. The two questions the
four factories answer are two kind-scoped fields: a new
`Gate::Channel channel` (Alpha | Luma) and the EXISTING `outside`, whose
meaning widened from "the complement of a region" to "the complement of
whatever this gate names".

### Ruling 1 — the inversion is a TERM, and the word is borrowed

The alternatives, weighed against how `by::shape`/`by::outside` already
read:

| candidate | why not |
| --- | --- |
| `.invert()` on the Gate | It is the MODE FLAG law 1 of the family explicitly forbids, and it would give `by::outside(r)` a second spelling. It would also be the only mutating verb in a vocabulary of pure factories. |
| `by::invert(gate)` wrapper | Same second-spelling problem, without the mutation. |
| `by::outside(Material)` overload | "Outside a gradient" is not a picture, and it cannot extend to luma without nesting (`outside(luma(m))`), at which point it is the wrapper above. |
| **four factories** | **Chosen.** |

The naming question that remains is only what to CALL the two new ones.
`by::outside` is an English spatial word because a region has one; a
coverage field does not. So the complement takes the morpheme the
neighbourhood already uses for exactly this operation — Skia's `clipOut`,
which is the call `by::outside`'s own doc comment cites as the thing it
was written to replace. `alphaOut`/`lumaOut`. **Consistency was kept at
the level that decides the audit (a term, not a flag); the word differs
because the picture differs.**

The mechanism is one enum value: the coverage layer composites with
`kDstOut` instead of `kDstIn`, and `dst·(1-a)` IS `1 - coverage` exactly,
for any source. No shader, no second path, no cost.

### Ruling 2 — luma is Rec. 601, on ENCODED values, PREMULTIPLIED

`Y' = 0.299 R' + 0.587 G' + 0.114 B'`, taken on the premultiplied colour.
Three rulings, and each has a control that fires:

**Encoded, because compose has no linear stage.** Every surface compose
paints into — gallery, sketch host, `Cache::Texture` bakes, `snapshot()`,
tests, the Metal/Graphite path — is `N32Premul` with a NULL
`SkColorSpace`. Skia does no transfer-function work in that mode, so a
shader's channels are the display-encoded numbers the author wrote.
Linearising inside the luma would invent a transfer function nothing else
in the pipeline applies, and would then multiply the result into an
encoded destination anyway. **This was not written down anywhere before
this wave** — DESIGN.md and API.md contained no colour-space statement at
all — and it is now DESIGN.md's colour rule, with the consequence stated:
giving compose a colour-managed surface is a breaking change, not a
configuration.

**Rec. 601, because those are the coefficients defined ON encoded
R'G'B'.** Rec. 709's 0.2126/0.7152/0.0722 are LUMINANCE coefficients,
defined on linear light; applying them to encoded values is the standard
mistake (Poynton's "luma vs luminance"). `shape/Materials.cpp:104` already
weights an encoded environment sample with the 601 set, so the repo had the
precedent without the rule. Cost of being wrong: 22/255 on red, 33/255 on
green.

**Premultiplied, because a transparent matte must read as black.** That is
AE's behaviour, and it falls out for free: a shader's channels arrive
premultiplied, so `dot(a·rgb, k) == a · dot(rgb, k)` is one dot product.
A half-transparent white and an opaque 50% grey are the same matte.

**THE PIXEL PIN IS NOT MADE OF GREYS, AND THAT IS THE POINT.**
`S7cTheLumaGateIsRec601OnEncodedPremultipliedValues` uses five plates —
pure red, pure green, pure blue, 50% grey, 50%-transparent white — because
greys pin nothing about the coefficients (every weighting of equal
channels is the same number) and primaries pin nothing about the transfer
function (0 and 1 are the sRGB curve's fixed points, the same trap
`world/`'s `RendersClearColorWhenEmpty` fell into). Each wrong answer dies
on a different plate, and the controls below show exactly that.

### Ruling 3 — the ELEMENT MATTE IS REFUSED, and the argument is the deliverable

AE's matte is another LAYER. The compose equivalent would be an element
tree baked with `snapshot()` and used as coverage — `brush::Art`'s
bake-and-replay, which exists and works. It is not built, and should not
be, for five reasons in descending order of force:

1. **THE FEATURE'S HEADLINE USE IS THE ONE IT HANDLES WORST.** `snapshot()`
   samples bindings at current values and runs no transitions — there is no
   live timeline in a one-shot render. So an ANIMATED matte tree, which is
   what a matte layer is FOR, would be silently frozen unless re-baked every
   frame, and re-baking every frame is item 2.
2. **It inherits §16's residual and puts it on the hot path.** §16's
   StampCache (closed 2026-07-27, reconciled 2026-07-30) keys bakes on
   the art Element's NODE, so a fresh brush value per describe now
   finds its bake — but a fresh art NODE per describe does not, by
   contract ("keep the art pointer-stable", every brush header). The
   natural authoring form for a matte is inline in the describe
   (`.mask(by::luma(Material::element(row().child(…)))))`), which builds
   exactly that fresh node per frame — so it re-rasterises per frame,
   now inside a `saveLayer`'d group.
3. **There is no answer for layout.** A matte must size against the MASKED
   node's laid-out box, which is known only at paint. Nothing in the
   architecture runs layout inside a recording, and adding that seam for
   this feature would be the largest change in the wave by an order of
   magnitude.
4. **Identity for the prune would be pointer identity** (`art.node()`), the
   weakest comparison in the library, on a value read live every frame.
5. **AE needs a matte layer because a layer is its only unit** — a layer's
   paint and its silhouette cannot be separated. Compose separates them
   already. The 1:1 translation of most matte uses is *put the material on
   the shaped node*: `textFill()` for a photo inside a headline, `Shape` /
   `Region::path` / `by::shape` for a silhouette, a `Material` for anything
   soft. The residual — genuinely arbitrary baked ART as coverage —
   composes TODAY, in one line more than the hypothetical:

   ```cpp
   sk_sp<SkPicture> pic = snapshot(matteTree, fonts);   // author's bake,
   // …raster it to an SkImage at a resolution the author chose…
   el.mask(by::luma(Material::image(baked)));           // author's cache
   ```

   and that extra line is precisely where the caching decision belongs.

**What would reopen it:** a describe-keyed (content-identity) bake cache
— the mechanism beyond §16's closed pointer-keyed StampCache, see §16's
2026-07-30 reconciliation — plus a
layout-at-record seam. Then `Material::element(Element)` — as a MATERIAL,
not a Gate kind, so it composes with `blend`, with fills, and with all four
matte variants at once — is a small feature. Until then it is a
frame-rate cliff with a frozen animation in it.

### Pins and controls

New: `ComposeR4Mask.S7cTheLumaGateIsRec601OnEncodedPremultipliedValues`
(the pixel pin), `ComposeR4Mask.S7cTheLumaLawIsTheSameThroughAShader`
(the colour path and the SkSL path are two implementations of one law —
the classic asymmetry), `ComposeR4Mask.S7dEachCoverageGateHasItsComplementAsItsOwnTerm`
(the pair must SUM to 255, not merely differ),
`ComposeR4Mask.ACoverageGatesChannelAndSenseReachTheComparator` (against
`propsEqual` directly, per §40's lesson).

**Six positive controls, all fired** (mutate → the NAMED test fails →
restore):

| control | what failed, and how |
| --- | --- |
| coefficients → Rec. 709, both paths | both luma tests fail; `S7d` PASSES, correctly — its matte is grey, and a grey cannot see a coefficient |
| luma taken on LINEARISED values, both paths | the two luma tests fail **on the grey plates only** (55 vs 128, off by 73); the three primary plates pass, which is the demonstration that primaries alone would have pinned nothing |
| luma NOT premultiplied (drop the `a` factor) | the transparent-white plate reads 255 vs 128; `S7d`'s luma pair fails too |
| `outside` ignored (always `kDstIn`) | `S7d` fails on both complements; the luma tests pass, correctly |
| `outside` dropped from the Coverage arm of `Gate::operator==` | the comparator test fails naming it — and NOTHING ELSE DOES, which is the whole reason it has a test: `outside` is not a new field, so no compile-time pin would have caught the new arm ignoring it |
| `channel` dropped from the same arm | the comparator test fails naming it |

**FIELD PIN.** `kFieldCount<Gate>` 7 → 8. Control: a `float feather` added
to `Gate` fails the build at `ComposeInternal.h:472` ("type 'Gate'
decomposes into 9 elements, but only 8 names were provided"); naming it in
the decomposition then fails the second gate at `Reconcile.cpp:384` ("Gate
gained or lost a field"). Two compiler errors, in order, as designed.

## 42. THE SLOT TABLE, and the shape-sketch link guard — two silent classes made loud (2026-07-29)

Both items are the same move made twice: take a mistake that compiles and
passes, and make it a build or test failure. §40 filed the first; the sketch
wave of f206364 filed the second.

### 42a. `Instance::Slot` had FOUR hand-written enumerations

`collectGroupScalars` and `computeVolatile` (Paint.cpp),
`applyMountTransitions` and `applyTransitions` (Transitions.cpp) each walked
the twelve slots by hand. §40 verified all four were complete and closed
with the hazard: nothing structural held them that way, and every absence is
silent —

| absent from | the symptom, and why nothing catches it |
| --- | --- |
| `applyTransitions` | `animate()` snaps instead of ramping; reads as a missing transition spec |
| `applyMountTransitions` | `animate(from().to())` plays no entrance; the node just appears settled |
| `computeVolatile` | the property is not volatility, so an ancestor caches across it and the motion FREEZES in a replayed picture — perfect in any still |
| `collectGroupScalars` | a `Cache::Group` holds its bake while the property moves, i.e. blits last second's pixels |

Not distant, either: `computeVolatile` is where §38's staleness bug lived —
the same function, one level up, four copies of a list, three of them
drifted.

**ONE TABLE SERVES ALL FOUR, and the reason is that the four wanted the same
two things.** `kSlotSpecs` (ComposeRuntime.h) is `kPopOpPso[]`'s shape: one
row per enum value, index-aligned, carrying

  - `of` — the description's `Animatable<float>` for the slot, null when the
    node does not carry the block that holds it, and
  - `role` — Opacity / Geometric / Content.

**The three roles are not a taxonomy invented for the table.** They are the
split `computeVolatile` ALREADY made: opacity is applied by paint()'s
saveLayer (a fading node replays its picture and does not move its device
rect), geometric by paint()'s matrix (so it refuses a device-pinned bake),
content rebuilds the recording. `collectGroupScalars` reads the same split
for its root exclusion; the two transition functions ignore it entirely. No
consumer wanted a fourth thing, which is what makes one table honest rather
than a lowest common denominator. `applyTransitions` needed exactly ONE
extra fact — the endpoint to ramp from when a node GAINS or LOSES the block
(a `travel()` path, kinetic text) — and that is `standing`, which is in
every case the field's own default and is pinned to it.

**WHY A TABLE HERE AND A SUBTRACTION IN §38.** §40 asked whether
`computeVolatile`'s cure (name the terms once, derive every consumer as a
subtraction) was the better model. It is the better model for THAT list and
not for this one. The content terms are a heterogeneous bag of booleans — a
bound fill, a GIF frame, a live effect — with no enum behind them, so the
only thing that can hold them together is a single named expression. These
twelve are an ENUMERATED AXIS, and an enumerated axis gets the
`variant_size_v` treatment, because the compiler can count it.

**THE ONE SLOT THAT DOES NOT FIT, said out loud.** `kFillLerp` is a
synthesized 0→1 progress over `paint.fill`'s `Transitioned<Fill>`; there is
no `Animatable<float>` in the description to point at. It carries a
`SlotRole::Bespoke` row with a written reason, `slotValueOf()` answers
nullptr for it so a consumer that forgets to special-case it is INERT rather
than crashing, and all four hand-written sites are labelled "the kFillLerp
row". The escape hatch cannot be taken blank: `role == Bespoke` ⇔ `of ==
nullptr` ⇔ `bespoke != nullptr` is a `static_assert`.

**THE HAZARD A TABLE INTRODUCES, and its pin.** Twelve separate call sites
could not MISAIM; a table can — a copy-pasted row returning the neighbouring
field would compile, and then `.scaleY(animate(…))` would ramp `scaleX` in
all four consumers at once. `ComposeSlotPins.EverySlotRowReachesItsOwnFieldAtItsStandingDefault`
walks the rows on a node carrying every block and demands each answer with a
DISTINCT address at its declared `standing` default.

**Behaviour-preserving, with one stated non-identity.**
`collectGroupScalars`'s vector now gathers in enum order, so its ELEMENT
ORDER changed (glyph progress moved ahead of the mask gates, scaleX/scaleY
ahead of skewX/skewY). The predicate is unchanged: the vector is only ever
compared against the vector this same function produced on the previous
frame (`groupScratch == inst.groupPrev`, Paint.cpp), so any fixed
permutation computes the identical verdict. Positional identity of that
vector is not achievable through a table at all, since the mask gates are
not slots and sit in the middle of it — which is why the property is stated
as stability rather than order.

**Ten positive controls, all fired** (mutate → the NAMED gate fails →
restore, mtime stamped):

| control | gate that failed |
| --- | --- |
| a 13th slot appended with no row | build: `std::size(kSlotSpecs) == Instance::kSlots` |
| a row's `slot` duplicated (kScaleY → kScaleX) | build: `slotTableWellFormed()` — index alignment |
| the Bespoke row's reason blanked to nullptr | build: `slotTableWellFormed()` |
| kScaleY's accessor aimed at `paint.scaleX` | `ComposeSlotPins.EverySlotRow…` (distinct addresses) |
| kMotionT's `standing` 0 → 1 | `ComposeSlotPins.EverySlotRow…` (the field default) |
| `applyTransitions` skips Geometric rows | `ComposeTransitions.RampsAndRetargetsFromCurrent` |
| `applyMountTransitions` skips the Opacity row | `ComposeMotion.AnimatePlaysEntranceOnMount` |
| kOpacity's role → Geometric | `ComposeCache.AGroupsOwnFadeDoesNotDropItsBake` |
| `collectGroupScalars` drops the root exclusion | `ComposeCache.AGroupsOwnFadeDoesNotDropItsBake` |
| `collectGroupScalars` stops gathering children's transforms | `ComposeCache.GroupDropsTheBakeOnTheFrameABindingTicks` |

### FOUND BY A CONTROL THAT DID NOT FIRE — glyph progress had no pin

The eleventh control was kGlyphProgress's role Content → Opacity, and it
PASSED against 66 tests of `ComposeKinetic`, `ComposeR4Mask` and
`ComposeCache`. Per the house rule, the pin is wrong and not the control:
**nothing in the repo checked that glyph progress is content volatility.**

Misclassify it and `computeVolatile` leaves `ownContent` false,
`subtreeVolatile` false, the picture un-reset — and a reveal FREEZES at
whatever progress the last describe happened to record. The reason the whole
kinetic family was blind is uniform: `StaggeredRiseRevealsInOrder` moves the
reveal by RE-DESCRIBING, which marks the node paint-dirty and never asks the
question; `TransitionedProgressPaintsLive` asserts only that some ink exists
after settling, which a frozen half-revealed recording satisfies exactly.
`ComposeKinetic.ABoundProgressRevealsWithoutARedescribe` closes it with a
BOUND Output — one describe, and the value moves underneath it. The control
then fired. This is §38's class at a second slot, and §40's audit had marked
this list "complete" with no test behind the word.

### 42b. A shape-using sketch could never hot-reload — now ctest 16 → 17

`SigilShape` was missing from the sketch host's `-force_load` list from the
day sketches could use it until f206364. Two registries of one fact: the
kit's PUBLIC dependencies decide what a sketch may `#include` (through
`sketch_flags.rsp`), the `-force_load` list decides what a sketch may LINK,
and nothing compared them. So `shapeworks_lab`, `easel_playground` and
`pop_lanes` all compiled, all ran as compiled-in gallery scenes — an object
library resolves nothing dynamically — and all three failed at `dlopen`.

Neither existing guard could see it: `compose_sketch_smoke` (hello.cpp) and
`compose_sketch_stock` (stock_materials.cpp) name no `shape::` symbol, so
both load happily with the archive absent. **Measured, not argued:** with
`SigilShape` removed again, both PASSED while the new test failed.

`compose_sketch_shape` runs `ComposeSketch sketches/shapeworks_lab.cpp
--frame …`, which is the real dynamic path — compile with sketch_flags.rsp,
`dlopen`, run, nonzero exit on a load failure. shapeworks_lab is the widest
shape surface of the three (Blend, Materials, Mesh, Space), so it also
stands a chance against a PARTIAL regression. 2.8 s Debug, 1.6 s Release.

**Positive control, fired**, with the actual message:

    sketch failed to build:
    dlopen(…/sketch_1.dylib, 0x0006): symbol not found in flat namespace
    '__ZN5sigil5shape4mesh5torusEffii'

### Suites and ledger

`compose_test` **506** (505 passed, 1 skipped — the expected
`AdvanceVariantAxisIsRefused`; +2: the slot walk and the glyph-progress
pin), `compose_kit_test` 47, `motion_test` 15, `shape_test` 83,
`world_test` 64, `ctest` **17/17** — both configs.

**Ledger: byte-neutral, as a behaviour-preserving refactor must be.**
55 byte-identical, 3 moved, 6 not in baseline, 0 failed. The three movers
are all attributed: `easel_playground` at the documented `39528e682c55`,
plus `genesis_fire` and `slitscan_2001` on the self-nondeterministic list
(`hitman_verlet` happened to land identical this run). The six are
`travel_path`, `wiggle_shake`, `env_theme`, `material_child`, `matte_luma`
and `pop_lanes` — the sketches of f206364, still unadopted; adopting them is
the owner's call. *(The call is made — noted 2026-08-03: the plate-baseline
rebase of that date adopted all seven study scenes — these six plus §19's
`blur_falloff` — into the 65-scene baseline
(`build/plate_baseline_<config>.sha256`), so the adoption ruling is taken
de facto and "not in baseline" no longer appears for any of them.)*

## 43. TIME REMAPPING and MOTION BLUR — the obstacle is one word wrong, and the corrected reading splits the wave clean (2026-07-29)

Two After Effects fundamentals were filed as one wave because both need
one primitive: **evaluating a property at a time other than "now."**
Time remapping retimes a layer's own timeline (freeze, stretch, reverse,
hold); motion blur accumulates a property ACROSS a frame interval instead
of sampling it at one instant.

The wave was filed with an obstacle attached: *compose's animation is
resolved from `choreograph::Output<float>`, an Output is a mutable cell
stepped by a Ticker and not a function of time, so the naive form of both
features is unavailable.*

**That obstacle is one word wrong, and correcting the word is most of the
design.** What follows is the corrected reading, the boundary it draws,
one feature designed, one feature REFUSED with an argument, and one
feature found already shipped.

### 43.0 THE OBSTACLE, CORRECTED — an Output is a cell WITH AN OPTIONAL FUNCTION BEHIND IT

`choreograph::Output<T>` is a cell. It is also, when a Motion drives it, a
**handle on that Motion**, and the chain from the cell to a pure function
of time is four public calls that this repository has never made:

```cpp
choreograph::Motion<float> *m = out.inputPtr();       // Output.hpp:104
if (m) {                                              //   null == unconnected
  const float now   = m->getSequence().getValue(m->time());
  const float later = m->getSequence().getValue(m->time() + 0.008f);
}
```

`Sequence<T>` is a `Phrase<T>` and `Phrase::getValue(Time)` is exactly the
`f(t)` the wave was told did not exist (`Phrase.hpp:74`). It is **total**:
`Sequence::getValue` returns `_initial_value` below 0 and `getEndValue()`
past the duration (`Sequence.hpp:265-287`), so an out-of-shutter sample
clamps rather than extrapolating or reading garbage. `TimelineItem::time()`
is the playhead, `step(dt)` is literally `_time += dt * _speed`
(`TimelineItem.cpp:65`), and **`setPlaybackSpeed()` is public** — so a
per-Motion time stretch, including a negative one, is already a one-call
operation. `Timeline` itself derives from `TimelineItem`, so a NESTED
timeline is a retiming unit with its own speed and its own `setTime`.

`choreograph/phrase/Retime.hpp` ships `LoopPhrase`, `PingPongPhrase`,
`ReversePhrase`, `ClipPhrase` and `SquashPhrase`, and `Choreograph.h:35`
includes it in every translation unit that touches animation.

**Grep result, and it is the headline: `inputPtr`, `getSequence`,
`setPlaybackSpeed`, `jumpTo` and all five Retime phrases have ZERO uses in
`src/`.** The entire evaluate-at-arbitrary-time substrate is present,
compiled, and untouched. This is the third entry this session (after §7 and
§10h) where the thing filed as unavailable was sitting in a header.

So the obstacle is not "an Output is a cell". It is:

> **An Output is evaluable at an arbitrary time exactly when a Motion drives
> it, and `isConnected()` is the discriminant.** A cell written by a
> steppable — `ticker.add([&]{ phase = f(elapsed); })` — has no function
> behind it, because the author's `f` is a lambda the library never sees.
> That half is genuinely unreachable, and no design fixes it.

The correction matters because it moves the boundary. It is NOT
`Transitioned` on one side and `bind()` on the other, as filed. Every
compose-manufactured transition is Motion-backed **because `Transitions.cpp`
builds the Motions itself** (`impl.ticker.timeline().apply(&anim->value)`,
five sites) — so `animate()` in all three forms is exactly evaluable, and
so is `bind(&out)` when the author happened to spell their phase through
`timeline().apply()`. And the corpus says they almost never do: **34 of 44
sketches drive motion through `ticker.add`, not through the timeline.** The
boundary is real, it is wide, and it falls on the wrong side of the demand.

### 43.1 THE TWO TIME CHANNELS, and they have nothing in common

The second correction. AE has one clock. **Compose has two, they are
plumbed differently, and they cost completely different amounts to retime.**
Nothing in DESIGN.md said so before this entry.

| | **the MOTION channel** | **the ELAPSED channel** |
| --- | --- | --- |
| carrier | `Ticker::timeline()` steps Motions, which write cells | `PaintContext::elapsedSeconds`, one double pulled per paint from `Composer::Impl::elapsed()` (`ComposeRuntime.h:706`) |
| cadence | once per `tick()`, before any steppable (`Ticker.cpp:62-63`) | once per node per `paint()` |
| who reads it | `resolveFloat` → every slot, span, gate | `Material` `uTime`/`quantizeTime` (`Material.cpp:203,286`), `custom()` programs, decorations, `imageAssetOf(node)->frameAt(elapsed()*1000)` (`Paint.cpp:1949`) |
| retiming cost | a kernel change: per-subtree sub-timelines carried through reconcile | **push a different double.** Zero |

The elapsed channel is already parameterised — `Material::build(live, ctx)`
takes the context, `paintCtx` is constructed per node (`Paint.cpp:1447`) and
already SHADOWED once for a different purpose (`metricCtx.size = {1,1}`,
`Paint.cpp:1896`). Retiming it is a field assignment on a struct compose
copies four times a frame anyway. Retiming the Motion channel is not.

That asymmetry is why this wave splits, and why "retime a subtree" is the
wrong ask: the two halves of a subtree's motion live in two different
machines.

### 43.2 THE BOUNDARY — per authoring spelling, three tiers

**Tier A — evaluable at an arbitrary time, exactly, by the library.**

| spelling | why |
| --- | --- |
| `animate(to(v), spec)` | `Transitions.cpp:103` builds the Motion; `getValue(t)` is exact |
| `animate(from(a).to(b))` | same, plus the entrance `Hold` in the same Sequence |
| `animate(through({…}))` | same; a multi-segment Sequence evaluates as one Phrase |
| the `kFillLerp` colour lerp | a synthesized 0→1 Motion, `Transitions.cpp:264` |
| `.stroke(spans::upTo(animate(…)))` | `spanAnims` are ordinary Motions |
| `.mask(by::spans(animate(…)))`, `by::edge(_, animate(…))` | `maskAnims`, same |
| a plain constant | constant in t. Trivially |
| `Material` `uTime`, `quantizeTime(hz)` | pure function of `ctx.elapsedSeconds` |
| `custom()` / decorations reading `ctx.elapsedSeconds` | same, **if** they read nothing else live |
| an animated `image()` leaf | `frameAt(elapsed()*1000)` — pure |
| `travel({.path, .t})` | the geometry is a pure function of `t`; the lane inherits its tier from `t`. §39's schedule/space split pays off here |
| `glyphFx` + `Stagger` | `Paint.cpp:990` is `(master*total - order*each)/duration` — pure in the master progress |
| `wiggle(&out, …)` / `.wiggle()` | pure in the normalised input by the 2026-07-29 ruling. **Inherits, does not confer** |

**Tier B — evaluable iff the author's Output happens to be Motion-backed.**
`bind(&out)…`, a bare `&out`, `travel({.t = bind(&out)})`, `wiggle(&out,…)`,
`Effect::uniform(name, &out)`, `Hatch::spacingBinding`,
`spans::range(&a, &b)`. Runtime-checkable in one call. **In practice this
tier is mostly empty**, per the 34-of-44 count.

**Tier C — not evaluable at all, ever.**
An Output assigned by a steppable. `instancing::Pool` in `Mode::Live` (the
author's SoA, re-read per frame). A `custom()` program that captures an
`Output*` and reads it. Anything the author recomputes outside the library's
sight. **`BoundFloat::apply()` is pure, but the cell it reads is not** —
which is exactly the manager's point 3, and it is right.

The load-bearing consequence: **a single node routinely spans all three
tiers.** A card with `animate()` opacity (A), a bound trim (B or C) and a
live `Material` (A) has no single answer. Any feature that retimes "a node"
must therefore say what happens to the tiers it cannot reach — §43.4.

### 43.3 Q1 — THE RETIMING UNIT IS A SCHEDULE, NOT A SUBTREE, AND NOT A NODE

**`env::` is the wrong door, and it is worth writing down because it is the
obvious-looking one.** `env::Provide` resolves during DESCRIBE, before the
Composer sees the tree — that is precisely what makes it free (§10g: the
element tree is environment-independent, `propsEqual` is already the
dependency tracker). A time warp resolved at describe time can only bake a
NUMBER into props. But retiming must change how a running Motion ADVANCES,
which happens in `tick()`, on a different cadence from describe and
independent of it. And the only possible reader of an env-borne time warp
would be the kernel — which would destroy the one property §10g bought.
**Refused: `env::` propagates values down a describe stack; it cannot
propagate a clock.**

A per-subtree sub-timeline carried through reconcile *would* work, and it
has a precedent: `mountDelayCarryMs` (`ComposeRuntime.h:654`,
`Reconcile.cpp:813-836`) is ALREADY a reconcile-time, dynamically scoped,
save/restore time value carried down the tree — `staggerChildren()` is a
subtree time offset in everything but name. So the mechanism is proven.
**It is still the wrong unit**, for two reasons:

1. It reaches Tier A's Motion half and nothing else. The same subtree's
   materials, `custom()` programs and GIFs run off the elapsed channel and
   would keep real time — a subtree in slow motion with its own textures
   at 1×. That is §41's silent-freeze failure with a different surface.
2. **The schedule is already a first-class shareable value in this library,
   and AE's is not.** AE retimes a layer because a layer is its only unit
   (the same shape of argument §41 used to refuse the element matte). Here,
   one phase Output drives eleven beats (`minard_1869.cpp:2921`) across
   dozens of nodes. Retiming the tree would retime the wrong thing.

**So the unit is the SCHEDULE — an Output.** This is the same ruling §39
made for the motion path six entries ago and DESIGN.md already states:
*the lane is `t`, the position ALONG the curve, so the whole `bind()` chain
applies to the SCHEDULE rather than to the geometry.* Compose separates
schedule from geometry (§39) and schedule from value (§1). Time remapping
is a transform of the schedule. It belongs on the schedule.

#### `derive()` — the design, and it is what the corpus has been asking for

A survey of all 44 sketches and 25 gallery scenes for hand-rolled retiming
found the demand is not "retime a subtree" at all. Ranked:

1. **A derived Output — `Output = f(Output)` that is ITSELF an
   `Output<float>*`.** ~30 sketches work around its absence, and two
   independent authors named it in near-identical words:
   `vertigo_titles.cpp:105-108` (*"No derived Outputs. `penTip` must be a
   second, independently-owned Output the ticker re-copies from
   `growth − 0.008` every tick… Four cards × two shadow cells = eight
   scalars kept in sync by hand"*), and `ScenesSkillTree.h:223-224`
   (`pulseS = -0.12f + u * 1.12f; pulseE = pulseS + 0.12f;`).
   `chladni_tab1.cpp:826` calls it *"the study's main gap"* over ~5,900
   hand-stepped local timelines.
2. Time quantization, `floor(t*N)/N` — 8 sites, and it is a PERFORMANCE
   feature, not a look (`eva_magi_interior.cpp:1816-1820` documents
   11.16 ms → a blit; `ScenesAero.h:235`).
3. A HOLD segment in a phase — 7 sites, each an inlined piecewise, and
   `ScenesVeloren.h:311-315` documents a bug caused by getting one wrong.
4. A per-subtree clock offset — 8+ sites, almost all spelled
   `ctx.elapsedSeconds + phase` (`twoadvanced_v4.cpp:360`).
5. Loop/ping-pong/reverse — `bg3_dice_roll.cpp:1383`,
   `xcom_battlescape.cpp:1708`, `ScenesFlourish.h:542`.

**Items 1, 2, 3 and 5 are one feature, and its entire vocabulary is ALREADY
IMPLEMENTED.** `BoundFloat` has `source`/`window` (= hold, it clamps),
`map`, `quantize` (= posterize time), `scale`/`offset`/`target`,
`invert` (= reverse), `wiggle`, `clamp`. What it cannot do is *materialise
the result into an Output*, so the chain reaches an `Animatable<float>`
property and nothing else — and every hit above is a case where the retimed
phase must be a real Output, because the consumer is an `Output*`-typed API:
a `Pool` write, `spans::range(&a, &b)`, a decoration field,
`Effect::uniform`, `Hatch::spacingBinding`.

§1 closed the half where a shaped chain reaches a PROPERTY. The open half is
the same chain reaching an OUTPUT:

```cpp
// SigilMotion. One cell the library owns, one steppable, zero new math.
choreograph::Output<float> phase, penTip, stepped, backwards;
ticker.add([&](double){ phase = ...; return true; });

ticker.derive(&penTip,    derive(&phase).offset(-0.008f).clamp(0, 1));
ticker.derive(&stepped,   derive(&phase).quantize(8));      // posterize time
ticker.derive(&backwards, derive(&phase).invert());         // reverse
ticker.derive(&held,      derive(&phase).window(0.0f, 0.7f)); // hold at 0.7
```

`derive(&src)` returns a `Bound` — the SAME builder, the same
`BoundFloat::apply`, verbatim. `Ticker::derive(dst, chain)` owns the write.
Cost per derived value per frame: one pointer read and `apply()`, which is
the cost of the property-side chain that already runs on every bound paint.

**THE ONE UNSETTLED QUESTION, which is why this entry designs it rather
than shipping it: STEP ORDER.** `Ticker::tick` steps the timeline, then
steppables in REGISTRATION ORDER (`Ticker.cpp:62-68`). A derivation
registered before its source reads a one-frame-stale value — silently, and
visibly only as a one-frame lag in a shadow that nobody will attribute to
registration order. Three candidate rulings:

- **(a) two-phase step: sources, then derivations.** Correct, ~10 lines
  (a second vector), and it makes the contract statable: *a derivation
  never reads a stale source.* Cost: derivations may not depend on
  derivations without a topological order, so the ruling must be either
  "one level only, enforced" or "declaration order within phase two,
  documented". **Preferred, and it needs its own pins.**
- (b) document the hazard and require the author to register in order.
  Rejected: this is a silent-wrong-answer class, and §40/§42's standing
  lesson is to make those loud.
- (c) evaluate lazily on read. Impossible — `Output::value()` is not
  virtual and the whole point is to satisfy `Output*`-typed consumers.

**THE HONEST LIMIT, and it must be stated at the call site.** `derive()`
remaps the schedule's VALUE. That equals a time remap **exactly when the
schedule is affine in time.** `phase = k·t` gives `0.5·phase(t) =
phase(t/2)`; a non-linear phase gives a value remap that is not a retime.
Compose does not retime time; it remaps schedules, and the corpus's
schedules are overwhelmingly `t*k` or `fmod(t*k, 1)` — which is why this
serves the demand and why the equivalence has to be written down rather
than glossed.

**ONE MISSING STAGE.** `BoundFloat` has no wrap. Six-plus sketches spell
`std::fmod(t*k, 1.0)` by hand. `Bound::wrap(period)` completes the
vocabulary and is four lines; it is also the only new math in the whole
item.

**WHAT `derive()` DOES NOT DO, named so nobody expects it to.** It cannot
retime a mount entrance (compose owns that Motion, the author has no
handle), a `Material`'s `uTime`, a `custom()` program's `elapsedSeconds`, or
a GIF's frame. Those are the elapsed channel — §43.6 item 2.

### 43.4 Q3 — THE FAILURE MODE, and why `derive()` has none to hide

§41 refused the element matte because `snapshot()` runs no transitions, so
an animated matte would be **silently frozen**. That is the standard this
wave is held to.

`derive()` clears it structurally rather than by defending a rule: it
consumes an Output and produces an Output. There is no "property that cannot
be retimed" case, because there is no property involved — the author points
it at a schedule they own, and a schedule they own is by construction
readable. **The failure mode is not silent, it is not possible.** That is
the strongest argument for the schedule-as-unit ruling and it is worth more
than the line count it saves.

Every OTHER shape considered in this wave fails the test, and here is the
table, because the failures are the reason for the verdicts:

| shape | failure when it meets a tier it cannot reach |
| --- | --- |
| subtree retime via sub-timelines | the subtree's materials/`custom()`/GIFs keep real time. Half-retimed, plausible-looking, silent |
| subtree retime via `env::` | applies only on frames that describe. Intermittent, silent |
| motion blur, transform-only | content stays at one instant while position smears. Plausible-looking, silent |
| motion blur, N re-records | correct, and §43.5's cost |
| element matte (§41, for reference) | frozen matte. Silent |

**If a subtree retime is ever built, the failure mode is a REFUSAL, not a
hold.** The precedent is `Material::quantizeTime` (`Material.cpp:629`),
which refuses unless the effect declares `uniform float uTime`, and
`VariationDrive`, which refuses `wght` with a warning. A `timeWarp()` node
would have to walk its subtree at reconcile, and log once per instance —
in the shape `computeVolatile` already uses for a `Cache::Group` refusal
(`Paint.cpp:716-733`) — naming every property it could not reach. A feature
whose diagnostic is longer than its implementation is a feature that should
not exist, which is itself an argument against building it.

### 43.5 Q2 — MOTION BLUR'S SAMPLING, and `echo()` is a red herring

**`echo()` is not a foundation.** It re-issues RAW DRAW OPS — `drawRRect`
or `drawPath` for the surface, `textLayout.drawBatched` for text — once per
`Echo`, wrapped in `save()/translate()/restore()` with a flat colour
override (`Paint.cpp:1757-1770`, `1823-1835`). It is translate-only (no
matrix), flat-coloured (it ignores the resolved fill entirely), it skips
`glyphFx`, image and custom leaves by contract, its offset and colour are
plain non-`Animatable` values, and **nothing is re-resolved per copy** —
`resolvedFill` and `inst.paragraph` are computed once above the loop. There
is no `t` in scope and no resolution point to put one. Also, per its own
header at `Compose.h:2393`, a study already spelled 117 full paragraph
re-draws through it. Every one of its three uses in the corpus is a static
misprint stamp. **Red herring, confirmed.**

The real precedent is `instancing::` (`Instances.h:535`), which genuinely
replays one baked drawing N times with per-copy transforms through
`drawSpriteAtlas`, and whose `Mode::Live` re-reads the pool every frame.

The sampling design splits on ONE question — **does anything but the
transform vary across the shutter?**

- **Transform-only:** record the node ONCE, replay the picture at N
  matrices under a `saveLayer`. Sampling: N at offsets `(i+0.5)/N − 0.5`
  scaled by `shutter/360 × frameInterval`, each at `1/N` alpha. Cheap.
- **Content-varying** (a trim phase, a gate, a material's `uTime`, glyph
  progress): N RECORDINGS. `computeVolatile`'s `scalarContent` /
  `opaqueToTheMemo` terms name exactly which properties force this.

And a node's tiers decide which branch it gets — which means **the same
authored `.motionBlur()` is cheap or 16× depending on properties the author
did not mention at that call site.** That alone is close to
disqualifying: §27's rule is that a default encoding a judgement about the
caller's art cannot be changed compatibly, and this is worse — a cost model
that inverts on an unrelated property.

**AND THE ACCUMULATION IS ARITHMETICALLY WRONG IN COMPOSE'S OWN COLOUR
SPACE.** §41 established this session, and wrote into DESIGN.md, that every
surface compose paints into is `N32Premul` with a NULL `SkColorSpace` and
that **compose has no linear stage.** A shutter integral is a sum of LIGHT;
light adds linearly. Averaging N encoded sRGB samples is the same class of
error as weighting encoded values with Rec. 709 coefficients — the mistake
§41 spent a five-plate pixel pin ruling out six days' worth of confusion
about. A moving white bar over black would come out visibly too bright, and
at 8 bits `1/N` alpha for N=16 is 16/255, so the quantisation error is
~6% per sample before the gamma error is counted. **A correct accumulation
buffer requires a linear float surface, which §41 already declared "a
breaking change, not a configuration."**

### 43.6 Q4 — PRUNING AND CACHING, with the estimates labelled

This project's rule is that unmeasured performance claims are worthless, so:
**everything in this section is an ESTIMATE derived from measurements
already in this file. Nothing here was measured by this wave, which
measured nothing, because it built nothing.**

**`derive()`: no interaction whatsoever, and that is the point.** A derived
Output is indistinguishable from a hand-stepped one. `computeVolatile` sees
`v.binding()` and behaves exactly as today; `collectGroupScalars` pushes it
exactly as today; §20's settle machinery sees a float that either moved or
did not. **The estimate is zero delta**, because the mechanism adds no new
kind of thing to the kernel — it adds a second writer of a kind the kernel
already handles. The corollary is honest and unflattering: `derive()` does
not IMPROVE caching either. An author who replaces `vertigo_titles`' 14
hand-stepped scalars with 14 derived ones pays §38's bound-value penalty
identically. `derive()` buys correctness and line count, not frames.

**A subtree retime: two costs, one of them structural.** (1) The subtree
describes and steps against a different clock, so `mountDelayCarryMs`'s
save/restore shape needs a sibling and every `applyTransitions` site needs
to know which timeline to `apply()` into. (2) The structural one: a retimed
subtree's motions never settle in step with anything else, and §20's
release (`kScalarSettleFrames = 8`) is measured in FRAMES, not in scaled
time — so a subtree at 0.1× speed holds still for 10× as many frames and
releases 10× sooner in its own terms. Estimated: the release threshold
becomes a function of the subtree's time scale, which is a change to §20's
contract, not a use of it.

**Motion blur: N× the blurred subtree's paint, PLUS the ancestor tax.** A
motion-blurred node is by definition `transformLive` or content-volatile,
so `computeVolatile` sets `ownPaint`, carries it up, and every ancestor is
`subtreeVolatile` — which §38 MEASURED, in this file, as
**Promotion::Volatile refused and 5.02 ms/frame lost on a 515-node panel
(19.6×) for one property that was not even moving.** Motion blur makes that
permanent by construction and then multiplies the blurred node's own cost
by N. Estimating from §38's ~11 µs per stroked cell replay: N=16 on a
modest 20-node blurred subtree is ~3.5 ms/frame of replay for the blur
alone, on top of the ~5 ms the ancestor refusal costs. **Both figures are
extrapolations from §38's arms, not measurements of a motion blur that does
not exist** — but the direction is not in doubt, and the ancestor half is
measured.

### 43.7 Q5 — IS MOTION BLUR COMPOSE'S JOB? NO, AND THE CORPUS SAYS SO LOUDEST

Weighed: a real accumulation buffer, versus a directional blur driven by a
velocity computed from the same curve.

The velocity IS cheaply available for Tier A —
`(getValue(t+h) − getValue(t−h)) / 2h`, two Sequence evaluations, no
re-record, no re-draw — and a directional blur is expressible in the
EXISTING `Effect` surface today: `SkImageFilters::MatrixTransform(rotate)`
→ `Blur(sigma, 0)` → rotate back, three nodes in a chain `Effect::then()`
already composes, with sigma bound through `Effect::uniform` (§11, shipped).
Nothing needs to be built for an author to do this.

**And no sketch does it. That is the finding.** Across 44 sketches and 25
scenes: nineteen `Blur(...)` call sites, every one a literal constant; four
with `sigmaX != sigmaY`; **zero with a phase- or velocity-driven sigma; zero
trail buffers; zero accumulation afterimages; and `echo()` never once used
for motion.** The primitive that would express a smeared multi-copy blur
exists and nobody reached for it that way.

What the two studies that genuinely needed motion blur actually did:

- **`genesis_fire.cpp`** — Reeves 1983, whose §3 IS particle motion blur.
  It drew a STREAK: a quad from `pos(f)` to `pos(f + 1/2)` built from
  `p.vel` in `drawVertices`, eight vertices with transparent edge colours
  so the falloff *is* the vertex colour (`:512-580`). Geometry, not a
  filter. The paper's own footnote 4, quoted in the sketch header, says the
  straight-line approximation *"has so far proved sufficient."*
- **`slitscan_2001.cpp`** — 406 additive stamps per wall across a
  discretised exposure integral, with the artwork drifting DURING the
  exposure (`:592-657`). The time integral was the SUBJECT of the study.
  A library `motionBlur()` verb would have ruined it.

So the demand, such as it is, reads *"give me the primitive so I can build
the effect"* — and the primitives (`drawVertices`, `kPlus`, `Effect`
chains, and now `getValue(t±h)`) are all present.

**The AE argument, which is the same shape as §41 ruling 3 item 5.** AE
needs motion blur because AE composites LAYERS OF PIXELS sampled at frame
boundaries; motion blur repairs a sampling artefact of AE's own
architecture. Compose draws vectors at the frame time. There is no
inter-frame sampling artefact to repair except for genuinely fast motion,
and the corpus's genuinely fast motion is drawn as geometry — which is
cheaper, more correct, and more expressive than any shutter integral over
the same content.

**VERDICT: motion blur is REFUSED.** Five reasons, descending:

1. **Zero demand for the EFFECT.** Two studies wanted primitives and had
   them; the cheap approximation is already expressible and unused.
2. **The accumulation is wrong in compose's colour space** (§43.5), and
   fixing it means a linear float surface, which §41 already called a
   breaking change rather than a configuration.
3. **The cost inverts on properties the call site does not mention**
   (transform-only vs content-varying), and it makes the ancestor
   volatility §38 measured at 19.6× permanent.
4. **The Tier B/C half fails silently and plausibly** — a node whose
   position smears while its content sits at one instant is worse than
   §41's frozen matte, because a freeze looks like a bug and a
   half-blur looks like a render.
5. **Compose separates paint from silhouette from schedule; AE cannot.**
   The 1:1 translation of most motion-blur uses in this idiom is *draw the
   streak* — and `genesis_fire` is the worked proof, in the corpus, of the
   reference that invented the technique.

**WHAT WOULD REOPEN IT:** a linear float compositing surface (a colour-space
decision, not this feature's), plus a describe-keyed (content-identity)
bake cache — the mechanism beyond §16's closed pointer-keyed StampCache
(see §16's 2026-07-30 reconciliation). The
reopening condition is the SAME PAIR §41 named for the element matte, which
is itself evidence that both refusals are the same refusal wearing two
faces.

**FILED SEPARATELY, NOT AS MOTION BLUR:** `Effect::directionalBlur(sigma,
angle)` over the rotate/blur/rotate sandwich. Four sites hand-build
anisotropic `Blur` and `lain_navi.cpp:1156-1177` explicitly WANTED a
directional blur and faked it with five gradient ramps because an animated
one cost nine `saveLayer`s. That is a real spatial-filter convenience with
measured demand. It is not motion blur, and smuggling it in under this
heading would be dishonest about what it does.

**SHIPPED 2026-07-30** as `Effect::directionalBlur(sigma, angleDeg,
across = 0)` — `sigma` along the axis, `across` perpendicular (the four
sites were anisotropic, not pure streaks, so the honest signature has
both). **The reuse ruling held: no new SkSL.** At an axis-aligned angle
the factory emits `SkImageFilters::Blur(x, y)` — the exact call the
sites wrote, proven bit-identical by pin
(`ComposeEffects.ADirectionalBlurAtAnAxisAngleIsBlurBitwise`,
whole-plate pixel compare, with a swapped-sigma control that fails the
compare) — and any other angle is this entry's own sandwich, three
existing DAG nodes (`ADirectionalBlurAtAnArbitraryAngleSmearsAlongIt`
pins the 45° streak with a cross-axis control). Beyond the name, the
factory carries a comparable RECIPE ({sigma, angle, across} + a field
pin, Effect's now at 7): a re-described equal one PRUNES where the
hand-built `filter()` re-patched on pointer inequality every describe
(`AStaticDirectionalBlurPrunesByRecipe`), and the named parameters ride
the EXISTING §11 uniform channel — `.uniform("angle", &out)` rebuilds
the sandwich per paint, no re-describe, volatility declared
(`ABoundDirectionalBlurAngleAnimatesWithoutRedescribe`); an unknown
name warns-and-ignores, Material's guardrail
(`AnUnknownDirectionalBlurUniformIsIgnoredNotLive`). Per-site
dispositions, targeted ledger byte-identical on all three touched
scenes:

- `twoadvanced_v4.cpp` water reflection `Blur(14, 26)` →
  `directionalBlur(26, 90, 14)` — PORTED, bit-identical.
- `twoadvanced_v4.cpp` specular column `Blur(10, 3)` →
  `directionalBlur(10, 0, 3)` — PORTED, bit-identical.
- `ds2_bench.cpp` strut `Blur(12, 18)` → `directionalBlur(18, 90, 12)`
  — PORTED, bit-identical.
- `ds2_bench.cpp` ceiling band `Blur(7, 10)` →
  `directionalBlur(10, 90, 7)` — PORTED, bit-identical.
- `lain_navi.cpp:1156` magenta streaks — **KEPT as authored.** A real
  blur is a different picture than five hand-shaped gradient ramps; the
  port would have moved pixels, so per the quantize-wave discipline it
  was not normalised. The sketch now carries a comment naming the
  animated spelling a new streak would use.

*Fallout, found and fixed 2026-07-30 (the §16 reconciliation tripped
it):* API.md's prose for this shipment spelled `SkImageFilters::Blur` —
the FIRST doc spelling of a member of Skia's static-factory classes —
and the doc-probe generator (`test/docs/api_doc_probes.py`) had
`SkImageFilters` classified in NS_EXTERNAL as a NAMESPACE, so it emitted
`using SkImageFilters::Blur;` at namespace scope, which is ill-formed
for a class member: `compose_test` could not COMPILE on any rebuild
after c358a48 (latent until the next regeneration; the `requires`
member-probe forms cannot see an overloaded static either). Fixed by
classification, not exclusion: a new `EXTERNAL_CLASSES` table
(SkImageFilters/SkColorFilters/SkGradientShader/SkFontMgr — all
`class SK_API X { static … }`, all previously mis-set as namespaces)
routes these to a DERIVED-CLASS using-declaration probe
(`struct Probe : SkImageFilters { using SkImageFilters::Blur; }`) — the
same one-spelling-fits-all rule the namespace probe follows, one scope
over, overload sets included — with the owning Skia header included
on demand. The name stays probed; headers still win.

### 43.8 FREEZE FRAME IS ALREADY SHIPPED, and §41 is the reason

The brief lists freeze frame first among the retiming asks. It exists, in
two spellings, and neither is new:

- **`Cache::Texture`** bakes a subtree to pixels and blits it. A frozen
  subtree is a subtree whose bake never invalidates.
- **`snapshot()`** — and §41's REFUSAL is the proof: *"`snapshot()` samples
  bindings at current values and runs no transitions — there is no live
  timeline in a one-shot render."* For an element matte that is a fatal
  silent freeze. **For a freeze frame it is the entire feature.**

```cpp
sk_sp<SkPicture> frozen = snapshot(subtree, fonts);   // the freeze
// …then draw it, or Material::image() it, at any later time.
```

The same sentence is a defect in one entry and a specification in another,
which is worth recording as a lesson in its own right: an architectural
property is not good or bad, it is good or bad FOR something.

What does NOT exist is a freeze that keeps the subtree LIVE in the tree
while holding its clock — and per §43.3 that is the subtree-retime ask,
refused above. The honest phrasing for the author: **you can freeze the
PIXELS today; you cannot freeze the CLOCK.**

### 43.9 VERDICTS

| feature | verdict |
| --- | --- |
| **Time remapping, as `derive()` on a SCHEDULE** | **YES.** The #1 measured gap in the corpus (~30 sketches, named by two independent authors), the whole vocabulary already exists in `BoundFloat`, no kernel change, no cache interaction, and a failure mode that is structurally impossible rather than defended. One unsettled ruling (step order) and one new stage (`wrap`) |
| Time remapping, as a subtree time warp | **NO.** `env::` is the wrong cadence; sub-timelines reach one of two time channels and half-retime the other, silently. Refused with the diagnostic-longer-than-the-feature argument |
| Freeze frame | **ALREADY SHIPPED** — `snapshot()` / `Cache::Texture`. §41's refusal reason is this feature's specification |
| Time stretch of a compose transition | **ALREADY AVAILABLE**, unused: `TimelineItem::setPlaybackSpeed()`, including negative. Needs a handle, not a mechanism |
| **Motion blur** | **NO.** Five reasons, §43.7. The corpus wanted primitives, has them, and drew streaks |
| `Effect::directionalBlur(sigma, angle)` | **FILED separately** — and **SHIPPED 2026-07-30** (see the filing above): existing filters only, four sites ported bit-identically, the faked fifth kept as authored. A spatial filter, not motion blur |

### 43.10 REJECTED ALTERNATIVES, collected

| alternative | reason |
| --- | --- |
| `env::Provide<Clock>` for subtree retiming | describe cadence ≠ tick cadence; the kernel would have to read the environment, destroying §10g's one property |
| per-subtree sub-timelines carried like `mountDelayCarryMs` | reaches the Motion channel only; the elapsed channel keeps real time. Silent half-retime |
| retime only `Transitioned` properties | too small to author against, and it is the wrong axis: §43.0 shows the discriminant is `isConnected()`, not the `Animatable` kind |
| a derived Output evaluated lazily on read | `Output::value()` is not virtual, and the whole demand is `Output*`-typed consumers |
| derived Outputs stepped in registration order, hazard documented | a silent one-frame lag; §40/§42's standing lesson is to make that class loud |
| `echo()` as the motion-blur foundation | translate-only, flat-coloured, nothing re-resolved per copy, skips glyph/image/custom leaves. Red herring |
| accumulation buffer for motion blur | integrates encoded sRGB; contradicts §41's own colour ruling. Needs a linear float surface |
| transform-only motion blur (cheap branch) | content sits at one instant while position smears. Plausible-looking silent wrong answer |
| `motionBlur()` with automatic cheap/expensive branching | the cost inverts on a property the call site does not mention |
| a `Timeline`/cue DSL to express retiming | already refused in `Studio.h:44` — 168 `animate()` sites, and a cue value saves zero lines |

### 43.11 SCOPED IMPLEMENTATION PLAN — `derive()`, one sitting, SigilMotion only

Nothing in this plan touches SigilCompose. That is the plan's main virtue.

1. **`Bound::wrap(float period)`** — `<sigilmotion/Animation.h>`. A stage
   after the affine chain, before `wiggle` (so a wrapped phase wiggles
   continuously across the seam) and before `clamp`. Four lines plus the
   field. Ruling to state: `wrap` at `period == 0` is a no-op, not a
   division.
2. **`derive(const choreograph::Output<float>*) -> Bound`** — a second
   factory over the existing builder. Zero new math; it exists so the call
   site reads as a derivation rather than as a property binding.
3. **`Ticker::derive(choreograph::Output<float> *dst, Bound chain)`** — the
   write, plus the two-phase step (§43.3 ruling (a)): a second steppable
   vector, stepped after the first, with the one-level rule ENFORCED — a
   `dst` that is also some registered chain's source is refused with a
   warning, in the shape `Material::quantizeTime` refuses.
4. **Pins** (`motion_test`, currently 15): `wrap` at the seam and at
   `period == 0`; a derivation reads its source's SAME-FRAME value (the
   pin that fails under registration order — write it so the source is
   registered SECOND, which is the failing arrangement, and confirm the
   two-phase step fixes it); a derivation of a derivation is refused and
   warns; `quantize`/`invert`/`window` reproduce three named corpus
   idioms bit-exactly against their hand-rolled originals.
5. **Docs.** The affine-equivalence limit (§43.3) goes in the `derive()`
   doc comment, not only here — an author reading the verb must learn that
   it retimes a LINEAR schedule and remaps any other. DESIGN.md's animation
   table gains the row.
6. **Gate.** `motion_test` 15 → ~20; `compose_test` 506 and the rest
   unchanged, since nothing in compose changes. No plate ledger run is
   needed for an additive SigilMotion verb no corpus scene calls — but say
   so in the entry rather than skipping it silently.

**NOT in scope, and named:** adopting `derive()` into the ~30 sketches that
hand-roll it. That is a corpus edit with a ledger cost, and it is the
owner's call, exactly as f206364's six unadopted sketches are.

### 43.12 FOUND IN THE SOURCE

1. **`choreograph::SquashPhrase` is unconditionally broken and is compiled
   into every animation TU** (`phrase/Retime.hpp`, included by
   `Choreograph.h:35`). Its constructor initialiser list reads
   `_source_duration(_source->getDuration())` while `_source` is declared
   FIRST and therefore still a null `shared_ptr` — a null dereference on
   construction — and the `source` parameter is never stored at all.
   `stretchTime` is also inverted (`(t/_source_duration)*_new_duration`
   where a squash needs the reciprocal). Nothing in `src/` instantiates it,
   so it is latent, not live. **Do not reach for it.** `LoopPhrase`,
   `PingPongPhrase`, `ReversePhrase` and `ClipPhrase` are sound. Fixable in
   the sigil-vcpkg-registry port if a future wave wants it.
2. **`Output::inputPtr()` is non-const**, so `Instance::resolveFloat` —
   which is `const` — cannot reach the Sequence without a `const_cast` or a
   non-const overload. A detail, but it is the first thing any
   evaluate-at-time work trips over. *(Attempted and SKIPPED 2026-08-04,
   left open: verified there is NO call site today —
   `Instance::resolveFloat` reads `binding->value()` and never touches
   `inputPtr()`, so a compose-side `const_cast` would be dead code
   guessing at future work. The real fix is a const overload in the
   vendored choreograph port (sigil-vcpkg-registry), which is a separate
   workflow; take it when the first evaluate-at-time call site exists.)*
3. **`mountDelayCarryMs` is a subtree time offset in everything but name**
   (`Reconcile.cpp:813-836`): reconcile-time, dynamically scoped,
   save/restore, compounding through nesting. Any future subtree-clock work
   should be built as its sibling, not invented.
4. **`quantizeTime` and hand-rolled `floor(t*N)/N` are the same feature at
   two altitudes.** The library ships it for SHADERS
   (`Material::quantizeTime`) and eight sketches hand-write it for
   SCHEDULES. `Bound::quantize` already exists for PROPERTIES. Three
   spellings of one idea, and `derive()` is what makes them one.
5. **The elapsed channel is already shadowed once** — `metricCtx` at
   `Paint.cpp:1896` copies `paintCtx` and overrides `size`. So a per-subtree
   `elapsedSeconds` offset has a precedent for how, and costs a field
   assignment. It is the cheapest unbuilt thing this wave found, and it is
   what `twoadvanced_v4.cpp:360` and twelve other sites hand-roll. Filed,
   NOT designed here, because it retimes only one of the two channels and
   this entry's whole argument is that a half-retime must not be silent.

### 43.13 SHIPPED 2026-07-29 — `derive()` as `Ticker::derive`, the quantize consolidation, and the verb audit

**The §43.11 plan is executed, with one deliberate deviation, named
first.** Plan item 2 called for a free `derive(&src)` factory over the
existing builder, "so the call site reads as a derivation rather than as
a property binding." **Not shipped, and the collision is the argument:**
`sigil::compose::derive` is already a NAMESPACE (the geometry derive
phase — `derive::around`, `derive::rail`), so the factory could never be
re-exported into compose — the library where most authors live — and a
verb that spells differently per library is worse than no synonym at
all. The verb is the MEMBER, `Ticker::derive(dst, chain)`, which is
where the new thing (the write) actually happens; the chain is `bind()`,
which the author already knows, verbatim. One word per idea, same
spelling in every library:

```cpp
choreograph::Output<float> phase, penTip, stepped, backwards, ring;
ticker.add([&](double) { phase = ...; return true; });
ticker.derive(&penTip,    bind(&phase).offset(-0.008f).clamp(0, 1));
ticker.derive(&stepped,   bind(&phase).quantize(8));   // posterize
ticker.derive(&backwards, bind(&phase).invert());      // reverse
ticker.derive(&ring,      bind(&phase).scale(0.5f).wrap(1.0f)); // loop
```

**What shipped, against the plan:**

1. **`Bound::wrap(period)`** — the one new stage. Floor-convention fold
   into [0, period) AFTER the affine chain, BEFORE wiggle (the noise
   phase reads the unwrapped schedule, so a wrapped phase wiggles
   continuously across the seam — pinned) and clamp. `period <= 0` is a
   no-op, not a division. Bit-identical to the corpus's
   `std::fmod(t*k, 1.0)` for positive schedules — pinned as identity,
   not approximation. BoundFloat 16 → 17 fields; the FIELD PIN chain
   ruled on it end to end (`fields()`, `kFieldCount == 17`,
   `boundMapEqual`, the walk row — positive control run: dropping the
   comparator term fails `EveryBoundFloatFieldParticipatesInEquality`
   naming field #16).
2. **`Ticker::derive(dst, chain)` + the TWO-PHASE STEP** (§43.3 ruling
   (a)): `tick()` steps the timeline and every steppable first, then
   every derivation, so **a derivation never reads a stale source**,
   whatever the registration order. The pin registers the derivation
   BEFORE its source's writer — the arrangement that is one frame stale
   under any registration-order step — and its positive control (step
   derivations first) fails it with the stale value, restored. The
   ONE-LEVEL rule is enforced loudly at registration, both directions
   plus self-derivation and double-write, each with a stderr warning and
   a false return (`DeriveEnforcesTheOneLevelRuleLoudly`). Derivations
   apply once at registration (correct before the first tick), are pure
   in their sources, and do NOT hold `active()` — hosts stay
   event-driven (pinned).
3. **The affine-equivalence limit is in the doc comment** on
   `Ticker::derive`, per the plan: derive() remaps a schedule's VALUE,
   which equals a time remap exactly when the schedule is affine in
   time. DESIGN.md's animation grammar table gained the row.
4. **Cross-library composition pinned device-free**:
   `WorldAnimation.DerivedOutputDrivesAWorldLaneDeviceFree` — a Ticker
   derivation feeding an `AnimatedMaterial` uv lane through
   `bind(&trail).target(...)`, resolved by
   `resolveAnimation(entt::registry&)` with no device; control (skip
   phase two) fails it on the same-frame assertion, restored.
   `bind(&derived)` and `wiggle(&derived, …)` are pinned in motion_test.
5. **Corpus idioms, bit-exact**: vertigo's penTip offset, skill-tree's
   pulse affine, the fmod ring phase, the inverted sawtooth, and
   window-vs-clamp on a dyadic grid
   (`ChainStagesReproduceTheCorpusIdiomsBitExactly`).

**§43.12 item 4 executed — the quantize consolidation.** The canonical
definition is `motion::quantizeTime(t, hz)` (Animation.h): a template on
ONE type so every routed site keeps its own precision and the rewrite is
bit-identical. Re-exported as `compose::quantizeTime`. Dispositions:

| site | disposition |
| --- | --- |
| `Material.cpp:207/290` (uTime digest + uniform) | ROUTED, double precision preserved |
| `ScenesAero.h:236` (8 Hz breathing) | ROUTED (double) |
| `ScenesPersona.h:257` (6 Hz caustics) | ROUTED (double, float cast kept) |
| `ksp_mapview.cpp:1845` (8 Hz instrument glow) | ROUTED (float) |
| `eva_magi_interior.cpp:1831` | KEPT + comment: deliberate `kPhase` stagger and `1e-6` epsilon — not the canonical arithmetic |
| `winamp_base.cpp:1417/1435` | KEPT + comment at :1435: step INDEX (hash key / edge detector), not requantized time |
| `lain_navi.cpp:1244/1260-62`, `cde_motif.cpp:2046-47` | KEPT, no comment: `floor` as int index / next-boundary math, visibly a different idea |
| winamp's `round(pct*28)` sliders | KEPT: that is `Bound::quantize`'s LEVELS shape (round on [0,1]), already named differently on purpose |

Byte-neutrality PROVEN, not assumed: a targeted `plate_ledger.py
--scenes` sweep over all eight affected scenes (aero desktop, persona
menu, ksp_mapview, twoadvanced_v4, spacejam_1996, ds2_bench,
eva_magi_interior, winamp_base) — **8 byte-identical, 0 moved**. No full
ledger run was needed: no other scene's code path changed, and the
routed arithmetic is type-identical by construction. No `--rebase`.

**The verb audit (2026-07-29), against the owner's four lenses (unify
verbs / consistency / no exceptional paths / composition):**

- `lookAhead`: compose's `travel()` reused world's `CameraPath` word,
  same meaning, same units convention. HOLDS.
- `t` as "the schedule lane": `MotionPath::t` == `CameraPath::t`. HOLDS.
- `wiggle`: one word, three altitudes (stage, free rig factory, world
  re-export), one semantics. The free `wiggle(&out,…)` DOCUMENTED
  exception (names `bind().scale(0).wiggle()`) holds up: it prevents a
  silent drift bug, returns an ordinary Bound, adds no second mechanism.
- The `Bespoke` slot row: confirmed as a documented exception with an
  argument; not relitigated.
- `quantize` vs `quantizeTime`: two contracts (levels/round on [0,1] vs
  rate/floor on seconds) — the DIFFERENT words are correct; the doc on
  the canonical function now states the split so nobody unifies them
  wrongly.
- **FIXED (cheap, new-this-wave):** the free `derive()` factory was cut
  before landing (the collision above) rather than shipped asymmetric.
- **FINDING, filed:** `shape::Pop`'s `.order(axis)` vs `.orderBy(attr)`
  mirrors `rampBy`'s overload family; consistent internally, but note
  `rampBy` has no bare `ramp` sibling while `order`/`orderBy` are a
  pair — a naming asymmetry, harmless, not churned.
- **FINDING, filed:** DESIGN.md §43.0's quotes name vendored choreograph
  members (`Output::inputPtr` etc.); the doc-probe gate refused them
  (correctly — they are outside the scanned headers) and they are now
  EXCLUDED_SPELLED with reasons in api_doc_probes.py. The previous wave
  left this latent: DESIGN.md was edited without rebuilding compose_test.

**Gates:** motion_test 15 → 21, world_test 64 → 65, compose_test 506
(505 + 1 expected skip), compose_kit_test 47, shape_test 83 — both
configs. Positive controls run and restored (mtime stamped) for: the
staleness pin, the wrap pins, the BoundFloat field walk, the world
cross-library pin.

**NOT adopted, per the plan:** the ~30 sketches that hand-roll shadow
cells. Corpus edits are the owner's call, as f206364's six unadopted
sketches are.

## 44. THE COMPOSER CAMERA — the crux inverts under measurement, and what the corpus actually asked for is not a camera — DESIGNED 2026-07-30; **DECLINED FOR COMPOSE ON SCOPE 2026-07-30, see 44.10** — the analysis stands, the feature does not

The doc-map has named this seam for as long as there has been a doc-map:
*"a Composer camera with local-space bake anchoring (infinite canvas — the
deepest item)."* Its provenance is `archive/SPATIAL.md` §0 (the
four-client convergence table) and §2 (the 2D-affine boundary, plus one
afternoon's experiment marked **UNMEASURED** in that document's own claims
table). The After Effects analogue is 3D layers: give a 2D layer a Z, add
a camera, get parallax and perspective.

It is designed here and **deliberately not built**, on the §19 model. Two
throwaway probes were written to answer it, both run against the Release
library through `ComposeSketch` (an arbitrary path compiles, so neither
probe entered the tree), and **both inverted the premise they were written
to test.**

### 44.0 First, the citation count, because the preamble says that is the only signal worth trusting

| Ask | Study citations | State |
|---|---|---|
| a perspective transform on a subtree | **1** — `eva_magi_interior.cpp:212` item E, and it wants a STATIC projection of a flat plate | open |
| parallax between planes | **1** — `thaumonomicon.cpp:1294`, and it is **already built**, out of `Cache::Texture` + bound `translateX/Y` at two divisors | shipped, no feature needed |
| a Composer camera | **0** | — |
| infinite canvas | **0** — it appears only as an anticipated client in SPATIAL.md's convergence table, never as a wall anybody hit | — |
| windowed/tiled bake for a long strip | 1 — world's marquee, §36 | **measured to a ceiling of zero**, closed, `tiles::` shipped instead |

Two rows of that table are the whole entry in miniature. The parallax row
is a study that wanted AE's headline 3D effect and **got it out of the
existing paint-only transform lanes**, with a comment that reasons about
the caching correctly and unprompted:

> the parallax is a bound transform, which is paint-only volatility, so the
> bakes replay under it and no shader re-runs per frame

And the camera row is empty. Per the preamble's ordering rule, a Composer
camera is not an entry the corpus filed; it is an entry a design
conversation anticipated. That does not make it wrong, but it means the
wall has to be reproduced before the fix is built — and when it was, it
was not there.

### 44.1 THE CRUX: "a camera moves every node every frame, so it invalidates everything" — FALSE, and measured

The naive reading is that a camera is maximally hostile to this
architecture: pruning and baking rest on a node comparing equal, and a
camera moves every node on screen continuously.

**Probe 2** (`persp_live.cpp`, thrown away) builds a live 13-node
`Composer` — small type, hairlines, a gradient — and draws it through a
`SkCanvas` carrying a CSS-model perspective `SkM44`, reading the
composer's own `stats()` frame by frame. Raster, Release, this machine.

| arm | recorded | baked | painted | picturesLive | paint ms |
|---|---|---|---|---|---|
| affine, frame 1 (cold) | 13 | 0 | 2 | 13 | 0.732 |
| affine, frames 2–5 | **0** | 0 | 0 | 13 | 0.206–0.213 |
| perspective, camera STILL, 5 frames | **0** | 0 | 0 | 13 | 1.014–1.213 |
| **perspective, camera MOVED every frame, 6 frames** | **0** | **0** | **0** | 13 | 0.990–1.041 |

**A moving camera invalidates nothing.** Not one recording, not one bake,
not one live paint, across six consecutive frames at six different camera
angles. Re-describing the tree on top of that changes nothing either:
`patched=1 recorded=1 reconcile=0.004–0.012 ms`, and that one node
patches identically whether the camera moves or not — the camera is the
host's CTM and never enters the tree, so it cannot be the cause.

**Why, in one sentence that is already written down in the source.**
`Paint.cpp`'s picture branch says it:

> A picture can be replayed under a DIFFERENT matrix than it was recorded
> at (an ancestor with a live transform keeps its picture and replays it
> under the motion). Anything inside must therefore be matrix-independent
> — which a device-space bake, snapped to one particular device rect, is
> not.

The picture tier is **matrix-independent by law**, and that law is old and
load-bearing and has nothing to do with cameras. The camera therefore
cannot reach describe, cannot reach reconcile, cannot reach layout, and
cannot reach the recordings. The only stage that reads the CTM to make a
CACHING decision is the **device-space pixel bake**, and there are exactly
three sites, all in `Paint.cpp`, all already guarded (44.1b names the
fourth reader, which is not a caching decision but can become one):

- `upright` (automatic texture promotion) — `!totalM.hasPerspective()`
- the `Cache::Group` device bake — `!totalM.hasPerspective()`
- the `Cache::Texture` device bake — `!totalM.hasPerspective()`

**So the answer to "which stage absorbs a camera move" is: the composite,
and nothing above it — and what enforces that is not a new rule but the
oldest one in the caching section.** The three `hasPerspective()` guards
SPATIAL.md called "the (latent) boundary" turn out to be a complete and
exact enumeration of every place in the library that is not
matrix-independent. The boundary is not a fence around a feature; it is
the list of sites that pin pixels to a device rect.

### 44.1b The FOURTH CTM reader, and it is the one caveat on 44.1

`Composer::draw()` derives `hostScale` — `PaintContext::contentScale` —
from `maxScaleOf(canvas.getTotalMatrix())`, so a camera move DOES change
one number that reaches paint. For everything but one case it is inert:
recordings capture the scale current when they re-record (the code says so
and calls it best effort), which is why probe 2 measured `recorded=0` under
a moving camera.

**The exception is a live `Material` that reads `uContentScale`.**
`Material.cpp` marks such a material `usesScale`, and `resolve()` memoizes
on a digest of its varying inputs — so a moving camera moves the digest,
`liveStable` goes false, and that node re-bakes every frame. It is exactly
the §17/§38 staleness contract working as designed (the input really did
change), and it is worth naming for two reasons: it is the ONE way a camera
move can reach the cache, and under perspective the number it is reacting
to is computed from the matrix diagonal, which is the defect in 44.2b.1.
So a `uContentScale` material under a projected node re-bakes every frame
in response to an estimate that is wrong. Neither half is a blocker; both
belong in the design's test list.

### 44.2 "Local-space bake anchoring" already exists, and its leverage HALVES under perspective

The canon's phrase describes a mechanism that shipped long ago: when a
`Cache::Texture` node is moving — `transformLive`, or its device rect
moved since last frame — the device branch is skipped and the node takes
the **quantized local bake**, rasterized at one of eight coarse scale
steps in the node's own space and blitted through the live matrix.
`Paint.cpp` already names camera-shaped causes for it in prose: *"a
resizing window, a pinch zoom, a pan."* A camera is a fourth item on that
list, not a new phenomenon.

Probe 2, arm E — the same panel with `.cache(Cache::Texture)` on its root,
under a camera moving every frame:

| frame | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| bakes | 1 | 0 | 0 | 0 | **1** | 0 |
| paint ms | 1.450 | 0.624 | 0.583 | 0.552 | 0.762 | 0.470 |

**It engages and it holds.** So local-space bake anchoring is real, is
shipped, and does survive a moving camera. What it does not do is carry
the feature, and two measurements say so.

**(a) The leverage collapses.** Probe 1 (`persp_probe.cpp`, thrown away)
takes one `snapshot()` picture of a 420×300 panel and draws it four ways
into a 600×400 raster surface, 300 reps each:

| | picture replay | bake blit | bake's advantage |
|---|---|---|---|
| **affine** | 0.210 ms | 0.051 ms | **4.1×** |
| **perspective (yaw 45)** | 0.995 ms | 0.510 ms | **1.95×** |

Probe 2's live composer agrees from the other side: the same node costs
0.216 ms affine live, 0.039 ms affine baked (**5.5×**), and ~1.00 ms
perspective live against ~0.50–0.62 ms perspective baked (**~1.7×**).

Read the columns, not the rows. A perspective CTM costs the picture replay
**4.7×** and costs the bake blit **10×** — because an affine blit of a
matching-size image is a fast rect path and a perspective one is a general
per-pixel inverse map with a w divide. **Perspective punishes the pixel
cache harder than it punishes the thing the pixel cache was supposed to
replace.** The tier compose reaches for under load is the tier perspective
takes away.

**(b) It is not the same pixels, and the bar here is 1 LSB.** Probe 1
compares the bake-then-homography against the vector truth over every
covered pixel:

| yaw | bake 1× mean \|Δ\| | max | differing | bake 2× mean \|Δ\| | max | differing |
|---|---|---|---|---|---|---|
| 0 | **0.00** | **0** | **0.0%** | 1.94 | 94 | 13.6% |
| 35 | 5.17 | 134 | 24.4% | 3.30 | 144 | 17.1% |
| 60 | 5.21 | 135 | 26.1% | 4.91 | 148 | 19.8% |

The yaw-0/1× row is the negative control and it reads exactly zero, which
is what makes the rest of the table believable. Against §29's own
standard — *"agreement to within 1 LSB is not agreement"* — a mean of 5.2
with a max of 135 is not agreement by two orders of magnitude, and it is
in the place it always is: small type and hairlines, magnified at the near
edge, where the flat bake has no texels to give. Eyeballed at 4×
magnification the 1× bake is legible and visibly soft; the 2× bake is
close to the vector. It is the same trade `Cache::Texture` already
documents for a rotated bake (mean 13.5 at ±90°) — **an opt-in the author
accepts, never a default the library takes.**

**So the doc-map's phrase is half a fact and half a misdirection.** The
mechanism exists; it is not what makes a camera possible. What makes a
camera possible is the picture tier, which needed nothing.

### 44.2b Two defects found on the way, filed not fixed *(both since resolved: 1 FIXED 2026-08-03 below; 2's stale comment repaired 2026-08-03)*

1. **`maxScaleOf()`'s perspective fallback is the matrix DIAGONAL, and
   that is not the maximum magnification of a projected quad.**
   `ComposeRuntime.h:569` returns `getMinMaxScales`'s upper singular value
   and falls back to `max(|scaleX|, |scaleY|)` when the matrix has
   perspective (Skia refuses the singular values there). A projected quad's
   near edge can magnify well past the diagonal, so the quantized bake
   ladder can pick a step BELOW the density the near edge needs. It also
   walks: arm E's one re-bake at frame 5 out of six is the diagonal
   crossing a `kBakeSteps` boundary as the yaw turned. The bounded cost
   (eight steps) is why this is a defect and not a wall. The honest fix is
   the maximum over the four mapped corners of the local bounds, which is
   exact for a plane and is the number every consumer of `maxScaleOf`
   actually wants.
   **FIXED 2026-08-03, by local linearization at the node.** `maxScaleOf`
   now takes the node's local bounds; under perspective it returns the
   largest singular value of the JACOBIAN of the projective map —
   J(p) = (1/w)·[[a−gX, b−hX], [d−gY, e−hY]] — maxed over the rect's
   center and four corners (for a plane the extremum sits at the corner
   nearest the horizon; max-over-samples errs CONSERVATIVE: an
   overestimate steps a bake finer, an underestimate ships a stale blur).
   Samples at/behind the horizon are skipped; all-degenerate falls back
   to the diagonal, bounded by the callers' clamps. All four consumers
   pass their honest rect (the ladder its bake bounds, the two
   affine-guarded device bakes theirs, `hostScale` the canvas). The
   affine path is untouched (`getMinMaxScales`). Pin:
   `ComposeMaxScale.PerspectiveFallbackTracksTheJacobianNotTheDiagonal`
   (`ComposeTestFieldPins.cpp`) — a 90°-rotated perspective matrix whose
   diagonal reads exactly 0 against a finite-difference ground truth of
   2.29, tolerance 2%; positive control run (diagonal reinstated → pin
   fails at 0 vs 2.29 → restored). Ledger byte-neutral (no corpus scene
   applies a host perspective CTM — the branch is unreachable there).
2. **A stale filed-gap comment.** `ComposeInternal.h`'s FIELD PINS block
   says *"the same pair is still missing from `recordBounds()`'s transform
   gate (filed, ROADMAP)."* It is not: `Paint.cpp`'s `recordBounds()` now
   gates on `tf.pivoted()`, which includes `sx`/`sy`, and its comment
   records the repair (*"Same field, same feature, second site — filed by
   the travel() wave, taken here"*). One sentence in a header, left for the
   owner rather than touched, because a comment edit in that header rebuilds
   the world.

### 44.3 The seven questions

**1. What is the unit — a layer Z, or a full camera? NEITHER, and the
existing lane set says why.** Compose already ships a complete 2D affine
transform vocabulary: `translateX/translateY`, `rotate`, `scale` +
`scaleX/scaleY`, `skewX/skewY`, over one `transformOrigin`, all
`Animatable<float>`, all paint-only, all in one resolver. Read that list
and the gap is not a camera and not a depth: **it is that `rotate` is
`rotateZ` and the other two axes are missing.** The smallest coherent
version is `rotateX`/`rotateY` plus the one scalar that makes them visible
— CSS's `perspective` distance — and that is three more float lanes in the
machinery that already exists.

Why not a Z lane: a Z is meaningless without a camera to project it, so
`z()` is not one property, it is one property plus an ambient value, and
compose's only mechanism for an ambient value (`env::`) resolves at
DESCRIBE. That is where the crux would actually have bitten (see rejected
shape (b)). Why not a camera: measured, the host already has one — a
concat before `draw()` — and it invalidates nothing (44.1). Three lanes
buy the one thing a host CTM cannot: **per-node** projection, so two
siblings can sit at different angles in one composition.

**2. Where does the projection happen — `paint()`'s matrix, or a real
per-node perspective draw?** `paint()`'s matrix, and the alternative
turns out not to be an alternative. `shape::space::drawPanel` — the
working sibling implementation the manager pointed at — is, in full,
`canvas.save(); canvas.concat(fullM44); draw(canvas); canvas.restore()`.
It IS the matrix path, wearing a camera's argument list. There is no
second draw path to choose, and the lens's objection to an exceptional
path never has to be raised.

The projection is therefore one more term in `transformOf()` and one more
concat in `paint()`, and its result is rasterized by Skia
perspective-correct — measured, legible, at 4.7× the affine cost on raster
(see 44.6 for why that number is not the deciding one).

**3. What happens to layout? Nothing, and the reason is stronger than
"Yoga is 2D."** Yoga being 2D is true and is the weak argument. The strong
one: a projection changes a node's SCREEN footprint but not its content's
intrinsic size, so if it fed layout, a foreshortened text node would
re-measure at a different width — and the Risks section already prices
that (*"animating text-affecting layout props re-measures per frame"*).
At 60 Hz, for every node in view, that IS the crux, arrived at by the one
route that would have made it real. `travel()` is the standing precedent
and the ruling is copied verbatim: **paint-only, `bounds()` keeps
reporting the laid-out box.** Perspective changes only the painted result.

The one honest consequence, which wants an API.md line: there are two
bounds notions in this library and the projection joins the second.
`bounds(key)` is the layout box and stays flat; `recordBounds()` — the
paint bounds that size effect layers, opacity layers and bakes — must map
through the projection, exactly as it already maps through a child's
rotate and scale.

**4. Pruning and caching, concretely.** Measured in 44.1, and the answer
is a table rather than an argument:

| stage | does a camera move touch it? | what makes that true |
|---|---|---|
| describe | **no** | the camera is not a prop |
| memo | **no** | same |
| reconcile / patch | **no** | same |
| layout | **no** | Q3's paint-only ruling |
| recordings (the picture tier) | **no** — measured 0 across a moving camera | pictures are matrix-independent BY LAW; the law predates this |
| device-space pixel bakes (`Cache::Texture` device, `Cache::Group` device) | refused | the two existing `!hasPerspective()` guards |
| automatic texture promotion | refused, reported as `Prom::Transformed` | the existing `upright` guard |
| quantized LOCAL pixel bake | survives; re-bakes on a scale-step crossing | the existing moving-node fallback |
| the final composite | **yes — this is where it lands** | it is the CTM |

"The camera lives below the recording and above the composite" is
therefore not a rule that needs enforcing — it is a restatement of *no
device space inside a picture recording*, which is enforced today by the
`recordingDepth == 0` conditions on all three device paths and covered by
their existing tests.

**5. Hit-testing — the existing seam generalises, and the reason it does
is a bill the library already paid.** `Query.cpp`'s `hitInstance` walks
paint's matrix stack backwards as a point-to-point map, one level at a
time, hand-unwinding skew, then scale, then rotation. A homography
inverts too (`SkMatrix::invert` handles perspective) and maps a point to a
point, so the walk is unchanged in shape — and because a projected node is
still a PLANE, there is exactly one preimage. The change is a net
DELETION: invert the one matrix the resolver already built instead of
unwinding six floats by hand.

That is only true because of the note already in `transformOf`: *"THE GATE
IS `pivoted()`, NOT A COPY OF IT. One resolver, three consumers — paint's
matrix, this child union, and `hitInstance`'s inverse — and the three must
build the SAME matrix or a node draws where it cannot be hit."* Three
hand-written copies would have been three chances to disagree about
perspective, and the library has already been bitten twice by exactly
that (`scaleX`/`scaleY`, two sites, §40).

**The one thing 3D does need its own answer for is the horizon.** Beyond
the vanishing line the w divide folds space: a point outside the drawn
quad maps back inside it, and `SkMatrix::mapRect` — which maps four
corners and takes their bounds — returns garbage. Both the hit test and
`recordBounds` therefore need the same gate: **w > 0 at all four mapped
corners, or the projection is refused for that node** (drawn unprojected,
warned once, reported through the existing `Promotion`-style refusal
vocabulary). This is the feature's one genuine soundness hazard and it has
a precedent for how to handle it: `VariationDrive` refuses an axis it
cannot prove safe and says so.

**6. Does this subsume `tiles::`? No — and the reason is the altitude, not
the capability.** `tiles::` slices a baked picture into N GPU textures for
a consumer that owns a depth buffer and mirrored sampling; three lanes on
an `Element` put one plane in ONE `SkCanvas` under the painter's rule.
They are the two halves of the boundary DESIGN.md already draws —
*surface-granular, not element-granular; between surfaces the depth rule.*
Concretely, the world marquee's ribbon wall is a CURVED surface, and a
homography maps planes; no number of lanes on an `Element` reaches it.
`tiles::` does not exist because there is no camera. It exists because
there is a second library that owns geometry, and it always will.

Nor does a camera help the infinite canvas, which is the client the
doc-map named. **Probe 2, arm G:** the same picture-cached tree, panned
10,000 px off screen —

| | ms/frame |
|---|---|
| on screen | 0.216 |
| panned fully off screen | **0.021** |
| clear the surface, draw nothing (floor) | **0.021** |

`recorded=0 painted=0`, and compose's own `paintMs` reads **0.0001**. An
off-screen picture-cached subtree already costs the floor, because
`drawPicture` rejects against its cull rect and the whole subtree is
inside one op. So the infinite canvas's three needs are pan/zoom (the host
matrix — shipped), density (the quantized bake ladder — shipped), and
culling within one enormous replay (`tiles::sliceable()`'s BBH, measured
in §36 to land ON the extraction floor — shipped). **What the infinite
canvas was missing was never a camera; naming it one is the NR-4 failure
in advance — a wrong mechanism sending the fix to the wrong place.** What
is left of it is Direction item 5, the retained-subtree snapshot at host
density, and §36 already measured that a REGION bake has nothing to win.

**7. Scope, honestly.** AE's 3D layers bring cameras, lights, shadows,
material options and 3D intersection ordering. Almost all of it is out,
and one item is out for a reason worth stating: **3D intersection ordering
contradicts the stacking law.** *"A component cannot z-escape the site it
was composed into"* is a kernel rule; a 3D node that interleaves with its
uncle by depth breaks it. AE's own answer to this question — 2D/3D
switches, precomps that flatten, "Collapse Transformations" — is the most
confusing part of AE, and CSS's answer (`transform-style: flat` by
default, `preserve-3d` an opt-in widely regarded as a footgun) is the one
to copy: **flat, always, no opt-in.**

### 44.4 The smallest coherent version — three float lanes, and not one new concept

```cpp
Element &rotateX(Animatable<float> deg);   // pitch, about transformOrigin
Element &rotateY(Animatable<float> deg);   // yaw,   about transformOrigin
Element &perspective(Animatable<float> distancePx); // CSS's perspective
```

Everything about this shape is chosen to reuse rather than to add:

- **They are floats, so they are lanes.** `Animatable<float>` means the
  whole `bind()` chain, `animate(to(…))`, transitions, `wiggle()`,
  `staggerChildren`, `window()`/`quantize()` — every verb in the animation
  grammar — applies on day one, with no plumbing. This is the same ruling
  world's camera lanes made (*"every lane is a float"*) and the same one
  `travel()` made (*"the lane is `t`"*), for the third time.
- **They pivot on `transformOrigin`**, like `rotate` and `scale`, so a
  card flips about its own edge with the property that already exists.
- **They live in a `Box<SpaceProps>` block**, not in `PaintProps` —
  `MotionPath` is the precedent and `Composer.cpp`'s
  `static_assert(sizeof(ElementNode) <= 768)` is the rule that says so.
  Cost: one null pointer, +8 bytes, 736 → 744.
- **`rotate` is not renamed.** It is `rotateZ` and always was; a doc line
  says so. Aliasing it would be a second grammar (§33), and renaming it
  would move every caller's call site for zero information.
- **A camera, if anyone wants one, is a util-tier or caller-side function
  that computes three floats per node** — which is exactly what
  thaumonomicon already does by hand for two planes, and exactly what the
  extraction test says to wait for: a helper earns a home when it makes a
  CHORE cheap, and nobody has yet done this chore twice.

What ships with it: the w > 0 refusal, the `recordBounds` projection, the
`hitInstance` inverse, and — finally — **a test on the
`!hasPerspective()` boundary**, which §28 has wanted since SPATIAL.md
called it latent.

### 44.5 What is explicitly OUT

- **A camera object, in any form** — `Composer::setCamera()`, an
  `env::`-provided camera, a camera node. Measured redundant at the host
  (44.1); priced by two EXISTING measurements at describe (rejected
  shape (b)).
- **`z()` / `translateZ()`** — a depth with no camera is not a property.
- **Depth in the model, depth sort, 3D intersection ordering,
  `preserve-3d`** — contradicts the stacking law (Q7).
- **Lights, shadows, material options, back-face culling.** A back-face
  test is one `w`-sign check and it is still out: nothing asked.
- **3D layout, 3D bounds from `bounds()`, perspective-aware measure.**
- **A matrix-valued `perspective(SkMatrix)`** — SPATIAL.md's and
  `eva_magi_interior`'s own proposal, rejected below. Note the NAME is
  taken by 44.4's scalar (CSS's perspective DISTANCE); the two are not
  overloads of one idea and only one of them can be a lane.
- **Curved surfaces, ribbons, tiled GPU strips** — `tiles::` and
  SigilShape/SigilWorld own those, and the boundary is unchanged.

### 44.6 THE MISSING NUMBER, and it is the one that decides whether to build

**Every number in this entry is CPU raster, and Graphite is the primary
target.** Doctrine 6 is explicit that the raster path cannot speak for the
GPU. The 4.7× perspective penalty on picture replay is very likely a
raster artefact: a perspective quad on a GPU is what texture units do in
hardware, and glyphs under perspective on raster fall off the atlas path
onto path filling, which is most of the cost.

The direction of that uncertainty is favourable to the design above — if
perspective is nearly free on Graphite, the live path wins outright, the
bake tier is not needed, and three lanes is the whole feature. But the
number does not exist, and the honest gate is therefore:

> **Before spending the kernel change, measure ONE projected panel on
> Graphite** (`compose_gpu_test` has the harness; `--gpu` on the gallery
> has the surface). If perspective is also ~5× there, the answer changes
> from "three lanes" to "refuse, and let the future 3D library own the
> projection" — because at 5× on the primary target, a projected plate is
> a per-frame budget decision and belongs where the depth buffer is, not
> in a paint-only lane an author can put on any node.

**MEASURED 2026-08-03, by 44.10's own recipe** — arms in
`bench/ComposeBench.cpp` after `BENCHMARK_MAIN()` (suite
`BM_Draw_DenseText_Persp_*_Graphite`): `denseBlock` glyphs at 800×2400,
`Cache::None`, synced submits (`submitGraphiteSynced`), three arms
differing only by the matrix inside `save()/concat()/restore()` — the
documented card tilt is rotateX(25°) about the panel center behind a
CSS-style perspective(2400), w ∈ [0.79, 1.21]; the affine control is the
same product without the perspective row. Release, 3 repetitions, CV
under 1% on every arm; two independent runs agreed:

| arm | run 1 mean | run 2 mean | CV (run 1 / run 2) |
|---|---|---|---|
| identity | 0.380 ms | 0.382 ms | 0.52% / 0.06% |
| affine tilt (control) | 0.396 ms | 0.393 ms | 0.21% / 0.85% |
| **perspective** | **0.694 ms** | **0.713 ms** | 0.26% / 0.14% |

**Perspective costs ~1.8× on Graphite (1.75× against the affine control),
not ~5× — the raster 4.7× does NOT carry to the GPU.** The residual cost
is CPU-side, not GPU-side: benchmark CPU time goes 0.060 → 0.35 ms while
wall adds ~0.31 ms, consistent with the glyph-atlas hypothesis living in
the recording path rather than in shading. On 44.6's own pre-registered
branches this lands on the FAVOURABLE side — the gate would have answered
"three lanes", not "refuse" — which for compose is moot (44.10's scope
ruling stands; the lanes stay unbuilt), but it is the number SigilWorld's
projection work should inherit: a projected glyph-bearing plate is ~2×,
not a per-frame budget decision. §44.6 is CLOSED for compose.

### 44.7 Rejected shapes, with reasons

**(a) `Composer::setCamera(...)` — a camera value on the Composer.**
Measured redundant: `Composer::draw()` already honours the incoming CTM
(and `hostScale` already derives from `maxScaleOf`, which already has
a perspective branch), so a host concat IS the camera, and probe 2 shows it costs zero invalidation. Additionally it
would be a SECOND camera vocabulary in one repo next to
`shape::space::Camera`, which compose may not depend on (the Eigen/glm
refusal; every seam here is a Skia seam) — so it would be a duplicate
type, not a shared one, which is the exact opposite of unifying verbs.

**(b) `Element::z()` plus an ambient camera through `env::`.** This is the
shape that looks most like AE and it is the one that makes the crux real.
`env::`'s rule is that an inherited value is **resolved at describe and
baked into the reading node's props**. A camera moving at 60 Hz would
therefore change the props of every 3D node every frame — missing every
`memo`, patching every instance, marking paint dirty up the spine — and
the two measurements that price it already exist: a full CDE palette
change costs **237 re-records over 1270 nodes** through `env::` (§10g) —
and that is the CHEAP path, the one whose whole virtue is that props are
the exact dependency tracker; the provider node rejected there would have
invalidated the desktop. §3's bound-`Fill` theme measured **0.33 ms/frame
steady against 0.033** for the same forty values held plain. A per-frame ambient value is the worst thing
`env::` can carry. **The crux is real for exactly one design, and this is
it.**

**(c) `Element::perspective(SkMatrix)` restricted to `Cache::Texture`
subtrees** — SPATIAL.md §2's "one concession worth taking", and
`eva_magi_interior`'s own wording (*"a bake is already a texture and Skia
will happily draw one under a 3×3"*). Three reasons it is the wrong shape,
and the first two are the probes':
1. **The restriction is unnecessary.** The live vector path under
   perspective works, and is *better* — measured byte-identical at yaw 0
   and mean 5.2/255 better than the 1× bake at yaw 35–60. Gating the
   feature on a bake would ship the worse of the two paths as the only
   path.
2. **The bake is not the win it was assumed to be.** 1.95× instead of the
   4.1× a bake buys in affine space (44.2), and paid for in visible
   softness on small type — which is where the study that filed this wants
   it.
3. **A matrix is not a float, so it cannot be a lane.** It would be the
   only paint transform in the library outside the animation grammar: no
   `bind()`, no `animate()`, no transition, no `wiggle()`. And a
   hand-built homography can put the vanishing line inside the node's own
   box, which three clamped floats cannot.

**(d) A real per-node perspective DRAW, like `shape::space::drawPanel`.**
Not a rejection so much as a dissolution: `drawPanel` is a `concat` and a
callback. There is no second path on offer, so the exceptional path the
lens would have objected to does not exist.

**(e) A `CameraPath` port, on the §39 model.** Nothing to port. §39
already took the transferable idea (a curve supplies the shape, one float
lane supplies the schedule) and spent it on `travel()`. A 2D camera flight
IS `travel()` applied to the whole tree, which is the host's matrix moving
— and the host owns that today.

**(f) A projected or windowed `snapshot()` for the infinite canvas.**
§36 measured the region bake at a ceiling of zero and showed it would be
structurally SLOWER (compose's paint traversal has no node-level
quick-reject, so a region paint walks the same ops while doing strictly
more per node). Probe 2 arm G adds the other half: an off-screen
picture-cached subtree already costs the clear-only floor. Both halves of
the infinite canvas's supposed need are already answered.

**(g) Deferring the whole thing and building nothing.** Tempting, and it
is the runner-up verdict. Against it: the three lanes are not a 3D
feature, they are the completion of a transform vocabulary that is
missing two of its three rotation axes; the boundary they cross is
already enumerated exactly (three guards, all correct); and the one study
that filed a citation filed it for a wall that is real — `eva_magi` had to
rectify its reference measurements by hand because it could not project a
flat plate, and that is 1,945 lines of arithmetic done in the wrong space.

### 44.8 The plan, and the estimate is LABELLED AS AN ESTIMATE

Step 0 is the one worth doing whether or not the rest is built.

| step | change | est. lines |
|---|---|---|
| **0** | `transformOf()` returns the MATRIX, not eight floats. `paint()`, `recordBounds()` and `hitInstance()` consume it. Deletes the duplicated pivot arithmetic at all three sites and the hand-unwound inverse in `Query.cpp`. **Byte-neutral, behaviour-identical, plate-ledger-verified — a consolidation the canon already asked for in `NodeTransform`'s own doc comment.** | **−40 net** |
| 1 | `Box<SpaceProps>{rotateX, rotateY, perspective}`, three `Element` setters, three `Instance` anim slots, `fields(SpaceProps)` field pin, `propsEqual` clause, `NodeTransform::fieldPin` count bumped | ~140 |
| 2 | the projection term in the resolver + the w > 0 corner gate + a warn-once refusal | ~70 |
| 3 | docs: `Compose.h` doc comments, API.md's transform section, the two-bounds-notions note, DESIGN.md's Boundaries rewrite | ~90 |
| 4 | tests: the `!hasPerspective()` boundary (§28's outstanding ask), the w-gate refusal, a hit-test round trip under perspective, `recordBounds` covers the projected quad, a pixel pin that a projected `Cache::Texture` node takes the LOCAL bake, the field-pin walk | ~250, 8–10 cases |

**ESTIMATE, not a measurement: ~500 lines net across six files, one
working session plus a plate-ledger sweep, which a purely additive lane
set should pass byte-identical.** No performance claim is made about the
result; the only performance claims in this entry are the probe tables
above, and they are raster.

**Step 0 EXECUTED 2026-08-03.** The three sites are routed through ONE
producer pair on `NodeTransform` (`ComposeRuntime.h`): `matrix(anchor, …)`
composes the stack for `recordBounds()`'s child union (anchor = the layout
offset, folded into the FIRST translate so the floats stay bitwise) and
for `hitInstance()`'s inverse (`SkMatrix::invert` + `mapPoint`, replacing
the hand-unwound inverse — degenerate lanes keep the old skip-not-refuse
semantics); `concatTo(canvas, …)` is the same op list applied as
elementary canvas ops for `paint()`. The pair exists because the obvious
single spelling FAILED THE GATE: composing the stack into one `SkMatrix`
and `canvas.concat()`ing it associates the float multiplies differently
than sequential `translate/rotate/scale/skew` ops, the CTM moves by ulps,
and **17 of 65 plates moved** through antialiased coverage. Reverted to
the elementary sequence, kept beside `matrix()` with the measurement in
its comment. Final ledger verdict: **byte-neutral — 63 byte-identical, 2
moved, both on the documented self-nondeterministic list (hitman_verlet,
slitscan_2001/genesis_fire across the two sweeps), auto-attributed.**
Suites: Release ctest 17/17, compose_test 517+1 skipped both configs.
Found on the way, FIXED BY the consolidation: the hand-unwound inverse
applied skew⁻¹→scale⁻¹→rot⁻¹ to the point in that order, which composes
to R⁻¹S⁻¹K⁻¹ — but `paint()`'s forward map is R·S·K, whose inverse is
K⁻¹S⁻¹R⁻¹. The two agree only when the stack commutes (rotation +
UNIFORM scale, or any single lane), so a hit test on a node combining
rotation with per-axis scale or skew un-transformed the point in the
wrong order for as long as the lanes existed. No pin had encoded the
wrong answer; `invert()` of the one produced matrix is correct by
construction.

### 44.9 THE VERDICT — design, then build, gated on one GPU number

Not a refusal. The crux dissolved under measurement — **a camera is not
hostile to this architecture at all, because nothing in this architecture
except the device-space pixel bake reads the CTM** — and the thing the
corpus actually asked for turns out to be three float lanes in machinery
that already exists, with a net DELETION at the site that would otherwise
have been three copies of one matrix.

But not a build order either, for one honest reason: **every number here
is raster, and this library's floor is 60 fps on Graphite.** Take 44.6's
one measurement first. It costs an afternoon and it is the difference
between a lane set and a refusal.

And two things are settled regardless of that number, and are worth more
than the feature:

- **The "Composer camera" seam is retired.** There is no camera to build.
  A host concat is one, it invalidates nothing, and it has been available
  since `draw(SkCanvas&)` existed.
- **"Local-space bake anchoring" is not the enabling mechanism.** It
  shipped long before this entry as `Cache::Texture`'s moving-node
  fallback, and under perspective its leverage halves while its error
  grows to 5.2/255. What carries a camera is the picture tier's
  matrix-independence — a law written for a completely different reason.
  That is the same shape §36 found in the tiled bake, where the win had
  already been paid for by one argument to `beginRecording`: **twice now,
  the deepest item on the list turned out to be already funded by a rule
  nobody wrote for it.**

### 44.10 THE RULING — DECLINED FOR COMPOSE **ON SCOPE**, 2026-07-30. Not on cost, and 44.6's gate number was never taken

**Owner ruling: the 3D and camera work belongs to SigilWorld. Compose stays
2D; the 3D library owns projection.** `rotateX`/`rotateY`/`perspective`,
`SpaceProps`, and the w > 0 corner gate are **not built and are not to be
built** in compose. 44.4, 44.8 steps 1–4 and 44.9's conditional build order
are closed.

**Read the reason, not just the outcome, because the outcome is a
coincidence.** This lands on the same words 44.6 pre-registered as its
failure branch — *"refuse, and let the future 3D library own the
projection"* — but it arrives by a completely different route, and a future
reader who conflates the two will draw a conclusion this entry does not
support:

- 44.6's refusal branch was conditioned on **a measurement**: perspective
  costing ~5× on Graphite.
- This refusal is **a scope decision about which library owns projection**,
  taken by the owner independent of cost.

**Nothing here is evidence that perspective is expensive on Graphite.
Nothing here is evidence that it is cheap.** Do not cite 44.10 for either.

#### The gate was NOT run, and the reason is the instrument, not the answer

The session that would have taken 44.6's measurement **had no working
shell**: every `Bash` invocation — including `true` and `exit 0` — returned
exit code 1 without executing, with empty stdout and stderr, in the parent
and in subagents, sandboxed and not. Nothing could be configured, compiled,
benchmarked, ledgered or rendered. **No number was produced, and none is
reported here.** 44.6 stays **OPEN and unmeasured**, and it is still the
right question for whoever builds projection in SigilWorld — the raster
4.7× has never been checked against the GPU. *(Overtaken 2026-08-03: the
number was taken by this recipe, exactly as written — see 44.6's MEASURED
table. Perspective is ~1.8× on Graphite, not ~5×; the raster penalty does
not carry.)*

What the session produced instead, from reads alone, is the recipe, so the
next attempt is an hour and not an afternoon:

- **The arm goes in `bench/ComposeBench.cpp`.** Google Benchmark, static
  registration; arms are already declared *after* `BENCHMARK_MAIN()` (1070)
  and register fine. **No CMake edit is needed to add one.**
- **Correction to 44.6's own parenthetical: `compose_bench` has no `--gpu`
  flag.** Graphite arms are selected by compile-time
  `COMPOSE_BENCH_GRAPHITE` (defined only on `APPLE AND TARGET
  SpellCircleSkia`) and at runtime by `--benchmark_filter=Graphite`. The
  `--gpu` flag belongs to `ComposeGallery --headless`.
- **Copy `graphiteVaryingArm` (`ComposeBench.cpp:754-809`)** — the §19
  family, and the only synced one.
- **It must submit through `submitGraphiteSynced`
  (`ComposeBench.cpp:742-752`, `SyncToCpu::kYes`), not `submitGraphite`
  (518).** The method note is already in the source at 735-741: the first
  draft of a §19 measurement *"had the most expensive shader looking like
  the cheapest because its queue never drained."* A perspective-vs-affine
  pair is exactly that shape.
- **The content must bear GLYPHS**, because 44.6's whole hypothesis is that
  the raster penalty is glyphs falling off the atlas onto path filling.
  `denseBlock` (`ComposeBench.cpp:257-268`) or `scoreboard` are the
  fixtures; the varying-blur panel is the right *skeleton* and the wrong
  *content*. No existing GPU arm touches the CTM at all, so the two arms
  differ only by a `save()/concat(SkM44)/restore()` and a new
  `SkM44.h` include.
- **Keep `Cache::None` or `Cache::Picture`** — the three device bakes refuse
  under perspective anyway (44.1's guards), so a `Cache::Texture` arm would
  measure the local ladder, not the projection.
- **Never `snap()` without `insertRecording`.** `makeRecorderOptions`
  (`SkiaGraphiteContextCommon.cpp:84-101`) sets
  `fRequireOrderedRecordings = true` as a stated PRECONDITION: a discarded
  recording kills the recorder permanently and silently.
- For the pixel half (projected-quad correctness, a w-gate refusal),
  `test/ComposeGpuTest.mm:50-95` is the headless draw+readback harness 44.6
  meant; it takes a `Composer&`, so a matrix arg or an inline copy is
  needed (`DirectPrimitiveMatrix`, `:159-238`, is the precedent for
  copying).

#### What still stands, and it is most of the entry

The measured findings are unaffected by the scope ruling and are the
durable value here — several of them retire seams rather than open them:

- **A camera reaches nothing in this architecture** (44.1). Six frames, six
  angles, `recorded=0 baked=0 painted=0`.
- **Recordings are matrix-independent by law**, and that law predates and
  outlives this entry (44.1).
- **The three device-space bakes are already `!hasPerspective()`-guarded**,
  and that set is a complete enumeration of the sites pinning pixels to a
  device rect (44.1) — **re-verified against source 2026-07-30**, see the
  corrections below. **And PINNED 2026-08-04** — the boundary test §28
  wanted and 44.10's closure of 44.8 step 4 orphaned:
  `ComposeCache.PromotionRefusesAHostPerspectiveCtm` (the `upright`
  guard, observable as `refused(Transformed)`; the no-perspective arm
  promotes — the control) and
  `ComposeCache.ATextureBakeUnderPerspectiveTracksTheCamera` (the
  `Cache::Texture` guard: under a host perspective concat the node holds
  ONE local bake that survives camera motion and stays within tolerance
  of the live twin; texturesBaked == 1 is the discriminating observable,
  because a wrongly-taken device bake mostly self-heals through its
  rect-stability test and pixels alone cannot convict it). Both controls
  run by deleting each guard clause in turn — each pin fails, restored,
  both pass. One finding for the next reader: a perspective·rotateY
  SkM44's 2D projection carries a SKEW term, so it cannot isolate the
  `hasPerspective()` clause — the first draft passed its own control
  through the older skew clauses; the pins use a pure keystone
  (persp-only, upright by every other test the gate makes). The THIRD
  guard, `Cache::Group`'s own `hasPerspective()` clause, stays unpinned
  — its bake path needs a settled bound-scalar subtree and the two
  pinned guards bracket the same refusal shape; take it if a Group ever
  regresses here.
- **The "Composer camera" seam is RETIRED** (44.9). A host concat is one.
  If DESIGN.md's doc-map still names *"a Composer camera with local-space
  bake anchoring"* as an open seam, it should now point here.
- **"Local-space bake anchoring" is not the enabling mechanism** (44.9).
- **The rejected shapes stay rejected on their own reasons** (44.7) — in
  particular (b), the `env::`-ambient camera, which is the one design where
  the crux would have been real, priced by §10g and §3.
- **`maxScaleOf()`'s perspective fallback is the matrix diagonal**
  (44.2b.1) — a live defect in compose *today*, independent of any 2.5D
  feature, since the quantized bake ladder reads it. ~~Still filed, still
  unfixed.~~ **FIXED 2026-08-03** — local linearization (the Jacobian at
  the rect's center and corners, maxed); mechanism, pin and gate results
  in 44.2b.1's own note.
- **`ComposeInternal.h:330`'s filed-gap sentence is stale** (44.2b.2) —
  confirmed; the repair is at `Paint.cpp:1279` and pinned by
  `test/ComposeTestFieldPins.cpp:162-193`; the comment itself was
  repaired 2026-08-03.

#### Corrections to this entry, found by reading it against the source

44.1–44.9 were written from probes. Verifying them against the tree on
2026-07-30 (reads only, no build) found the measurements sound and **five
attributions wrong**. They are recorded because this entry now stands as
*analysis*, which is a thing future readers cite:

1. **"the doc comment already asks for the matrix consolidation" (44.8 step
   0) — NOT AS DESCRIBED.** `NodeTransform`'s comment
   (`ComposeRuntime.h:751-759`) argues for ONE RESOLVER of the eight
   numbers. It never asks for an `SkMatrix` return. Step 0 is a good idea
   on its own merit; it is not a debt the canon already booked.
2. **"the note already in `transformOf`" (44.3 Q5) — WRONG LOCATION.** The
   *"THE GATE IS `pivoted()`, NOT A COPY OF IT"* note is in
   `recordBounds()`, `Paint.cpp:1270-1278`. `transformOf` (1228-1257) has
   no such note.
3. **"the fourth CTM reader is not a caching decision" (44.1b) —
   UNDERSTATED.** `Paint.cpp:2998-3007`'s quantized bake-step ladder reads
   the CTM *and is* a caching decision (it feeds the re-bake test at 3015).
   It is also precisely where 44.2b.1's diagonal defect bites. 44.1b names
   only `Composer.cpp:298`.
4. **The `upright` guard has TWO consumers, not one.** Automatic promotion
   (`Paint.cpp:2421`) **and the §15 split bake** (`Paint.cpp:2620`). 44.3
   Q4's table omits the second.
5. **"`VariationDrive` … reported through the existing `Promotion`-style
   refusal vocabulary" (44.3 Q5, 44.4) — TWO MECHANISMS DESCRIBED AS ONE.**
   `VariationDrive` refuses via a per-instance tri-state latch plus a
   one-shot `SkDebugf` (`Paint.cpp:87-110`). `Prom::` is a separate bitset
   published to the profiler (`Paint.cpp:2400-2435`). `Cache::Group`'s
   `groupWarned` refusal (`Paint.cpp:717-733`) is the closest thing to what
   44.4 describes. Any future refusal path has to *choose* — extending
   `Composer::Promotion` is a public-enum API change.

Also **unsourced: 44.4's "736 → 744".** The only sizes written down are
`Composer.cpp:33-34`'s 2752 → 1288 → 688 B, and the `<= 768` ceiling at
`Composer.cpp:36-37`. 736 could not be found anywhere in the tree. Any
future field addition should re-measure the headroom rather than quote it
from here.

#### Step 0 is UNCLAIMED and still worth doing *(claimed and EXECUTED 2026-08-03 — see 44.8's dated note; ledger byte-neutral)*

44.8 step 0 — `transformOf()` returns the matrix; `paint()`,
`recordBounds()` and `hitInstance()` consume it — **was not started** (no
shell). It survives the scope ruling intact, because it is a consolidation
under the unify/reuse lens and has nothing to do with 2.5D: three sites
build one matrix from eight floats by hand today, and §40 has already been
bitten twice by exactly that duplication.

Read against source, the −40 estimate is credible and the sites are:

| site | today | after |
|---|---|---|
| `paint()`'s matrix build | `Paint.cpp:2116-2128`, 13 lines of pivot arithmetic | one `canvas.concat(m)` |
| `recordBounds()`'s child union | `Paint.cpp:1279-1291`, 13 lines — **already builds an `SkMatrix`** | one assignment |
| `Query.cpp`'s `hitInstance` | `Query.cpp:57-86`, an 18–22 line HAND-UNWOUND inverse plus six float aliases | `SkMatrix::invert` + `mapPoints`, ~5 lines |

`recordBounds()` is the cheapest first move — it is the one consumer whose
output is already the matrix. **Byte-neutral by construction, and it must
be plate-ledger-verified as such; it is not a behaviour change and any
plate movement means the consolidation is wrong.**
