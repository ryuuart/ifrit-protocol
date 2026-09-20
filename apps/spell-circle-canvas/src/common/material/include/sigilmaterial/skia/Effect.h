#pragma once

/** @file
 * POST-PROCESSING over a rendered layer: an image-filter recipe as a
 * comparable value.
 *
 * A `Paint` shades a shape; an `Effect` takes the layer a consumer has
 * already rendered and runs a filter over it — a blur, a displacement, a
 * lighting pass, an SkSL program whose `content` slot IS that layer.
 * Chained with `then()`, and comparable, so a consumer that caches a
 * filtered layer proves two frames asked for the same one.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/UniformBlock.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Animated.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material::skia {

/**
 * Post-processing at stacking-context boundaries. `filter` wraps any
 * SkImageFilter (blur, displacement, lighting, compose chains);
 * `shader` wraps an SkSL runtime effect whose `content` slot is the
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
  /** A COLOUR FILTER as the effect — a per-pixel colour map with no
   *  neighbourhood, so a consumer applies it to the layer's paint rather
   *  than through the filter graph and pays a blit instead of a pass.
   *  `colorFilter()` is how that consumer reads it back; an effect built
   *  this way carries no image filter of its own, and where one is asked
   *  for — a `then()` chain, `resolvedImageFilter()` — the colour filter
   *  is lifted into the filter graph so the picture is right either
   *  way. */
  static Effect filter(sk_sp<SkColorFilter> f);
  /** A SigilMaterial recipe as the effect: its program runs over the
   *  layer, which arrives in the slot named `content`; every other
   *  slot and every uniform is bound from the material as it stands now.
   *  sampleRadius bounds the largest local-coordinate source offset.
   *  Built once: the material's bindings are read at construction, so
   *  animate by re-describing. A static material compares by its value
   *  and compiled program; a material with live inputs compares by the
   *  built filter's identity because those inputs were sampled once. */
  static Effect recipe(const Material& material);
  static Effect recipe(const Material& material, float sampleRadius);
  /** THE SAME RECIPE, LOWERED FOR THE SURFACE IT WILL LAND ON.
   *
   *  A recipe that declares itself `channelwise` maps each channel
   *  through one row of samples and touches nothing else, which on a
   *  surface carrying eight bits per channel is a 256-entry table per
   *  channel and no program at all: this answers that table as a colour
   *  filter, which the consumer hangs on the layer's paint. Every other
   *  case — a recipe that is not channelwise, a row that is not 256
   *  samples wide or cannot be read, a surface with more precision than
   *  a table can carry, and `kUnknown_SkColorType`, which is what a
   *  canvas backed by neither raster nor GPU answers — falls to
   *  `recipe(material)` and the program, so the picture is the same
   *  either way and only the cost differs.
   *
   *  @p surface is the colour type of the surface the effect will be
   *  composited on, which a consumer reads from its canvas at the moment
   *  it paints. */
  static Effect recipe(const Material& material, SkColorType surface);
  /** The layer re-emitted blurred beneath itself in @p color — a drop
   *  shadow at zero offset, which keeps the content on top. Chain with
   *  `then()` for a tighter core over a wider halo. */
  static Effect glow(SkColor4f color, float sigma);
  /** THE LAYER WITH EVERYTHING BUT ITS LIGHT TAKEN OUT: what is brighter
   *  than @p threshold, faded in over @p knee above it, carrying that
   *  brightness as its own coverage. The first half of a bloom, on its
   *  own, so a consumer can spend a blur and a `kPlus` composite where
   *  `phosphorBloom()` would spend a gather: chain it with a blur and lay
   *  the result back over the source.
   *
   *  The gate is read on the STRAIGHT colour and the coverage is
   *  rewritten from it, because what comes back is a layer rather than
   *  light to add — a pixel half covered by white is white, and gating it
   *  premultiplied would call it grey and eat the edge of every source in
   *  the layer. `phosphorBloom()`'s own gate reads the premultiplied
   *  colour for the opposite reason: it never emits a layer, it
   *  accumulates energy, and there coverage IS part of how much light a
   *  pixel contributes.
   *
   *  Brightness is the peak channel, not luminance, so a saturated
   *  primary blooms as readily as a white — which is what a phosphor and
   *  a lamp both do, and what a luminance gate would refuse a deep blue
   *  source.
   *
   *  IT READS ITS OWN PIXEL AND NO NEIGHBOUR, so it is a COLOUR MAP and
   *  `colorFilter()` answers it rather than `imageFilter()`. That is what
   *  keeps `emit()` honest: a filter graph holding a program over
   *  coordinates is evaluated in the LAYER's pixels and resampled onto a
   *  scaled canvas, so a light made with one would soften the sharp layer
   *  it is laid back over even where the light is wholly transparent. A
   *  colour map carries no such constraint and the layer keeps the
   *  device's own pixels. */
  static Effect brightPass(float threshold = 0.68f, float knee = 0.30f);
  /** The layer blurred by a Gaussian of @p sigma local pixels in both
   *  directions — the stage a glow spreads its light with. */
  static Effect blur(float sigma);
  /** THE ROUNDED SPREAD: every edge grown outward by @p pixels, so the
   *  layer's colour carries past it as a body before anything feathers
   *  it, as a shadow's spread does. A blur at one and a half times the
   *  distance leaves a quarter of an edge's coverage one distance out, and
   *  quadrupling coverage restores it there, so corners stay round and
   *  the gaps between letters stay open until the spread reaches them —
   *  where a square morphological kernel would fill them as plates. The
   *  straight colour is kept. It grows COVERAGE, so over an opaque ground
   *  it is only a blur: spread a light — `brightPass()`, which carries
   *  brightness as coverage, or a layer drawn on transparency. */
  static Effect dilate(float pixels);
  /** FAINT LIGHT LOSES ITS WEAKER CHANNELS FIRST, as a tone curve's toe
   *  drops them: the straight colour, normalised to its peak, is raised
   *  to 1 + @p amount × (1 − coverage), so a dense layer keeps its colour
   *  and a thin one sinks toward its strongest channel — orange toward
   *  red, yellow toward orange, a blue-leaning cyan toward blue — with its
   *  brightest channel held. Over a blurred light, the halo deepens as it
   *  fades. */
  static Effect deepen(float amount);
  /** THE OTHER END OF THAT CURVE: where the straight colour's peak is
   *  above @p threshold, faded in over @p knee, it moves @p amount of the
   *  way toward white at that peak, as an overexposed core does, so a lit
   *  shape reads lighter than the deeper light around it. Colour below
   *  the threshold is untouched. */
  static Effect whiten(float amount, float threshold = 0.2f,
                       float knee = 0.2f);
  /** Display bloom over the completed layer. Pixels above @p threshold feed
   *  three concentric kernels; their red, green and blue channels are
   *  recombined with progressively different reach — red the widest, blue
   *  the tightest, as a phosphor's own spread is — so the feather changes
   *  hue instead of behaving like a same-colour software blur. @p radius is
   *  the outer kernel radius in pixels, @p intensity is its additive energy,
   *  and @p chroma blends from an achromatic falloff at zero to full spectral
   *  separation at one. The sharp source is retained on top.
   *
   *  @p hueDrift is the turn, in degrees, the halo's hue has made at the
   *  outer radius: each kernel turns in proportion to its reach, and only
   *  where the pixel is lit by a halo rather than by a source of its own,
   *  so the source keeps its colour and its decay tail drifts. A NEGATIVE
   *  turn is the direction a phosphor decays — a warm source's halo goes
   *  through orange toward red, a cool source's through cyan toward
   *  green. @p tail is extra energy on the outermost kernel beyond the
   *  three-kernel falloff, so a stronger glow reaches further rather than
   *  only brighter. Both default to zero, which is exactly the falloff
   *  without them.
   *
   *  THE HALO IS GATHERED COARSE AND LAID BACK OVER THE SHARP SOURCE.
   *  Twenty-four samples of the layer per pixel — three radii of eight
   *  headings — is what a gather costs, so it is not spent at the layer's
   *  own resolution: the bright pass, the rings, the drift and the tail
   *  run over a layer reduced until the INNERMOST ring is about a pixel
   *  across, and the result is resampled up and added to the untouched
   *  source, which is one tap of each. A halo is a low-frequency picture
   *  and survives that; the reduction is why the effect costs near a
   *  bright pass rather than twenty-four times one. What it changes is
   *  the halo's fine structure — a hard-edged source hands its step to a
   *  resample — and never the source itself, which is composited at full
   *  resolution and to the bit. A reach small enough to be blurred away
   *  by the reduction is gathered whole instead.
   *
   *  The layer this runs over should still be bounded: put the glow
   *  sources on their own node under `Cache::Texture` and the bloom is
   *  baked with them once. */
  static Effect phosphorBloom(float radius = 9.0f, float threshold = 0.52f,
                              float intensity = 0.46f, float chroma = 0.80f,
                              float hueDrift = 0.0f, float tail = 0.0f);
  /** @p uniforms are float uniforms set by name on the SkSL effect;
   *  the layer arrives as the slot named "content".
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
   *  `slot("sigma", otherMap)` re-aims the map on an existing blur.
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
  /** THE SLOT — `Material::slot` on the effect seam: same name,
   *  same shape, same semantics. The effect declares `uniform shader
   *  NAME;` and this fills it with a Material, so the SkSL can read a
   *  source the node has NOT painted: a parameter field, a mask channel, a
   *  gradient, a second texture. `Effect::shader` fills exactly one slot
   *  itself — `content`, the node's own rendered layer — and this is how
   *  any further declared `uniform shader` gets a source. The Material
   *  resolves against THIS NODE's box, so unit-space authoring
   *  (linearUnit / glowUnit) works here as it does on a fill.
   *
   *  TIER INHERITANCE is the load-bearing half, and it calls Material's
   *  own recursion rather than repeating its rule: a live source makes the
   *  effect isAnimated(), so the node is declared volatile and no cache
   *  can freeze the parameter; the slots also ride the prune signature,
   *  so two effects with different sources never compare equal.
   *
   *  SILENT-ISH GUARDRAILS, matching Material::slot. A name the effect
   *  does not declare as `uniform shader` warns and is IGNORED. On an
   *  effect kind with no slot to fill — `filter()`, which wraps an
   *  already-built SkImageFilter, or a bare `directionalBlur()` — the call
   *  is a no-op with a warning, exactly as uniform() is there. On a
   *  blur() the one fillable name is "sigma", its sigma map. */
  Effect& slot(std::string name, Paint source);
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
  /** THE LAYER AND A LIGHT MADE FROM IT: `light` runs over the same
   *  input this effect does, and its result is blended over this effect's
   *  own output with @p mode — so `Effect().emit(light)` is the layer with
   *  its light screened over it, and `whitened.emit(light)` is a whitened
   *  core under a light drawn from the untouched layer. Each `emit` reads
   *  that same input, so lights stack rather than compound: a second
   *  `emit` adds a light of the layer, never a light of the first light.
   *  Static sides blend once; a live side re-blends at each paint. */
  Effect emit(const Effect& light,
              SkBlendMode mode = SkBlendMode::kScreen) const;

  const sk_sp<SkImageFilter>& imageFilter() const { return m_filter; }
  /** The colour filter, when the effect is one — set only by
   *  `filter(sk_sp<SkColorFilter>)` and by the lowered `recipe()`. A
   *  consumer that composites a layer sets it on the layer's paint
   *  beside `imageFilter()`; the two are never both present. */
  const sk_sp<SkColorFilter>& colorFilter() const { return m_colorFilter; }
  /** The filter with any bound uniforms resolved NOW — what the paint
   *  phase applies. Identical to imageFilter() for a static effect.
   *  @p paintFrame is the painting node's PaintFrame, which the slots'
   * materials resolve against (its box, its clock) — exactly the context
   *  Material::slot hands its sources. Null is the context-free form:
   *  static children keep their snapshot, and it is what a caller holding
   *  an Effect outside a paint can ask for. */
  sk_sp<SkImageFilter> resolvedImageFilter(
      const PaintFrame* paintFrame = nullptr) const;
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
   *  MAP itself lives in m_slots under "sigma", so one slot vector
   *  carries every Material an effect samples: one equality, one tier
   *  walk, one resolve loop, and `slot("sigma", …)` re-aims the map for
   *  free. */
  struct ParametricBlur {
    float maxSigma = 0;
    bool operator==(const ParametricBlur&) const = default;
  };

  /** A COLOUR MAP AS A COMPARABLE VALUE: @p program is a colour-filter
   *  runtime effect and @p uniforms are its declared floats by name. The
   *  built filter lands in the colour lane, and the program and the
   *  names ride operator== so a re-described equal map prunes where an
   *  already-built SkColorFilter — which carries no recipe — cannot. */
  static Effect colorProgram(
      sk_sp<SkRuntimeEffect> program,
      std::vector<std::pair<std::string, float>> uniforms);

  /** The comparable source and compiled program of one recipe()
   *  snapshot. Live sources are not retained: the filter owns their
   *  sampled values and compares by identity. */
  struct RecipeSnapshot {
    std::optional<Material> material;
    sk_sp<const SkRuntimeEffect> program;
    float sampleRadius = 0;
  };

  sk_sp<SkImageFilter> m_filter;
  // The colour-filter lane: a map with no neighbourhood, which a
  // consumer hangs on a paint rather than running through the filter
  // graph. Exclusive with m_filter.
  sk_sp<SkColorFilter> m_colorFilter;
  std::shared_ptr<const RecipeSnapshot> m_recipeSnapshot;
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
  std::optional<DirectionalBlur>
      m_directionalBlur;                           // directionalBlur()'s recipe
  std::optional<ParametricBlur> m_parametricBlur;  // blur()'s recipe
  std::shared_ptr<const BlurLevels> m_blurLevels;  // …and its held passes
  // phosphorBloom(): the shader recipe above is the HALO program alone,
  // and this says the node is that program gathered over a reduced layer
  // and composited back over the sharp one, rather than one pass over it.
  // Derived from nothing else, so it takes part in equality: two effects
  // over the same program and uniforms paint differently by it.
  bool m_gatheredHalo = false;
  // brightPass(), deepen() and whiten(): the program in m_effect is a
  // COLOUR-FILTER program rather than a shader one, so the value built
  // from it and the constant lanes is a colour map and not a pass over
  // the layer. A map has no neighbourhood, and a filter graph carrying
  // one is evaluated in the device's own pixels rather than in the
  // layer's, which is what keeps a source sharp under an emitted light.
  // Derived from nothing else, so it takes part in equality beside the
  // program.
  bool m_colorProgram = false;
  // The slots: `uniform shader NAME` → Paint. Held by shared_ptr
  // so a copied Effect shares its slots rather than deep-copying a
  // whole paint tree per copy; the surface is still slot(name, Paint) by
  // value, and filling a slot replaces the pointer rather than mutating
  // what another copy is holding.
  std::vector<std::pair<std::string, std::shared_ptr<const Paint>>> m_slots;
  // then()- or emit()-chain retained only when a side needs a paint
  // frame (static chains precompose into m_filter and carry no nodes).
  std::shared_ptr<const Effect> m_chainA, m_chainB;
  // emit()'s blend of B over A; empty for then(), which composes B after A.
  std::optional<SkBlendMode> m_chainBlend;

  /** Does any child need a PaintFrame to resolve (live or geometry
   *  tier)? Material::build's memo asks exactly this of its own children,
   *  for the same reason: a static child's snapshot is already correct,
   *  and a context-needing one must be rebuilt per paint or it freezes. */
  bool anyChildNeedsContext() const;
  /** The slot @p name as a shader, resolved against @p paintFrame. */
  sk_sp<SkShader> childShaderFor(std::string_view name,
                                 const PaintFrame* paintFrame) const;
  /** The recipe's filter, built unconditionally — the store-time snapshot
   *  (null paintFrame) and the per-paint resolve are one construction. */
  sk_sp<SkImageFilter> buildFilter(const PaintFrame* paintFrame) const;
  /** THE EFFECT AS ONE IMAGE FILTER: the colour lane lifted into the
   *  filter graph. A chain composes image filters, and a consumer that
   *  knows only image filters must still get the right picture; the
   *  paint-side shortcut is for the consumer that asks `colorFilter()`
   *  for it by name. */
  sk_sp<SkImageFilter> liftedFilter() const;

  /** FIELD PIN: `operator==` is hand-written and reads these members
   *  directly, so a member added without a rule in it would silently not
   *  take part in equality. The structured binding below fails to compile
   *  when the member list changes, and the static_assert's message says
   *  what to decide. The state is private, so the decomposition lives
   *  inside the class. */
  static void fieldPin(Effect& v) {
    auto& [filter, colorFilter, recipeSnapshot, effect, uniforms, uniforms2,
           uniforms4, uniformArrays, bound, blocks, directionalBlur,
           parametricBlur, blurLevels, gatheredHalo, colorProgram, children,
           chainA, chainB, chainBlend] = v;
    static_assert(
        std::tuple_size_v<decltype(std::tie(
                filter, colorFilter, recipeSnapshot, effect, uniforms,
                uniforms2, uniforms4, uniformArrays, bound, blocks,
                directionalBlur, parametricBlur, blurLevels, gatheredHalo,
                colorProgram, children, chainA, chainB, chainBlend))> == 19,
        "Effect gained or lost a member — rule on it in "
        "Effect::operator==, then bump this count. "
        "(m_colorFilter compares by pointer, like m_filter, an "
        "already-built SkColorFilter carrying no recipe either; "
        "m_recipeSnapshot compares material value and compiled program, "
        "or m_filter identity when its source has live inputs; "
        "m_filter is EXCLUDED on the shader, directionalBlur and "
        "blur paths because it is derived from m_effect + the "
        "constant lanes / m_directionalBlur / m_parametricBlur + m_slots, "
        "and m_blurLevels is derived from m_parametricBlur alone, "
        "while m_gatheredHalo is derived from nothing and is "
        "compared beside the shader recipe; "
        "m_colorProgram says the program in m_effect is a colour filter "
        "rather than a shader, so the value built from it is a colour "
        "map; it is derived from nothing and is compared beside the "
        "program; "
        "m_bound and m_blocks make the effect isAnimated(), which "
        "operator== already refuses; m_chainA/B and m_chainBlend "
        "exist only on a chain with a side that needs a paint frame, "
        "and compare side by side with the blend.)");
  }
};

}  // namespace sigil::material::skia
