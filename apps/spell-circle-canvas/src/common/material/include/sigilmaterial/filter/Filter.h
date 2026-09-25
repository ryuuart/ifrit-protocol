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

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/BlendMode.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/UniformBlock.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmotion/values/Animatable.h>

#include <array>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <vector>

namespace sigil::material {

namespace skia {
struct FilterAccess;
}  // namespace skia

/** Where a drop shadow falls and how soft it is. */
struct ShadowOptions {
  /** The Gaussian's standard deviation in local pixels; 0 is a hard edge. */
  float blur = 0;
  /** How far the shadow is displaced from the layer, in local pixels. */
  glm::vec2 offset = {0, 0};
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
  /** A blur that smears ALONG one direction: @p sigma along the axis at
   *  @p angleDegrees (screen sense — 0 horizontal, 90 vertical), @p across
   *  perpendicular to it. Its "sigma", "angle" and "across" take a bound
   *  value.
   *  @trap A spatial filter, not motion blur: it knows nothing about how
   *  the node moved. */
  static Filter directionalBlur(float sigma, float angleDegrees,
                                float across = 0);
  /** The layer's shadow in @p color beneath it — CSS `drop-shadow()`. */
  static Filter dropShadow(Color color, ShadowOptions options = {});
  /** The layer re-emitted blurred beneath itself in @p color — a drop
   *  shadow at zero offset, which keeps the content on top. */
  static Filter glow(Color color, float sigma);
  /** Optical bloom: bright colour extracted, spread, deepened and
   *  whitened, and laid back over the source. Hold the returned filter:
   *  its graph compares by identity, and copies share it. */
  static Filter bloom(BloomOptions options = {});
  /** Display bloom over the completed layer, the sharp source retained on
   *  top. @p radius is the outer kernel radius in px, @p intensity its
   *  additive energy, @p chroma the spectral separation in 0..1,
   *  @p hueDrift the turn in DEGREES the halo's hue has made at that
   *  radius, and @p tail extra energy on the outermost kernel.
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
   *  normalised to its peak, is raised to 1 + @p amount × (1 − coverage). */
  static Filter deepen(float amount);
  /** Where the straight colour's peak is above @p threshold, faded in over
   *  @p knee, it moves @p amount of the way toward white at that peak, as
   *  an overexposed core does. */
  static Filter whiten(float amount, float threshold = 0.2f,
                       float knee = 0.2f);
  /** @} */

  /** THE ROUNDED SPREAD: every edge grown outward by @p pixels, so the
   *  layer's colour carries past it as a body before anything feathers it.
   *  @trap It grows COVERAGE, so over an opaque ground it is only a blur;
   *  spread a light instead. */
  static Filter dilate(float pixels);

  /** A PROGRAM OVER THE LAYER: @p program runs with the rendered layer in
   *  its slot named `content`. @p sampleRadius bounds the largest
   *  local-coordinate offset the body samples the layer at.
   *  @trap The material's bindings are read ONCE, at construction, so
   *  animate by re-describing or through `bind` on the filter. */
  static Filter of(const Material& program);
  static Filter of(const Material& program, float sampleRadius);

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
   *  filter prunes. The array form fills a declared array matched by TOTAL
   *  float count.
   *  @silent the name is undeclared or its size is not the value's
   *  (warned once). */
  Filter& set(std::string name, float value);
  Filter& set(std::string name, std::array<float, 2> value);
  Filter& set(std::string name, std::array<float, 4> value);
  Filter& set(std::string name, std::vector<float> values);
  /** A LIVE parameter, read at every paint, so the node repaints every
   *  frame while the filter is attached. A program's float parameters, a
   *  directional blur's "sigma", "angle" and "across", a varying blur's
   *  "maxSigma".
   *  @trap A filter holds no instance, so a value carrying its own
   *  TRANSITION has nothing to run it and reads as its target. */
  Filter& bind(std::string name, motion::Animatable<float> value);
  /** A LIVE ARRAY — a `UniformBlock` the caller owns, writes and
   *  commit()s, read at every paint. */
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
