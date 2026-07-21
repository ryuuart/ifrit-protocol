#pragma once

/** @file
 * SigilCompose Material — the polymorphic paint value. A small tree of paint
 * nodes that compiles to ONE `sk_sp<SkShader>` (layers via SkShaders::Blend,
 * never stacked saveLayer) or a plain solid color. Material supersedes the
 * kernel's three-case `Fill` as the authoring value for `fill()`; `Fill`
 * stays as the low-level {none,color,shader} carrier the reconciler stores.
 *
 * This is the LEAN first cut (the "static" vocabulary): every constructor
 * resolves eagerly to a color or shader, so a Material collapses to a `Fill`
 * (`toFill()`) and rides the existing caching/prune/paint path unchanged. The
 * new power over `Fill` + `util` gradients is real: N-stop ramps, sprite/image
 * fills, an SkSL escape, and — the headline — `blend()`, which flattens a
 * layered, per-layer-blended stack into a single cacheable shader (the
 * saveLayer-free path DESIGN.md asks for). Live (ch::Output-bound) uniforms
 * and structural-signature pruning are the next cut; the OCIO working-space /
 * view transform is a Composer output stage (see color::View), orthogonal to
 * Material the way SigilLoader is to SigilImage.
 *
 * Node names mirror the MaterialX standard library (solid/mix/ramp/image/
 * blend) so a MaterialX document importer is a clean later addition — but no
 * MaterialX dependency is pulled; we own the SkSL/SkShader backend.
 */

#include "sigilcompose/Compose.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkTileMode.h>

#include <string>
#include <utility>
#include <vector>

class SkImage;
class SkShader;
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
 *  Element::fill(). Resolves eagerly (this cut) to a solid color or one
 *  flattened SkShader. */
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
  /** The one raw escape: an SkSL runtime effect as a shader, with constant
   *  float uniforms set by name. (ch::Output-bound uniforms are the next
   *  cut.) */
  static Material sksl(sk_sp<SkRuntimeEffect> effect,
                       std::vector<std::pair<std::string, float>> uniforms = {});
  /** Wrap a raw shader (interop / escape). */
  static Material shader(sk_sp<SkShader> shader);

  // ---- combinator ----------------------------------------------------------
  /** Layer materials into ONE flattened shader: layers paint bottom-to-top,
   *  each composited over the accumulation with its SkBlendMode (the first
   *  layer's mode is the base and ignored). Nested SkShaders::Blend — one
   *  draw, fully picture-cacheable, no saveLayer. */
  static Material blend(std::vector<std::pair<Material, SkBlendMode>> layers);

  // ---- resolution ----------------------------------------------------------
  bool isNone() const { return !m_isSolid && !m_shader; }
  bool isSolid() const { return m_isSolid; }
  SkColor4f solidColor() const { return m_solid; }
  /** Always produces a shader (a solid becomes SkShaders::Color) — what
   *  blend() composes and what a shader-only fill uses. */
  sk_sp<SkShader> asShader() const;
  /** The static collapse the Composer stores: none/solid/shader Fill, so a
   *  Material rides the existing fill caching/prune path. */
  Fill toFill() const;

  /** Value equality (solid by color, shader by pointer identity — matching
   *  Fill's own operator==; structural-signature pruning of gradients is the
   *  next cut). */
  bool operator==(const Material &o) const {
    return m_isSolid == o.m_isSolid && m_solid == o.m_solid &&
           m_shader == o.m_shader;
  }

private:
  bool m_isSolid = false;
  SkColor4f m_solid = {0, 0, 0, 0};
  sk_sp<SkShader> m_shader; // null when solid/none
};

} // namespace sigil::compose
