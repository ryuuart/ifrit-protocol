#pragma once

/** @file
 * @ingroup material-paint
 *
 * THE PAINT: what a 2D region is painted with, as one comparable value —
 * a colour, one of the three gradients, a recipe instance, a stack of
 * paints each blended over the ones beneath it, or a source a renderer
 * supplied (an image, a caller-owned raster, a program, a shader). A
 * paint sits in one of three volatility tiers, decided by what it is made
 * of rather than declared: STATIC resolves with no frame, GEOMETRY
 * resolves when its node records, LIVE re-resolves every frame.
 *
 * Nothing here names a renderer. What a paint BECOMES when a renderer
 * draws it — and the sources only a renderer can hand over — are that
 * renderer's executor: <sigilmaterial/skia/Paint.h> for Skia.
 */

#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/BlendMode.h>
#include <sigilmaterial/core/Gradient.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/values/Animatable.h>

#include <array>
#include <cstdint>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sigil::material {

namespace skia {
struct PaintAccess;
}  // namespace skia

/** HOW AN IMAGE MEETS THE BOX it is painting. `Native` is the absence of
 *  the question — source pixels at their own size, placed by the image's
 *  own mapping. The other three are resolved against the box when the
 *  node records.
 *  @trap A fit SUPERSEDES the image's mapping: a sprite's sub-rect and a
 *  fit are two different mappings and only one of them can decide. */
enum class Fit : uint8_t {
  Native,   ///< source px 1:1, placed by the image's own mapping
  Stretch,  ///< fill the box on both axes; the aspect is not kept
  Cover,    ///< keep the aspect, fill the box, crop what overflows
  Contain,  ///< keep the aspect, sit inside the box, leave the margin
};

/** The paint value. Construct through the static factories, or through a
 *  renderer's executor for the sources only it can supply; pass it to a
 *  node's `fill`. */
class Paint {
 public:
  Paint() = default;  ///< none (fully transparent — draws nothing)

  /** @name Leaves
   *  The paints that are not made of other paints: a colour, the three
   *  gradients and a recipe instance.
   *  @{ */
  static Paint solid(Color color);
  /** THE LINEAR GRADIENT from @p start to @p end. In box units — the
   *  default — the two points are fractions of the painted box, so
   *  `linearGradient({0, 0}, {0, 1}, stops)` runs top to bottom of
   *  whatever box it lands on; `{.units = GradientUnits::Pixels}` places
   *  them in the node's px. A box-unit gradient rides the GEOMETRY tier;
   *  a pixel one is static. */
  static Paint linearGradient(glm::vec2 start, glm::vec2 end, ColorStops stops,
                              GradientOptions options = {});
  /** THE RADIAL GRADIENT out of @p center to @p radius. In box units the
   *  centre is a fraction of the box and the radius a multiple of
   *  `options.extent` — at the default, a centred radius of 1 reaches the
   *  box's corners, and with `RadialExtent::ClosestSide` it is the
   *  ellipse inscribed in the box. In pixel units both are px.
   *  `options.focus` moves where the stops start from, as an offset
   *  highlight, while the outer circle stays put. */
  static Paint radialGradient(glm::vec2 center, float radius, ColorStops stops,
                              GradientOptions options = {});
  /** THE CONIC GRADIENT: the stops swept around @p center, over the
   *  window `options.startDegrees` to `options.endDegrees` (clockwise from
   *  3 o'clock). In box units the centre is a fraction of the box and the
   *  angles stay true angles on a box that is not square.
   *  @trap The window CLAMPS outside [0, 360] rather than wrapping; the
   *  factory warns once when it leaves the circle. */
  static Paint conicGradient(glm::vec2 center, ColorStops stops,
                             GradientOptions options = {});
  /** A `Material` instance as the paint. The recipe's declared frame
   *  inputs set the tier exactly as a program's uniforms do, and its
   *  bindings make it live. Equality is the instance's, so two paints
   *  built from equal instances prune.
   *  @trap A pass body reads four names the pass runtime supplies —
   *  `uContent`, `uUnitRect`, `uUnitPhase`, `kUnitCount`. Declaring them
   *  in the recipe, or using such a material as an ordinary fill, does
   *  not compile. */
  static Paint recipe(Material material);
  /** The `Material` instance behind a recipe() paint, or null. */
  const Material* recipeMaterial() const;
  /** @} */

  /** @name The combinator
   *  The one paint made of other paints.
   *  @{ */
  /** Layer paints into ONE: bottom to top, each composited over the
   *  accumulation with its blend mode. The blend inherits its layers'
   *  volatility tier, and flattens eagerly only when every layer is
   *  static.
   *  @trap The first layer IS the accumulation, so its own blend mode
   *  and its `amount` are both ignored — there is nothing beneath it to
   *  composite with or mix back toward. */
  static Paint blend(std::vector<std::pair<Paint, BlendMode>> layers);
  /** @} */

  /** @name Parameters and layer properties
   *  What is set on a paint after it is built: named parameters of a
   *  program-backed paint, and the properties that say how the paint sits
   *  in the layer it paints. Every one copies on write.
   *  @{ */
  /** Bakes a constant into a NAMED float parameter; the paint stays
   *  static. `uTime`, `uResolution` and `uContentScale` are supplied by
   *  the frame wherever the program declares them.
   *  @silent the program declares no such float parameter, or the paint
   *  is a solid, a gradient, an image or a blend (warned once, never an
   *  abort — one sketch typo must not kill a hot-reload host). */
  Paint& set(std::string name, float value);
  /** A constant two-float parameter (`float2`) — offsets, margins,
   *  direction vectors. */
  Paint& set(std::string name, std::array<float, 2> value);
  /** A constant `float4` parameter set from a colour (straight, not
   *  premultiplied). */
  Paint& set(std::string name, Color value);
  /** A constant `float4` parameter from plain numbers — a rect, a
   *  quaternion, anything that is not a colour. */
  Paint& set(std::string name, std::array<float, 4> value);
  /** A complete floating uniform, stored flat and matched against its
   *  TOTAL float count. Vectors, matrices and arrays are accepted — 12
   *  floats fill `float4 uRect[3]`, `float2 uPts[6]` and
   *  `float uWeights[12]` alike.
   *  @silent the declaration is integer or the count differs; a partial
   *  write is refused. Typed scalar, float2 and float4 uploads above
   *  require their matching non-array declarations. */
  Paint& set(std::string name, std::vector<float> values);
  /** A LIVE SCALAR, read at every paint, so the paint is live and its
   *  node volatile for as long as the binding moves. An animatable, so
   *  the arithmetic that shapes the number sits beside the parameter it
   *  feeds.
   *  @trap A paint holds no instance, so a value carrying its own
   *  TRANSITION has nothing to run it and reads as its target. */
  Paint& bind(std::string name, motion::Animatable<float> value);
  /** A COLOUR UNIFORM: four straight sRGB components, without clamping,
   *  read at every paint. A constant keeps the paint static; a live colour
   *  keeps it live. A described motion reads its resting value. */
  Paint& bind(std::string name, motion::Animatable<Color> value);
  /** A LIVE FLOATING UNIFORM — a `UniformBlock` the caller owns, writes
   *  and commit()s, matched by the complete float count like the flat
   *  constant overload. Each paint reads the last committed values, and
   *  the resolve memo reads its revision. Draft edits remain hidden when
   *  another input rebuilds the shader. The binding makes the paint live.
   *  @trap The binding compares by block identity and the values never
   *  prune; hold the block beside your model, not in the describe. */
  Paint& bind(std::string name, std::shared_ptr<const UniformBlock> block);

  /** THE SLOT — a SECOND SOURCE for a program-backed paint, filling a
   *  declared `uniform shader NAME`. Any paint fills one, slots nest,
   *  and the whole tree still resolves as one; the parent inherits its
   *  slots' volatility and compares on them.
   *  @trap Sample anything whose pixel VALUES are data with the nearest
   *  filter: read linearly, an index texture samples a blend of two
   *  unrelated palette entries.
   *  @silent the program declares no such `uniform shader`, or the paint
   *  is not program-backed (warned once). */
  Paint& slot(std::string name, Paint source);

  /** LAYER STRENGTH inside a blend(), in 0..1 — the layer composites
   *  with its blend mode IN FULL and the result then mixes back toward
   *  the accumulation by @p fraction, which is not the same picture as
   *  thinning the layer's own alpha. Clamped; the default 1 is free.
   *  @silent the paint is the blend's FIRST layer or is used directly as
   *  a fill — there is no accumulation to mix back toward. */
  Paint& amount(float fraction);

  /** RECORDING-CULL RESERVE: how far this paint's node paints beyond its
   *  own box, in px, for a shape silhouette larger than the layout rect.
   *  The cached picture or texture is culled to the box plus this, and
   *  paint outside it is cut off. It moves no pixels itself, the default
   *  0 changes nothing, and it joins equality because a changed reserve
   *  has to force a re-record. */
  Paint& bleed(float px);
  /** The declared reserve (0 unless bleed() was set). */
  float bleed() const { return m_bleed; }

  /** WORLD SPACE: this paint's coordinates are the ROOT's frame — canvas
   *  px — instead of the node's, so one field stays continuous across
   *  separately-laid-out nodes. `uResolution` becomes the root canvas
   *  size. Off when unstated, and per LAYER: a blend() does not flag its
   *  layers, a program-backed parent does not flag its slots. It rides
   *  the GEOMETRY tier, since the node→root matrix is layout-derived.
   *  @trap Resolved with no frame in hand — a snapshot, a standalone
   *  decoration, a measurement — it degrades to node-local
   *  coordinates. */
  Paint& worldSpace(bool on = true);

  /** HOW THE SOURCE MEETS THE BOX: stretch it, cover it, or sit inside
   *  it. A stated fit makes the paint GEOMETRY-DEPENDENT — it resolves
   *  when its node records and re-records when layout changes the box —
   *  and a bound pan post-translates it. Unstated it is `Fit::Native`.
   *  @trap Resolved with no box in reach it degrades to `Fit::Native`.
   *  @silent the paint is not image- or buffer-backed (warned once). */
  Paint& fit(Fit how);
  /** Is THIS paint flagged world-space (the layer-local flag)? */
  bool worldSpace() const { return m_worldSpace; }
  /** Does this paint — or any blend() layer or slot below it — read or
   *  anchor to root coordinates? The reconcile walk uses this dependency
   *  to invalidate retained pixels after placement changes. */
  bool usesWorldSpace() const;

  /** THE BOUND PAN: move an image-backed paint LIVE, in the node's own
   *  px, with no re-describe. It composes with the image's own mapping
   *  rather than replacing it, so a static phase origin and a bound pan
   *  add; either axis may be left empty to pan the other alone. The
   *  binding joins operator== as an animatable does.
   *  @silent the paint is not image- or buffer-backed (warned once). */
  Paint& offset(std::optional<motion::Animatable<float>> x,
                std::optional<motion::Animatable<float>> y);
  /** IS THIS PAINT'S OWN PAN THE WHOLE OF WHAT IT ANIMATES? A PARTITION
   *  of the animated paints, not a hint: yes is two floats a consumer
   *  reads back and prunes by, no is opaque. A constant pan answers yes;
   *  a live parameter, uTime and any animated slot, blend() layer or
   *  NESTED pan answer no. */
  bool boundOffsetOnly() const {
    return hasBoundOffset() && !animatedBeyondBoundOffset();
  }
  /** The pan as of NOW, in node px — what each axis's animatable reads
   *  as, 0 for an axis that carries none. The one body every consumer
   *  reads the pan through. */
  glm::vec2 boundOffsetValue() const;

  /** Step the supplied uTime at @p rate, as floor(t·rate)/rate —
   *  deliberate choppiness declared as a property of the PAINT. 0, the
   *  default, is continuous time.
   *  @silent the paint is not program-backed, or its program declares no
   *  uTime (warned once). */
  Paint& quantizeTime(float rate);
  /** @} */

  /** @name What a finished paint answers
   *  @{ */
  /** THE VOLATILITY DECLARATION — the same word every value in this tree
   *  answers with. True once any animatable parameter is bound or the
   *  program reads uTime: the paint re-resolves per frame and its node
   *  stays volatile. A blend() inherits it from its layers. */
  bool isRunning() const;
  /** True when the paint needs the frame it is drawn in to resolve — a
   *  program declaring uResolution or uContentScale, a stated fit(),
   *  worldSpace(). It resolves when its node records, CACHES between
   *  layouts, and re-records when the size or the destination's scale
   *  changes. A blend() inherits it from its layers. */
  bool geometryDependent() const;

  bool isNone() const {
    return !m_isSolid && !m_shader && !m_live && !m_backed && !m_recipe;
  }
  bool isSolid() const { return m_isSolid; }
  Color solidColor() const { return m_solid; }

  /** STRUCTURAL value equality — the prune signature. Two paints
   *  compare equal when they were built from the same recipe, so
   *  re-running the same describe code yields EQUAL paints though each
   *  run minted a fresh renderer object. A renderer's raw source
   *  compares by identity, and bound paints by recipe identity alone.
   *  @trap A program-backed paint compares by PROGRAM identity, so a
   *  helper that compiles a fresh program per call never compares equal
   *  to itself and every memo above it misses. Compile the program once
   *  and hold the resulting paint. */
  bool operator==(const Paint& other) const;
  /** @} */

 private:
  friend struct skia::PaintAccess;

  /** Does THIS paint carry a pan channel at all (the layer-local
   *  answer)? A channel carrying a plain number answers yes: it is still a
   *  pan the paint has to apply. Whether it MOVES is the next question. */
  bool hasBoundOffset() const {
    return m_boundOffset[0].has_value() || m_boundOffset[1].has_value();
  }
  /** Is either axis of that pan actually moving? A pan that is a constant
   *  is a placed tile, not an animation, and must not put its node on the
   *  live path forever. */
  bool boundOffsetLive() const;
  /** Everything isRunning() reports EXCEPT this paint's own bound
   *  offset: live parameter bindings, uTime, and any
   *  animated slot or blend() layer — including a NESTED bound offset,
   *  which the node-level scalar lane cannot reach. */
  bool animatedBeyondBoundOffset() const;
  /** Does the recipe state a fit the box has to answer? */
  bool hasFit() const;
  void detachLive();    // copy-on-write before any recipe mutation
  void detachBacked();  // the same, for the Material instance

  struct Live;      // a program and its parameters, bindings and slots
  struct Recipe;    // comparable build recipe (gradients/image/blend)
  struct Backed;    // a Material instance and its resolve memo
  struct Snapshot;  // the renderer's static resolution

  bool m_isSolid = false;
  bool m_worldSpace = false;  // root-frame anchoring (see worldSpace())
  float m_amount = 1.0f;      // blend-layer strength (see amount())
  float m_bleed = 0.0f;       // recording-cull reserve (see bleed())
  // The bound pan (x, y) — see offset(). Recipe: compared as an
  // animatable is, so a live axis compares by its identity.
  std::array<std::optional<motion::Animatable<float>>, 2> m_boundOffset{};
  Color m_solid = {0, 0, 0, 0};
  // The static resolution: null for solid/none; for a program a
  // constants-only snapshot (a live paint ignores it and resolves per
  // frame).
  std::shared_ptr<const Snapshot> m_shader;
  std::shared_ptr<Live> m_live;  // program recipe; LIVE iff it has bindings
  std::shared_ptr<const Recipe> m_recipe;  // comparable recipe (null for
                                           // solid/none/raw source/program
                                           // — those compare by their own
                                           // state)
  std::shared_ptr<Backed> m_backed;        // the Material instance (recipe())

  /** FIELD PIN. `operator==` is hand-written in another translation
   *  unit, and a paint that compares equal when it is not lets its node
   *  prune and keep painting the old result indefinitely. This
   *  decomposition stops compiling the moment a member is added or
   *  removed. It is inside the class because the state is private. */
  static void fieldPin(Paint& pinned) {
    auto& [isSolid, worldSpace, amount, bleed, boundOffset, solid, shader, live,
           recipe, backed] = pinned;
    static_assert(
        std::tuple_size_v<decltype(std::tie(isSolid, worldSpace, amount, bleed,
                                            boundOffset, solid, shader, live,
                                            recipe, backed))> == 10,
        "Paint gained or lost a member — rule on it in Paint::operator== "
        "(is it RECIPE, or is it derived from the recipe?), then bump "
        "this count.");
  }
};

}  // namespace sigil::material
