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

  /** SkPaint compares every scalar and effect-object identity; the
   *  material by pointer. */
  bool operator==(const PaintLayer& other) const {
    return paint == other.paint && offset == other.offset &&
           material == other.material;
  }
};

}  // namespace sigil::weave
