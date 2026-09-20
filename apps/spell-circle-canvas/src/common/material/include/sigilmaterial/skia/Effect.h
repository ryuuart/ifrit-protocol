#pragma once

/** @file
 * @ingroup material-skia
 *
 * POST-PROCESSING over a rendered layer: an image-filter recipe as a
 * comparable value. A `Paint` shades a shape; an `Effect` takes the
 * layer a consumer has already rendered and runs a filter over it.
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
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material::skia {

/**
 * Post-processing at stacking-context boundaries, as a comparable
 * value: `filter` wraps any SkImageFilter, `shader` an SkSL runtime
 * effect whose `content` slot is the rendered layer. A consumer
 * attaches one to the layer a node paints, or to what is painted
 * beneath it.
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
   *  `colorFilter()` reads it back; where an image filter is asked for
   *  instead the colour filter is lifted into the graph. */
  static Effect filter(sk_sp<SkColorFilter> f);
  /** A SigilMaterial recipe as the effect: its program runs over the
   *  layer, which arrives in the slot named `content`. @p sampleRadius
   *  bounds the largest local-coordinate source offset.
   *  @trap The material's bindings are read ONCE, at construction, so
   *  animate by re-describing rather than by moving the material. */
  static Effect recipe(const Material& material);
  static Effect recipe(const Material& material, float sampleRadius);
  /** THE SAME RECIPE, LOWERED FOR THE SURFACE IT WILL LAND ON: a
   *  channelwise recipe over an eight-bit surface becomes a 256-entry
   *  table per channel and no program at all. @p surface is the colour
   *  type a consumer reads from its canvas at the moment it paints.
   *  Every other case falls back to `recipe(material)` and the program,
   *  so the picture is the same and only the cost differs. */
  static Effect recipe(const Material& material, SkColorType surface);
  /** The layer re-emitted blurred beneath itself in @p color — a drop
   *  shadow at zero offset, which keeps the content on top. */
  static Effect glow(SkColor4f color, float sigma);
  /** THE LAYER WITH EVERYTHING BUT ITS LIGHT TAKEN OUT: what is brighter
   *  than @p threshold, faded in over @p knee above it, carrying that
   *  brightness as its own coverage — the first half of a bloom, to
   *  chain with a blur and lay back over the source. Brightness is the
   *  peak channel, not luminance, and the gate reads the STRAIGHT colour
   *  because what comes back is a layer rather than light to add.
   *  @trap It reads its own pixel and no neighbour, so it is a COLOUR
   *  MAP: `colorFilter()` answers it and `imageFilter()` does not. */
  static Effect brightPass(float threshold = 0.68f, float knee = 0.30f);
  /** The layer blurred by a Gaussian of @p sigma local pixels in both
   *  directions — the stage a glow spreads its light with. */
  static Effect blur(float sigma);
  /** THE ROUNDED SPREAD: every edge grown outward by @p pixels, so the
   *  layer's colour carries past it as a body before anything feathers
   *  it, as a shadow's spread does. Corners stay round and the gaps
   *  between letters stay open, where a square morphological kernel
   *  would fill them as plates. The straight colour is kept.
   *  @trap It grows COVERAGE, so over an opaque ground it is only a
   *  blur; spread a light instead. */
  static Effect dilate(float pixels);
  /** FAINT LIGHT LOSES ITS WEAKER CHANNELS FIRST, as a tone curve's toe
   *  drops them: the straight colour, normalised to its peak, is raised
   *  to 1 + @p amount × (1 − coverage), so a thin layer sinks toward its
   *  strongest channel with its brightest held. */
  static Effect deepen(float amount);
  /** THE OTHER END OF THAT CURVE: where the straight colour's peak is
   *  above @p threshold, faded in over @p knee, it moves @p amount of
   *  the way toward white at that peak, as an overexposed core does.
   *  Colour below the threshold is untouched. */
  static Effect whiten(float amount, float threshold = 0.2f,
                       float knee = 0.2f);
  /** Display bloom over the completed layer, the sharp source retained
   *  on top. @p radius is the outer kernel radius in px, @p intensity
   *  its additive energy, @p chroma the spectral separation in 0..1,
   *  @p hueDrift the turn in DEGREES the halo's hue has made at that
   *  radius, and @p tail extra energy on the outermost kernel. The last
   *  two default to zero, which is exactly the falloff without them.
   *  @trap The halo is gathered over a REDUCED layer and resampled up,
   *  so its fine structure moves where a wide radius is asked for; the
   *  source itself is composited at full resolution and to the bit. */
  static Effect phosphorBloom(float radius = 9.0f, float threshold = 0.52f,
                              float intensity = 0.46f, float chroma = 0.80f,
                              float hueDrift = 0.0f, float tail = 0.0f);
  /** @p uniforms are float uniforms set by name on the SkSL effect; the
   *  layer arrives as the slot named "content".
   *  @silent a name the effect does not declare as a float uniform —
   *  including a float2, a float4 or an array, none of which this door
   *  can fill (warned once, never a debug abort). */
  static Effect shader(
      sk_sp<SkRuntimeEffect> effect,
      std::vector<std::pair<std::string, float>> uniforms = {});
  /** A blur that smears ALONG one direction: @p sigma along the axis at
   *  @p angleDeg (degrees, screen sense — 0 horizontal, 90 vertical, 45
   *  down-right), @p across perpendicular to it, 0 by default for a pure
   *  streak. It carries a comparable RECIPE, so an equal re-described
   *  one prunes, and its "sigma", "angle" and "across" take a bound
   *  uniform.
   *  @trap A spatial filter, not motion blur: it knows nothing about how
   *  the node moved. */
  static Effect directionalBlur(float sigma, float angleDeg, float across = 0);
  /** A blur whose SIGMA VARIES ACROSS THE NODE — a depth-of-field
   *  falloff, a lens edge, a tube's curvature. @p sigmaMap is a paint
   *  read as a NUMBER: its RED channel at a pixel, times @p maxSigma, is
   *  the blur radius there. It carries a comparable RECIPE, the sigma
   *  map rides it, and `slot("sigma", …)` re-aims the map.
   *  @trap @p maxSigma is the RANGE a bound sigma rides inside, not a
   *  ceiling on one value: declare the largest the binding will reach,
   *  because a declared 0 rebuilds every pass at every paint. */
  static Effect blur(Paint sigmaMap, float maxSigma);
  /** THE SLOT — `Material::slot` on the effect seam: the effect
   *  declares `uniform shader NAME;` and this fills it with a paint, so
   *  the SkSL reads a source the node has NOT painted. `shader()` fills
   *  `content` itself. The source resolves against THIS NODE's box, and
   *  the effect inherits its volatility and compares on it.
   *  @silent the effect declares no such `uniform shader`, or its kind
   *  has no slot to fill — a wrapped filter(), a bare directionalBlur();
   *  a blur()'s one fillable name is "sigma" (warned once). */
  Effect& slot(std::string name, Paint source);
  /** A LIVE float uniform, read at every paint, so the node repaints
   *  every frame while the effect is attached. Meaningful on a shader()
   *  effect, a directionalBlur() ("sigma", "angle", "across") or a
   *  blur() ("maxSigma").
   *  @trap An effect holds no instance, so a value carrying its own
   *  TRANSITION has nothing to run it and reads as its target.
   *  @silent the name is not one the effect declares, or its kind takes
   *  no uniform (warned once; no volatility is declared either). */
  Effect& uniform(std::string name, motion::Animatable<float> value);
  /** CONSTANT uniforms after construction, for the sizes the shader()
   *  constructor list cannot carry: the float2 and float4 forms fill
   *  those declarations, and the vector form fills a declared ARRAY
   *  matched by TOTAL float count. They join operator==, so an equal
   *  re-described effect prunes.
   *  @silent the paint is not a shader() effect, the name is
   *  undeclared, or its declared size is not the value's (warned
   *  once). */
  Effect& uniform(std::string name, float value);
  Effect& uniform(std::string name, std::array<float, 2> value);
  Effect& uniform(std::string name, std::array<float, 4> value);
  Effect& uniform(std::string name, std::vector<float> values);
  /** A LIVE ARRAY — a `UniformBlock` the caller owns, writes and
   *  commit()s, read at every paint. It declares volatility as a bound
   *  scalar does, and is size-checked at store against the declared
   *  array's total float count.
   *  @trap The binding compares by block identity; the values belong to
   *  the system and never prune. */
  Effect& uniform(std::string name, std::shared_ptr<const UniformBlock> block);
  /** Chain: apply @p next AFTER this effect. Static chains precompose
   *  once; a chain with a live side re-composes at each paint. */
  Effect then(const Effect& next) const;
  /** THE LAYER AND A LIGHT MADE FROM IT: @p light runs over the same
   *  input this effect does, and its result is blended over this
   *  effect's own output with @p mode. Static sides blend once; a live
   *  side re-blends at each paint.
   *  @trap Each emit reads that same input, so lights STACK rather than
   *  compound: a second one adds a light of the layer, never a light of
   *  the first light. */
  Effect emit(const Effect& light,
              SkBlendMode mode = SkBlendMode::kScreen) const;

  const sk_sp<SkImageFilter>& imageFilter() const { return m_filter; }
  /** The colour filter, when the effect is one — the colour-filter
   *  factory, the lowered `recipe()`, and the per-pixel stages
   *  `brightPass()`, `deepen()` and `whiten()`. A consumer sets it on
   *  the layer's paint beside `imageFilter()`.
   *  @trap The two are never both present. */
  const sk_sp<SkColorFilter>& colorFilter() const { return m_colorFilter; }
  /** The filter with any bound uniforms resolved NOW — what the paint
   *  phase applies, and identical to imageFilter() for a static effect.
   *  @p paintFrame is the painting node's, which the slots' sources
   *  resolve against; null is the context-free form, where static
   *  children keep their snapshot. */
  sk_sp<SkImageFilter> resolvedImageFilter(
      const PaintFrame* paintFrame = nullptr) const;
  /** THE VOLATILITY DECLARATION — one word across the whole library:
   *  does this effect change without a re-describe? True while any
   *  uniform is bound, or while any child material is live. */
  bool isAnimated() const;
  /** Does any child paint anchor to the root frame? The reconcile walk
   *  asks this so it can mark the node's world matrix stale when an
   *  ancestor's static transform is re-described. */
  bool usesWorldSpace() const;
  /** Structural equality for the reconciler. A static shader effect
   *  compares by RECIPE, a filter() effect by filter pointer, and a live
   *  effect never compares equal at all.
   *  @trap A re-described shader effect prunes only while the caller
   *  holds ONE SkRuntimeEffect and rebuilds the wrapper around it. */
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

/** EVERY SkSL BODY AN EFFECT IS BUILT OUT OF, as one list in a fixed
 *  order. These are the very objects the effects go on to use, not
 *  copies, so a device backend can name one and rebuild its program at
 *  the next launch. Asking compiles all of them. The recipes a `Paint`
 *  runs are not here — those reach the program cache, one per recipe.
 *  @trap An effect's PLACE in the list is part of its name, so the order
 *  is fixed and a body that would not compile is absent rather than
 *  null. */
std::span<const sk_sp<SkRuntimeEffect>> everyEffectProgram();

}  // namespace sigil::material::skia
