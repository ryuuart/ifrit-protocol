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
#include <include/core/SkTileMode.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

class SkImage;
class SkRuntimeEffect;

namespace sigil::compose {

/** A gradient ramp stop (position 0..1 + color) — the MaterialX <ramp> atom.
 *  Authored in the working color space. */
struct Stop {
  float pos = 0.0f;
  SkColor4f color = {0, 0, 0, 1};
  bool operator==(const Stop &) const = default;
};

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
  /** Constant float4 uniform set from a color (straight, not premultiplied —
   *  what the SkSL declares as `uniform float4`). */
  Material &uniform(std::string name, SkColor4f value);
  Material &uniform(std::string name, const choreograph::Output<float> *output);

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

} // namespace sigil::compose
