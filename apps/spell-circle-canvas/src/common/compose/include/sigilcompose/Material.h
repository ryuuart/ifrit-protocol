#pragma once

/** @file
 * SigilCompose Material — the polymorphic paint value. A small tree of paint
 * nodes that compiles to ONE `sk_sp<SkShader>` (layers via SkShaders::Blend,
 * never stacked saveLayer) or a plain solid color. Material supersedes the
 * kernel's three-case `Fill` as the authoring value for `fill()`; `Fill`
 * stays as the low-level {none,color,shader} carrier the reconciler stores.
 *
 * A Material sits in one of three volatility tiers:
 *  - STATIC (solid, gradient ramp, image/sprite, blend, sksl with only
 *    constant uniforms): resolves eagerly to a color or shader, so it
 *    collapses to a `Fill` (toFill()) and rides the kernel's existing
 *    caching/prune/paint path unchanged.
 *  - GEOMETRY (an sksl() material declaring only `uResolution`): resolves
 *    when the node RECORDS and caches between layouts — it depends on the
 *    box, not on the clock (see geometryDependent()).
 *  - LIVE (an sksl() material with a ch::Output-bound uniform, or one
 *    reading `uTime`/`uContentScale`): carries the runtime-effect recipe
 *    and is re-resolved every frame from the current values (resolve());
 *    its node is declared volatile exactly like a bound fill, so it paints
 *    live and never freezes into a cache. This is what makes
 *    `.uniform(name, &output)` actually drive pixels — see the note on
 *    uniform() below.
 *
 * The material vocabulary mirrors MaterialX's solid/mix/ramp/image/blend
 * atoms so a MaterialX document importer is a clean later addition;
 * no MaterialX dependency is pulled — we own the SkSL/SkShader backend. The
 * OCIO working-space / view transform is a Composer output stage
 * (`Composer::setView` + `ocio::display` — <sigilcompose/Ocio.h>),
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

class SkBitmap;
class SkImage;

namespace sigil::compose {

/** A gradient ramp stop (position 0..1 + color) — the MaterialX <ramp> atom.
 *  Authored in the working color space. */
struct Stop {
  float pos = 0.0f;
  SkColor4f color = {0, 0, 0, 1};
  bool operator==(const Stop &) const = default;
};

/** The user-owned raster behind `Material::buffer()` (§4) — the Pool
 *  contract for pixels: draw into `bitmap()` (or through `canvas()`),
 *  then `commit()` to publish. The material's recipe carries
 *  (source, revision), so nothing repaints between commits and one
 *  commit patches exactly once on its next describe. `image()` snapshots
 *  lazily, cached per revision — a describe that prunes never copies a
 *  pixel. Not thread-safe by design (the Pool rule: one owner, one
 *  writer). */
class PixelBuffer {
public:
  PixelBuffer(int width, int height);
  ~PixelBuffer();
  PixelBuffer(const PixelBuffer &) = delete;
  PixelBuffer &operator=(const PixelBuffer &) = delete;

  /** The pixels, yours to write. N32 premul. */
  SkBitmap &bitmap();
  /** A raster canvas over the same pixels — the convenient writer. */
  SkCanvas &canvas();
  /** PUBLISH the edit: the next describe carries the new revision and
   *  the reconciler repaints the material's node exactly once. */
  void commit() { ++m_revision; }
  uint64_t revision() const { return m_revision; }
  /** The current snapshot (copied from the bitmap once per revision). */
  sk_sp<SkImage> image();

private:
  struct State; // SkBitmap + SkCanvas live out of line (heavy includes)
  std::unique_ptr<State> m_state;
  uint64_t m_revision = 0;
  uint64_t m_snapshotRevision = ~0ull;
  sk_sp<SkImage> m_snapshot;
};

class Material;

namespace detail {
/** The unit-square ramp both linearUnit() and radialUnit() compile to: one
 *  SkSL pass that divides by uResolution, so the gradient's coordinates are
 *  fractions of the node's laid-out box rather than pixels. ANY number of
 *  stops: the count is baked into the source and one effect is cached per
 *  count (the rule Patterns.h uses for grain's octaves), chained mixes,
 *  each only taking effect past its own start. */
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
  /** CONTENT THAT CHANGES WITHOUT RE-DESCRIBING (§4): a user-owned
   *  raster the material samples — a simulation, a decoded video frame,
   *  a paint surface, a scrollback. The seam Instances.h invented,
   *  verbatim: you own the PixelBuffer, draw into it, `commit()`; the
   *  material's recipe compares by (source, revision), so an identical
   *  re-describe PRUNES between commits and the commit's next describe
   *  patches exactly once. No `custom()` + `Cache::None`, no forfeited
   *  picture caching, decorations intact. */
  static Material buffer(std::shared_ptr<class PixelBuffer> source,
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
   *  cached between layouts — no per-frame cost. Any number of stops. */
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
   *  `.shape(shapes::circle())` the glow gets a visible hard rim. The
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

  /** LAYER STRENGTH inside a blend() — "soft-light this noise at 30%"
   *  (ROADMAP §5: the only route used to be forking the generator's SkSL
   *  to bake `0.5 + (v-0.5)*amp` into it). Photoshop layer-opacity
   *  semantics: the layer composites with its blend mode in full, then
   *  the result mixes back toward the accumulation by `a01` — which is
   *  NOT the same as thinning the layer's alpha first, and is the number
   *  a reference's layer panel states. Clamped to [0, 1]; 1 (the
   *  default) is free. Read ONLY by blend(); a material used directly as
   *  a fill ignores it (there is no accumulation to mix back toward).
   *  Participates in equality like every recipe field. */
  Material &amount(float a01);

  /** RECORDING-CULL RESERVE (§14): how far this material's node paints
   *  beyond its own box, in px. A DecorationScheme can declare `bleed()`
   *  so the recording cull grows; a Material could not, so a fill on an
   *  outline that escapes the box (a `shape()` silhouette larger than
   *  the layout rect — overflow is legal) truncated at the cached
   *  picture/texture bounds, and the arithmetic fell to the caller.
   *  Declares the same number on the same word. Read by the recording
   *  cull only — it moves no pixels itself; the default 0 changes
   *  nothing. Participates in equality like every recipe field (a
   *  changed reserve must re-record). */
  Material &bleed(float px);
  /** The declared reserve (0 unless bleed() was set). */
  float bleed() const { return m_bleed; }

  /** Step the auto-injected uTime at `hz` (floor(t·hz)/hz) — declared
   *  choppiness as a MATERIAL property, not per-consumer ticker plumbing.
   *  The P3R sea rule: its caustics run at 6 Hz ("we imagine the
   *  interpolation ourselves"); stop-motion/flipbook surfaces generally.
   *  Meaningful only on an sksl() material whose effect declares uTime
   *  (warn-and-ignore otherwise); 0 restores continuous time. */
  Material &quantizeTime(float hz);

  // ---- resolution ----------------------------------------------------------
  /** THE VOLATILITY DECLARATION — the same word every decoration scheme
   *  spells (see the AnimatedDecoration concept). True once any ch::Output
   *  uniform is bound OR the effect reads uTime or uContentScale (both
   *  change independently of the node): the material re-resolves per frame
   *  and its node stays volatile. A blend() inherits liveness from its
   *  layers.
   *
   *  Was `isLive()` until R3 (ROADMAP §33 rulings 2 and 13). That word
   *  said "live DATA" about a thing that is animated GRAPHICS, and it was
   *  the fifth spelling of one idea; one word now, and it is this one. */
  bool isAnimated() const;
  /** True when the effect declares uResolution (the node's layout size):
   *  the material needs PaintContext at resolve, but is stable between
   *  layouts — it resolves when its node records, CACHES like static
   *  content, and re-records on size change. A blend() inherits this from
   *  its layers (the flatten defers to resolve time). */
  bool geometryDependent() const;

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
  float m_amount = 1.0f; // blend-layer strength (see amount())
  float m_bleed = 0.0f;  // recording-cull reserve (see bleed())
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
  // The stop count is BAKED INTO THE SOURCE and one effect is cached per
  // count — the rule Patterns.h already follows for grain's octaves, and
  // for the same two reasons: a uniform-guarded loop faults across the
  // split-Skia boundary, and main() has to stay monolithic.
  //
  // It used to be a fixed six with the tail clamped, which two studies
  // ran out of from opposite directions: a 24-run tartan sett and a
  // 72-step chromatic sweep, both falling back to hand-written pattern
  // programs for want of stops.
  if (stops.empty())
    return Material::solid(SkColor4f{0, 0, 0, 0});
  if (stops.size() == 1)
    return Material::solid(stops.front().color);
  constexpr size_t kMaxStops = 256; // uniform budget, not a design limit
  if (stops.size() > kMaxStops)
    stops.resize(kMaxStops);
  const size_t n = stops.size();

  struct Cached {
    size_t count;
    sk_sp<SkRuntimeEffect> effect;
  };
  static std::vector<Cached> cache;
  sk_sp<SkRuntimeEffect> fx;
  for (const Cached &c : cache)
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
      src += "  col = mix(col, uC" + p1 + ", clamp((t - uS" + p0 +
             ") / max(uS" + p1 + " - uS" + p0 + ", 1e-6), 0.0, 1.0));\n";
    }
    src += "  return half4(half3(col.rgb) * half(col.a), half(col.a));\n}\n";
    auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
    if (!effect) {
      SkDebugf("sigilcompose unitRamp shader (%zu stops): %s\n", n,
               error.c_str());
      return Material::solid(stops.front().color);
    }
    fx = effect;
    cache.push_back({n, fx});
  }

  Material m = Material::sksl(fx, {{"uRadial", radial ? 1.0f : 0.0f}});
  m.uniform("uA", std::array<float, 2>{a.x(), a.y()});
  m.uniform("uB", std::array<float, 2>{b.x(), b.y()});
  for (size_t i = 0; i < n; ++i) {
    m.uniform(("uC" + std::to_string(i)).c_str(), stops[i].color);
    m.uniform(("uS" + std::to_string(i)).c_str(), stops[i].pos);
  }
  return m;
}

} // namespace detail

} // namespace sigil::compose
