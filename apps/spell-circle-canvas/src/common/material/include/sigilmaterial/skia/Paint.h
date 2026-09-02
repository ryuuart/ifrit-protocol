#pragma once

/** @file
 * THE SKIA PAINT: this library's material model as a Skia shader. A small
 * tree of paint nodes that compiles to ONE `sk_sp<SkShader>` (layers via
 * SkShaders::Blend, never stacked saveLayer) or a plain solid colour.
 *
 * A Paint sits in one of three volatility tiers:
 *  - STATIC (solid, gradient ramp, image/sprite, blend, sksl with only
 *    constant uniforms): resolves eagerly to a color or shader, so it
 *    answers its colour or its shader without a frame, so a consumer
 *    can cache and prune it like any other static value.
 *  - GEOMETRY (an sksl() material declaring only `uResolution`): resolves
 *    when the node RECORDS and caches between layouts — it depends on the
 *    box, not on the clock (see geometryDependent()).
 *  - LIVE (an sksl() material with a ch::Output-bound uniform, or one
 *    reading `uTime`/`uContentScale`, or one whose CHILD is live): carries
 *    the runtime-effect recipe
 *    and is re-resolved every frame from the current values (resolve());
 *    its node is declared volatile exactly like a bound fill, so it paints
 *    live and never freezes into a cache. This is what makes
 *    `.uniform(name, &output)` actually drive pixels — see the note on
 *    uniform() below.
 *
 * The vocabulary mirrors the solid/mix/ramp/image/blend atoms a material
 * graph format would use, but nothing of the sort is linked: the backend
 * here is SkSL and SkShader and nothing else.
 *
 * A RECIPE-BACKED paint (`recipe()`) is one over a `Material` instance:
 * the recipe's params are its uniforms, its bindings and child slots are
 * the instance's, and it resolves through the core's program cache with
 * the frame the caller supplies. It sits in the same three tiers by the
 * same rules, and uniform()/child() on it are the core's doors spelled in
 * this class's words.
 *
 * Colour management is not part of a paint. A view transform belongs to
 * the consumer's output stage and applies to a whole composite.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>  // sk_sp<SkShader> data member
#include <include/core/SkString.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>       // the gradient Fills
#include <include/effects/SkRuntimeEffect.h>  // the unit-space ramps
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/values/Animated.h>
#include <sigilmotion/values/Time.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

class SkBitmap;
class SkCanvas;
class SkImage;

namespace sigil::material::skia {

/** WHAT ONE DRAW SUPPLIES — the values a paint is resolved against that
 *  no author sets: the box being painted, the clock, the device scale,
 *  and where the box sits in the root frame (which is what a world-space
 *  paint anchors to). A consumer builds one per draw. */
struct PaintFrame {
  /** The painted box in px; `uResolution` for a node-local paint. */
  SkSize size = SkSize::MakeEmpty();
  /** The root's laid-out size in canvas px — `uResolution` for a
   *  world-space paint. Empty falls back to `size`. */
  SkSize rootSize = SkSize::MakeEmpty();
  /** The box's local space to the root. Identity outside a composite,
   *  which degrades a world-space paint deterministically to a
   *  box-local one. */
  SkMatrix toRoot = SkMatrix::I();
  /** Seconds on the consumer's clock; the `uTime` uniform. */
  double seconds = 0.0;
  /** Device pixels per logical pixel; the `uContentScale` uniform. */
  float contentScale = 1.0f;
};

/** A gradient ramp stop (position 0..1 + color) — the MaterialX `<ramp>` atom.
 *  Authored in the working color space. */
struct Stop {
  float pos = 0.0f;
  SkColor4f color = {0, 0, 0, 1};
  bool operator==(const Stop&) const = default;
};

/** The caller-owned raster behind `Paint::buffer()`: draw into
 *  `bitmap()`, or through `canvas()`, then `commit()` to publish.
 *
 *  The material's recipe carries (source, revision), so an identical
 *  re-describe between commits prunes and nothing repaints, and the first
 *  describe after a commit patches exactly once. `image()` snapshots
 *  lazily and caches per revision, so a describe that prunes copies no
 *  pixels at all.
 *
 *  Not thread-safe, deliberately: one owner, one writer. */
class PixelBuffer {
 public:
  PixelBuffer(int width, int height);
  ~PixelBuffer();
  PixelBuffer(const PixelBuffer&) = delete;
  PixelBuffer& operator=(const PixelBuffer&) = delete;

  /** The pixels, yours to write. N32 premul. */
  SkBitmap& bitmap();
  /** A raster canvas over the same pixels — the convenient writer. */
  SkCanvas& canvas();
  /** PUBLISH the edit: the next describe carries the new revision and
   *  the reconciler repaints the material's node exactly once. */
  void commit() { ++m_revision; }
  uint64_t revision() const { return m_revision; }
  /** The current snapshot (copied from the bitmap once per revision). */
  sk_sp<SkImage> image();

 private:
  struct State;  // SkBitmap + SkCanvas live out of line (heavy includes)
  std::unique_ptr<State> m_state;
  uint64_t m_revision = 0;
  uint64_t m_snapshotRevision = ~0ull;
  sk_sp<SkImage> m_snapshot;
};

class Paint;

namespace detail {
/** The unit-square ramp both linearUnit() and radialUnit() compile to: one
 *  SkSL pass that divides by uResolution, so the gradient's coordinates are
 *  fractions of the node's laid-out box rather than pixels. Any number of
 *  stops — the count is baked into the generated source as a chain of
 *  mixes, each taking effect past its own start, and one effect is cached
 *  per stop count. */
inline Paint unitRamp(SkPoint a, SkPoint b, std::vector<Stop> stops,
                      bool radial);

/** THE CHILD-SLOT CONVERSION, in one place because its callers must agree:
 *  sksl()'s children, blend()'s layers and Effect's children all need this
 *  Paint as the SkShader a builder slot takes. @p frame non-null is the
 *  per-draw `shaderFor()` form and null the frameless `asShader()`
 *  snapshot — the same split every child site makes — and a solid
 *  collapses to a colour shader either way. */
sk_sp<SkShader> childShader(const Paint& source, const PaintFrame* frame);

/** Does @p effect declare @p name as a `uniform shader`? Assigning a child
 *  an effect does not declare aborts in a debug build, so both child()
 *  doors — Paint's and Effect's — validate at STORE time and then warn
 *  and ignore: one typo in a live-reloaded sketch must not take the host
 *  process down. */
bool declaresShaderChild(const sk_sp<SkRuntimeEffect>& effect,
                         std::string_view name);

/** Does @p effect declare @p name as a uniform of exactly @p bytes? The
 *  same guardrail one paragraph up, for the other kind of slot: assigning
 *  an undeclared uniform — or one whose declared size is not the caller's,
 *  which is every mismatched float2, float4 and array — aborts a debug
 *  build and drops
 *  the value silently in a release one. Every door that takes a uniform
 *  name from an author validates here at STORE time, so the builder is
 *  never handed an entry it would refuse. An ARRAY validates by its TOTAL
 *  byte size, which is all the builder checks: 12 floats fill
 *  `float4 uRect[3]` and `float uWeights[12]` alike. */
bool declaresUniform(const sk_sp<SkRuntimeEffect>& effect,
                     std::string_view name, size_t bytes);

/** THE PER-UNIT DATA A TEXT PASS IS HANDED — what the fx() runtime fills
 *  for a `fx::pass` track's material and `Paint::resolvePass` uploads.
 *  `content` is the addressed units' rendered layer; `rects` is 4 floats
 *  per unit (x, y, w, h, node-local px); `phases` is 2 per unit (that
 *  unit's cascade-local 0→1, then its stable seed). Non-owning views,
 *  valid for the call. */
struct PassInputs {
  sk_sp<SkShader> content;
  const float* rects = nullptr;
  const float* phases = nullptr;
  uint32_t units = 0;
};

/** THE PASS SPECIALIZATION of @p authored at @p units: a recipe with the
 *  same params ABI whose SkSL body is the runtime's declarations —
 *  `uContent`, `uUnitRect[N]`, `uUnitPhase[N]`, `kUnitCount` — followed by
 *  the author's, held once per distinct (recipe, N) for the process. The
 *  unit count must be baked into the definition because a runtime effect's
 *  array size is fixed at compile and SkSL has no uniform-bounded loop;
 *  holding one specialization per count is what keeps that from meaning a
 *  compile per frame. Null when @p authored carries no SkSL body. */
std::shared_ptr<const sigil::material::Recipe> passRecipeFor(
    const std::shared_ptr<const sigil::material::Recipe>& authored,
    uint32_t units);
}  // namespace detail

/** The polymorphic paint value. Construct via the static factories; pass to
 *  Element::fill(). */
class Paint {
 public:
  Paint() = default;  // none (fully transparent — draws nothing)

  // ---- leaves --------------------------------------------------------------
  static Paint solid(SkColor4f color);
  /** N-stop linear ramp between two points (working-space colors). */
  static Paint linear(SkPoint a, SkPoint b, std::vector<Stop> stops,
                      SkTileMode tile = SkTileMode::kClamp);
  static Paint radial(SkPoint center, float radius, std::vector<Stop> stops,
                      SkTileMode tile = SkTileMode::kClamp);
  /** OFFSET-FOCUS radial: the ramp runs from the circle
   *  (@p focus, @p focusRadius) to the circle (@p center, @p radius), so a
   *  highlight displaced off a sphere's centre is
   *  `conical(hot, 0, centre, R, …)`.
   *
   *  This is what a plain radial() cannot do. Moving a radial's centre
   *  couples the falloff to the displacement — the entire ramp slides,
   *  including its outer edge — where here the outer circle stays put and
   *  only the hot spot moves. Both radii are node-local px. */
  static Paint conical(SkPoint focus, float focusRadius, SkPoint center,
                       float radius, std::vector<Stop> stops,
                       SkTileMode tile = SkTileMode::kClamp);
  /** Angular sweep from startDeg (12 o'clock is -90°) around center.
   *
   *  Angles outside [0, 360) CLAMP, they do not wrap: `sweep(c, stops,
   *  90, 450)` — the obvious way to start a hue wheel at red — paints
   *  the quarter before 90° in the first stop's flat colour, because no
   *  canvas angle ever reaches past 360. Rotate the STOPS into [0, 360)
   *  instead; the factory warns once when a window leaves the circle. */
  static Paint sweep(SkPoint center, std::vector<Stop> stops,
                     float startDeg = 0.0f, float endDeg = 360.0f);
  /** Image/sprite as a fill (tiled or clamped); `local` maps source px into
   *  the node's space (a sprite's atlas sub-rect is a translate+scale). */
  static Paint image(sk_sp<SkImage> image, SkTileMode tx = SkTileMode::kClamp,
                     SkTileMode ty = SkTileMode::kClamp,
                     const SkMatrix& local = SkMatrix::I(),
                     SkSamplingOptions sampling = {});
  /** CONTENT THAT CHANGES WITHOUT RE-DESCRIBING: a caller-owned raster the
   *  material samples — a simulation, a decoded video frame, a paint
   *  surface, a scrollback. Own the PixelBuffer, draw into it, `commit()`.
   *
   *  The recipe compares by (source, revision), so an identical
   *  re-describe between commits PRUNES and the first describe after a
   *  commit patches exactly once. That is the whole point: the node keeps
   *  its picture caching and its decorations, where the alternative — a
   *  `custom()` leaf at Cache::None — gives up both. */
  static Paint buffer(std::shared_ptr<class PixelBuffer> source,
                      SkTileMode tx = SkTileMode::kClamp,
                      SkTileMode ty = SkTileMode::kClamp,
                      const SkMatrix& local = SkMatrix::I(),
                      SkSamplingOptions sampling = {});
  /** An SkSL runtime effect as a shader. `constants` set named float uniforms
   *  once; bind live uniforms with uniform(name, &output) below, and fill
   *  declared `uniform shader` slots with child(name, material) — a second
   *  source (an index texture read through a palette, a mask, a noise field).
   *  Declaring
   *  `uTime` or `uContentScale` takes the LIVE path (re-resolved each frame:
   *  the clock ticks and the host's zoom changes independently of the node —
   *  reading them IS the volatility declaration); declaring only
   *  `uResolution` takes the cheaper GEOMETRY tier (resolved when the node
   *  records, cached between layouts — see geometryDependent()). */
  static Paint sksl(sk_sp<SkRuntimeEffect> effect,
                    std::vector<std::pair<std::string, float>> constants = {});
  /** Wrap a raw shader (interop / escape). */
  static Paint shader(sk_sp<SkShader> shader);
  /** A `Material` instance as the paint. The recipe's declared frame
   *  inputs set the tier exactly as an sksl() effect's uniforms do —
   *  time or content scale is LIVE, the resolution is GEOMETRY — and its
   *  bindings make it live. uniform() and child() below reach the
   *  instance's fields and slots; equality is the instance's, so two
   *  paints built from equal instances prune.
   *
   *  It is also the ONLY form `fx::pass` takes. A pass body is written
   *  against declarations the RUNTIME supplies — `uniform shader uContent`
   *  (the addressed units' rendered layer), `uniform float4 uUnitRect[N]`,
   *  `uniform float2 uUnitPhase[N]` and `const int kUnitCount = N` — and N
   *  is the track's unit count, known only at paint, so the runtime holds
   *  a specialization of the recipe per distinct count. Do not declare
   *  those four names in the recipe, and read them only from a material
   *  handed to `fx::pass`: used as an ordinary fill, the recipe compiles
   *  without them and a body that mentions them does not compile at all. */
  static Paint recipe(sigil::material::Material material);
  /** The `Material` instance behind a recipe() paint, or null. */
  const sigil::material::Material* recipeMaterial() const;

  // ---- combinator ----------------------------------------------------------
  /** Layer materials into ONE flattened shader: layers paint bottom-to-top,
   *  each composited over the accumulation with its SkBlendMode (the first
   *  layer's mode is the base and ignored). Nested SkShaders::Blend — one
   *  draw, fully picture-cacheable, no saveLayer. A blend whose layers are
   *  all static flattens eagerly; one containing a LIVE or geometry-
   *  dependent layer DEFERS the flatten to resolve time (per frame / per
   *  record respectively), so bound uniforms and SDF layers contribute
   *  their correct current form — the blend simply inherits its layers'
   *  volatility tier. */
  static Paint blend(std::vector<std::pair<Paint, SkBlendMode>> layers);

  // ---- unit-space ramps ----------------------------------------------------
  /** The same linear ramp as linear(), authored in the node's UNIT SQUARE:
   *  (0,0) is the box's top-left, (1,1) its bottom-right, whatever the box
   *  turns out to be.
   *
   *  linear() takes PIXELS in node-local space, which is workable for a box
   *  whose size you wrote down and impossible for one the layout decides —
   *  a card as tall as its copy, a button that grows with its label. There
   *  is no number to guess here.
   *
   *  Rides the GEOMETRY tier through uResolution: resolved when the node
   *  records and cached between layouts, so it costs nothing per frame.
   *  Any number of stops. */
  static Paint linearUnit(SkPoint from01, SkPoint to01,
                          std::vector<Stop> stops) {
    return detail::unitRamp(from01, to01, std::move(stops), false);
  }
  /** The unit-square radial: @p center01 and a radius as a fraction of the
   *  box's HALF-DIAGONAL, so a ramp centred at {0.5, 0.5} with radius 1
   *  reaches the CORNERS of any box.
   *
   *  Which is a trap for the commonest use. A soft round light authored at
   *  radius 1 still has alpha left where the INSCRIBED circle is, so if the
   *  node also carries `.shape(shapes::circle())` the shape cuts the ramp
   *  off mid-falloff and the glow gets a visible hard rim. The number that
   *  reaches the inscribed circle instead is 0.707. The trap cuts the other
   *  way too: a ramp
   *  authored past 1 (a planet terminator at radius 1.28) puts its far
   *  end entirely OUTSIDE the inscribed disc, so on a circle-shaped node
   *  the shading silently disappears — nothing is drawn wrong, the
   *  interesting part of the ramp just never intersects the shape. Use
   *  glowUnit() below when you mean "fills this box" (it is the
   *  min-side-relative variant: radius 1 IS the inscribed circle), and
   *  keep radialUnit for when you genuinely mean the corners (a
   *  vignette, a corner-to-corner wash). */
  static Paint radialUnit(SkPoint center01, float radius01,
                          std::vector<Stop> stops) {
    return detail::unitRamp(center01, {radius01, radius01}, std::move(stops),
                            true);
  }
  /** A soft round light that FILLS the box: the radius is a fraction of
   *  the box's shorter side, so radius 1 reaches the inscribed circle —
   *  which is what "a glow filling this node" means every time anyone
   *  writes it. Everything else is radialUnit.
   *
   *  Like linearUnit and radialUnit it works in the box's UNIT SQUARE, so
   *  on a non-square box the falloff is elliptical — it fills the box
   *  rather than staying circular. That is what you want for a panel
   *  wash and not for a lamp; for a true circle, put it on a square node. */
  static Paint glowUnit(SkPoint center01, float radius01,
                        std::vector<Stop> stops) {
    // half-diagonal = sqrt(2)/2 of the side on a square; the ratio a
    // caller wants is radius-in-half-diagonals = radius01 / sqrt(2).
    return detail::unitRamp(center01,
                            {radius01 * 0.70710678f, radius01 * 0.70710678f},
                            std::move(stops), true);
  }

  // ---- uniforms ------------------------------------------------------------
  /** Set / bind a NAMED uniform. This is meaningful ONLY on an sksl()
   *  material — the one kind that has named uniforms to hook against:
   *   - `uniform(name, value)` bakes a constant in; the material stays static.
   *   - `uniform(name, &output)` binds a ch::Output; the material becomes LIVE
   *     (re-resolved every frame from the Output's current value, and its node
   *     is declared volatile so it paints live — this is how a material
   *     animates).
   *  Additionally, `uTime` (float seconds), `uResolution` (float2 px), and
   *  `uContentScale` (float) are auto-injected each frame IF the effect
   *  declares them (at the matching size).
   *
   *  Guardrails: a name the effect doesn't declare as a float uniform is
   *  warned and IGNORED (never a debug abort — one sketch typo must not kill
   *  the hot-reload host). On any other material kind (solid/gradient/image/
   *  blend) there is nothing to bind: the call is a no-op with a warning.
   *  Reach for sksl() when you want animatable uniforms. Materials are
   *  VALUES: uniform() copies-on-write, so binding on a copy never affects
   *  the material it was copied from. */
  Paint& uniform(std::string name, float value);
  /** Constant float2 uniform (`uniform float2` in the SkSL) — offsets,
   *  margins, direction vectors. */
  Paint& uniform(std::string name, std::array<float, 2> value);
  /** Constant float4 uniform set from a color (straight, not premultiplied —
   *  what the SkSL declares as `uniform float4`). */
  Paint& uniform(std::string name, SkColor4f value);
  /** Constant float4 uniform from plain numbers — a rect, a quaternion,
   *  anything that is not a colour. Same slot the SkColor4f form fills. */
  Paint& uniform(std::string name, std::array<float, 4> value);
  /** CONSTANT ARRAY, stored flat and matched against the declared
   *  uniform's TOTAL float count — 12 floats fill `float4 uRect[3]`,
   *  `float2 uPts[6]` and `float uWeights[12]` alike, because total size
   *  is all the builder distinguishes. The whole array must be supplied:
   *  the builder refuses a partial write, so a count that is not the
   *  declaration's warns once and is ignored. */
  Paint& uniform(std::string name, std::vector<float> values);
  /** A LIVE ARRAY — a `UniformBlock` (Compose.h) the caller owns, writes
   *  and commit()s, read at every paint. The material becomes LIVE exactly
   *  as a bound scalar makes it: re-resolved per frame, its node declared
   *  volatile, no cache can freeze the table — and the resolve memo reads
   *  the block's REVISION, so an uncommitted frame reuses the built shader.
   *  The binding compares by block identity and the values never prune;
   *  hold the block beside your model, not in the describe. Size-checked
   *  at store against the declared array's total float count. */
  Paint& uniform(std::string name,
                 std::shared_ptr<const material::UniformBlock> block);
  /** A LIVE SCALAR: the value is read at every paint, so the material is
   *  live and its node volatile for as long as the binding is attached.
   *  An animatable, so the arithmetic that shapes the number — a wrapped
   *  ramp, a raised cosine, a wiggle — sits beside the uniform it feeds
   *  rather than in a second Output somebody steps by hand. A material
   *  holds no instance, so a value carrying its own TRANSITION has nothing
   *  to run it and reads as its target. */
  Paint& uniform(std::string name, motion::Animatable<float> output);

  /** THE CHILD SLOT — a SECOND SOURCE for an sksl() material. The effect
   *  declares `uniform shader NAME;` and this fills it with another
   *  Paint, so one shader can read two sources and combine them by a
   *  rule only SkSL can state: an index texture sampled through a palette
   *  lookup (index arithmetic on the sampled value, which no blend mode can
   *  express), a mask channel, a noise field, a second gradient.
   *  `Effect::filter` has exactly one child, `content`, which is the
   *  already-painted layer; this is the door for sources the node has NOT
   *  painted.
   *
   *  Any Paint is a legal child, including another sksl() one — children
   *  nest, and the whole tree still compiles to ONE shader (no saveLayer).
   *  For an image child, wrap it: `child("uIndex", Paint::image(img, ...))`
   *  — and pass `SkSamplingOptions(SkFilterMode::kNearest)` for anything
   *  whose pixel VALUES are data (an index texture read at kLinear samples
   *  a blend of two unrelated palette entries).
   *
   *  TIER INHERITANCE, the load-bearing half: the parent inherits its
   *  children's volatility. A live child (bound Output, uTime) makes the
   *  parent live; a geometry-dependent child (uResolution) propagates the
   *  geometry tier. The children also ride the prune signature, so two
   *  materials with DIFFERENT children never compare equal and two with
   *  identical ones prune. That is required, not incidental: a child left
   *  out of equality would let a node prune while its second source had
   *  changed, and it would sample the old texture indefinitely.
   *
   *  Guardrails match uniform()'s: a name the effect does not declare as a
   *  shader child is warned and IGNORED — assigning a missing child aborts
   *  in a debug build, which would take a live-reload host down over one
   *  typo — and
   *  on a non-sksl() material there is nothing to fill — no-op with a
   *  warning. Copy-on-write like every other recipe mutation. */
  Paint& child(std::string name, Paint source);

  /** LAYER STRENGTH inside a blend() — "soft-light this noise at 30%".
   *
   *  Layer-opacity semantics, as an image editor's layer panel means them:
   *  the layer composites with its blend mode IN FULL, and the result then
   *  mixes back toward the accumulation by `a01`. That is not the same
   *  picture as thinning the layer's own alpha first, which changes what
   *  the blend mode sees. Clamped to [0, 1]; the default 1 is free.
   *
   *  Read ONLY by blend(). A material used directly as a fill ignores it,
   *  because there is no accumulation to mix back toward. Participates in
   *  equality like every recipe field. */
  Paint& amount(float a01);

  /** RECORDING-CULL RESERVE: how far this material's node paints beyond
   *  its own box, in px, declared with the same word a decoration uses.
   *
   *  Needed when a node's outline escapes its layout rect — a `shape()`
   *  silhouette larger than the box, which is legal — because the cached
   *  picture or texture is culled to the box plus whatever reserve was
   *  declared, and paint outside that is simply cut off. Read by the
   *  recording cull only: it moves no pixels itself, and the default 0
   *  changes nothing. Participates in equality, since a changed reserve
   *  has to force a re-record. */
  Paint& bleed(float px);
  /** The declared reserve (0 unless bleed() was set). */
  float bleed() const { return m_bleed; }

  /** WORLD SPACE: this material's coordinates are the COMPOSER ROOT's
   *  frame — canvas px — instead of the node's.
   *
   *  It is for a field that must be continuous ACROSS separately-laid-out
   *  nodes: one light over a whole instrument, weathering across a floor's
   *  tiles. Author the field once against the canvas and every flagged node
   *  samples it where it actually sits, through its layout offset and its
   *  transforms. A ROTATED node samples through its rotation, so the
   *  highlight stays put while the object turns — which is the behaviour a
   *  per-node hand conversion cannot reproduce, since that turns with the
   *  node. uResolution becomes the ROOT canvas size when flagged, so
   *  linearUnit() and glowUnit() read as fractions of the canvas.
   *
   *  Per-material-LAYER, deliberately not inherited: flagging a blend()
   *  does not flag its layers, flagging an sksl() parent does not flag its
   *  child() materials — each Paint anchors (or not) for itself. (A
   *  flagged sksl() parent's children still SEE root coordinates, because
   *  the wrap re-maps the coordinates the parent's SkSL evaluates them at
   *  — that is Skia's local-matrix composition, not flag inheritance.)
   *
   *  Mechanism: at resolve the built shader is wrapped
   *  `makeWithLocalMatrix(W⁻¹)`, where W is the node→root matrix the paint
   *  walk accumulated (`PaintFrame::toRoot`) — the same matrix the hit
   *  test inverts, so a node draws its field exactly where it can be hit.
   *  There is one such seam, inside resolve()/build(), so every consumer
   *  inherits it: fill(), coverage gates, Effect::child() materials and
   *  blend() layers.
   *
   *  Rides the GEOMETRY tier, like uResolution: W is layout-derived, so the
   *  material resolves when the node records, and the library re-records it
   *  when the node or any ancestor moves, when an ancestor's described
   *  transform changes, and every frame while a BOUND transform above it is
   *  connected — releasing that once the transform provably holds still.
   *  The flag is recipe and participates in operator==; W itself belongs to
   *  the system and never compares.
   *
   *  Resolved OUTSIDE a composer (asShader(), a standalone decoration,
   *  measure()): toRoot is identity, and the material deterministically
   *  degrades to NODE-LOCAL coordinates — same picture as the unflagged
   *  material. A Cache::Group subtree refuses to hold a bake across a
   *  moving world-space field (W is not among the floats the group memo
   *  compares), so it drops to live paint there — conservative, never
   *  stale. */
  Paint& worldSpace(bool on = true);
  /** Is THIS material flagged world-space (the layer-local flag)? */
  bool worldSpace() const { return m_worldSpace; }
  /** Does this material — or any blend() layer or child() below it —
   *  anchor to the root? The reconcile walk asks this to flag the
   *  instance for W-invalidation; authors want worldSpace() above. */
  bool usesWorldSpace() const;

  /** THE BOUND PAN: move an image-backed material LIVE, in the node's own
   *  px, with no re-describe — `Pattern::offset(SkPoint)` in its bound
   *  form.
   *
   *  The resolved pair is treated as content scalars, so the node lifts to
   *  content volatility while the pan is moving and is released once the
   *  values provably hold still, letting ancestors cache again; the
   *  per-draw scan re-declares it on the frame an externally-driven pan
   *  resumes.
   *
   *  Meaningful on image() and buffer() materials only — the kinds whose
   *  recipe carries a local matrix for the pan to translate. On any other
   *  kind it is warned and IGNORED, matching uniform()'s guardrails.
   *  Composes with the recipe's static matrix rather than replacing it: the
   *  bound values post-translate, so a static phase origin and a bound pan
   *  add. Either axis may be left empty to pan the other alone, and an
   *  axis may be a shaped `bind()` chain — a wrapped ramp is a conveyor, a
   *  ping-pong is a rocking weave — so the arithmetic sits beside the pan
   *  it drives.
   *
   *  The BINDING is recipe and participates in operator== as an animatable
   *  does: a live axis by its Output's identity, like a bound fill; the
   *  values it resolves to belong to the system and never enter the prune
   *  comparison. */
  Paint& offset(std::optional<motion::Animatable<float>> x,
                std::optional<motion::Animatable<float>> y);
  /** Does THIS material carry a pan channel at all (the layer-local
   *  answer)? A channel carrying a plain number answers yes: it is still a
   *  pan the paint has to apply. Whether it MOVES is the next question. */
  bool hasBoundOffset() const {
    return m_boundOffset[0].has_value() || m_boundOffset[1].has_value();
  }
  /** Is either axis of that pan actually moving? A pan that is a constant
   *  is a placed tile, not an animation, and must not put its node on the
   *  live path forever. */
  bool boundOffsetLive() const {
    return (m_boundOffset[0] && motion::isLive(nullptr, *m_boundOffset[0])) ||
           (m_boundOffset[1] && motion::isLive(nullptr, *m_boundOffset[1]));
  }
  /** The pan as of NOW — what each axis's animatable reads as, 0 for an
   *  axis that carries none. Every consumer reads the pan through this one
   * body, so the volatility release, the per-draw scan and the paint itself
   * cannot disagree about what the current value is. */
  SkPoint boundOffsetValue() const;
  /** Everything isAnimated() reports EXCEPT this material's own bound
   *  offset: live uniform bindings, uTime/uContentScale, and any animated
   *  child() or blend() layer — including a NESTED bound offset, which the
   *  node-level scalar lane cannot reach and must therefore treat as
   *  opaque. Subtracting this from isAnimated() is what routes a pan-only
   *  material onto the comparable-scalar path instead of the coarser
   *  live-material memo. */
  bool animatedBeyondBoundOffset() const;

  /** Step the auto-injected uTime at `hz`, as floor(t·hz)/hz — deliberate
   *  choppiness declared as a property of the MATERIAL rather than plumbed
   *  through whatever drives the clock. Stop-motion and flipbook surfaces
   *  read as intentional at a low rate where a smoothly interpolated one
   *  reads as sliding. Meaningful only on an sksl() material whose effect
   *  declares uTime; warned and ignored otherwise, and 0 restores
   *  continuous time. */
  Paint& quantizeTime(float hz);

  // ---- resolution ----------------------------------------------------------
  /** THE VOLATILITY DECLARATION — the same word every decoration scheme
   *  spells (see the AnimatedDecoration concept). True once any ch::Output
   *  uniform is bound OR the effect reads uTime or uContentScale (both
   *  change independently of the node): the material re-resolves per frame
   *  and its node stays volatile. A blend() inherits this from its
   *  layers. */
  bool isAnimated() const;
  /** True when the effect declares uResolution (the node's layout size):
   *  the paint needs a frame at resolve, but is stable between
   *  layouts — it resolves when its node records, CACHES like static
   *  content, and re-records on size change. A blend() inherits this from
   *  its layers (the flatten defers to resolve time). */
  bool geometryDependent() const;

  bool isNone() const {
    return !m_isSolid && !m_shader && !m_live && !m_backed;
  }
  bool isSolid() const { return m_isSolid; }
  SkColor4f solidColor() const { return m_solid; }
  /** Always produces a shader (a solid becomes SkShaders::Color) — what
   *  blend() composes. For a live material this builds a fresh shader
   *  sampling bound Outputs at their CURRENT values (a snapshot, not a
   *  binding); a blend() with a live LAYER folds its layers per call for the
   *  same reason. Use resolve() for the per-frame paint path. */
  sk_sp<SkShader> asShader() const;
  /** The STATIC snapshot: the shader a non-live paint already holds, or
   *  null for a solid, for nothing, and for a paint that needs a frame.
   *  A consumer that has asked `isSolid()` first reads its colour and
   *  never comes here. */
  sk_sp<SkShader> staticShader() const { return m_shader; }
  /** THE PER-DRAW SHADER: for a live paint, rebuilt from the bound
   *  Outputs and @p frame (uTime/uResolution/uContentScale); for a
   *  geometry-dependent one, built against the frame's box; for a static
   *  one, exactly `staticShader()`. Null for a solid and for nothing —
   *  ask `isSolid()` and `isNone()` first. */
  sk_sp<SkShader> shaderFor(const PaintFrame& frame) const;

  /** THE PASS RESOLVE — what the fx() runtime calls for a `fx::pass`
   *  track's material, once per draw: the recipe specialized to
   *  `in.units` (one definition per count, compiled once), the instance's
   *  values, bindings and children resolved exactly as resolve() resolves
   *  them, and the runtime's own slots — uContent, uUnitRect, uUnitPhase —
   *  filled from @p in. Null when the material is not recipe-backed or its
   *  specialization does not compile; the caller draws the units plainly
   *  then, so a broken pass shows resting letters rather than nothing. */
  sk_sp<SkShader> resolvePass(const detail::PassInputs& in,
                              const PaintFrame& frame) const;

  /** STRUCTURAL value equality — the prune signature. Two materials
   *  compare equal when they were built from the same recipe: solids by
   *  colour; gradients by geometry, stops and tile mode; images by (image
   *  pointer, tile modes, matrix, sampling); static sksl by (effect
   *  pointer, constant values, CHILD materials); blend stacks recursively
   *  by (layer recipes, modes). So re-running the same describe code
   *  yields EQUAL materials even though each run minted a fresh SkShader,
   *  which is what lets a material-filled node prune across renders. Raw
   *  shader() wrappers compare by pointer, and Output-bound materials
   *  compare by recipe identity only — they are volatile and never prune
   *  regardless.
   *
   *  **An sksl() material compares by EFFECT POINTER, so a helper that
   *  compiles a fresh `SkRuntimeEffect` on every call never compares equal
   *  to itself.** Its node re-patches on every describe, and every memo
   *  above it misses. Compile the effect once — a function-local static,
   *  or a cache keyed on whatever varies — and hold the resulting Paint
   *  rather than re-minting it. */
  bool operator==(const Paint& o) const;

 private:
  struct Live;    // sksl recipe (effect + constants + Output bindings)
  struct Recipe;  // comparable build recipe (gradients/image/blend)
  struct Backed;  // a Material instance and its resolve memo
  /** @p worldSpace routes root anchoring through this ONE build. Three
   *  things follow from it: the digest of varying inputs gains W's six
   *  floats, because a digest cannot detect a change in an input it was
   *  never fed; uResolution becomes the root canvas size; and the built
   *  shader is wrapped in W⁻¹ BEFORE the memo stores it, so a field that
   *  is holding still keeps a stable shader pointer. */
  static sk_sp<SkShader> build(const Live& live, const PaintFrame* frame,
                               bool worldSpace = false);
  /** Fold a Blend recipe's layers into one shader — `frame` null is the
   *  frameless form (asShader), non-null the per-draw one (shaderFor).
   *  One function so the two can never disagree. */
  sk_sp<SkShader> foldBlend(const PaintFrame* frame) const;
  /** The image shader rebuilt with the bound pan's CURRENT values
   *  post-translated onto the recipe matrix — one construction shared by
   *  resolve() and asShader(), so a bound-offset material cannot look
   *  different depending on which asked. */
  sk_sp<SkShader> pannedImageShader() const;
  void detachLive();    // copy-on-write before any recipe mutation
  void detachBacked();  // the same, for the Material instance
  /** The recipe-backed resolve: @p frame (null is the static snapshot),
   *  the tree's resolved bytes as the memo key, the world-space wrap
   *  inside the memo. */
  sk_sp<SkShader> buildBacked(const PaintFrame* frame) const;

  bool m_isSolid = false;
  bool m_worldSpace = false;  // root-frame anchoring (see worldSpace())
  float m_amount = 1.0f;      // blend-layer strength (see amount())
  float m_bleed = 0.0f;       // recording-cull reserve (see bleed())
  // The bound pan (x, y) — see offset(). Recipe: compared as an
  // animatable is, so a live axis compares by its Output's identity.
  std::array<std::optional<motion::Animatable<float>>, 2> m_boundOffset{};
  SkColor4f m_solid = {0, 0, 0, 0};
  sk_sp<SkShader> m_shader;      // static resolution: null for solid/none; for
                                 // sksl a constants-only snapshot (live paint
                                 // ignores it and goes through resolve())
  std::shared_ptr<Live> m_live;  // sksl recipe; LIVE iff it has Output bindings
  std::shared_ptr<const Recipe> m_recipe;  // comparable recipe (null for
                                           // solid/none/raw-shader/sksl —
                                           // those compare by their own state)
  std::shared_ptr<Backed> m_backed;        // the Material instance (recipe())

  /** FIELD PIN. `operator==` is hand-written, in another translation unit,
   *  and a material that compares equal when it is not lets its node prune
   *  and keep painting the old shader indefinitely — a failure with no
   *  symptom at the point of the mistake. This decomposition names every
   *  member and stops compiling the moment one is added or removed, which
   *  forces the comparator to be revisited. It lives inside the class
   *  because the state is private. */
  static void fieldPin(Paint& v) {
    auto& [isSolid, worldSpace, amount, bleed, boundOffset, solid, shader, live,
           recipe, backed] = v;
    static_assert(
        std::tuple_size_v<decltype(std::tie(isSolid, worldSpace, amount, bleed,
                                            boundOffset, solid, shader, live,
                                            recipe, backed))> == 10,
        "Paint gained or lost a member — rule on it in Paint::operator== "
        "(Paint.cpp: is it RECIPE, or is it derived from the recipe?), "
        "then bump this count.");
  }
};

namespace detail {

inline Paint unitRamp(SkPoint a, SkPoint b, std::vector<Stop> stops,
                      bool radial) {
  // The stop count is BAKED INTO THE SOURCE and one effect is cached per
  // count. The alternative — one effect with a uniform-guarded loop over a
  // fixed maximum — is not available: such a loop faults here, and main()
  // has to stay a single function. Generating per count also means there
  // is no arbitrary ceiling on stops below the uniform budget.
  if (stops.empty()) return Paint::solid(SkColor4f{0, 0, 0, 0});
  if (stops.size() == 1) return Paint::solid(stops.front().color);
  constexpr size_t kMaxStops = 256;  // uniform budget, not a design limit
  if (stops.size() > kMaxStops) stops.resize(kMaxStops);
  const size_t n = stops.size();

  struct Cached {
    size_t count;
    sk_sp<SkRuntimeEffect> effect;
  };
  static std::vector<Cached> cache;
  sk_sp<SkRuntimeEffect> fx;
  for (const Cached& c : cache)
    if (c.count == n) {
      fx = c.effect;
      break;
    }
  if (!fx) {
    std::string src =
        "uniform float2 uResolution;\n"
        "uniform float2 uA;\n"
        "uniform float2 uB;\n"
        "uniform float  uRadial;\n";
    for (size_t i = 0; i < n; ++i) {
      src += "uniform float4 uC" + std::to_string(i) + ";\n";
      src += "uniform float  uS" + std::to_string(i) + ";\n";
    }
    src +=
        "half4 main(float2 xy) {\n"
        "  float2 p = xy / max(uResolution, float2(1.0, 1.0));\n"
        "  float t;\n"
        "  if (uRadial > 0.5) {\n"
        // radius is a fraction of the box's half-diagonal, so {0.5,0.5}
        // r = 1 reaches the corners of ANY box (glowUnit divides it back
        // down to the inscribed circle).
        "    float2 d = p - uA;\n"
        "    t = length(d) / max(uB.x * 0.70710678, 1e-6);\n"
        "  } else {\n"
        "    float2 d = uB - uA;\n"
        "    t = dot(p - uA, d) / max(dot(d, d), 1e-6);\n"
        "  }\n"
        "  t = clamp(t, 0.0, 1.0);\n"
        "  float4 col = uC0;\n";
    for (size_t i = 1; i < n; ++i) {
      const std::string p0 = std::to_string(i - 1), p1 = std::to_string(i);
      src += "  col = mix(col, uC";
      src += p1;
      src += ", clamp((t - uS";
      src += p0;
      src += ") / max(uS";
      src += p1;
      src += " - uS";
      src += p0;
      src += ", 1e-6), 0.0, 1.0));\n";
    }
    src += "  return half4(half3(col.rgb) * half(col.a), half(col.a));\n}\n";
    auto [effect, error] =
        SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
    if (!effect) {
      SkDebugf("sigilmaterial unitRamp shader (%zu stops): %s\n", n,
               error.c_str());
      return Paint::solid(stops.front().color);
    }
    fx = effect;
    cache.push_back({n, fx});
  }

  Paint m = Paint::sksl(fx, {{"uRadial", radial ? 1.0f : 0.0f}});
  m.uniform("uA", std::array<float, 2>{a.x(), a.y()});
  m.uniform("uB", std::array<float, 2>{b.x(), b.y()});
  for (size_t i = 0; i < n; ++i) {
    m.uniform("uC" + std::to_string(i), stops[i].color);
    m.uniform("uS" + std::to_string(i), stops[i].pos);
  }
  return m;
}

}  // namespace detail

}  // namespace sigil::material::skia
