#pragma once

/** @file
 * SigilCompose Material — the polymorphic paint value. A small tree of paint
 * nodes that compiles to ONE `sk_sp<SkShader>` (layers via SkShaders::Blend,
 * never stacked saveLayer) or a plain solid color. Material supersedes the
 * kernel's three-case `Fill` as the authoring value for `fill()`; `Fill`
 * stays as the low-level {none,color,shader} carrier the reconciler stores.
 *
 * A Material is either STATIC or LIVE:
 *  - STATIC (solid, gradient ramp, image/sprite, blend, sksl with only
 *    constant uniforms): resolves eagerly to a color or shader, so it
 *    collapses to a `Fill` (toFill()) and rides the kernel's existing
 *    caching/prune/paint path unchanged.
 *  - LIVE (an sksl() material with at least one ch::Output-bound uniform):
 *    carries the runtime-effect recipe and is re-resolved every frame from
 *    the current uniform values (resolve()); its node is declared volatile
 *    exactly like a bound fill, so it paints live and never freezes into a
 *    cache. This is what makes `.uniform(name, &output)` actually drive
 *    pixels — see the note on uniform() below.
 *
 * The material vocabulary (solid/mix/ramp/image/blend) mirrors the MaterialX
 * standard library so a MaterialX document importer is a clean later addition;
 * no MaterialX dependency is pulled — we own the SkSL/SkShader backend. The
 * OCIO working-space / view transform is a Composer output stage (color::View),
 * orthogonal to Material the way SigilLoader is to SigilImage.
 */

#include "sigilcompose/Compose.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h> // sk_sp<SkShader> data member
#include <include/core/SkString.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h> // the unit-space ramps

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class SkImage;

namespace sigil::compose {

/** A gradient ramp stop (position 0..1 + color) — the MaterialX <ramp> atom.
 *  Authored in the working color space. */
struct Stop {
  float pos = 0.0f;
  SkColor4f color = {0, 0, 0, 1};
  bool operator==(const Stop &) const = default;
};

class Material;

namespace detail {
/** The unit-square ramp both linearUnit() and radialUnit() compile to: one
 *  SkSL pass that divides by uResolution, so the gradient's coordinates are
 *  fractions of the node's laid-out box rather than pixels. Six stops,
 *  chained mixes — each only takes effect past its own start. */
inline Material unitRamp(SkPoint a, SkPoint b, std::vector<Stop> stops,
                         bool radial);
} // namespace detail

/** The polymorphic paint value. Construct via the static factories; pass to
 *  Element::fill(). */
class Material {
public:
  Material() = default; // none (fully transparent — draws nothing)

  // ---- leaves --------------------------------------------------------------
  static Material solid(SkColor4f color);
  /** N-stop linear ramp between two points (working-space colors). */
  static Material linear(SkPoint a, SkPoint b, std::vector<Stop> stops,
                         SkTileMode tile = SkTileMode::kClamp);
  static Material radial(SkPoint center, float radius, std::vector<Stop> stops,
                         SkTileMode tile = SkTileMode::kClamp);
  /** Angular sweep from startDeg (12 o'clock is -90°) around center. */
  static Material sweep(SkPoint center, std::vector<Stop> stops,
                        float startDeg = 0.0f, float endDeg = 360.0f);
  /** Image/sprite as a fill (tiled or clamped); `local` maps source px into
   *  the node's space (a sprite's atlas sub-rect is a translate+scale). */
  static Material image(sk_sp<SkImage> image,
                        SkTileMode tx = SkTileMode::kClamp,
                        SkTileMode ty = SkTileMode::kClamp,
                        const SkMatrix &local = SkMatrix::I(),
                        SkSamplingOptions sampling = {});
  /** An SkSL runtime effect as a shader. `constants` set named float uniforms
   *  once; bind live uniforms with uniform(name, &output) below. Declaring
   *  `uTime` or `uContentScale` takes the LIVE path (re-resolved each frame:
   *  the clock ticks and the host's zoom changes independently of the node —
   *  reading them IS the volatility declaration); declaring only
   *  `uResolution` takes the cheaper GEOMETRY tier (resolved when the node
   *  records, cached between layouts — see geometryDependent()). */
  static Material sksl(sk_sp<SkRuntimeEffect> effect,
                       std::vector<std::pair<std::string, float>> constants = {});
  /** Wrap a raw shader (interop / escape). */
  static Material shader(sk_sp<SkShader> shader);

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
  static Material blend(std::vector<std::pair<Material, SkBlendMode>> layers);

  // ---- unit-space ramps ----------------------------------------------------
  /** The same linear ramp as linear(), authored in the node's UNIT SQUARE:
   *  (0,0) is the box's top-left, (1,1) its bottom-right, whatever the box
   *  turns out to be.
   *
   *  linear() takes PIXELS in node-local space, which is fine for a box
   *  whose size you wrote down and impossible for one whose size the layout
   *  decides — a tooltip card as tall as its copy, a stat panel, a button
   *  that grows with its label. The alternative was guessing a number, and
   *  every gallery scene that guessed one guessed wrong. textFill() already
   *  solved exactly this for glyphs by mapping the material's unit square
   *  onto the text metrics; this is the same trick for a box fill.
   *
   *  Rides the GEOMETRY tier (uResolution): resolved when the node records,
   *  cached between layouts — no per-frame cost. Up to six stops. */
  static Material linearUnit(SkPoint from01, SkPoint to01,
                             std::vector<Stop> stops) {
    return detail::unitRamp(from01, to01, std::move(stops), false);
  }
  /** The unit-square radial: @p center01 and a radius as a fraction of the
   *  box's HALF-DIAGONAL, so a ramp centred at {0.5, 0.5} with radius 1
   *  reaches the CORNERS of any box.
   *
   *  Which is a trap for the commonest use, and caught two studies: a
   *  soft round light authored at radius 1 is still at ~10% alpha where
   *  the inscribed circle is, so if the node also carries
   *  `.outline(shapes::circle())` the glow gets a visible hard rim. The
   *  magic number is 0.707. Use glowUnit() below when you mean "fills
   *  this box", and keep radialUnit for when you genuinely mean the
   *  corners (a vignette, a corner-to-corner wash). */
  static Material radialUnit(SkPoint center01, float radius01,
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
  static Material glowUnit(SkPoint center01, float radius01,
                           std::vector<Stop> stops) {
    // half-diagonal = sqrt(2)/2 of the side on a square; the ratio a
    // caller wants is radius-in-half-diagonals = radius01 / sqrt(2).
    return detail::unitRamp(center01, {radius01 * 0.70710678f,
                                       radius01 * 0.70710678f},
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
  Material &uniform(std::string name, float value);
  /** Constant float2 uniform (`uniform float2` in the SkSL) — offsets,
   *  margins, direction vectors. */
  Material &uniform(std::string name, std::array<float, 2> value);
  /** Constant float4 uniform set from a color (straight, not premultiplied —
   *  what the SkSL declares as `uniform float4`). */
  Material &uniform(std::string name, SkColor4f value);
  Material &uniform(std::string name, const choreograph::Output<float> *output);

  /** Step the auto-injected uTime at `hz` (floor(t·hz)/hz) — declared
   *  choppiness as a MATERIAL property, not per-consumer ticker plumbing.
   *  The P3R sea rule: its caustics run at 6 Hz ("we imagine the
   *  interpolation ourselves"); stop-motion/flipbook surfaces generally.
   *  Meaningful only on an sksl() material whose effect declares uTime
   *  (warn-and-ignore otherwise); 0 restores continuous time. */
  Material &quantizeTime(float hz);

  // ---- resolution ----------------------------------------------------------
  /** True once any ch::Output uniform is bound OR the effect reads uTime or
   *  uContentScale (both change independently of the node): the material
   *  re-resolves per frame and its node stays volatile. A blend() inherits
   *  liveness from its layers. */
  bool isLive() const;
  /** True when the effect declares uResolution (the node's layout size):
   *  the material needs PaintContext at resolve, but is stable between
   *  layouts — it resolves when its node records, CACHES like static
   *  content, and re-records on size change. A blend() inherits this from
   *  its layers (the flatten defers to resolve time). */
  bool geometryDependent() const;
  /** Declared-volatility hook (mirrors DecorationScheme::animated()). */
  bool animated() const { return isLive(); }

  bool isNone() const { return !m_isSolid && !m_shader && !m_live; }
  bool isSolid() const { return m_isSolid; }
  SkColor4f solidColor() const { return m_solid; }
  /** Always produces a shader (a solid becomes SkShaders::Color) — what
   *  blend() composes. For a live material this builds a fresh shader
   *  sampling bound Outputs at their CURRENT values (a snapshot, not a
   *  binding); use resolve() for the per-frame paint path. */
  sk_sp<SkShader> asShader() const;
  /** The static collapse the Composer stores for a NON-live material, so it
   *  rides the existing fill caching/prune path. */
  Fill toFill() const;
  /** The current-frame fill: for a live material, rebuilds the shader from the
   *  bound Outputs + the PaintContext (uTime/uResolution/uContentScale); for a
   *  static one, exactly toFill(). What the painter calls for a live fill. */
  Fill resolve(const PaintContext &ctx) const;

  /** STRUCTURAL value equality — the prune signature (§8.1 fix #1). Two
   *  materials compare equal when they were built from the same recipe:
   *  solids by color; gradients by geometry + stops + tile; images by
   *  (image pointer, tiles, matrix, sampling); static sksl by (effect
   *  pointer, constant values); blend stacks recursively by (layer recipes,
   *  modes). Rebuilding the same describe code therefore yields EQUAL
   *  materials even though each build minted a fresh SkShader — which is
   *  what lets a material-filled node prune across re-renders. Raw
   *  shader() wraps compare by pointer; live (Output-bound) materials
   *  compare by recipe identity only (they never prune anyway). */
  bool operator==(const Material &o) const;

private:
  struct Live;   // sksl recipe (effect + constants + Output bindings)
  struct Recipe; // comparable build recipe (gradients/image/blend)
  static sk_sp<SkShader> build(const Live &live, const PaintContext *ctx);
  void detachLive(); // copy-on-write before any recipe mutation

  bool m_isSolid = false;
  SkColor4f m_solid = {0, 0, 0, 0};
  sk_sp<SkShader> m_shader;     // static resolution: null for solid/none; for
                                // sksl a constants-only snapshot (live paint
                                // ignores it and goes through resolve())
  std::shared_ptr<Live> m_live; // sksl recipe; LIVE iff it has Output bindings
  std::shared_ptr<const Recipe> m_recipe; // comparable recipe (null for
                                          // solid/none/raw-shader/sksl —
                                          // those compare by their own state)
};


namespace detail {

inline Material unitRamp(SkPoint a, SkPoint b, std::vector<Stop> stops,
                         bool radial) {
  static constexpr char kSrc[] = R"(
uniform float2 uResolution;
uniform float2 uA;      // linear: from; radial: centre
uniform float2 uB;      // linear: to;   radial: (radius, radius)
uniform float  uRadial;
uniform float  uCount;
uniform float4 uC0; uniform float4 uC1; uniform float4 uC2;
uniform float4 uC3; uniform float4 uC4; uniform float4 uC5;
uniform float  uS0; uniform float  uS1; uniform float  uS2;
uniform float  uS3; uniform float  uS4; uniform float  uS5;

half4 main(float2 xy) {
  float2 p = xy / max(uResolution, float2(1.0, 1.0));
  float t;
  if (uRadial > 0.5) {
    // radius is a fraction of the box's half-diagonal, so {0.5,0.5} r=1
    // reaches the corners of ANY box
    float2 d = p - uA;
    t = length(d) / max(uB.x * 0.70710678, 1e-6);
  } else {
    float2 d = uB - uA;
    t = dot(p - uA, d) / max(dot(d, d), 1e-6);
  }
  t = clamp(t, 0.0, 1.0);
  float4 col = uC0;
  if (uCount > 1.5) { col = mix(col, uC1, clamp((t - uS0) / max(uS1 - uS0, 1e-6), 0.0, 1.0)); }
  if (uCount > 2.5) { col = mix(col, uC2, clamp((t - uS1) / max(uS2 - uS1, 1e-6), 0.0, 1.0)); }
  if (uCount > 3.5) { col = mix(col, uC3, clamp((t - uS2) / max(uS3 - uS2, 1e-6), 0.0, 1.0)); }
  if (uCount > 4.5) { col = mix(col, uC4, clamp((t - uS3) / max(uS4 - uS3, 1e-6), 0.0, 1.0)); }
  if (uCount > 5.5) { col = mix(col, uC5, clamp((t - uS4) / max(uS5 - uS4, 1e-6), 0.0, 1.0)); }
  return half4(half3(col.rgb) * half(col.a), half(col.a));
}
)";
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(kSrc));
    if (!effect)
      SkDebugf("sigilcompose unitRamp shader: %s\n", error.c_str());
    return effect;
  }();
  if (!fx || stops.empty())
    return Material::solid(stops.empty() ? SkColor4f{0, 0, 0, 0}
                                         : stops.front().color);
  if (stops.size() > 6)
    stops.resize(6);
  Material m = Material::sksl(fx, {{"uRadial", radial ? 1.0f : 0.0f},
                                   {"uCount", (float)stops.size()}});
  m.uniform("uA", std::array<float, 2>{a.x(), a.y()});
  m.uniform("uB", std::array<float, 2>{b.x(), b.y()});
  static const char *kColorNames[6] = {"uC0", "uC1", "uC2",
                                       "uC3", "uC4", "uC5"};
  static const char *kStopNames[6] = {"uS0", "uS1", "uS2",
                                      "uS3", "uS4", "uS5"};
  for (size_t i = 0; i < 6; ++i) {
    const bool live = i < stops.size();
    m.uniform(kColorNames[i],
              live ? stops[i].color : stops.back().color);
    m.uniform(kStopNames[i],
              live ? stops[i].pos : 1.0f);
  }
  return m;
}

} // namespace detail

} // namespace sigil::compose
