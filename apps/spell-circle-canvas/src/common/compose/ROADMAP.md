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

Two more details that the lane list does not cover, both from the Chladni
plate's 9,580 settling sand grains:

- **`tints()` is the only per-instance opacity lane**, so fading a subset
  means rewriting RGBA every frame when only alpha moves.
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
`custom()`. The cost is not only pruning: it also forfeits `trim()`,
which was the natural spelling for "the curve draws itself over 300 ms",
and that had to be hand-rolled as a clipRect over a progress Output. A
`PropValue`-aware outline, or `trim` on a `PathFormat` (§7), covers it.

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

## 6. No directional wipe

`trim()` walks the **perimeter**, so on a filled shape 0→1 sweeps a wedge
round the outline rather than revealing it along an axis — which is the
actual gesture for a piece sliding into its slot, a panel opening, a bar
filling. `scaleX/scaleY` covers the axis-aligned case now; an arbitrary
angle still has none.

Natural API: `.wipe(angleDeg, PropValue<float>)` — an axis-aligned clip
fraction, rotatable.

## 7. One trim window per node

`trim()` is an `Element` property, and decorations receive the **already
trimmed** outline. So a second window over the same geometry is not a
second `stroke()` — it is a duplicate node that rebuilds and re-measures
the same path. The Vertigo cards pay for a 2000-segment precessing rosette
twice each, purely so a bright pen tip can ride just behind the drawn
head.

That gesture is not exotic: the comet on a radar sweep, a marching
highlight on a border, a second progress arc on one ring, the leading spark
on a drawn signature. All of them are one geometry with two windows.

Natural API: `trim` as a field on `PathFormat`, so each stroke and each
decoration carries its own window over the shared outline, with the node
property staying as the shorthand.

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
- **No glyph-level stroke.** `Element::stroke()` dresses the node's *box*.
  Thickening engraved display type means dropping to
  `PaintStyle::addUnderlay` with a hand-built stroke paint.
  Wanted: `Element::textStroke(width, Fill)` — the sibling of the item
  above (that one *hollows* a face, this one *thickens* one).
- **No way to shape a run without building an Element.** `measure()` is
  per-Element, so hand-placing 230 glyphs costs 230 layouts. Wanted:
  `FontContext::measureRun(u8string, TextStyle) -> vector<float>`.
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

## 10b. Animated lines and paths: two missing bindings

- **`dashPhase` has no bound-Output form.** `PathFormat::trimPhase` takes
  a `ch::Output<float>*` and declares `animated()`; `dashPhase` — in both
  `PathFormat` and `lines::Line` — is a plain float. So marching ants can
  only be had by re-describing every frame, which defeats the pruning the
  library is built on. Marching dashes are the commonest animated-line
  idiom in map and diagram UI; the KSP study wrote a 25-line
  `MarchingDots` DecorationScheme instead (the seam worked, which is the
  good news). Wanted: `dashPhaseBinding` beside `trimPhase`.
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
- **No paint slot between the fill and the content.** `foreground()`
  paints ABOVE children, so a hazard stripe greys out its own digit;
  `background()` hides under the fill. Every textured button with a label
  meets this and works around it with a sibling stack. Wanted:
  `Element::overlay()` — one entry in the paint order.
- **`patterns::grain` over a near-transparent base composites as its own
  luminance** rather than modulating what is beneath; the first nebula
  came back a white cloud at 15% alpha. The header warns about `noise` vs
  `grain` channels; it should also say grain wants an OPAQUE surface.

## 11. `Effect` has no live uniforms

`Material` solved this with `uniform(name, &output)` and a volatility
contract. `Effect::shader(fx, uniforms)` takes constants only, so
animating a ripple phase or a bloom threshold requires a full re-describe
per frame.

## 12. `Ticker` has no fixed-timestep helper

`add()` yields variable `dt` only, so every simulation-shaped sketch
reinvents the accumulator **and** its spiral-of-death clamp. Conspicuous
because `Material::quantizeTime(hz)` already exists: the library has
declared choppiness for shaders and nothing for logic.

Natural API: `ticker.addFixed(hz, fn)`.

## 13. Sampling, and the pixel-art path

`Paint.cpp`, `Decorations.h` (Slice), `Pattern.h`, `Instances.h`,
`Brushes.h` and `Web.h` all hardcode `SkFilterMode::kLinear`.
`Material::image()` is the **only** place a caller can pass
`SkSamplingOptions`, so any pixel-art, tilemap or simulation-buffer
content drawn through the `image()` element factory is silently blurred —
and the fix is discoverable only by diffing two signatures.

Natural API: `Element::sampling(SkSamplingOptions)` on image leaves, plus
a `sampling` field on `Slice` / `PatternBrush` / `Atlas`.

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
- **`Pattern` cannot pan.** `rotate()`/`scale()` only remap the shader
  matrix at describe time, so a conveyor needs the pattern written twice —
  once baked, once as hand-written SkSL with a bound uniform. Wanted:
  `Pattern::offset(PropValue<SkPoint>)` under the paint-only volatility
  contract bound transforms already have.
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
