/** @file
 * The three arrangements of a paint layer, with their constants chosen.
 */

#include "sigilweave/kit/PaintLayers.h"

#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/skia/Color.h>

#include <algorithm>
#include <utility>

namespace sigil::weave::kit {

PaintLayer dropShadow(material::Color color, glm::vec2 offset,
                      float blurSigma, float spread, float intensity) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor4f(material::skia::toSkColor(color));
  if (intensity != 1.0f)
    paint.setAlphaf(std::clamp(color.a * intensity, 0.0f, 1.0f));
  if (spread > 0) {
    paint.setStyle(SkPaint::kStrokeAndFill_Style);
    paint.setStrokeWidth(spread);
  }
  return PaintLayer::blurred(std::move(paint), blurSigma, offset);
}

PaintLayer glow(material::Color color, float blurSigma, float spread,
                float intensity) {
  return dropShadow(color, {0, 0}, blurSigma, spread, intensity);
}

PaintLayer outline(material::Color color, float width,
                   geometry::path::Join join) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor4f(material::skia::toSkColor(color));
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setStrokeJoin(geometry::path::toSk(join));
  return PaintLayer(std::move(paint));
}

}  // namespace sigil::weave::kit
