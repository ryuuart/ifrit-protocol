# SigilCompose — what the studies asked for

This list is not speculation. Every entry below came out of *building
something real* and finding the library could not express it: seven
gallery scenes rebuilt on referenced geometry, and a set of study
sketches under `sketch/sketches/` each reconstructing a named artefact —
a Dead Space 2 bench screen, a hinoki asanoha ranma, Nightingale's 1858
mortality plate, 2Advanced Studios' v4 Prophecy interface, the DOOM
PlayStation title flame.

The rule the program ran on: an author who hits a wall works around it
and **writes down what the natural API would have been**. Entries are
ordered by how many independent studies asked for the same thing, because
that is the only signal here worth trusting.

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

---

## 1. Bindings that cannot be shaped — *four studies*

A bound `choreograph::Output<float>` lands on the property **raw**. There
is no scale, no offset, no curve at the binding site.

The consequences compound. A phase that lives in `[0,1]` — which is what
`trim()` and `opacity()` want — cannot drive a translation in pixels
without a **second** Output updated in the same steppable. One per-piece
progress driving both opacity and scale needs two Output vectors: the
kumiko study carries 1028 objects where 514 would do, with the easing
written in the tick loop, far from the properties it shapes.

```cpp
// wanted
.translateX(bind(&phase).scale(240).offset(-70))
.opacity(bind(&progress).map(ease::outBack()))
```

Natural API: `PropValue(const Output<float>*, std::function<float(float)>)`,
or a `.map()`/`.scale()`/`.offset()` chain on the binding. The paint path
already reads through a pointer; this is one call site.

## 2. Instancing covers "many copies of one thing", not "many variations of one recipe" — *four studies*

`Instances.h` names inventory cells and node-graph nodes as its cases.
Both are usually **labelled**, and a `Pool` carries only position,
rotation, uniform scale, tint and frame. So:

- a labelled node graph cannot be one atlas stamp (Dead Space);
- a lattice of 514 differently-mitred boards has no flyweight (kumiko);
- a staggered assembly has no per-instance progress or delay (kumiko, 2Advanced);
- strips of varying length are outside RSXform's uniform scale (kumiko);
- press-wire rows, chips and readout windows are all ineligible (2Advanced).

Also `place::repeat` writes lanes it does not own — it clobbers
`tints[i].fA` and cannot set `frame`, so every mixed-frame call site
re-walks the lanes by hand and calls `touch()` again.

Natural API: optional per-instance **alpha**, **delay** and **short
string** lanes; non-uniform per-instance size; generators that write only
their own lanes. Or, failing that, say plainly in the header that
labelled lists stay real elements — the doc currently reads as if they
are covered.

## 3. `outline()` can never prune — *three studies*

It takes an incomparable `std::function<SkPath(SkSize)>`, so every shaped
node re-records on each `render()`: 514 in the kumiko lattice, every
chamfered panel at four nesting depths in the 2Advanced reconstruction.
A runtime-parameterised pattern re-records its whole panel per change.

`Material` already solved exactly this by comparing **structurally by
recipe** rather than by shader pointer, and `Brushes.h` solved it with the
`GeometryOp` value / `PathOp` lambda split. Shapes want the identical
move: a comparable `Outline` value (kind + params) covering the stock
generators, with the raw lambda as the escape hatch that never prunes.

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

## 7. `routers::orthogonal()` is unusable for its most natural application

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

## 8. Text: three missing spellings

- **No glyph-level stroke.** `Element::stroke()` dresses the node's *box*.
  Thickening engraved display type means dropping to
  `PaintStyle::addUnderlay` with a hand-built stroke paint.
  Wanted: `Element::textStroke(width, Fill)`.
- **No way to shape a run without building an Element.** `measure()` is
  per-Element, so hand-placing 230 glyphs costs 230 layouts. Wanted:
  `FontContext::measureRun(u8string, TextStyle) -> vector<float>`.
- **No binding path for text CONTENT.** `PropValue` covers floats, colors
  and fills but not `u8string`, so a counter or a timer must re-describe.
  `slot()`/`renderSlot()` is the right answer and is genuinely cheap —
  but the obvious first attempt is to hunt for `text(&output)`, and
  API.md never names slots as *the* counter idiom.

## 9. Decorations: adaptors and frames

- **No inset adaptor.** "The same bevel again, 6 px in" is the whole
  vocabulary of nested chrome, and there is no way to run a decoration
  against an offset outline. `shapes::EdgeSlice` is the exact precedent —
  an adaptor that rewrites `PaintContext::outline` and delegates. Its
  sibling `shapes::inset(px, Decoration)` is two lines of library and
  turns every nested frame into composition.
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

## 10. `Effect` has no live uniforms

`Material` solved this with `uniform(name, &output)` and a volatility
contract. `Effect::shader(fx, uniforms)` takes constants only, so
animating a ripple phase or a bloom threshold requires a full re-describe
per frame.

## 11. `Ticker` has no fixed-timestep helper

`add()` yields variable `dt` only, so every simulation-shaped sketch
reinvents the accumulator **and** its spiral-of-death clamp. Conspicuous
because `Material::quantizeTime(hz)` already exists: the library has
declared choppiness for shaders and nothing for logic.

Natural API: `ticker.addFixed(hz, fn)`.

## 12. Sampling, and the pixel-art path

`Paint.cpp`, `Decorations.h` (Slice), `Pattern.h`, `Instances.h`,
`Brushes.h` and `Web.h` all hardcode `SkFilterMode::kLinear`.
`Material::image()` is the **only** place a caller can pass
`SkSamplingOptions`, so any pixel-art, tilemap or simulation-buffer
content drawn through the `image()` element factory is silently blurred —
and the fix is discoverable only by diffing two signatures.

Natural API: `Element::sampling(SkSamplingOptions)` on image leaves, plus
a `sampling` field on `Slice` / `PatternBrush` / `Atlas`.

## 13. Smaller, but each cost someone an iteration

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
- **`shapes::arrow()` does not exist**, and every HUD, gizmo and diagram
  draws one.
- **`util::disc(centre, radius)`** — five lines; two sketches wrote it
  locally because every inscribed-in-the-box shape in a polar chart needs
  `width(2r).height(2r).centerAt(c)`.
- **A fixed `width()` flex child still shrinks**, and the failure is
  silent overlap. Faithful Yoga semantics, so not a bug — but `width(150)`
  reads as "this is 150" at the call site. One sentence in API.md's layout
  section (pair with `.shrink(0)` when you mean it) would pay for itself.

---

## Host and tooling

- **A guest crash surfaces only as exit 139.** `ComposeSketch` reports
  nothing when a sketch dylib faults. Catching SIGSEGV in the guest and
  naming the sketch would have saved most of the time four agents spent
  localising one bad shader.
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
