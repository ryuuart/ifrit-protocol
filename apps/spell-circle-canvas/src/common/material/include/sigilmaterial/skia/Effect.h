#pragma once

/** @file
 * POST-PROCESSING over a rendered layer: an image-filter recipe as a
 * comparable value.
 *
 * A `Paint` shades a shape; an `Effect` takes the layer a consumer has
 * already rendered and runs a filter over it — a blur, a displacement, a
 * lighting pass, an SkSL program whose `content` child IS that layer.
 * Chained with `then()`, and comparable, so a consumer that caches a
 * filtered layer proves two frames asked for the same one.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/UniformBlock.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Animated.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material::skia {

/**
 * Post-processing at stacking-context boundaries. `filter` wraps any
 * SkImageFilter (blur, displacement, lighting, compose chains);
 * `shader` wraps an SkSL runtime effect whose child shader is the
 * rendered layer. A consumer attaches one to the layer a node paints, or
 * to what is already painted beneath it; where the consumer caches that
 * layer, an expensive filter over static content is paid once.
 */
class Effect {
 public:
  /** blur()'s held passes — the constant-sigma blurs of the layer at the
   *  declared range, built once and shared by every copy of the value.
   *  Defined with the effect's body; a consumer never holds one. */
  struct BlurLevels;

  static Effect filter(sk_sp<SkImageFilter> f);
  /** A SigilMaterial recipe as the effect: its program runs over the
   *  layer, which arrives in the child slot named `content`; every other
   *  slot and every uniform is bound from the material as it stands now.
   *  Built once, like filter(): the material's bindings are read at
   *  construction and the effect compares by its built filter's identity,
   *  so animate by re-describing. */
  static Effect recipe(const Material& material);
  /** The layer re-emitted blurred beneath itself in @p color — a drop
   *  shadow at zero offset, which keeps the content on top. Chain with
   *  `then()` for a tighter core over a wider halo. */
  static Effect glow(SkColor4f color, float sigma);
  /** Display bloom over the completed layer. Pixels above @p threshold feed
   *  three concentric kernels; their red, green and blue channels are
   *  recombined with progressively different reach, so the feather changes
   *  hue instead of behaving like a same-colour software blur. @p radius is
   *  the outer kernel radius in pixels, @p intensity is its additive energy,
   *  and @p chroma blends from an achromatic falloff at zero to full spectral
   *  separation at one. The sharp source is retained on top. */
  static Effect phosphorBloom(float radius = 9.0f, float threshold = 0.52f,
                              float intensity = 0.46f, float chroma = 0.80f);
  /** @p uniforms are float uniforms set by name on the SkSL effect;
   *  the layer arrives as the child shader named "content".
   *
   *  A name the effect does not declare as a float uniform — a typo, or a
   *  float2/float4/array, none of which this door can fill — is warned
   *  about once and IGNORED, never a debug abort: one typo in a
   *  live-reloaded sketch must not take the host process down. */
  static Effect shader(
      sk_sp<SkRuntimeEffect> effect,
      std::vector<std::pair<std::string, float>> uniforms = {});
  /** A blur that smears ALONG one direction: @p sigma along the axis at
   *  @p angleDeg (degrees, screen sense — 0 smears
   *  horizontally, 90 vertically, 45 down-right), @p across
   *  perpendicular to it (default 0, a pure streak). A spatial filter,
   *  not motion blur: it knows nothing about how the node moved.
   *
   *  Built entirely from existing filters, no new SkSL. At an
   *  axis-aligned angle it IS `SkImageFilters::Blur(x, y)`, bit-identical;
   *  at any other angle it is a rotate → Blur → unrotate sandwich, three
   *  nodes the filter DAG composes.
   *
   *  Unlike a raw filter() this carries a comparable RECIPE, so a
   *  re-described equal directionalBlur PRUNES where filter() — which can
   *  only compare its already-built filter by pointer — does not. The
   *  named parameters "sigma" / "angle" / "across" accept
   *  uniform(name, &output) below, so an animated smear angle rides the
   *  live channel instead of re-describing per frame. */
  static Effect directionalBlur(float sigma, float angleDeg, float across = 0);
  /** A blur whose SIGMA VARIES ACROSS THE NODE — a depth-of-field
   *  falloff, a lens edge, a tube's curvature. @p sigmaMap is a Material
   *  read as a NUMBER rather than as paint: its RED channel at a pixel,
   *  times @p maxSigma, is the blur radius there. The natural authoring is
   *  therefore a unit-space ramp — `Material::linearUnit({0,0}, {1,0},
   *  {{0, black}, {1, white}})` is "sharp at the left edge, softest at the
   *  right" over whatever box the layout decides — and any sksl() material
   *  is an arbitrary field.
   *
   *  Written as its own effect rather than left to Effect::shader because
   *  a hand-written SkSL kernel would have to pay the WORST sigma at every
   *  pixel: SkSL has no cheap dynamic loop bound, so the kernel must be
   *  sized for the largest radius anywhere in the node, and a Gaussian
   *  stops being separable once sigma varies. This spends a fixed number
   *  of passes instead, so cost grows far more slowly with @p maxSigma.
   *  How it spends them is the library's business and deliberately absent
   *  from this signature: the author says "blur varying by this map".
   *
   *  Rides the same rails as directionalBlur: a comparable RECIPE (an
   *  equal re-described blur PRUNES, and the sigma map's Material
   *  participates in that equality), the named parameter "maxSigma"
   *  accepts uniform(name, &output), and a LIVE sigma map (a bound
   *  uniform, uTime) makes the whole effect isAnimated() by tier
   *  inheritance — so a bake can never sample the map once and freeze it.
   *  `child("sigma", otherMap)` re-aims the map on an existing blur.
   *
   *  THE DECLARED VALUE IS THE RANGE A BOUND SIGMA RIDES INSIDE. The
   *  passes are built once from @p maxSigma and held; a bound "maxSigma"
   *  re-wraps only the final mix with a scale, so a sigma that breathes
   *  every frame costs the same fixed passes over the same held inputs
   *  and Skia's filter cache keeps hitting. The result is exact at the
   *  pass sigmas and linear in sigma between them, and a bound value
   *  above the declared range clamps to it. Declare the LARGEST sigma
   *  the binding will reach: a declared 0 declares no range, and a
   *  bound value then rebuilds every pass at every paint, which is the
   *  full cost the range exists to avoid. */
  static Effect blur(Paint sigmaMap, float maxSigma);
  /** THE CHILD SLOT — `Material::child` on the effect seam: same name,
   *  same shape, same semantics. The effect declares `uniform shader
   *  NAME;` and this fills it with a Material, so the SkSL can read a
   *  source the node has NOT painted: a parameter field, a mask channel, a
   *  gradient, a second texture. `Effect::shader` fills exactly one child
   *  itself — `content`, the node's own rendered layer — and this is how
   *  any further declared `uniform shader` gets a source. The Material
   *  resolves against THIS NODE's box, so unit-space authoring
   *  (linearUnit / glowUnit) works here as it does on a fill.
   *
   *  TIER INHERITANCE is the load-bearing half, and it calls Material's
   *  own recursion rather than repeating its rule: a live child makes the
   *  effect isAnimated(), so the node is declared volatile and no cache
   *  can freeze the parameter; the children also ride the prune signature,
   *  so two effects with different children never compare equal.
   *
   *  SILENT-ISH GUARDRAILS, matching Material::child. A name the effect
   *  does not declare as `uniform shader` warns and is IGNORED. On an
   *  effect kind with no child to fill — `filter()`, which wraps an
   *  already-built SkImageFilter, or a bare `directionalBlur()` — the call
   *  is a no-op with a warning, exactly as uniform() is there. On a
   *  blur() the one fillable name is "sigma", its sigma map. */
  Effect& child(std::string name, Paint source);
  /** A LIVE float uniform — Material's contract, on the effect seam. The
   *  value is read from the Output at every paint, and the node repaints
   *  every frame while the effect is attached: a bound uniform declares
   *  volatility exactly as a live material does, which is what lets a
   *  ripple phase or a bloom threshold animate without re-describing.
   *
   *  Meaningful on a shader() effect (any declared float uniform), a
   *  directionalBlur() ("sigma" / "angle" / "across") or a blur()
   *  ("maxSigma"). A filter() has no uniform to receive the value: the
   *  binding warns and is ignored there, and no volatility is declared, so
   *  nothing animates. Every other rejection behaves the same way — a name
   *  a shader() effect does not declare as a float uniform, an unknown
   *  recipe name on the other kinds: warned about once, not recorded, and
   *  no volatility declared for it, because a binding nothing reads must
   *  not cost a repaint per frame forever.
   *
   *  An animatable, so a shaped `bind()` chain drives the uniform
   *  directly. An effect holds no instance, so a value carrying its own
   *  TRANSITION has nothing to run it and reads as its target. */
  Effect& uniform(std::string name, motion::Animatable<float> value);
  /** CONSTANT uniforms after construction — Material::uniform's shapes on
   *  the effect seam, for the sizes the shader() constructor list cannot
   *  carry. The float form is the constructor list's late spelling; the
   *  float2 and float4 forms fill `uniform float2` / `uniform float4`
   *  declarations; the vector form fills a declared ARRAY, matched by
   *  TOTAL float count, so 12 floats fill `float4 uRect[3]` and
   *  `float uWeights[12]` alike.
   *
   *  Meaningful on a shader() effect only — the other kinds have no named
   *  declarations to fill. Guardrails are uniform()'s: an undeclared name,
   *  or one whose declared size is not the value's, warns once and is
   *  IGNORED, never a debug abort. The recipe stays comparable: constants
   *  participate in operator==, so a re-described equal effect prunes. */
  Effect& uniform(std::string name, float value);
  Effect& uniform(std::string name, std::array<float, 2> value);
  Effect& uniform(std::string name, std::array<float, 4> value);
  Effect& uniform(std::string name, std::vector<float> values);
  /** A LIVE ARRAY — a `UniformBlock` the caller owns, writes and
   *  commit()s, read at every paint. Declares volatility exactly as a
   *  bound scalar Output does: the node paints live while the effect is
   *  attached, and no cache can freeze the table. The binding compares by
   *  block identity; the values belong to the system and never prune.
   *  Size-checked at store time against the declared array's total float
   *  count, because the builder refuses a partial array write. */
  Effect& uniform(std::string name, std::shared_ptr<const UniformBlock> block);
  /** Chain: apply `next` AFTER this effect (SkImageFilters::Compose) —
   *  e.g. the DWM glass formula: Effect::filter(Blur(3,3)).then(
   *  Effect::shader(colorize)). Static chains precompose once; a chain
   *  with a live side re-composes at each paint. */
  Effect then(const Effect& next) const;

  const sk_sp<SkImageFilter>& imageFilter() const { return m_filter; }
  /** The filter with any bound uniforms resolved NOW — what the paint
   *  phase applies. Identical to imageFilter() for a static effect.
   *  @p ctx is the painting node's PaintFrame, which child() materials
   *  resolve against (its box, its clock) — exactly the context
   *  Material::child hands its children. Null is the context-free form:
   *  static children keep their snapshot, and it is what a caller holding
   *  an Effect outside a paint can ask for. */
  sk_sp<SkImageFilter> resolvedImageFilter(
      const PaintFrame* ctx = nullptr) const;
  /** THE VOLATILITY DECLARATION — one word across the whole library: does
   *  this effect change without a re-describe? True while any uniform is
   *  bound, or while any child Material is live. The tier inheritance
   *  calls `Material::isAnimated()`'s own recursion rather than repeating
   *  its rule. */
  bool isAnimated() const;
  /** Does any child Paint anchor to the root frame
   *  (`Material::worldSpace`)? The reconcile walk asks this so it can mark
   *  the node's world matrix stale when an ancestor's static transform is
   *  re-described. Same tier-inheritance shape as isAnimated(). */
  bool usesWorldSpace() const;
  /** Structural equality for the reconciler. A static shader effect
   *  compares by RECIPE — runtime-effect pointer plus constant uniforms —
   *  so a re-described effect prunes as long as the caller holds ONE
   *  SkRuntimeEffect and rebuilds only the wrapper around it. A live
   *  effect never compares equal, conservatively, like a live material.
   *  A filter() effect compares by filter pointer, since an already-built
   *  SkImageFilter carries no recipe to compare. */
  bool operator==(const Effect& o) const;

 private:
  /** directionalBlur()'s comparable recipe — what operator== compares
   *  (structural, like a shader recipe) and what bound "sigma" / "angle"
   *  / "across" uniforms rebuild from per paint. */
  struct DirectionalBlur {
    float sigma = 0, angleDeg = 0, across = 0;
    bool operator==(const DirectionalBlur&) const = default;
  };
  /** blur()'s comparable recipe — the parameter's RANGE only. The sigma
   *  MAP itself lives in m_children under "sigma", so one child vector
   *  carries every Material an effect samples: one equality, one tier
   *  walk, one resolve loop, and `child("sigma", …)` re-aims the map for
   *  free. */
  struct ParamBlur {
    float maxSigma = 0;
    bool operator==(const ParamBlur&) const = default;
  };

  sk_sp<SkImageFilter> m_filter;
  // The shader recipe (kept so bound uniforms can rebuild per paint and
  // so equality can compare structurally).
  sk_sp<SkRuntimeEffect> m_effect;
  std::vector<std::pair<std::string, float>> m_uniforms;
  // The wider constant shapes, one lane per declared size the builder
  // distinguishes; arrays are stored flat and matched by total float count.
  std::vector<std::pair<std::string, std::array<float, 2>>> m_uniforms2;
  std::vector<std::pair<std::string, std::array<float, 4>>> m_uniforms4;
  std::vector<std::pair<std::string, std::vector<float>>> m_uniformArrays;
  std::vector<std::pair<std::string, motion::Animatable<float>>> m_bound;
  // Live arrays: caller-owned UniformBlocks, read at every paint.
  // Their presence makes the effect isAnimated(), like a bound scalar.
  std::vector<std::pair<std::string, std::shared_ptr<const UniformBlock>>>
      m_blocks;
  std::optional<DirectionalBlur> m_dirBlur;  // directionalBlur()'s recipe
  std::optional<ParamBlur> m_paramBlur;      // blur()'s recipe
  std::shared_ptr<const BlurLevels> m_blurLevels;  // …and its held passes
  // The child slots: `uniform shader NAME` → Material. Held by
  // shared_ptr because Material is only FORWARD-DECLARED here (Material.h
  // includes this header, so it cannot be included back) — the surface is
  // still child(name, Material) by value, and a copied Effect never
  // mutates a shared child, it replaces the pointer.
  std::vector<std::pair<std::string, std::shared_ptr<const Paint>>> m_children;
  // then()-chain retained only when a side is live (static chains
  // precompose into m_filter and carry no nodes).
  std::shared_ptr<const Effect> m_chainA, m_chainB;

  /** Does any child need a PaintFrame to resolve (live or geometry
   *  tier)? Material::build's memo asks exactly this of its own children,
   *  for the same reason: a static child's snapshot is already correct,
   *  and a context-needing one must be rebuilt per paint or it freezes. */
  bool anyChildNeedsContext() const;
  /** The child slot @p name as a shader, resolved against @p ctx. */
  sk_sp<SkShader> childShaderFor(std::string_view name,
                                 const PaintFrame* ctx) const;
  /** The recipe's filter, built unconditionally — the store-time snapshot
   *  (null ctx) and the per-paint resolve are one construction. */
  sk_sp<SkImageFilter> buildFilter(const PaintFrame* ctx) const;

  /** FIELD PIN (see ComposeInternal.h's FIELD PINS block). operator== is
   *  hand-written in Effects.cpp and reads these members directly; the state
   *  is private, so the decomposition lives inside the class. */
  static void fieldPin(Effect& v) {
    auto& [filter, effect, uniforms, uniforms2, uniforms4, uniformArrays, bound,
           blocks, dirBlur, paramBlur, blurLevels, children, chainA, chainB] = v;
    static_assert(std::tuple_size_v<decltype(std::tie(
                          filter, effect, uniforms, uniforms2, uniforms4,
                          uniformArrays, bound, blocks, dirBlur, paramBlur,
                          blurLevels, children, chainA, chainB))> == 14,
                  "Effect gained or lost a member — rule on it in "
                  "Effect::operator== (Effects.cpp), then bump this count. "
                  "(m_filter is EXCLUDED on the shader, directionalBlur and "
                  "blur paths because it is derived from m_effect + the "
                  "constant lanes / m_dirBlur / m_paramBlur + m_children, "
                  "and m_blurLevels is derived from m_paramBlur alone; "
                  "m_bound and m_blocks make the effect isAnimated(), which "
                  "operator== already refuses; m_chainA/B only exist on a "
                  "live chain, ditto.)");
  }
};

}  // namespace sigil::material::skia
