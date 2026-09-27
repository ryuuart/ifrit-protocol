/** @file
 * The legibility underlays: a halo, a shade and an emboldening stroke,
 * each a paint layer laid beneath the glyphs of a style or a partial.
 */

#include "sigilcompose/kit/Legibility.h"

#include <include/core/SkPaint.h>
#include <sigilgeometry/path/StrokeSkia.h>
#include <sigilmaterial/skia/Color.h>

#include <utility>

namespace sigil::compose::kit {

sigil::weave::TextStyle haloed(sigil::weave::TextStyle style,
                                      const Halo& halo) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(material::skia::toSkColor(halo.colour), nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(halo.width);
  p.setStrokeJoin(geometry::path::toSk(halo.join));
  style.paint.addUnderlay(sigil::weave::PaintLayer(std::move(p)));
  return style;
}

sigil::weave::TextStyle shaded(sigil::weave::TextStyle style,
                                      const Shade& shade) {
  sigil::weave::PaintLayer layer;
  layer.paint.setAntiAlias(true);
  layer.paint.setColor4f(material::skia::toSkColor(shade.colour), nullptr);
  layer.offset = {shade.offset.x, shade.offset.y};
  style.paint.addUnderlay(std::move(layer));
  return style;
}

sigil::weave::Type haloed(sigil::weave::Type type,
                                 const Halo& halo) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(material::skia::toSkColor(halo.colour), nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(halo.width);
  p.setStrokeJoin(geometry::path::toSk(halo.join));
  if (!type.underlays) type.underlays.emplace();
  type.underlays->push_back(sigil::weave::PaintLayer(std::move(p)));
  return type;
}

sigil::weave::Type shaded(sigil::weave::Type type,
                                 const Shade& shade) {
  sigil::weave::PaintLayer layer;
  layer.paint.setAntiAlias(true);
  layer.paint.setColor4f(material::skia::toSkColor(shade.colour), nullptr);
  layer.offset = {shade.offset.x, shade.offset.y};
  if (!type.underlays) type.underlays.emplace();
  type.underlays->push_back(std::move(layer));
  return type;
}

sigil::weave::TextStyle emboldened(sigil::weave::TextStyle style,
                                          float width, material::Color colour) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(material::skia::toSkColor(colour), nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setStrokeJoin(SkPaint::kRound_Join);
  style.paint.addUnderlay(sigil::weave::PaintLayer(std::move(p)));
  return style;
}

sigil::weave::Type emboldened(sigil::weave::Type type, float width,
                                     material::Color colour) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(material::skia::toSkColor(colour), nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setStrokeJoin(SkPaint::kRound_Join);
  if (!type.underlays) type.underlays.emplace();
  type.underlays->push_back(sigil::weave::PaintLayer(std::move(p)));
  return type;
}

}  // namespace sigil::compose::kit
