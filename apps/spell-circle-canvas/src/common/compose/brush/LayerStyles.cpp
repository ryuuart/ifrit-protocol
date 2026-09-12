/** @file
 * Layer styles: bevels, sheens and inner shadows built from gradients and
 * blurs, and the gel and chrome bundles over the kit's colour tables.
 */

#include <include/core/SkColorFilter.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <include/core/SkString.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/TextPaint.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::compose::styles {

namespace kit = sigil::material::kit;

void InnerShadow::paint(SkCanvas& c, const PaintContext& ctx) const {
  c.save();
  // The clip is the SHAPE the outline encloses — `outline` itself unless
  // an edge adaptor or a span gate narrowed it to runs bounding no area
  // and left the shape in `silhouette`, where clipping to the run would
  // discard the whole mark. The ring drawn is still `outline`, which is
  // the part of the boundary that is shown.
  c.clipPath(ctx.silhouette.isEmpty() ? ctx.outline : ctx.silhouette, true);
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color, nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  const float reach =
      std::max(size, 1.0f) + std::max(std::abs(offset.fX), std::abs(offset.fY));
  p.setStrokeWidth(reach);
  if (size > 0)
    p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, size * 0.5f));
  // Cast semantics: shifting the stroked ring WITH the cast direction
  // thickens the half that stays inside the clip on the edge the shadow
  // falls from — offset (0,3) casts down, so the band hugs the top.
  c.translate(offset.fX, offset.fY);
  c.drawPath(ctx.outline, p);
  c.restore();
}

void OuterGlow::paint(SkCanvas& c, const PaintContext& ctx) const {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color, nullptr);
  if (spread > 0) {
    p.setStyle(SkPaint::kStrokeAndFill_Style);
    p.setStrokeWidth(spread * 2);
  }
  if (size > 0)
    p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, size * 0.5f));
  c.drawPath(ctx.outline, p);
}

void BevelEmboss::paint(SkCanvas& c, const PaintContext& ctx) const {
  const float rad = geometry::path::radians(angleDeg);
  // Canvas y grows downward: light FROM angle → the vector pointing away
  // from the light. An inner shadow's visible edge is OPPOSITE its offset.
  const SkVector away = {-std::cos(rad) * depth, std::sin(rad) * depth};
  InnerShadow{highlight, away, size}.paint(c, ctx);               // lit edges
  InnerShadow{shadow, {-away.fX, -away.fY}, size}.paint(c, ctx);  // far edges
}

void Overlay::paint(SkCanvas& c, const PaintContext& ctx) const {
  SkPaint p;
  p.setAntiAlias(true);
  if (material.isSolid())
    p.setColor4f(material.solidColor(), nullptr);
  else if (sk_sp<SkShader> s = material.asShader())
    p.setShader(std::move(s));
  else
    return;
  p.setBlendMode(blend);
  if (opacity < 1.0f) p.setAlphaf(p.getAlphaf() * opacity);
  c.drawPath(ctx.outline, p);
}

material::skia::Effect ripple(float amplitudePx, float wavelengthPx,
                              float phase, bool vertical) {
  return material::skia::Effect::recipe(
      material::field::ripple(amplitudePx, wavelengthPx, phase, vertical));
}

}  // namespace sigil::compose::styles
