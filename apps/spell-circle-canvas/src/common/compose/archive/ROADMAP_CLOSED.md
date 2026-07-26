# ROADMAP — closed entries (archive)

> **ARCHIVE (2026-07-26).** Full bodies of ROADMAP entries that are
> fully CLOSED/SHIPPED/DELETED. Moved here to keep the live ledger
> lean; entry numbers are stable and stubs in ROADMAP.md preserve
> every §N anchor. A closed entry's numbers and lessons remain
> citable from here; nothing in this file is open work.

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
| `Element::hitTestable(false)` | A keyed full-bleed layout shell with no fill swallowed every hit in the frame, silently and totally, with no opt-out | `Compose.h`, `Query.cpp` |
| `bounds()` absent rather than NaN | Layout runs inside `draw()`, so a query in the same `update()` as the `render()` before it returned width = NaN for every key | `Composer.cpp` |
| `ShapingStyle::aliased` | Skia takes glyph edging from the FONT, never the paint, so `setAntiAlias(false)` was silently ignored and a 1-bit era reconstruction had to leave SigilWeave entirely | `sigilweave/Style.h`, `Shaper.cpp` |
| `Pool::texWindows()` — per-instance UV window | The per-sprite tex rect existed all the way down; the only narrowing was that a Pool could name a cell INDEX and never a RECT | `Instances.h` |
| `shapes::circle(direction, startIndex)` | The winding IS the engraver's convention — glyph-up points radially IN on one plate and OUT on another, and half of all ring inscriptions hand-rolled an OutlineFn over a default nobody chose | `Shapes.h` |
| `Ribbon::widthMax` | `bleed()` cannot look inside a `widthFn`, so a 166 px flow band declared 10 px of reach and was silently clipped | `Brushes.h` |
| `Atlas::filter()`, two-axis `patterns::gridLines` | Instancing's biggest use is tilemaps — pixel grids — and a lattice whose pitch differs per axis is not exotic (an X-COM panel's is 5 × 2) | `Instances.h`, `Patterns.h` |
| `addFixed` exact across draw rates, and a clamp signal | The step count came from an accumulator, which slips one comparison over a long pre-roll; and a frame that DROPPED time makes anything measured on it meaningless | `sigilmotion/Ticker.*` |
| Unit ramps take ANY number of stops | Six, fixed, with the tail clamped — which a 24-run sett and a 72-step sweep both ran out of, from opposite directions | `Material.h` |
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
| **`withKeyframes` repainted through its own hold segments** (§17) | Volatility asked whether a motion was CONNECTED and never whether the value MOVED, so a keyframe hold, a settled easing, or any waypoint pair with equal values repainted every frame while provably constant — 29 ms of a 38 ms frame in one study. The recording made with those numbers is still exact while they hold | `Paint.cpp`, `ComposeRuntime.h` |
| `Ticker::elapsed()` | Steppables could not read the clock they were being driven by, so every study that wanted a phase kept a private accumulator beside the ticker. Three lines. The extraction study then proposed `phase()`/`breath()` helpers over that pattern and WITHDREW them: a helper wrapping an unreachable clock just becomes copy thirty-seven — the getter is the fix, the sugar is not | `sigilmotion/Ticker.*` |
| Four silent traps documented | `custom()` measures ZERO on the main axis and draws nothing; `grain`'s `stretch` multiplies the y frequency until it aliases; a `Pool` position is the cell's CENTRE; and there IS a bound `Fill` — a study concluded there was not and left the binding path over it | `Compose.h`, `Patterns.h`, `Instances.h`, `API.md` |
| **`Cache::Texture` baked a quarter turn at QUARTER resolution** | `getScaleX/getScaleY` are the matrix DIAGONAL, and Skia snaps cos(90°) to exactly zero — so a ±90° node reported scale 0, clamped to the 0.25 floor and linear-upscaled 4×. Measured mean \|Δ\| over ink: 30–32/255 at ±90°, 14.5 at 45°, 2.4 at 180°. Singular values (`maxScaleOf`) instead | `ComposeRuntime.h`, `Paint.cpp`, `Composer.cpp` |
| **Promotion could not SEE a leaf** | A bare box never records a picture (one `drawRect` beats a nested recording) and the promoter only ever measured the replay path — so the corpus's largest cost centre, a full-canvas box carrying one shader, was structurally invisible to it: 663 of 697 ms in `chladni_tab1`, 476 of 568 in `twoadvanced_v4`, 818 of 1115 in `chaucer_astrolabe`, every one of them `live paint` | `Paint.cpp` |
| **Temporal promotion: "stable since last bake", not "static"** | An animated full-canvas material could not be cached at all, though `quantizeTime(10)` already means five frames in six resolve to the SAME shader and therefore the same pixels. Gated on a MEASURED stability rate, so a continuous Output never promotes and a quantized material driven past its step rate demotes itself | `Paint.cpp`, `ComposeRuntime.h` |
| **The profile says WHY a node was not baked** | Every refusal is individually correct and individually invisible; `live paint 663 ms` with nothing beside it is how sixteen studies shipped over the gate. `NodeCost::promotion` + `--bench` printing it | `Compose.h`, `Composer.cpp`, `sketch_main.cpp` |
| Two `PatternBrush` corner defects | The scan straddles the vertex, so the break is first seen one step late and the midpoint guess landed the art up to step/2 past the bend (3 px at advance 24, measured); and the bisector was re-probed at d±2 from a point already past the vertex, so both probes hit the same leg and every corner faced the OUTGOING tangent — except a closed contour's seam, making three corners of a rect agree and the fourth 45° off. Bisect the bracket, carry the leg tangents out with it, plus `cornerAlign` | `Brushes.h` |
| `LayeredBrush` declared no `bleed()` | An additive stack paints wide of the path by construction — `filament()` is 31 px — and the node's recording culls at its own bounds, so every stock brush's halo was clipped | `Brushes.h` |
| **§15 — a node's own paint, cached apart from its volatile children** | Volatility is declared per NODE, so a static full-canvas ground plane carrying one moving disc shared the disc's verdict and lost: 35.19 and 15.13 ms of SELF time on two 888×666 nodes of `genesis_fire`, both reporting "its content changes every frame" about a child. Controlled A/B on one binary — **p50 74.16 → 23.60 ms, 3.15×** — with 35 of 35 studies pixel-identical | `Paint.cpp`, `ComposeRuntime.h`, `Compose.h` |
| **A refusal names EVERY reason, not the first** | `promotion` is a first-match verdict, so a node that is both volatile and clipped reported only `Volatile` — and an author who fixed the volatility met a second refusal nobody had mentioned. `genesis_fire`'s plane has three at once. `NodeCost::refusals` carries them all; `why` is DERIVED from the mask so the two cannot disagree | `Compose.h`, `Paint.cpp`, `sketch_main.cpp` |
| **A device bake at an ANGLE is not pixel-exact — measured, after the opposite was argued** | The relaxation looked obviously right: a device bake concatenates the full matrix and blits at an integer offset, so it "cannot resample at any angle". Measured on a shader-filled box: 0 differing pixels at 0°, **5 at 45° and 2 at 30° (Δ1)**, and **1157 (Δ up to 40) once the bounds overflow the canvas**. A shader's local coordinates come back through an INVERSE, and only an axis-aligned matrix cancels an integer device offset exactly. The gate stays square; the refusal now names `Cache::Texture` as the author's own remedy instead of describing the geometry | `Paint.cpp`, `Composer.cpp` |

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

---

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

---

## 12. `Ticker` has no fixed-timestep helper — **CLOSED**

`ticker.addFixed(hz, fn, maxCatchUp = 8)` calls `fn()` zero or more times
per frame so it advances at exactly `hz`, whatever the host draws at.

Two studies had reinvented it — a cellular automaton at 27 Hz behind the
DOOM PlayStation titles, particles at 24 — and both had to reinvent the
spiral-of-death clamp with it. The clamp's contract is now stated rather
than rediscovered: beyond `maxCatchUp` steps the backlog is **discarded**,
not carried, because carrying it makes the next frame longer, which grows
the backlog. The sim running slow for one frame is the correct failure.

---

## 15. A node's OWN paint cannot be cached apart from its volatile children — **CLOSED**

> Shipped as the SPLIT BAKE in `Paint.cpp`. The section stays with its
> number and its original text below, because three things it predicted
> were wrong and the corrections are the useful part.
>
> **The mechanism.** `paintContent` gained a `Phase` parameter (`All` /
> `OwnOnly` / `ChildrenOnly`) and two skips. A candidate paints in two
> phases from its first eligible frame — which is also how the own half
> gets TIMED by itself, so the promotion is decided on the node's own cost
> rather than on a total that a child dominates. `Instance` grew
> `ownImage`, `ownBakeRect`, `ownPaintMs`, `ownHotFrames`, `ownRebakes`
> and `ownPaintDirty`.
>
> **Measured, on one binary with the feature forced off and on, so that
> nothing but the split differs:**
>
> | | split off | split on |
> |---|---|---|
> | `genesis_fire` p50 | 74.16 ms | **23.60 ms** (3.15×) |
> | `genesis_fire` p99 | 79.04 ms | 28.59 ms |
> | the 888×666 plane, self | 35.19 ms | **0.28 ms**, `SplitOwn` |
> | the second 888×666 node | 15.13 ms | out of the top six |
> | `fallout2_charsheet` p50 | 119.31 ms | 89.90 ms (1.33×) |
> | `twoadvanced_v4` p50 | 569.94 ms | 483.16 ms (1.18×) |
> | `hello` p50 | 1.33 ms | 0.65 ms |
>
> **35 of 35 studies render pixel-identical.** The single apparent
> exception is `genesis_fire`, 56 pixels in a 10×8 cluster, which is the
> study's own `BUILD %.2f ms` readout printing 1.29 against 1.32 — its
> instrument, not its picture. No study regressed.
>
> ### Three corrections to what this section said
>
> **1. `clipContent` and `wipe()` must NOT be excluded.** The text below
> says to exclude them alongside layer effects because all three "wrap
> both halves". Only the layer effect has to be: a filter applies to the
> UNION of own paint and children, and filtering the own half alone is a
> different picture. A clip is opened and closed INSIDE each phase — the
> phase flag skips only the CONTENT — so both halves get the identical
> clip in identical device geometry. That is not a nicety: this section's
> own citation node, `regolith()`, carries `.clip(true)` because it clips
> its disc to a limb outline, **which is exactly why the disc is a child
> rather than a sibling.** Excluding clips would have shipped a feature
> that refuses the one example it was written for, and every other test
> would still have passed.
>
> **2. Foregrounds paint AFTER the children** and therefore belong to the
> children half. The text below implies the own paint is everything except
> the children. It is not — it is a contiguous PREFIX ending at the
> children loop.
>
> **3. The bake must be sized by `ownPaintBounds`, not `recordBounds`.**
> `recordBounds` unions the children in, so it moves every frame a child
> moves — and a bake rect that moves every frame is a bake remade every
> frame, on precisely the scenes this exists for. `ownPaintBounds` is the
> node's box, its decoration bleed and its routed path, and nothing from
> below it.
>
> ### Two traps that cost real time
>
> **`ownPaintDirty` cannot be `paintDirty`.** `markPaintDirtyUp()`
> propagates a descendant's patch to every ancestor, which is right for a
> RECORDING (it baked the child's draw calls) and wrong here: the children
> were never in this bake, and the whole point is that they change.
> `Layout.cpp`'s "a child's position moved" case now passes
> `ownPaint = false` for the same reason. If that inverts, the feature
> silently does nothing and still passes every pixel test.
>
> **The split needs `upright` exactly as promotion does.** It is the same
> construction — an integer device offset concatenated onto the node's
> matrix — so it carries the same ~1 LSB divergence under rotation. It
> shipped without it, in the same hour that fact was established three
> lines above, and a POSITIVE CONTROL is what caught it. That is the
> commonest failure in this program: a fix that lands on one path and not
> its sibling, each reading correctly alone.

---

## 15 (original text). A node's OWN paint cannot be cached apart from its volatile children — *new, measured, and the next promotion item*

Found by the per-node profiler after leaf promotion landed, and it is the
same shape as that finding: a cost centre the cache model cannot see,
for a structural reason rather than an authoring mistake.

Volatility is per NODE. A node whose own fill is an expensive static
material, and one of whose children moves, is `subtreeVolatile` — so
nothing about it is cached, and the full-canvas shader is re-rasterized
every frame in order to redraw a small moving child on top of it.

`genesis_fire` is the citation: two 888×666 nodes at **34.9 ms and
14.9 ms of self time**, both reporting `not baked: its content changes
every frame`, and in both cases the volatility is a child. `regolith()`
is a generated ground plane — a three-layer `Material::blend` of a
radial ramp, grain and speckle — carrying one 264 px disc that rides a
bound Output. The plane is static. The disc is not. They share a
cacheability verdict, and the plane loses.

The profiler already separates them: `selfMs` excludes children, which
is how the number above was measured at all. What does not exist is a
way to act on it. The node's own paint (fill + background/overlay
decorations) is a layer with its own volatility, distinct from the
children painted over it.

Wanted, roughly in order of how much they change:

- **Split the cache at the node's own paint.** Bake the self-layer when
  it is stable, paint volatile children live over the blit. Pixel
  identity holds by the same argument promotion already makes, as long
  as the children composite srcOver onto it — which is the common case
  and is checkable (`subtreeReadsBackdrop` already computes the
  awkward half).
- The authoring workaround, which is what the corpus does and should be
  documented until the above exists: **lift the moving child out into a
  sibling** so the expensive plane is a static leaf and promotes on its
  own. Cheap, and it costs the author the clip/containment relationship
  that made them nest it in the first place — `regolith()` clips its
  disc to a limb outline, which is exactly why it is a child.

Note the ordering trap this exposed in the refusal reasons: `Volatile`
is reported before `Filtered`, so an author who fixes the volatility
here then meets a second refusal (`clip(true)`) behind it. That is
honest but it costs an iteration; a reason that could name *all* the
conditions rather than the first would be better. **That change belongs
in this batch**, because splitting the self-layer creates more nodes
with several simultaneous reasons, not fewer. The shape: keep
`NodeCost::promotion` as the primary outcome and add a `uint16_t`
refusal mask beside it, so the existing values and every test that
asserts on them keep working.

### The pixel-identity argument, stated before the implementation

Promotion's argument does not carry over, so it has to be made again
from the start. Promotion bakes a WHOLE subtree and blits it in place of
everything it contains; the claim is "an integer device-space
translation cannot change rasterisation". Here the bake replaces only
PART of what the node paints, and live children are drawn over the blit
afterwards — so the claim needed is different and strictly stronger:

> Painting the self-layer into a transparent device-aligned surface,
> blitting it, and then painting the children over the result must
> produce the same pixels as painting self-then-children directly onto
> the canvas.

That holds exactly when **every child composites srcOver onto the
node's own paint**, and fails otherwise:

- A child with a non-srcOver blend resolves against what is beneath it.
  After a blit that is the same destination, so this one is actually
  FINE — the blit lands before the children, unlike promotion, where the
  children were inside the bake. This is the one place the split is
  *safer* than promotion.
- A child with a **backdrop filter** samples the destination, which is
  now a blitted copy rather than freshly rasterised pixels. Identical in
  value, so also fine.
- The real failure is the **self-layer itself** reading the backdrop —
  a non-srcOver blend or backdrop filter on the NODE, where the bake
  would resolve against transparent black. `subtreeReadsBackdrop`
  already computes exactly this and is the condition to reuse; it is
  currently computed for the whole subtree, so it needs splitting into
  own-paint and children halves.
- Antialiasing at the boundary between self-paint and children is not a
  concern: they are separate draws either way, and srcOver of a
  coverage-modulated child over a blitted destination is the same 8-bit
  sequence as over a directly-rasterised one. That is the same argument
  the leaf promotion rests on and it is already asserted by
  `PromotesAnExpensiveLeafAndKeepsEveryPixel`.

So the eligibility rule is: **the node's OWN paint must not read the
backdrop, and the node must be upright and unscaled in device space**
(the same device-snapping requirement, for the same reason). Children
may be anything at all. The test must render a node with an expensive
static fill under a moving child and assert zero differing pixels
against the same tree with the split disabled — every frame across the
child's motion, not one still.

---

## 17. `withKeyframes` is live volatility even where its value is constant — **CLOSED**

> Shipped as `Instance::scalarMemo` in `Paint.cpp`. The section stays with
> its number. One correction to what it said when it was filed: **§15 and
> §17 are not the same change.** They share a slogan — "provably not
> changing, believed to be changing" — and nothing else. §17 extends an
> INPUT MEMO (compare what you baked with against what you have; the
> material memo already did exactly this for shaders). §15 splits paint
> GRANULARITY (a node's own layer, separate from its children). Merging
> them would have produced one design serving two mechanisms.
>
> Kept deliberately disjoint from `liveMatOnly` rather than unified with
> it: the two memos compare different things — a shader pointer and five
> floats — and unifying them meant rewriting fifteen call sites of the
> subtlest function in the library to gain nothing. A node carrying BOTH a
> live material and an animated trim takes neither memo, which is the
> conservative answer and costs exactly what it costs today.
>
> Scope is the content-volatility slots only: trim start/end/offset, wipe
> fraction, glyph progress. Not the transform slots — those are paint-only
> volatility and already replay the content picture under a live matrix.
> Note `TextPath::at` is a plain float, not a `PropValue`, so an animated
> `onPath` position is a re-describe and is NOT covered by this.

Reported with a number: **29 ms of a 38 ms frame**, seven text-on-path
runs whose keyframe paths were between waypoints and therefore not
changing at all.

Volatility asks "is a motion connected", not "did the value change". A
keyframe path with a hold segment, an easing that has settled, or any
waypoint pair with equal values is *provably* constant for that stretch,
and the node repaints every frame anyway.

This is the same shape as the two findings that preceded it — a thing
that is provably not changing which the machine believes is changing —
and the mechanism to fix it already exists in two places. `Material`'s
resolve memo compares the byte-identical digest of its varying inputs and
returns the same shader; temporal promotion turns that into "stable since
the last bake". The scalar case wants exactly that: record the animated
values a node's recording was baked with, and treat the recording as
valid while they still match. That generalises `liveMatOnly`/`liveStable`
from materials to every animated scalar the content reads (trim, glyph
progress, wipe fraction, `onPath` `at`).

Not the transform slots — those are paint-only volatility and already
replay the content picture under the live matrix. This is about the ones
that rebuild painted geometry.

---

## 24. `layouts::stickerScatter` is DELETED, and the record is the point

Zero users. Refused in writing by the one scene that wants a sticker
ladder, which kept its hand-authored `{−25,−15,−20,−15,…,+8}`. Conceded
in its own doc comment. Shipped anyway.

The generator encoded one reference plate's scatter as six parameters —
decaying rotations with the last item flipped positive, x-jitter,
overlapping pitch, shuffled z. The refusal is the interesting part and
it is not "the generator was inaccurate": it produced ladders *in that
family*, and **a ladder in that family is exactly what the design is
not.** The value of the reference ladder is that somebody CHOSE it.

> A scheme belongs in `Layouts.h` when the placement is a FUNCTION the
> author would otherwise write out — a radial ring, a modular grid, a
> baseline rhythm. It does not when the placement IS the design
> decision. Parameterising a judgement produces something that can only
> be right by accident, and it costs a maintained API forever.

The header keeps that paragraph where the code was. `archive/EXTRACT.md`
§1.2 carries the long version.

---

## 26. Two studies are not reproducible captures, and every pixel sweep will blame the wrong change

`genesis_fire` prints `BUILD %.2f ms` and `slitscan_2001` prints
`... BAKE %d ms` — each measures itself and draws the number into its own
plate. Measured, **same binary, two consecutive runs, same `--at`**:

```
genesis_fire    34 differing pixels   (BUILD 1.29 vs 1.32 ms)
slitscan_2001   16 differing pixels   (BAKE  39   vs 50   ms)
kumiko_asanoha   0 differing pixels   <- the negative control
```

So a corpus pixel sweep reports these two as CHANGED by any patch,
including a patch that changes nothing, and the natural reading of that
is "my change did something". It cost this session two rounds of cropping
and looking to establish that a 3.15× perf win had altered no pixels
anywhere — and the only reason it did not cost more is that the diffs
were tiny and clustered, which is exactly the shape a real subtle
regression also has.

This is the same family as the gallery's machine-dependent capture frame,
and the same corollary applies: **an instrument that draws its own reading
into the thing being measured makes that thing untestable by comparison.**
The negative control is what turns "explaining away a difference" into a
measurement, and it should be the standard move: re-render with the SAME
binary before attributing any diff to a change.

**CLOSED — `--deterministic`.** `SketchContext` carries the flag and a
channel for it:

```cpp
double measured(double value, double pinned = 0.0) const;   // ctx.measured(buildMs)
bool   deterministic;                                       // whole-panel case
```

Deliberately NOT a list of blanked readouts. The unit is **any value the
sketch computed from its own execution rather than from its data** — a
build time, a bake cost, a live node count, a frame counter. A node count
is usually stable and a bake time never is, and both belong here, because
*"usually stable"* is exactly what makes the eventual diff mystifying.
The host cannot identify such values by inspection, so the study routes
them and the flag pins them.

Measured, three runs each, same binary, same `--at`:

```
                 without the flag        with it
genesis_fire       33 differing px       0
slitscan_2001      20 differing px       0
```

And inert where it is not read — `kumiko_asanoha`, `thaumonomicon`,
`stroke_atlas`, `penrose_paving` all render identically with and without.

> Masking the known regions in the sweep tool was the third option and
> remains the worst: the mask is a second thing to keep in sync, and it
> would have had to grow an entry every time a study learned to measure
> itself.

### A postscript, because the fix reproduced the bug it was fixing

The first attempt "did not work" — both studies still differed under the
flag. The flag was fine. The private test harness linked a **stale
`SketchHost.o`**, compiled against the previous `SketchContext`, so the
new field was never propagated and the two struct layouts disagreed.

That is the **third** form of the same error in one session: a stale
binary, a stale baseline, and a control that had been rebuilt with the
code under test. Each one produced a confident, plausible, wrong reading,
and each was caught only by a control built for the comparison. It is
worth stating in the imperative:

> Before believing a negative result about your own change, check that the
> thing you ran contains it.

---

## 26b. `renderSlot()` on a name that does not exist was SILENT — **CLOSED**

The symptom is not "my slot is empty". It is **"my slot lays out W × 0"**,
which reads as a layout bug and sends you into Yoga — and it was filed as
exactly that, twice, before anyone looked at the name.

The cause is almost always the one the message now names: **`slot(name)`
stores the name in `key`**, so any later `.key(...)` on that element
renames the slot. No type error, no second field to disagree with itself,
and `renderSlot` then returned silently. It cost an hour, and the probe
written to investigate it reproduced the same mistake for the same
reason.

`renderSlot` now warns once per name, lists the slot names that DO exist,
and names the rename trap with the caller's own string substituted in —
which turns the diagnosis into one read. Same shape as §27's remedy: the
library cannot know which spelling the author meant, so it says what it
saw instead of choosing.
