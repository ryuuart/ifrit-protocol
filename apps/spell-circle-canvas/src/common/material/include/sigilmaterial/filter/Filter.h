#pragma once

/** @file
 * @ingroup material-filter
 *
 * THE FILTER: what runs over a layer a consumer has already rendered —
 * CSS's `filter` and `backdrop-filter` and SVG's filter primitives, as one
 * comparable value. A filter is built from stock passes (a blur, a drop
 * shadow, a glow, a bloom, the colour functions) or from a program over
 * the layer, chained in order with `then`, and its parameters bind to a
 * moving value as a paint's do. Nothing here names a renderer; the Skia
 * executor (<sigilmaterial/skia/Filter.h>) turns one into an image filter.
 */

#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/BlendMode.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmotion/values/Animatable.h>

#include <array>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace sigil::material {

namespace skia {
struct FilterAccess;
}  // namespace skia

/** Where a shadow falls and how soft it is. */
struct ShadowOptions {
  /** The Gaussian's standard deviation in local pixels; 0 is a hard edge. */
  float blur = 0;
  /** How far the shadow is displaced from the layer, in local pixels. */
  glm::vec2 offset = {0, 0};
  /** Local pixels the coverage grows by before the blur (a material's
   *  effects stage only): a spread shadow with no offset is an outer
   *  glow. */
  float spread = 0;
  /** Inside the coverage rather than outside it (a material's effects
   *  stage only): an inner shadow, or with no offset an inner glow. The
   *  offset is the direction the shadow is CAST, so (0, 3) casts downward
   *  and the band hugs the TOP inner edge.
   *
   *  An inner shadow is a FINITE band — the edge stroked `blur` plus the
   *  offset wide, blurred and clipped inside the shape — and an executor
   *  must draw it so. A blurred inverse fill has bounds that depend on
   *  the device, so it floods the whole interior once the layer is cached
   *  at an offset from the origin. */
  bool inside = false;
  bool operator==(const ShadowOptions&) const = default;
};

/** Which side of the coverage's edge a stroke sits on. */
enum class StrokePosition : uint8_t { Inside, Center, Outside };

/** A keyline around a layer's coverage. */
struct StrokeOptions {
  /** Local pixels. */
  float width = 1;
  StrokePosition position = StrokePosition::Outside;
  bool operator==(const StrokeOptions&) const = default;
};

/** THE BEVEL: two opposed inner shadows lit from one direction, the
 *  fake-3D edge an image editor's layer style draws. */
struct BevelOptions {
  /** How far the lit and the shaded plane are pushed apart, in local
   *  pixels. */
  float depth = 3;
  /** How soft the edge is, in local pixels. */
  float size = 4;
  /** Where the light comes from, in degrees counter-clockwise from 3
   *  o'clock. */
  float angleDegrees = 120;
  Color highlight = {1, 1, 1, 0.65f};
  Color shadow = {0, 0, 0, 0.45f};
  bool operator==(const BevelOptions&) const = default;
};

/** ONE STEP THAT READS A LAYER'S COVERAGE — its distance to the edge, its
 *  alpha — rather than its pixels: a shadow, a stroke or a bevel in a
 *  material's effects stage. A renderer that knows the layer's shape
 *  draws these around it; shadows draw under the layer, the rest over
 *  it. */
struct CoverageEffect {
  enum class Kind : uint8_t { Shadow, Stroke, Bevel };
  Kind kind = Kind::Shadow;
  Color color = {0, 0, 0, 1};
  ShadowOptions shadow;
  StrokeOptions stroke;
  BevelOptions bevel;
  bool operator==(const CoverageEffect&) const = default;
};

/** OPTICAL BLOOM in local pixels: the bright part of the layer, spread by
 *  two separable Gaussians and laid back over it. `sigma` sets the near
 *  Gaussian and `spread` multiplies it for the broad halo; `strength` and
 *  `tail` weight the two. `softness` blurs the source independently of
 *  the emitted light. */
struct BloomOptions {
  float sigma = 4;
  float strength = 1;
  float spread = 4;
  float tail = 1;
  float threshold = 0.2f;
  float knee = 0.2f;
  float softness = 0;
  /** The share of the way a lit source moves toward white at its own
   *  peak, as an overexposed core does, so it reads lighter than the
   *  deeper halo around it. Colour below the threshold is untouched. */
  float whitening = 0;
  /** Local pixels the extracted light grows by before either blur, so
   *  the halo's colour extends past the source as a body and then fades,
   *  as a shadow's spread does. Corners stay round and gaps between
   *  letters stay open until the dilation reaches them. */
  float dilation = 0;
  /** How far a fading halo sinks toward its strongest channel, as a tone
   *  curve's toe drops the weaker channels of faint light first. The
   *  straight colour is raised to 1 + deepening × (1 − coverage), so a
   *  dense halo keeps the source's colour and a thin one takes the whole
   *  depth. */
  float deepening = 0;
  /** The most the halo covers, which keeps dark lettering visible inside
   *  a lit panel. */
  float maximumOpacity = 0.65f;
};

/** Orthographic refraction in node-local logical pixels. Diffusion and
 *  surface reflection are separate operations. */
struct GlassOptions {
  /** Air-to-glass index. One or less produces no displacement. */
  float ior = 1.5f;
  /** Propagation depth along the page's z axis, in logical pixels.
   *  Zero or less produces no displacement. */
  float thickness = 12;
  /** Finite nonnegative ceiling on each sampling-offset component.
   *  Fixed at construction; set() and bind() cannot change it. */
  float sampleRadius = 32;
  /** Opaque RGB encoding (normal + 1) / 2, sampled in node-local pixels.
   *  Normalized before use, facing the viewer with z >= 0. Absent,
   *  nonopaque, invalid or back-facing samples cause no displacement.
   *  Only the material's color stack is sampled. */
  std::optional<Material> normal;
  /** Green points down the image when true, up when false. */
  bool normalDirectX = false;
};

/** A filter over a rendered layer. The default filter is none: the layer
 *  passes through untouched. */
class Filter {
 public:
  Filter() = default;

  /** @name Stock passes
   *  @{ */
  /** The layer blurred by a Gaussian of @p sigma local pixels in both
   *  directions — CSS `blur()`. */
  static Filter blur(float sigma);
  /** A blur whose SIGMA VARIES ACROSS THE NODE — a depth-of-field falloff,
   *  a lens edge. @p sigmaMap is a paint read as a NUMBER: its red channel
   *  at a pixel, times @p maximumSigma, is the blur radius there. The map
   *  rides the filter's `slot("sigma", …)`.
   *  @trap @p maximumSigma is the RANGE a bound sigma rides inside:
   *  declare the largest the binding will reach, because a declared 0
   *  rebuilds every pass at every paint. */
  static Filter blur(Paint sigmaMap, float maximumSigma);
  /** The same with the map written as a material — a gradient, a noise,
   *  an image — lowered as a fill of it would be. */
  static Filter blur(const Material& sigmaMap, float maximumSigma);
  /** A blur that smears ALONG one direction: @p sigma along the axis at
   *  @p angleDegrees (screen sense — 0 horizontal, 90 vertical), @p across
   *  perpendicular to it. Its "sigma", "angle" and "across" take a bound
   *  value.
   *  @trap A spatial filter, not motion blur: it knows nothing about how
   *  the node moved. */
  static Filter directionalBlur(float sigma, float angleDegrees,
                                float across = 0);
  /** Refract the layer this filter is attached to: its color is
   *  displaced while its incoming alpha at each output pixel is kept. The
   * scalar names "ior", "thickness" and "normalDirectX" take set() and bind();
   * the "normal" slot resolves against the current node frame. The sampling
   * ceiling is immutable. A transparent or invalid shifted lookup retains the
   * original pixel. Index <= 1, depth <= 0 and radius 0 are identity; nonfinite
   * index or depth also preserves the original. A negative or nonfinite radius
   * is reported and returns no filter. */
  static Filter glass(GlassOptions options = {});
  /** The layer's shadow in @p color beneath it — CSS `drop-shadow()`. */
  static Filter dropShadow(Color color, ShadowOptions options = {});
  /** The layer re-emitted blurred beneath itself in @p color — a drop
   *  shadow at zero offset, which keeps the content on top. */
  static Filter glow(Color color, float sigma);
  /** A SHADOW OF THE LAYER'S COVERAGE, for a material's effects stage:
   *  outside the shape and under it by default, inside it with
   *  `.inside`; with no offset and a spread, an outer (or inner) glow; with
   *  no blur, a hard echo of the shape. Over a whole subtree
   *  (`Element::filter`) use `dropShadow`, which reads pixels.
   *  @silent used as a subtree filter it paints nothing (said once). */
  static Filter shadow(Color color, ShadowOptions options = {});
  /** A KEYLINE around the layer's coverage, for a material's effects
   *  stage. */
  static Filter stroke(Color color, StrokeOptions options = {});
  /** A BEVEL of the layer's coverage, for a material's effects stage. */
  static Filter bevel(BevelOptions options = {});
  /** Optical bloom: bright colour extracted, spread, deepened and
   *  whitened, and laid back over the source. Hold the returned filter:
   *  its graph compares by identity, and copies share it. */
  static Filter bloom(BloomOptions options = {});
  /** Display bloom over the completed layer, the sharp source retained on
   *  top. @p radius is the outer kernel radius in px, @p intensity its
   *  additive energy, @p chroma the spectral separation in 0..1,
   *  @p hueDrift the turn in DEGREES the halo's hue has made at that
   *  radius, and @p tail extra energy on the outermost kernel. Painted in
   *  a box, it runs over that box grown by @p radius and nothing beyond,
   *  so its cost follows the layer it filters rather than the canvas.
   *  @trap The halo is gathered over a REDUCED layer and resampled up, so
   *  its fine structure moves where a wide radius is asked for. */
  static Filter phosphorBloom(float radius = 9.0f, float threshold = 0.52f,
                              float intensity = 0.46f, float chroma = 0.80f,
                              float hueDrift = 0.0f, float tail = 0.0f);
  /** @} */

  /** @name The colour functions
   *  Per-pixel maps over the straight colour, coverage kept — each is
   *  CSS's function of the same name. A consumer applies one to the
   *  layer's paint rather than through a pass.
   *  @{ */
  /** The colour scaled by @p amount: 0 is black, 1 unchanged. */
  static Filter brightness(float amount);
  /** The colour scaled by @p amount about mid-grey: 0 is grey, 1
   *  unchanged. */
  static Filter contrast(float amount);
  /** @p amount of the way from the colour's luminance grey: 0 is grey, 1
   *  unchanged, above 1 oversaturated. */
  static Filter saturate(float amount);
  /** The hue turned by @p degrees around the luminance axis. */
  static Filter hueRotate(float degrees);
  /** THE LAYER WITH EVERYTHING BUT ITS LIGHT TAKEN OUT: what is brighter
   *  than @p threshold, faded in over @p knee above it, carrying that
   *  brightness as its own coverage — the first half of a bloom. */
  static Filter brightPass(float threshold = 0.68f, float knee = 0.30f);
  /** FAINT LIGHT LOSES ITS WEAKER CHANNELS FIRST: the straight colour,
   *  normalised to its peak, is raised to 1 + @p amount × (1 − coverage).
   *  Positive amounts clamp negative RGB to zero while preserving the
   *  positive peak, including values above one, and alpha. Zero or less
   *  leaves the input unchanged. */
  static Filter deepen(float amount);
  /** Where the straight colour's peak is above @p threshold, faded in over
   *  @p knee, it moves @p amount of the way toward white at that peak, as
   *  an overexposed core does. */
  static Filter whiten(float amount, float threshold = 0.2f, float knee = 0.2f);
  /** @} */

  /** THE ROUNDED SPREAD: every edge grown outward by @p pixels, so the
   *  layer's colour carries past it as a body before anything feathers it.
   *  @trap It grows COVERAGE, so over an opaque ground it is only a blur;
   *  spread a light instead. */
  static Filter dilate(float pixels);

  /** A PROGRAM OVER THE LAYER: @p program runs with the rendered layer in
   *  its slot named `content`. @p sampleRadius bounds the largest
   *  local-coordinate offset the body samples the layer at.
   *  @trap The material's bindings and frame inputs are captured at
   *  construction. Re-describe to sample live inputs again. */
  static Filter of(const Material& program);
  static Filter of(const Material& program, float sampleRadius);
  /** An enumeration names a mode, never a distance to sample. */
  template <class Enum>
    requires std::is_enum_v<Enum>
  static Filter of(const Material&, Enum) = delete;

  /** @name Parameters, slots and chains
   *  Every one copies on write.
   *  @{ */
  /** THE SLOT: fills the program's declared `uniform shader NAME` with a
   *  paint, so the program reads a source the node has NOT painted. The
   *  source resolves against THIS node's box, and the filter inherits its
   *  volatility and compares on it.
   *  @silent the program declares no such slot (warned once). */
  Filter& slot(std::string name, Paint source);
  /** Constant parameters, joining equality, so an equal re-described
   *  filter prunes. Typed values require matching non-array float, float2
   *  or float4 declarations. The flat vector fills a complete floating
   *  scalar, vector, matrix or array, matched by total float count.
   *  @silent an absent or incompatible declaration retains its previous
   *  input (warned once). */
  Filter& set(std::string name, float value);
  Filter& set(std::string name, std::array<float, 2> value);
  Filter& set(std::string name, std::array<float, 4> value);
  Filter& set(std::string name, std::vector<float> values);
  /** A LIVE parameter, read at every paint, so the node repaints every
   *  frame while the filter is attached. A program's float parameters, a
   *  directional blur's "sigma", "angle" and "across", a varying blur's
   *  "maxSigma". A raw program requires one float, never an array or integer.
   *  @trap A filter holds no instance, so a value carrying its own
   *  TRANSITION has nothing to run it and reads as its target. */
  Filter& bind(std::string name, motion::Animatable<float> value);
  /** A LIVE FLOATING UNIFORM — a `UniformBlock` the caller owns, writes
   *  and commit()s, matched by the complete float count like the flat
   *  constant overload. Each paint reads its last committed values; draft
   *  edits stay hidden even when another input changes. */
  Filter& bind(std::string name, std::shared_ptr<const UniformBlock> block);
  /** THIS FILTER, THEN @p next over its result. Static chains compose once;
   *  a chain with a live side re-composes at each paint. */
  [[nodiscard]] Filter then(const Filter& next) const;
  /** THE LAYER AND A LIGHT MADE FROM IT: @p light runs over the same input
   *  this filter does, and its result is blended over this filter's own
   *  output with @p mode.
   *  @trap Each emit reads that same input, so lights STACK rather than
   *  compound. */
  [[nodiscard]] Filter emit(const Filter& light,
                            BlendMode mode = BlendMode::Screen) const;
  /** @} */

  /** The filter that passes the layer through untouched. */
  bool isNone() const { return m_node == nullptr; }
  /** The steps that read the layer's coverage, in chain order. */
  std::span<const CoverageEffect> coverage() const;
  /** This filter without its coverage steps: the passes that read
   *  pixels. */
  [[nodiscard]] Filter withoutCoverage() const;
  /** THE VOLATILITY DECLARATION: does this filter change without a
   *  re-describe? True while any parameter is bound, or while any slot's
   *  paint is live. */
  bool isRunning() const;
  /** Does any slot's paint anchor to the root frame? */
  bool usesWorldSpace() const;
  /** Structural equality for the reconciler: an equal filter described
   *  again compares equal, so its node prunes. A stock pass — a blur, a
   *  drop shadow, a glow, a dilation, a colour function — and a static
   *  program filter compare by what they were built from, a chain by its
   *  sides, a renderer's own filter by identity; a live filter never
   *  compares equal, not even to itself. */
  bool operator==(const Filter& other) const;

 private:
  friend struct skia::FilterAccess;
  struct Node;  // the executor's filter
  explicit Filter(std::shared_ptr<const Node> node) : m_node(std::move(node)) {}
  Node& edit();
  std::shared_ptr<const Node> m_node;
};

}  // namespace sigil::material
