#pragma once

/** @file
 * @ingroup shaping
 *
 * One additional rendering of the positioned glyphs — a complete SkPaint,
 * an offset for this pass alone, and optionally a SigilMaterial instance
 * the pass shades with — and the shadow, glow and outline presets over it.
 */

#include <include/core/SkBlurTypes.h>
#include <include/core/SkColor.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPoint.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace sigil::material {
class Material;
}

namespace sigil::weave {

/** One additional rendering of the positioned glyphs.
 *
 * `paint` is intentionally the complete SkPaint vocabulary rather than a
 * SigilWeave mirror of selected fields: callers may use colors, animated
 * shaders, strokes, mask/image/color filters, path effects, and custom
 * blenders. `offset` moves only this pass, which makes shadows and displaced
 * highlights cheap without saveLayer().
 */
struct PaintLayer {
  SkPaint paint;             ///< applied as configured — nothing is overridden
  SkVector offset = {0, 0};  ///< px translation of this pass only
  /// A SigilMaterial instance this pass shades with, in place of the
  /// paint's own shader. Held by pointer: the style feature links no
  /// renderer, so the material is resolved at draw time through the
  /// resolver the paint feature registers (paint/Paint.h) — a pass whose
  /// material has no resolver draws with `paint` alone. Compares by
  /// identity, like every binding: two passes sharing one instance are
  /// one pass, and two equal instances held separately are two.
  std::shared_ptr<const sigil::material::Material> material;

  /** Constructs an anti-aliased black fill pass. */
  PaintLayer() { paint.setAntiAlias(true); }

  /** Constructs an anti-aliased solid-color fill pass. */
  explicit PaintLayer(SkColor color, SkVector layerOffset = {0, 0})
      : offset(layerOffset) {
    paint.setAntiAlias(true);
    paint.setColor(color);
  }

  /** Wraps a caller-configured SkPaint without changing any of its settings. */
  explicit PaintLayer(SkPaint layerPaint, SkVector layerOffset = {0, 0})
      : paint(std::move(layerPaint)), offset(layerOffset) {}

  /** Returns an arbitrary paint with a normal blur mask attached. */
  static PaintLayer blurred(SkPaint paint, float sigma,
                            SkVector offset = {0, 0}) {
    if (sigma > 0)
      paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));
    return PaintLayer(std::move(paint), offset);
  }

  /** Preset: a blurred, offset solid copy, normally used as an underlay.
   *
   * `spread` dilates the source shape (stroke-and-fill) before the blur
   * mask is applied, so a wide blur keeps a solid core instead of thinning
   * a hairline glyph outline down to near-transparency. `intensity` scales
   * `color`'s alpha, letting callers push a pass brighter without picking a
   * new hex value; values above 1 are clamped to fully opaque.
   */
  static PaintLayer dropShadow(SkColor color = 0x66000000,
                               SkVector offset = {2, 2}, float blurSigma = 2.0f,
                               float spread = 0.0f, float intensity = 1.0f) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(color);
    if (intensity != 1.0f) {
      const float alpha =
          std::clamp(SkColorGetA(color) * intensity, 0.0f, 255.0f);
      paint.setAlphaf(alpha / 255.0f);
    }
    if (spread > 0) {
      paint.setStyle(SkPaint::kStrokeAndFill_Style);
      paint.setStrokeWidth(spread);
    }
    return blurred(std::move(paint), blurSigma, offset);
  }

  /** Preset: a zero-offset blurred copy, normally used as an underlay. See
   *  dropShadow() for what `spread` and `intensity` control. */
  static PaintLayer glow(SkColor color, float blurSigma, float spread = 0.0f,
                         float intensity = 1.0f) {
    return dropShadow(color, {0, 0}, blurSigma, spread, intensity);
  }

  /** Preset: a stroked glyph copy, normally placed beneath the foreground. */
  static PaintLayer outline(SkColor color, float width,
                            SkPaint::Join join = SkPaint::kRound_Join) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(color);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    paint.setStrokeJoin(join);
    return PaintLayer(std::move(paint));
  }

  /** SkPaint compares every scalar and effect-object identity; the
   *  material by pointer. */
  bool operator==(const PaintLayer& other) const {
    return paint == other.paint && offset == other.offset &&
           material == other.material;
  }
};

}  // namespace sigil::weave
