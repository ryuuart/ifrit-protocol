/** @file
 * The three arrangements of a paint layer, with their constants chosen.
 */

#include "sigilweave/kit/PaintLayers.h"

#include <algorithm>
#include <utility>

namespace sigil::weave::kit {

PaintLayer dropShadow(SkColor color, SkVector offset, float blurSigma,
                      float spread, float intensity) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  if (intensity != 1.0f) {
    const float alpha = std::clamp(SkColorGetA(color) * intensity, 0.0f, 255.0f);
    paint.setAlphaf(alpha / 255.0f);
  }
  if (spread > 0) {
    paint.setStyle(SkPaint::kStrokeAndFill_Style);
    paint.setStrokeWidth(spread);
  }
  return PaintLayer::blurred(std::move(paint), blurSigma, offset);
}

PaintLayer glow(SkColor color, float blurSigma, float spread, float intensity) {
  return dropShadow(color, {0, 0}, blurSigma, spread, intensity);
}

PaintLayer outline(SkColor color, float width, SkPaint::Join join) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setStrokeJoin(join);
  return PaintLayer(std::move(paint));
}

}  // namespace sigil::weave::kit
