# SigilCompose — first-principles review & direction

Companion to `DESIGN.md` (current architecture), `API.md` (current surface),
`STRESS_TESTS.md` (acceptance numbers). This is a **review pass**: what
Compose is, the one structural tension it has, and a concrete direction to
grow it into a data-driven *graphics* engine — game-UI chrome, motion
posters, VFX, node graphs, sci-fi consoles, transit maps — without losing
the retained/cacheable spine that already makes it good.

Grounded in a cross-domain survey (Unreal Slate/UMG, Godot StyleBox,
Flutter, SwiftUI, Jetpack Compose, skottie/Lottie, Rive, PixiJS, WPF/Direct2D
brushes, Inigo Quilez 2D SDF, LOOM/transit-map research, tldraw/Excalidraw
bindings, litegraph/React Flow/Cytoscape, Alacritty, GSAP). Named sources are
cited inline so each recommendation is traceable.

**Verification (2026-07-21):** every load-bearing claim in this document was
re-checked by an adversarial Fable 5 pass against primary sources (60+
verdicts across all 8 domains, plus a read-only code audit of the landed
work): **zero design-level refutations** — citation-level corrections are
folded into the text below. The §9 split was independently verified
behavior-preserving against the pre-split monolith by normalized
function-body comparison.

---

## 0. TL;DR

Compose is **strong on one axis and nearly empty on the other, and the two
were — until the P0 split landed — entangled in one 1489-line file.**

- **Strong axis — the retained structural spine.** Keyed reconciliation,
  `memo`, the structural prune, Yoga layout with SigilWeave-measured text,
  stacking paint, *automatic* picture/texture caching gated by declared
  volatility, the derive phase, paint-only transforms. This is "React/Flutter
  minus the framework," it is coherent, and it is the thing to protect.
- **Missing axis — a material / visual vocabulary.** Anything past *rounded
  box + solid/gradient fill + text + image + dashed stroke + 9-slice + contour
  stamp* falls through `custom()` into a raw `SkCanvas` lambda — losing
  composition, cache granularity, animatability, hit-testing, and batching all
  at once.

**The felt "too many concepts" problem is misattributed.** It is not the
reconcile/layout/cache kernel that overloads newcomers — that is one clean
idea. It is (a) the missing material layer forcing the raw-Skia escape
constantly, and (b) the *box model baked in as the default* (`Kind::Box`,
rrect geometry, Yoga-always) so freeform things — lines, graphs, particle
fields, chaotic stars — feel like they fight the grain.

**Verdict, unanimous across all eight research domains: do not split into
multiple frameworks.** Keep one kernel; make three seams pluggable
(*geometry ⊗ material ⊗ placement*); name the *composition modes* as documented
profiles over that one kernel. The single highest-leverage change is a
polymorphic **`Material`/`Brush`** value replacing `Fill` — the seam Slate
calls `FSlateBrush.ResourceObject` and Jetpack Compose calls `Brush`.

The rest of this doc: the diagnosis with evidence (§1–2), the architecture
(§3), the four capability additions — Material (§4), geometry-beyond-boxes
(§5), instancing/text (§6), the isolation boundary + taming `custom()` (§7),
performance (§8), the `Composer.cpp` split (§9), each target use case
expressed concretely (§10), the gallery rewrite (§11), phasing (§12), and the
onboarding restructure (§13).

---

## 1. What Compose is today (honest inventory)

The kernel already owns, and owns well:

| Concern | Mechanism | State |
| --- | --- | --- |
| Structure | `Element` values, keyed reconcile, `memo`, structural prune | solid |
| Layout | Yoga flex + `stack()` + `LayoutScheme` seam; SigilWeave text measure/baseline | solid |
| Stacking | `(zIndex, order)` paint, stacking contexts, `blend()`, paint-only transforms | solid |
| Caching | auto SkPicture of static subtrees, `Cache::Texture`, declared volatility, per-node partial invalidation | solid |
| Animation | `ch::Output` bindings, implicit `transition()`, retarget-from-current, unmount-cancels | solid |
| Derive | `flowAround` (ExclusionFlow), `connector()`/`Router` | present, narrow |
| Shape | `outline()` + `shapes::` star/polygon/blob/squircle/`rounded()`/`edges()` | present |
| Decoration | `PathFormat`, `Slice` (9-patch), `ContourWalk` (arc-length stamp) | present, thin |
| Effect | `.effect()`/`.backdrop()` SkImageFilter + SkSL | present |
| Query | `bounds()`, `paragraphLayout()`, `hitTest()`, `snapshot()` | solid |

66 unit tests pin these behaviors (56 at review time; P0/P1 added 10); the
perf gate reads well (cached 100-row
draw ~0.4 ms raster, ~0.31 ms Graphite; 1M plus-blended sprites at 3.7
ms/frame on Graphite as one `drawAtlas` leaf).

The seams that make the "one kernel, many modes" future viable **already
exist**: `LayoutScheme` (custom placement), `DecorationScheme` (custom paint),
`Router` (custom edge geometry), `outline()` (custom silhouette),
`snapshot()` (element-tree-as-a-brush). The proposal below mostly *fills in*
these seams and adds one new peer seam (Material), rather than restructuring.

---

## 2. The core diagnosis, with evidence

Every place the current library reaches for richness, it leaves the retained
model and hand-writes Skia:

- **`gallery/ScenesGame.h` — `RpgHudScene`**, labeled "vibrant game UI, the
  practical-usage showcase." The health/mana bars, the ember sigil (~40 lines
  of raw Skia), the character portrait, and every inventory icon are `custom([]
  (SkCanvas&, const PaintContext&){…})`. Compose positions the boxes; the
  *look* is hand-rolled and uncacheable (`Cache::None` throughout).
- **`gallery/OrnamentKit.h`** — 451 lines of raw-Skia drawing utilities
  (`appendCubic`, `appendSpiral`, `taperedStroke`, `drawDiamond`, `sprig`,
  `edgeFlourish`) that *cannot be expressed as library values* and so live
  outside the library entirely.
- **`sketch/sketches/flourish_border.cpp:108`** — the comment reads:
  *"Calligraphic-stroke primitives (self-contained ports of the ornament kit,
  which is not on the sketch include path — pure public Skia)."* The author
  **re-typed** the drawing primitives because there is no reusable vocabulary
  for them. This is the friction in one line.
- **The gallery scenes are "one seam per scene" proofs**, not product-grade
  artifacts: `ScoreboardScene` demonstrates reconcile+transitions,
  `BlendScene` demonstrates blend-as-layer, `DeriveScene` demonstrates
  flowAround+connector — each is a unit test with a face. None is a *compelling
  composition* that shows the library is worth reaching for. (You already flagged
  this; agreed — toss and rewrite, §11.)

This is exactly the anti-pattern every mature system bounds and brackets
rather than leaving open. Slate, Noesis, Godot, Flutter (`CustomPainter` +
`shouldRepaint`), and Skia (`SkDrawable`) all refuse the *unbounded*
raw-canvas callback: bounds are mandatory (`SkDrawable::onGetBounds` is pure
virtual), invalidation is a declared contract, and richness routes through
typed retained elements (escape hatches exist — Slate `MakeCustom`, SwiftUI
`Canvas`, Compose `drawBehind` — but always bracketed). The unbounded form has
no bounds (breaks cull/hitTest), no cache, no invalidation signal, and no
batching — and worse, an un-boundaried one **forces its ancestors to
re-record** (which is literally what happens today — see §8.1).

**Reframe:** the fix is not fewer concepts in the kernel. It is (1) a *material
vocabulary* so richness is a value, not a lambda; (2) making the *box* one
geometry among several instead of the definition; (3) *naming the modes* so a
newcomer learns a spine and picks a lane, rather than meeting all seams at once.

---

## 3. The architecture: one kernel, three seams, named modes

### 3.1 The mental-model reframe (the onboarding fix)

State the kernel as **one sentence with four slots**:

> An **Element** is **geometry ⊗ material ⊗ content**, **placed** by its
> parent's layout, **animated** by bindings, and **cached** by declared
> volatility. The **Composer** reconciles, lays out, derives, paints, and
> caches a tree of them.

Everything the library does hangs off those nouns. "Box" is not in the
sentence — it is the *default* geometry (rrect) with the *default* placement
(Yoga flex). A star is a different geometry; a subway line is a different
geometry (a routed polyline) with a different placement (anchored to
stations); a particle field is a leaf with an instanced material. Same kernel.

This is the universal factoring the survey found: Flutter splits
Widget/Element/RenderObject and generalizes `Decoration`; SwiftUI makes
`Layout` a protocol and paint a modifier chain; Jetpack Compose makes
`MeasurePolicy` and `Modifier` swappable; mxGraph unifies vertex and edge in
one `mxCell`. None hardwires "box."

### 3.2 The three orthogonal seams

| Seam | What it decides | Today | Grows into |
| --- | --- | --- | --- |
| **Geometry** | the node's silhouette / path | `corners()`, `outline()`, `connector()` | `GeometryScheme` modifier stack (Trim/Round/Offset/Dash/Merge); `route()`/`rail()` first-class lines; `CustomGeometry` (SkVertices) |
| **Material** | how every pixel of paint is produced | `Fill{None,Color,Shader}` | polymorphic `Material` value: solid/gradient-ramp/sprite/9-slice/SDF/noise/blend/below/sksl, animatable uniforms |
| **Placement** | where children sit | Yoga flex, `stack()`, `LayoutScheme` | + instancing pool; + `BaselineGrid`/`ModularGrid`; + along-path/anchored |

These are independent: any geometry can wear any material and be placed by any
scheme. That orthogonality *is* the "primitives that work together" you want.

### 3.3 The composition modes (documented profiles, not libraries)

The recurring modes are the same spine with different plug-ins on the three
seams — this is the strongest cross-domain verdict, and it maps onto the
existing five-phase pipeline (Describe → Layout → Derive → Paint → Frame) with
near-zero structural change:

| Mode | Geometry | Placement | Material emphasis | Prior art |
| --- | --- | --- | --- | --- |
| **Flow / box UI** | rrect | Yoga flex | fills, 9-slice, SDF chrome | React/SwiftUI/Slate |
| **Freeform / canvas** | outline/CustomGeometry | absolute/along-path | SDF, noise, sksl | Cavalry, Figma canvas |
| **Graph (node+edge)** | nodes = stamped chrome; edges = `route()` | force/layered (dagre/ELK) in Derive | node chrome material + edge flow | litegraph, React Flow, TouchDesigner |
| **Rail / line (transit)** | `rail()` polyline is the component | octilinear router in Derive | per-line stroke, along-path labels | LOOM, GTFS shapes |
| **Field / particle** | leaf | SoA instance pool | one instanced material | PixiJS `ParticleContainer`, Godot `MultiMesh` |
| **Motion poster / editorial** | rrect/text | `BaselineGrid` + Yoga | gradient/variable-font/glyph FX | Müller-Brockmann, skottie |

A newcomer reads the kernel, then **one mode quickstart**, and sees only the
concepts that mode needs. That is the "easier to break in" fix (§13). "Poster"
survives as a mode, exactly as `DESIGN.md` intended it to survive as a use
case.

---

## 4. The missing layer: `Material` / `Brush` (the #1 change)

Introduce `Material` (alias `Brush`) as a **polymorphic immutable value with
the same discipline as `Element`**, absorbing today's three-case `Fill`. It is
a tagged tree of paint nodes that **compiles to one `sk_sp<SkShader>`** and is
usable **everywhere paint is taken** — `fill()`, `PathFormat.strokeFill`,
decoration fills, text color, along-path stamps. This is the seam Slate calls
`FSlateBrush.ResourceObject` (one polymorphic slot where a color, a texture, a
9-slice, or a UI-domain *material* are interchangeable) and Jetpack Compose
calls `Brush` / WPF calls `Brush : Freezable`.

### 4.1 Node kinds (reconciling every domain)

```cpp
// <sigilcompose/Material.h> — the polymorphic paint value.
class Material {
public:
  // ---- leaves --------------------------------------------------------
  static Material solid(SkColor4f);
  // N-stop ANIMATABLE ramp (skottie merges color+opacity buffers by
  // position on sync) — not two opaque endpoints.
  static Material linear(SkPoint a, SkPoint b, std::vector<Stop>);
  static Material radial(SkPoint c, float r, std::vector<Stop>,
                         SkPoint focal = {});      // focal per skottie
  static Material sweep (SkPoint c, float startDeg, std::vector<Stop>);
  static Material sprite(Sprite);                  // atlas/9-slice aware (§4.4)
  static Material sdf   (SdfShape, SdfStyle);       // shape+border+glow+shadow, ONE pass (§4.3)
  static Material noise (NoisePreset);              // fbm/worley/domainWarp presets
  static Material sksl  (sk_sp<SkRuntimeEffect>, Uniforms);   // the one raw escape

  // ---- combinators ---------------------------------------------------
  // Layered stack: nested SkShaders::Blend, ONE flattened program, ONE
  // draw — never stacked saveLayer (the design doc's own flagged risk).
  static Material blend(std::vector<std::pair<Material, SkBlendMode>>);
  Material &recolor(Ramp lut);         // 1-D LUT gradient-map recolor (the L4D2
                                       // zombie-variation shader — Grimes/Valve, GDC 2010)
  Material &below();                   // sample the sibling-snapshot beneath (§4.5)

  // ---- animation -----------------------------------------------------
  Material &uniform(std::string name, PropValue<float>);
  Material &uniform(std::string name, PropValue<SkColor4f>);
  // uTime / uResolution / uContentScale / uHit auto-injected from
  // PaintContext. Reading uTime IS the volatility declaration.
};
```

### 4.2 The three laws (each maps onto an existing kernel rule)

- **Layers via `SkShaders::Blend`, not `saveLayer`.** Skia flattens the whole
  material tree to one fragment program regardless of depth, so a five-layer
  parchment/energy/gloss stack is *one* cacheable draw. `saveLayer` is the #1
  avoidable 2D cost (offscreen + extra pass + blit, worst on tiled GPUs) and
  `DESIGN.md` already lists it as a risk.
- **Animates via named uniforms bound to `ch::Output`.** This is UE's
  Material-vs-Material-Instance split: a material whose uniforms are all
  constants **bakes into the SkPicture** (a static permutation); a material
  with any bound uniform **demotes only its own leaf** to volatile and, per
  frame, re-uploads *only the uniform bytes* on an `SkRuntimeEffect` held by
  reference. This is the kernel's declared-volatility rule extended *down* into
  the paint value.
- **Caches by structural signature.** Memoize the compiled `SkRuntimeEffect`
  on `(node kinds, blend modes, which uniforms are dynamic)`. **Never rebuild
  the shader tree to change a uniform** — that is the silent
  cost-reintroduction trap (and the UE permutation-explosion lesson: bake a
  per-*data* static permutation and you compile a shader per row).

### 4.3 SDF material — the unification win

Fold shape + centered/inset border + glow + soft-shadow into **one** shader
pass using Inigo Quilez's 2D SDF operators, sourcing the distance from the
analytic rrect *or* from the existing `shapes::` star/blob/squircle outline:

```
fill   = 1 - smoothstep(-aa, aa, d)
border = 1 - smoothstep(w-aa, w+aa, abs(d))      // opOnion/annular for inset rings
glow   = exp(-k * max(d,0))                       // distance falloff
shadow = smoothstep(0, blur, d + offset)
```

This collapses several of today's *separate* decoration passes (stroke +
`util::shadow` + a glow lambda, each a canvas op or a `saveLayer`) into a
single draw, makes **alive borders** a bound uniform on `w`/`k`, and makes
**multi-pointed chaotic stars** first-class chrome at any zoom (resolution
independence — no per-frame CPU tessellation). *Pitfalls to encode in the
type:* compute distance in aspect-corrected **pixel** space, not UV (else
borders go lopsided on a stretched health bar); use **MSDF (median-of-3)** for
sharp star/rune corners and small text (single-channel SDF rounds them).

### 4.4 `Sprite` / `Atlas` + dual-inset 9-slice

```cpp
struct Sprite {
  std::shared_ptr<const sigil::image::ImageAsset> atlas;
  SkRect src;                                  // sub-rect in the atlas
  std::optional<EdgeValues> sliceInsets;       // 9-patch STRETCH region
  std::optional<EdgeValues> contentInsets;     // padding region — defaults to sliceInsets
  TileMode xTile = TileMode::Stretch, yTile = TileMode::Stretch;  // per-axis
};
```

Two lessons the current `Slice` misses:

- **Dual insets** (Android 9-patch; CSS `border-image-slice` vs
  `border-image-width`): a frame carries *stretch* insets **and** *content*
  insets. Feed `contentInsets` into the Derive phase as padding so **chrome
  reserves its own interior automatically** — an ornate frame stops needing a
  hand-tuned inner `.padding()`.
- **Per-axis tile mode** (Godot `axis_stretch_horizontal/vertical` —
  Stretch/Tile/TileFit; note Slate's `ESlateBrushTileType` applies only to
  Image-mode brushes, not Box-mode 9-slice cells, so per-axis cell tiling +
  TileFit are the Godot lesson): filigree edges must **tile**, not stretch, or
  they smear. Add `TileMode::{Stretch, Tile, TileFit}` per axis (`TileFit`
  slightly scales so tiles seam — the reason it exists).

A named **`Atlas`** table (RmlUi `@spritesheet`: one image, many named
sub-rects) gives one GPU bind for a whole ARPG chrome set, which is the
batching substrate for §6 instancing.

### 4.5 Sampling the layer beneath — two explicit tiers

Encode the cost cliff in the type (Noesis brush-shader vs ShaderEffect):

- **`below()` = cheap sibling Snapshot** (default): prior siblings baked to an
  `SkPicture`/`SkImage`, passed as `uniform shader below` — glass, dissolve,
  refraction, backdrop-glow with no readback, fully picture-cacheable.
- **True `.backdrop()`** (already exists): live destination readback, breaks
  `Cache::Texture` — reserve for when semantically required (Flutter had to add
  `BackdropGroup` to make this rare). *Non-separable modes* (Hue/Sat/Color/
  Luminosity) are, per the W3C compositing spec, still pure per-pixel functions
  of (src, dst) — inside `Material::blend` the lower layers ARE the dst shader,
  so they work fine in the flattened path; only blending against the
  *destination framebuffer* forces the live backdrop route.

### 4.6 Before / after — the RPG health bar

Today (`ScenesGame.h`, paraphrased): a `custom()` leaf, `Cache::None`, ~18
lines building a gradient and drawing a clipped rounded rect every frame,
uncacheable, unhittable.

After — the Godot `TextureProgressBar` three-layer model (under track /
value-clipped fill / over gloss that hides the fill's cut edge) as a material,
with "juice" (a lag/ghost bar) as a second bound uniform:

```cpp
box().height(22).corners({9})
    .fill(Material::blend({
        {Material::solid(kTrackDim),                     SkBlendMode::kSrcOver},
        {Material::sdf(SdfShape::bar(&hp),               // value-clipped fill
                       {.fillRamp = kHpRamp})            // color-by-ratio (recolor)
             .uniform("uGhost", &hpGhost),               // lagging juice bar
         SkBlendMode::kSrcOver},
        {Material::sprite(kBarGloss),                    SkBlendMode::kScreen},
    }));
// hp is the existing ch::Output already bound in the scene; hpGhost trails it.
```

One cacheable value, animates by uniform, hit-tests via the node's real
bounds, and reads as data — no `custom()`.

---

## 5. Geometry beyond boxes

The field is unanimous (Flutter `RenderBox` vs `RenderSliver`; swappable
Compose `MeasurePolicy`; mxGraph unifying vertex/edge; LOOM's line-graph): the
**edge/line is a peer of the box**, and geometry is a protocol seam. Three
additions, all landing in the *existing* Derive phase and `Router`/`outline`
seams.

### 5.1 `GeometryScheme` — an ordered path-modifier stack

Model 1:1 on Skia's own `sksg::GeometryEffect` (pure `SkPath → SkPath`
decorators, each memoizing its output — the shipped set is Trim/Round/Offset/
Dash plus GeometryTransform and FillTypeOverride; boolean merge lives one
level up as `sksg::Merge`, a multi-child GeometryNode, which is also where
`mergePaths()` over children shapes belongs here):

```cpp
Element &trim(PropValue<float> start, PropValue<float> end, float offset = 0);
Element &roundJoins(float radius);
Element &insetPath(float delta, Join = Join::Miter);
Element &dashPath(std::vector<float> intervals, PropValue<float> phase);
Element &mergePaths(BooleanOp);   // union/difference/intersect over children shapes
```

**Trim Path is the single highest-value missing primitive** (`SkTrimPathEffect`
over `SkContourMeasure`): draw-on reveals, self-drawing subway lines, animated
node-graph edges, loader rings — and it marks only the *stroked-path recompute*
volatile while fills stay cached. Path **morph** (lerp matched bezier control
points; resample via `SkContourMeasure` to a common count) gives blob/star/
squircle "alive" borders — surface the vertex-compatibility constraint honestly
(AE/Lottie both impose it). *Pitfall:* modifier order is semantic
(trim-before-vs-after-repeater, fill-vs-stroke, shadow-inside-vs-outside-clip
all differ) — define and document one evaluation order
(sources → merge → offset/round → trim → transform) or renders aren't
reproducible.

### 5.2 `route()` / `rail()` — the component *is* a line

Generalize today's `connector(fromKey, toKey, Router)` (a from/to pair) into an
**ordered span of anchors** carrying its own stroke, so a subway line threading
eight stations is one element:

```cpp
struct Anchor { std::string nodeKey; SkPoint norm{0.5f, 0.5f};   // or named port
                float gap = 0; };
using Router = std::function<SkPath(std::span<const SkRect>, const RouteContext&)>;

Element route(std::span<const Anchor>, Router = routers::spline());
// routers::octilinear(radius)  // 45° metro snap + SkCornerPathEffect
//          orthogonal, spline, taxi, haystack, bundle   // React Flow / Cytoscape / Graphviz
```

Its resolved polyline becomes `PaintContext::outline` exactly as `connector()`
does today, so `PathFormat`/`ContourWalk`/materials dress it unchanged. Two
things to steal precisely:

- **Anchors, never absolute endpoints** (tldraw `TLArrowBinding` with a
  normalized 0–1 `normalizedAnchor`; Excalidraw's current `FixedPointBinding`
  likewise replaced its legacy focus/gap model with a normalized fixedPoint —
  both editors converged on exactly the `Anchor.norm` form proposed here;
  absolute endpoints remain "the #1 diagramming mistake"). Resolve `Anchor` in Derive
  against keyed node bounds, and keep a **node → routes back-index** so moving
  one node re-derives only its incident lines — the fine-grained invalidation
  seam.
- **Shared corridors carry an ordered bundle of lines** (LOOM): multiple
  routes over one segment offset in parallel with a per-segment ordering. That
  is what makes a transit map read.

*Pitfall:* inside one SVG document paint order is document order and CSS
z-index is inert (the SVG rendering model — React Flow's edge `zIndex` option
works only because the library regroups edges into stacked containers). Our
edges are ordinary stacking children, so `zIndex` reorders them natively —
keep it that way.

### 5.3 Arc-length as the one placement coordinate

GTFS `shape_dist_traveled`, mxGraph relative edge geometry, and
`SkPathMeasure::getMatrix(d)` are all the same tuple: `(t, perpendicularOffset,
orientation ∈ {upright, follow-tangent})`, invariant under node moves.
Generalize `ContourWalk` from uniform spacing to **anchored placement**
(stations/labels at explicit distances) and unify **all** along-edge motion
under one stamp driven by a single Choreograph phase:

| Want | Mechanism | Volatile input |
| --- | --- | --- |
| marching ants | `dashPath` phase | one scalar |
| traveling pulse / particles | `SkPath1DPathEffect` kRotate phase | one scalar |
| signal travel / draw-on | `trim` end | one scalar |
| **text flowing along the edge** | `getMatrix(d)` → RSXform blob (§6.3) | one scalar |
| gradient-along-length | `linear` ramp mapped to arc length | — |

TouchDesigner's "only cooking wires animate" becomes a per-edge `.flow(active)`
volatility flag: active edges mark *only their stamp layer* volatile; the base
stroked contour stays cached. A dense graph pays per-frame cost only for the
few edges currently signaling.

---

## 6. Instancing, console, and text motion

Node-graph nodes, inventory cells, and log lines are **one need**: a flyweight
template + a Struct-of-Arrays per-instance pool + single-draw stamping. The
kernel already half-implements this in its particle leaf (`drawAtlas` over an
EnTT SoA pool); generalize it.

### 6.1 `instances()` — data-driven repeats

```cpp
template <class Datum>
Element instances(std::span<const Datum> data,
                  Element (*templateFn)(const Datum&),  // measured/laid-out ONCE
                  Placement);                            // grid/ring/free/along-path
```

- **Template laid out once + SoA pool** (PixiJS `ParticleContainer` → 1M @
  60fps; Godot `MultiMesh`): never N Yoga subtrees. Stamp via
  `SkCanvas::drawAtlas` + `RSXform` (rotate + *uniform* scale + translate, one
  per-sprite color via blend mode); fall to `drawVertices` for skew/non-square
  cells (`drawAtlas` is uniform-scale-only — encode that in the API).
- **Node-chrome picture flyweight** keyed on `(nodeType, resolvedSize, theme,
  material-uniform digest)` (Cytoscape's off-screen sprite-sheet: 20 → 100
  fps): render static chrome once, stamp N under translation; only per-instance
  *dynamic* leaves (a health value, a count badge, a title) re-run.
  **Content-address on the full visual-input digest** or you cache stale
  visuals.
- **Virtualize** (Flutter Slivers): materialize only viewport + overscan,
  compute cell rects arithmetically, cull at the *data* level. Keeps
  Describe/Layout O(visible). *Pitfall:* culling carries an unconditional
  bookkeeping overhead and only pays off at large counts (React Flow documents
  exactly this tradeoff for `onlyRenderVisibleElements`) — gate it past a
  threshold.
- **Repeater** (skottie `RepeaterAdapter` transform math — linear
  translate/rotate, *exponential* `pow(s,t)` scale, start→end opacity lerp;
  note skottie replays the retained subtree per copy — cloning a cached
  *snapshot* is our improvement over it). Radial menus, tick arrays,
  equalizer bars, dotted borders at O(1) layout cost — unify with
  `snapshot()`-as-brush.

### 6.2 `console()` — the streaming log, the cleanest proof

```cpp
Element console(const LineRing& lines, ConsoleStyle);  // seq-id keyed, virtualized
```

An append-only `LineRing` keyed by **monotonic seq id** (Alacritty
`Storage<Row>` + `display_offset`), virtualized viewport, only the **tail line
+ cursor** declared volatile. *Pitfalls, all load-bearing:* key by seq id
**never array index** (append shifts indices → busts every cached line's
picture, O(1) append becomes O(n)); a blinking cursor must be its **own**
volatile span or it keeps all scrollback hot; re-lay-out only the tail, keep a
cumulative-height index (else streaming is quadratic). This is the sci-fi
hacker console directly — and the cleanest possible demonstration that the
existing spine (keyed reconcile + per-line picture cache + declared volatility)
*already* composes into a virtualized append-only view.

### 6.3 Per-glyph motion — one SoA buffer, not N nodes

```cpp
text(...).glyphEffect(glyph::typewriter(&progress))     // reveal
         .glyphEffect(glyph::scramble(&progress, charset))
         .stagger({.each = 30ms, .from = Stagger::Start});   // GSAP stagger{each,from}
```

A `GlyphModBuffer` (SoA `{RSXform, alpha, colorMul, codepointOverride}`) folded
into **one** `SkTextBlob::MakeFromRSXform`/`drawAtlas` call — never one Instance
per glyph (the SplitText/DOM anti-pattern that explodes node count and cache
invalidation). Reveal/scramble/wave/decode are pure
`(GlyphInfo, localProgress, rng) → GlyphMod` **values** (you asked for these as
reusable values, not baked components) composed through a `Stagger` remap
`f(index, count, master) → localProgress`. This also delivers **text-on-path**
(§5.3) and marquee from the same buffer. *Pitfall:* equal-advance scramble
charsets stay a Paint-only atlas swap; a proportional charset changes advances
→ reshape → route to Derive. Tag that in the API.

Editorial companions from the same survey: a `BaselineGrid` `LayoutScheme`
(snap first-baseline to a rhythm, text-box-trim — the property's current CSS
name; Müller-Brockmann) and
`VariationDrive` (animate fvar axes via `SkFontArguments`). An axis's metric
impact is per-FONT, not per-registry: `wght` typically CHANGES advance widths
(the OpenType registry explicitly permits glyph-width variation under weight —
`GRAD` exists precisely as the advance-invariant weight substitute), so
VariationDrive must PROBE each axis (measure advances at the extremes once per
font) and tag it reshape-vs-paint-safe from measurement, never from a fixed
axis list. Both deterministic, cache-stable, orthogonal to flexbox.

---

## 7. The isolation boundary + taming `custom()`

The one cross-cutting mechanism that makes multi-mode composition affordable is
the **isolation boundary** — Flutter `RepaintBoundary` = PixiJS v8
`RenderGroup` = the cache/invalidation/GPU-transform unit, all three at once.
The kernel's "declared-volatility subtree" is 80% of this already; promote it to
first-class:

- A boundary that only **translates/scales/fades** replays its cached
  `SkPicture` under a new matrix with **no re-record** (the kernel already does
  transform-replay caching — generalize and name it).
- A re-recording `custom()`/animated-material leaf **behind** a boundary
  **cannot force ancestors to re-record** (fixes §8.1's contagion).
- *Pitfall:* over-boundarying fragments batching (PixiJS explicitly warns) —
  keep it a deliberate opt-in / auto-promotion heuristic, not a per-leaf
  default.

And give `custom()` a **declared contract** (Flutter `CustomPainter.
shouldRepaint`, React `useMemo` deps): a required cull/bounds rect, an optional
dependency digest, and record-once-replay-many via `SkDrawable`. A callable
that declares its inputs becomes memoizable like any leaf; an undeclared one is
*contained*, not contagious. The principled escape hatch is
**`CustomGeometry`** (emit `SkVertices` + a `Material` into the retained list —
Slate `MakeCustomVerts`), never an opaque lambda. `custom()` stays for the last
5%, bounded.

---

## 8. Performance

### 8.1 THE issue: the prune vs callables (decorations: **FIXED in P0**)

At review time, `propsEqual` bailed to "unequal" the moment a node carried
*any* callable **or decoration** — so every decorated node re-patched on each
`render()`, `markPaintDirtyUp()` staled its whole ancestor chain, and
`contentDirty` stuck. P0 (`b02c98e`, now `Reconcile.cpp`) fixed the decoration
half: `Decoration` carries type-erased equality, `PathFormat`/`Slice`/`Shadow`
are comparable values, and `propsEqual` compares decoration vectors
element-wise. Measured: a 100-row static decorated scene under the standard
`if (dirty()) draw()` host loop dropped **10.96 ms → 0.13 ms/frame** (draws
100% → 0%; re-records 101 → 0 per frame). What still bails, deliberately
conservative:

```cpp
if (a.shapeFn || b.shapeFn || a.program || b.program || a.placeFn ||
    b.placeFn || a.router || b.router)
  return false;                        // raw callables: incomparable
```

So a raw `custom()` leaf or `outline()` shape still re-patches per `render()`
— the direct cost of look-as-lambda (raw lambdas *can't* be compared;
`Material` values *can*). Remaining fixes, in order:
1. **Material structural-signature equality** — gradient/sksl materials
   rebuild a fresh `SkShader` per describe, so their collapsed `Fill` compares
   unequal by pointer and doesn't prune across re-render (they still
   draw-cache within a render). Compare the recipe (ramp stops, effect +
   uniform values/bindings), not the shader pointer.
2. **Give `program`/`shapeFn` an optional identity/version token** (the
   `custom()` deps digest from §7) so an unchanged custom leaf compares equal.
3. **The `memo` rule is now documented** on `custom()`/`outline()`:
   `memo(...)` fully prunes a callable subtree (payload pointer identity
   short-circuits `patch`).

### 8.2 The rest of the list (highest leverage first)

| # | Win | Mechanism | Notes |
| --- | --- | --- | --- |
| 1 | prune covers chrome (§8.1) | comparable materials/decorations + custom deps | unblocks caching for the target modes |
| 2 | thousands of instances | `instances()` template + SoA + `drawAtlas` (§6.1) | replaces N Yoga subtrees; O(visible) |
| 3 | fewer draws/layers | `Material` `SkShaders::Blend` + SDF single-pass (§4) | removes stacked `saveLayer` and multi-op chrome |
| 4 | isolation boundary (§7) | triple-duty cache/damage/transform unit | quarantines volatile leaves |
| 5 | incremental key index | maintain `byKey` during patch/mount/unmount | today `rebuildKeyIndex()` walks the whole tree every `render()` |
| 6 | skip trivial sort | only `stable_sort(paintOrder)` when a zIndex is set | today sorts every patch |
| 7 | texture-bake thrash guard | keep the existing quantized-scale bucket; document it | blurs/bakes must not re-bake every intermediate zoom |
| 8 | snapshot pooling | reuse one snapshot Composer for animated `ContourWalk` stamps | `animatedWalk` currently builds a Composer+Ticker per frame |

*Avoid-list (the recurring traps):* over-caching a cheap subtree (record + BBH
+ blit loses to redraw — gate on N-frame stability and explicit boundaries,
and treat complexity scores as unreliable: Flutter disabled complexity-based
raster caching outright, #131206 — "texture sampling can be multiple times
slower than executing the original shaders" — and Impeller ships no raster
cache at all);
`saveLayer`-per-layer; animating a uniform by rebuilding the shader tree or
reshaping text every frame (the Lottie CPU-renderer cliff — per Airbnb's own
docs the cost concentrates in masks/mattes and merge-path `Path.Op`,
re-evaluated per frame); running NP-hard
graph solves (crossing-min, octilinear MIP, Sugiyama, edge-bundling) in paint
instead of memoizing them in Layout/Derive.

---

## 9. The `Composer.cpp` split (1489 → 210-line facade — **landed, P0**)

Landed in `b02c98e` and independently verified behavior-preserving against
the pre-split monolith (normalized function-body comparison; only whitespace
deltas plus the intentional §8.1 decorations hunk). The `Instance`/`Impl`
declarations live in the private `ComposeRuntime.h` (next to
`ComposeInternal.h`, which keeps `ElementNode`); the definitions partition by
phase — the map of where things live:

| New TU | Contents (current line ranges) |
| --- | --- |
| `Reconcile.cpp` | `propsEqual` + equality helpers; Yoga mappers (`toYogaAlign/Justify/applyDim`, `cornersRRect`); `resolveMemo`, `mount`, `patch`, `patchChildren`, `applyLayoutProps`; `render`, `renderSlot`; key index (34–177, 402–643, 1297–1312) |
| `Layout.cpp` | `ensureLayout`, `applyCustomLayouts`, `layoutText` + measure/baseline callbacks, `instanceRect`, `absoluteRect` (346–397, 734–792, 921–926) |
| `Derive.cpp` | `resolveDerived` (flowAround + connectors/routers) (794–854) |
| `Paint.cpp` | `resolveOutline`, `paintContent`, `paint`, `computeVolatile`, the cache tiers (856–1222) |
| `Transitions.cpp` | `resolveFloat`, `transitionFloat`, `applyTransitions` (334–344, 648–732) |
| `Query.cpp` | `shapeContains`, `hitInstance`, `bounds`, `paragraphLayout`, `hitTest`, `stats` (1224–1295, 1438–1487) |
| `Composer.cpp` | the public facade, `Impl` lifecycle, `snapshot()` (252–332, 1314–1436) |

Each new capability then lands in its natural file (materials → `Paint.cpp`;
`route()`/geometry modifiers → `Derive.cpp`; `instances()` → a new
`Instancing.cpp`), instead of growing the monolith.

---

## 10. The target use cases, expressed after the additions

Each is a *mode* (§3.3) built from the new seams — none needs a raw `custom()`
except as a bounded last resort.

- **Animated node graph** — nodes are `instances()` of a chrome template
  (one flyweight picture, per-node data leaves); edges are `route(anchors,
  routers::spline)` with a `PathFormat` stroke, `.flow(active)` marking only the
  travelling-pulse stamp volatile, and a `glyph`-on-path label flowing the data
  value along the wire (§5.3, §6.3). Layout via a dagre/ELK scheme memoized in
  Derive.
- **Sci-fi console / logger** — `console(lineRing, …)` with per-line
  `glyph::typewriter`/`glyph::scramble` reveals staggered by a `Falloff`
  effector, a monospace atlas, and the tail+cursor as the only volatile span.
  Decoding-text and marquee strips are the same `GlyphEffect` values.
- **Subway / transit map** — stations are keyed nodes; each line is a
  `rail(anchors, routers::octilinear(r))` carrying its own color/width; labels
  ride the rail via along-path placement; shared corridors offset as an ordered
  bundle (LOOM). The "components are lines" case, first-class.
- **Path-of-Exile / Diablo inventory** — a `Sprite`/`Atlas` chrome set; cells
  are `instances()` over a grid placement; rarity frames are a dual-inset
  9-slice `Material` recolored per item (`recolor(rarityRamp)`); the socket
  glow and item-hover are bound SDF uniforms; the tooltip is an ordinary
  composed+cached subtree positioned by `bounds()`.
- **Animated health/resource bars** — the §4.6 three-layer
  `Material::blend` with a color-by-ratio `recolor`, a lagging "ghost" uniform,
  and a shimmer sprite — data-driven, cacheable, hittable.
- **Alive / animated borders** — an `sdf` material with a bound `border-width`
  / `glow` uniform, or a `trim`-animated stroked outline; the ornament
  vocabulary (`OrnamentKit`'s tapered strokes, spirals) moves *into* the library
  as `GeometryScheme`/`Material` values so it stops being re-typed raw Skia.
- **Motion poster** — `BaselineGrid` placement + gradient-ramp `Material` +
  `VariationDrive` kinetic type + a `glyph` reveal; the "Describe once, animate
  by binding" path the `Sketch.h` header already preaches.

---

## 11. Gallery: toss and rewrite

Kill the 19 "one seam per scene" proofs. Replace with **6–8 product-grade
scenes**, each of which (a) is a *compelling artifact* worth screenshotting,
(b) exercises a *mode*, and (c) doubles as the acceptance test for a new
capability — so the gallery is the spec, not an afterthought:

1. **Node graph** — data-flow with flowing-text edges and travelling pulses
   (Graph mode; `instances` + `route` + glyph-on-path).
2. **Sci-fi console / mission log** — streaming scramble/typewriter log with a
   staggered cascade (Field/text mode; `console` + `GlyphEffect` + `Falloff`).
3. **Transit map** — a small octilinear metro with bundled corridors and
   along-rail labels (Rail mode; `rail` + octilinear router).
4. **ARPG inventory + tooltip** — atlas chrome, rarity frames, socket glow,
   hover tooltip (Flow mode + Material; `Sprite`/9-slice + SDF + `bounds()`).
5. **Live HUD** — health/mana/juice bars, radial cooldowns, an alive border
   (Material/SDF; §4.6 + Repeater ticks).
6. **Motion poster** — baseline-grid editorial with kinetic variable type and a
   gradient-ramp ground (Poster mode; `BaselineGrid` + `VariationDrive`).

Move the reusable drawing vocabulary that currently lives in `OrnamentKit.h`
and is re-typed in `flourish_border.cpp` **into the library** as
`GeometryScheme`/`Material` values, so a flourish is a value, not 451 lines of
gallery-local Skia. Keep the sketch host — it is the right live-coding vehicle;
it just needs a real vocabulary to call.

---

## 12. Phasing

Sequenced so each phase is independently valuable and de-risks the next:

- **P0 — Split + prune fix. LANDED** (`b02c98e`): §9 file split (verified
  behavior-preserving); comparable decorations prune without memo (measured
  10.96 → 0.13 ms/frame on the dirty()-gated loop); `memo` rule documented on
  `custom()`/`outline()`. Still open from P0: the `custom()` deps token
  (folds into the §7 isolation-boundary work).
- **P1 — Material / Brush (the unlock). Lean core LANDED** (`511a948` +
  follow-up): `Material` value (solid/gradient ramps/image/sksl/blend-as-one-
  shader), live `ch::Output` uniforms with copy-on-write value semantics and
  warn-don't-abort uniform validation, `fill(Material)` with last-wins setter
  symmetry — all Fable-audited. Open: SDF node, `Sprite`/dual-inset 9-slice,
  `below()`, gradient/recipe structural-signature pruning (§8.1), the OCIO
  `color::View` output stage. `Fill::color/shader` remain as the low-level
  carrier.
- **P2 — Geometry beyond boxes.** §5: `GeometryScheme` modifier stack (Trim
  first), `route()`/`rail()` + N-anchor `Router` + anchor-binding + node→routes
  back-index, arc-length unification of along-edge motion.
- **P3 — Instancing + text.** §6: `instances()` + SoA + `drawAtlas` flyweight,
  `console()`/`LineRing`, `GlyphModBuffer` + `GlyphEffect` + `Stagger`,
  `BaselineGrid`/`VariationDrive`.
- **P4 — Modes docs + gallery rewrite.** §11 scenes + §13 onboarding.

The isolation-boundary generalization (§7) threads through P0–P3 as the caching
substrate rather than being one phase.

---

## 13. Onboarding — the "easier to break in" fix

Keep the existing kernel/util/extensions tiering (it is good) and add a
**mode-first on-ramp** on top of it:

1. **Kernel** — the one-sentence model (§3.1) and the smallest surface. What
   everyone learns.
2. **Pick a mode** — a two-page quickstart per mode (Flow, Graph, Rail, Field,
   Poster) showing *only* the seams that mode needs, each ending in a runnable
   sketch. A newcomer never meets `flowAround`, `Router`, `ContourWalk`,
   `LayoutScheme`, and SDF materials at once — they meet the three their mode
   uses.
3. **Kits** — Material/Shapes/Geometry/Routers/Decorations/Layouts reference,
   opt-in.
4. **Reference** — `API.md` as today.

That restructure, not fewer capabilities, is what fixes "so many concepts
concentrated." The concepts are fine; they were just all presented at once.

---

## Appendix — prior-art map

| Recommendation | Named sources |
| --- | --- |
| `Material`/`Brush` polymorphic value | Slate `FSlateBrush.ResourceObject`; Jetpack Compose `Brush`; WPF `Brush : Freezable`; Direct2D `ID2D1Effect` graph; Skia `SkShader`/`SkBlender` tree |
| uniforms bound + effect memoized | UE Material vs Material Instance; `SkRuntimeEffect` held-by-reference |
| SDF material (shape+border+glow+shadow) | Inigo Quilez 2D SDF primitives/operators; MSDF (Chlumský) |
| layer via `SkShaders::Blend` not `saveLayer` | Skia flatten-to-one-program; `DESIGN.md` saveLayer risk |
| `below()` sibling snapshot vs live backdrop | Noesis brush-shader vs ShaderEffect; Flutter `BackdropGroup` |
| dual-inset 9-slice + per-axis tile | Android 9-patch; CSS `border-image`; Godot `axis_stretch_mode` (per-axis cell tiling + TileFit are Godot-only; Slate's tiling is Image-mode-only) |
| `Sprite`/`Atlas` | RmlUi `@spritesheet`; ARPG icon/frame libraries |
| `GeometryScheme` modifier stack; Trim; morph | skottie `sksg::GeometryEffect` (Trim/Round/Offset/Dash; boolean merge via `sksg::Merge`); `SkTrimPathEffect`; Lottie trim/repeater |
| `route()`/`rail()` + anchors + bundle | LOOM line-graph; GTFS; tldraw/Excalidraw bindings; React Flow/Cytoscape/Graphviz routers; Nöllenburg–Wolff octilinear |
| arc-length placement / along-edge flow | `SkPathMeasure::getMatrix`; mxGraph relative geometry; TouchDesigner cooking-wire flow |
| `instances()` SoA + `drawAtlas` flyweight | PixiJS `ParticleContainer`; Godot `MultiMesh`; Cytoscape sprite-sheet; skottie `RepeaterAdapter` |
| `console()`/`LineRing` | Alacritty `Storage<Row>`/`display_offset`; xterm.js |
| `GlyphModBuffer` + `GlyphEffect` + `Stagger` | `SkTextBlob::MakeFromRSXform`; GSAP SplitText/ScrambleText stagger |
| `BaselineGrid`/`VariationDrive` | Müller-Brockmann grid systems; `SkFontArguments` fvar |
| isolation boundary (cache/damage/transform) | Flutter `RepaintBoundary`; PixiJS v8 `RenderGroup`; Slate invalidation/retainer boxes |
| bounded `custom()` / `CustomGeometry` | Flutter `CustomPainter.shouldRepaint`; Slate `MakeCustomVerts`; Skia `SkDrawable` |
