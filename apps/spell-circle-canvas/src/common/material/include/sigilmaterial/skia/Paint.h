#pragma once

/** @file
 * @ingroup material-skia
 *
 * THE SKIA PAINT: this library's material model as a Skia shader. A small
 * tree of paint nodes that compiles to ONE `sk_sp<SkShader>` — layers
 * through SkShaders::Blend, never a stacked saveLayer — or a plain solid
 * colour. A paint sits in one of three volatility tiers, decided by what
 * it is made of rather than declared: STATIC resolves with no frame,
 * GEOMETRY resolves when its node records, LIVE re-resolves every frame.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>  // sk_sp<SkShader> data member
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>  // the unit-space ramps
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Pass.h>
#include <sigilmaterial/skia/PixelBuffer.h>  // the source buffer() takes
#include <sigilmotion/values/Animated.h>
#include <sigilmotion/values/Time.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

class SkImage;

/** The Skia-facing half of this library: what a material BECOMES when a
 *  Skia canvas has to draw it. `Paint` is the value; beside it sit the
 *  post-processing `Effect` over a rendered layer, the conversions
 *  between Skia's colour and this library's, and the one call that fills
 *  a path with a material. */
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

/** HOW AN IMAGE MEETS THE BOX it is painting. `Native` is the absence of
 *  the question — source pixels at their own size, placed by `local`.
 *  The other three are resolved against the box when the node records.
 *  @trap A fit SUPERSEDES `local`: a sprite's sub-rect and a fit are two
 *  different mappings and only one of them can decide. */
enum class Fit : uint8_t {
  Native,   ///< source px 1:1, placed by `local`
  Stretch,  ///< fill the box on both axes; the aspect is not kept
  Cover,    ///< keep the aspect, fill the box, crop what overflows
  Contain,  ///< keep the aspect, sit inside the box, leave the margin
};

/** A gradient ramp stop: where along the ramp it sits, in 0..1, and the
 *  colour there, authored in the working colour space. */
struct Stop {
  float pos = 0.0f;
  SkColor4f color = {0, 0, 0, 1};
  bool operator==(const Stop&) const = default;
};

/** The polymorphic paint value. Construct via the static factories; pass to
 *  Element::fill(). */
class Paint {
 public:
  Paint() = default;  ///< none (fully transparent — draws nothing)

  /** @name Leaves
   *  The paints that are not made of other paints: a colour, the
   *  gradients, an image or a caller-owned raster, a hand-built
   *  shader, an SkSL body, and a recipe instance.
   *  @{ */
  static Paint solid(SkColor4f color);
  /** N-stop linear ramp between two points (working-space colors). */
  static Paint linear(SkPoint a, SkPoint b, std::vector<Stop> stops,
                      SkTileMode tile = SkTileMode::kClamp);
  static Paint radial(SkPoint center, float radius, std::vector<Stop> stops,
                      SkTileMode tile = SkTileMode::kClamp);
  /** OFFSET-FOCUS radial: the ramp runs from the circle
   *  (@p focus, @p focusRadius) to the circle (@p center, @p radius), so a
   *  highlight displaced off a sphere's centre is
   *  `conical(hot, 0, centre, R, …)`. Both radii are node-local px.
   *  Unlike a moved radial's, the outer circle stays put. */
  static Paint conical(SkPoint focus, float focusRadius, SkPoint center,
                       float radius, std::vector<Stop> stops,
                       SkTileMode tile = SkTileMode::kClamp);
  /** Angular sweep from @p startDeg (12 o'clock is -90°) around
   *  @p center, in degrees.
   *  @trap Angles outside [0, 360) CLAMP rather than wrapping, so
   *  `sweep(c, stops, 90, 450)` paints a flat quarter; rotate the STOPS
   *  instead. The factory warns once when a window leaves the circle. */
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
   *  The recipe compares by (source, revision), so an identical
   *  re-describe between commits PRUNES and the first describe after a
   *  commit patches exactly once. */
  static Paint buffer(std::shared_ptr<class PixelBuffer> source,
                      SkTileMode tx = SkTileMode::kClamp,
                      SkTileMode ty = SkTileMode::kClamp,
                      const SkMatrix& local = SkMatrix::I(),
                      SkSamplingOptions sampling = {});
  /** An SkSL runtime effect as a shader. @p constants set named float
   *  uniforms once; uniform() binds live ones and slot() fills declared
   *  `uniform shader` sockets. The body's own reads set the tier:
   *  `uTime` or `uContentScale` is LIVE, `uResolution` alone is the
   *  cheaper GEOMETRY tier. */
  static Paint sksl(sk_sp<SkRuntimeEffect> effect,
                    std::vector<std::pair<std::string, float>> constants = {});
  /** Wrap a raw shader (interop / escape). */
  static Paint shader(sk_sp<SkShader> shader);
  /** A `Material` instance as the paint. The recipe's declared frame
   *  inputs set the tier exactly as an sksl() effect's uniforms do, and
   *  its bindings make it live. Equality is the instance's, so two
   *  paints built from equal instances prune.
   *  @trap A pass body reads four names the pass runtime supplies —
   *  `uContent`, `uUnitRect`, `uUnitPhase`, `kUnitCount`. Declaring them
   *  in the recipe, or using such a material as an ordinary fill, does
   *  not compile. */
  static Paint recipe(sigil::material::Material material);
  /** The `Material` instance behind a recipe() paint, or null. */
  const sigil::material::Material* recipeMaterial() const;
  /** @} */

  /** @name The combinator
   *  The one paint made of other paints.
   *  @{ */
  /** Layer materials into ONE flattened shader: bottom to top, each
   *  composited over the accumulation with its SkBlendMode. The blend
   *  inherits its layers' volatility tier, and flattens eagerly only
   *  when every layer is static.
   *  @trap The first layer IS the accumulation, so its own blend mode
   *  and its `amount` are both ignored — there is nothing beneath it to
   *  composite with or mix back toward. */
  static Paint blend(std::vector<std::pair<Paint, SkBlendMode>> layers);
  /** @} */

  /** @name Unit-space ramps
   *  The same gradients authored in the box's UNIT SQUARE rather
   *  than in pixels, for a box whose size the layout decides. Each
   *  rides the GEOMETRY tier through uResolution and takes any number
   *  of stops.
   *  @{ */
  /** linear() authored in the node's UNIT SQUARE: (0,0) is the box's
   *  top-left, (1,1) its bottom-right, whatever the box turns out to
   *  be. */
  static Paint linearUnit(SkPoint from01, SkPoint to01,
                          std::vector<Stop> stops);
  /** The unit-square radial: @p center01 and @p radius01 as a fraction
   *  of the box's HALF-DIAGONAL, so a ramp centred at {0.5, 0.5} with
   *  radius 1 reaches the box's CORNERS.
   *  @trap 0.707 is the radius that reaches the INSCRIBED circle, so a
   *  glow authored at 1 on a circle-shaped node is cut off mid-falloff
   *  and one authored past 1 never intersects the shape at all. Use
   *  glowUnit() for "fills this box". */
  static Paint radialUnit(SkPoint center01, float radius01,
                          std::vector<Stop> stops);
  /** A soft round light that FILLS the box: @p radius01 is a fraction of
   *  the box's SHORTER SIDE, so radius 1 is the inscribed circle.
   *  @trap Like the other two it works in the UNIT SQUARE, so on a
   *  non-square box the falloff is elliptical; put it on a square node
   *  for a true circle. */
  static Paint glowUnit(SkPoint center01, float radius01,
                        std::vector<Stop> stops);
  /** @} */

  /** @name Uniforms and layer properties
   *  What is set on a paint after it is built: named uniforms, and the
   *  properties that say how the paint sits in the layer it paints.
   *  Every one copies on write.
   *  @{ */
  /** Bakes a constant into a NAMED float uniform; the paint stays
   *  static. `uTime`, `uResolution` and `uContentScale` are
   *  auto-injected each frame where the effect declares them.
   *  @silent the effect declares no such float uniform, or the paint is
   *  a solid, a gradient, an image or a blend (warned once, never an
   *  abort — one sketch typo must not kill a hot-reload host). */
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
   *  `float2 uPts[6]` and `float uWeights[12]` alike.
   *  @silent the count is not the declaration's; the builder refuses a
   *  partial write, so the whole array must be supplied. */
  Paint& uniform(std::string name, std::vector<float> values);
  /** A LIVE ARRAY — a `UniformBlock` the caller owns, writes and
   *  commit()s, read at every paint. The paint becomes LIVE exactly as a
   *  bound scalar makes it, and the resolve memo reads the block's
   *  REVISION, so an uncommitted frame reuses the built shader.
   *  @trap The binding compares by block identity and the values never
   *  prune; hold the block beside your model, not in the describe. */
  Paint& uniform(std::string name,
                 std::shared_ptr<const material::UniformBlock> block);
  /** A LIVE SCALAR, read at every paint, so the paint is live and its
   *  node volatile for as long as the binding is attached. An
   *  animatable, so the arithmetic that shapes the number sits beside
   *  the uniform it feeds.
   *  @trap A paint holds no instance, so a value carrying its own
   *  TRANSITION has nothing to run it and reads as its target. */
  Paint& uniform(std::string name, motion::Animatable<float> output);

  /** THE SLOT — a SECOND SOURCE for an sksl() material, filling a
   *  declared `uniform shader NAME`. Any Paint fills one, slots nest,
   *  and the whole tree still compiles to ONE shader; the parent
   *  inherits its slots' volatility and compares on them.
   *  @trap Pass a NEAREST sampling mode for anything whose pixel VALUES
   *  are data: read linearly, an index texture samples a blend of two
   *  unrelated palette entries.
   *  @silent the effect declares no such `uniform shader`, or the paint
   *  is not sksl()-backed (warned once). */
  Paint& slot(std::string name, Paint source);

  /** LAYER STRENGTH inside a blend(), in 0..1 — the layer composites
   *  with its blend mode IN FULL and the result then mixes back toward
   *  the accumulation by @p a01, which is not the same picture as
   *  thinning the layer's own alpha. Clamped; the default 1 is free.
   *  @silent the paint is the blend's FIRST layer or is used directly as
   *  a fill — there is no accumulation to mix back toward. */
  Paint& amount(float a01);

  /** RECORDING-CULL RESERVE: how far this material's node paints beyond
   *  its own box, in px, for a shape silhouette larger than the layout
   *  rect. The cached picture or texture is culled to the box plus this,
   *  and paint outside it is cut off. It moves no pixels itself, the
   *  default 0 changes nothing, and it joins equality because a changed
   *  reserve has to force a re-record. */
  Paint& bleed(float px);
  /** The declared reserve (0 unless bleed() was set). */
  float bleed() const { return m_bleed; }

  /** WORLD SPACE: this material's coordinates are the COMPOSER ROOT's
   *  frame — canvas px — instead of the node's, so one field stays
   *  continuous across separately-laid-out nodes. `uResolution` becomes
   *  the root canvas size. Off when unstated, and per material LAYER: a
   *  blend() does not flag its layers, an sksl() parent does not flag
   *  its slots. It rides the GEOMETRY tier, since the node→root matrix
   *  is layout-derived.
   *  @trap Resolved outside a composer — asShader(), a standalone
   *  decoration, a measurement — it degrades to node-local coordinates. */
  Paint& worldSpace(bool on = true);

  /** HOW THE SOURCE MEETS THE BOX: stretch it, cover it, or sit inside
   *  it. A stated fit makes the paint GEOMETRY-DEPENDENT — it resolves
   *  when its node records and re-records when layout changes the box —
   *  and a bound pan post-translates it. Unstated it is `Fit::Native`.
   *  @trap Resolved with no box in reach it degrades to `Fit::Native`.
   *  @silent the paint is not image()- or buffer()-backed (warned once). */
  Paint& fit(Fit how);
  /** Is THIS material flagged world-space (the layer-local flag)? */
  bool worldSpace() const { return m_worldSpace; }
  /** Does this material — or any blend() layer or slot below it —
   *  anchor to the root? The reconcile walk asks this to flag the
   *  instance for W-invalidation; authors want worldSpace() above. */
  bool usesWorldSpace() const;

  /** THE BOUND PAN: move an image-backed material LIVE, in the node's own
   *  px, with no re-describe. It composes with the recipe's static
   *  matrix rather than replacing it, so a static phase origin and a
   *  bound pan add; either axis may be left empty to pan the other
   *  alone. The binding joins operator== as an animatable does.
   *  @silent the paint is not image()- or buffer()-backed (warned once). */
  Paint& offset(std::optional<motion::Animatable<float>> x,
                std::optional<motion::Animatable<float>> y);
  /** IS THIS MATERIAL'S OWN PAN THE WHOLE OF WHAT IT ANIMATES? A
   *  PARTITION of the animated materials, not a hint: yes is two floats
   *  a consumer reads back and prunes by, no is opaque. A constant pan
   *  answers yes; a live uniform, uTime, uContentScale and any animated
   *  slot, blend() layer or NESTED pan answer no. */
  bool boundOffsetOnly() const {
    return hasBoundOffset() && !animatedBeyondBoundOffset();
  }
  /** The pan as of NOW, in node px — what each axis's animatable reads
   *  as, 0 for an axis that carries none. The one body every consumer
   *  reads the pan through. */
  SkPoint boundOffsetValue() const;

  /** Step the auto-injected uTime at @p hz, as floor(t·hz)/hz —
   *  deliberate choppiness declared as a property of the MATERIAL. 0,
   *  the default, is continuous time.
   *  @silent the paint is not sksl()-backed, or its effect declares no
   *  uTime (warned once). */
  Paint& quantizeTime(float hz);
  /** @} */

  /** @name Resolution
   *  What a consumer asks a finished paint: which volatility tier it
   *  rides, what it is made of, and the shader or the colour to draw
   *  with.
   *  @{ */
  /** THE VOLATILITY DECLARATION — the same word every value in this tree
   *  answers with. True once any animatable uniform is bound or the
   *  effect reads uTime or uContentScale: the paint re-resolves per
   *  frame and its node stays volatile. A blend() inherits it from its
   *  layers. */
  bool isAnimated() const;
  /** True when the paint needs the node's layout size to resolve — an
   *  effect declaring uResolution, a stated fit(), worldSpace(). It
   *  resolves when its node records, CACHES between layouts, and
   *  re-records on size change. A blend() inherits it from its
   *  layers. */
  bool geometryDependent() const;

  bool isNone() const {
    return !m_isSolid && !m_shader && !m_live && !m_backed;
  }
  bool isSolid() const { return m_isSolid; }
  SkColor4f solidColor() const { return m_solid; }
  /** Always produces a shader — a solid becomes a colour shader — which
   *  is what blend() composes. For a live paint it builds a fresh shader
   *  sampling bound values at their CURRENT readings: a snapshot, not a
   *  binding. */
  sk_sp<SkShader> asShader() const;
  /** The STATIC snapshot: the shader a non-live paint already holds, or
   *  null for a solid, for nothing, and for a paint that needs a
   *  frame. */
  sk_sp<SkShader> staticShader() const { return m_shader; }
  /** THE PER-DRAW SHADER: for a live paint, rebuilt from the bound
   *  values and @p frame; for a geometry-dependent one, built against
   *  the frame's box; for a static one, exactly `staticShader()`.
   *  @trap Null for a solid and for nothing — ask `isSolid()` and
   *  `isNone()` first. */
  sk_sp<SkShader> shaderFor(const PaintFrame& frame) const;

  /** THE PASS RESOLVE — what a text runtime calls for a pass track's
   *  material, once per draw: the recipe specialized to `in.units`, the
   *  instance's values, bindings and slots, and the runtime's own slots
   *  filled from @p in. Null when the material is not recipe-backed or
   *  its specialization does not compile, so a broken pass shows resting
   *  letters rather than nothing. */
  sk_sp<SkShader> resolvePass(const PassInputs& in,
                              const PaintFrame& frame) const;

  /** STRUCTURAL value equality — the prune signature. Two paints
   *  compare equal when they were built from the same recipe, so
   *  re-running the same describe code yields EQUAL paints though each
   *  run minted a fresh SkShader. Raw shader() wrappers compare by
   *  pointer, and bound paints by recipe identity alone.
   *  @trap An sksl() paint compares by EFFECT POINTER, so a helper that
   *  compiles a fresh `SkRuntimeEffect` per call never compares equal to
   *  itself and every memo above it misses. Compile the effect once and
   *  hold the resulting paint. */
  bool operator==(const Paint& o) const;
  /** @} */

 private:
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
  /** Everything isAnimated() reports EXCEPT this material's own bound
   *  offset: live uniform bindings, uTime/uContentScale, and any
   *  animated slot or blend() layer — including a NESTED bound offset,
   *  which the node-level scalar lane cannot reach. */
  bool animatedBeyondBoundOffset() const;

  struct Live;    // sksl recipe (effect + constants + Output bindings)
  struct Recipe;  // comparable build recipe (gradients/image/blend)
  struct Backed;  // a Material instance and its resolve memo
  /** @p worldSpace routes root anchoring through this ONE build: the
   *  digest of varying inputs gains W's six floats, uResolution becomes
   *  the root canvas size, and the shader is wrapped in W⁻¹ before the
   *  memo stores it. */
  static sk_sp<SkShader> build(const Live& live, const PaintFrame* frame,
                               bool worldSpace = false);
  /** Fold a Blend recipe's layers into one shader — `frame` null is the
   *  frameless form (asShader), non-null the per-draw one (shaderFor).
   *  One function so the two can never disagree. */
  sk_sp<SkShader> foldBlend(const PaintFrame* frame) const;
  /** THE FOLD ITSELF, which blend()'s eager flatten and foldBlend's
   *  deferred one both are: each layer after the first composited over
   *  the accumulation with its mode, then mixed back toward it by its
   *  `amount`. @p shaderOf resolves ONE layer, which is the whole of
   *  what the two callers differ by. */
  static sk_sp<SkShader> foldLayers(
      std::span<const std::pair<Paint, SkBlendMode>> layers,
      const std::function<sk_sp<SkShader>(const Paint&)>& shaderOf);
  /** The image shader rebuilt with the bound pan's CURRENT values
   *  post-translated onto the recipe matrix — one construction shared by
   *  resolve() and asShader(), so a bound-offset material cannot look
   *  different depending on which asked. */
  sk_sp<SkShader> pannedImageShader() const;
  sk_sp<SkShader> fittedImageShader(const PaintFrame& frame) const;
  /** Does the recipe state a fit the box has to answer? */
  bool hasFit() const;
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

  /** FIELD PIN. `operator==` is hand-written in another translation
   *  unit, and a material that compares equal when it is not lets its
   *  node prune and keep painting the old shader indefinitely. This
   *  decomposition stops compiling the moment a member is added or
   *  removed. It is inside the class because the state is private. */
  static void fieldPin(Paint& v) {
    auto& [isSolid, worldSpace, amount, bleed, boundOffset, solid, shader, live,
           recipe, backed] = v;
    static_assert(
        std::tuple_size_v<decltype(std::tie(isSolid, worldSpace, amount, bleed,
                                            boundOffset, solid, shader, live,
                                            recipe, backed))> == 10,
        "Paint gained or lost a member — rule on it in Paint::operator== "
        "(is it RECIPE, or is it derived from the recipe?), then bump "
        "this count.");
  }
};

}  // namespace sigil::material::skia
