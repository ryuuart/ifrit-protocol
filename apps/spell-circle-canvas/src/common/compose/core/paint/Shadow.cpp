/** @file
 * CSS's box shadow painted: the outline moved by its offset, grown or
 * shrunk by its spread and softened by its blur beneath the node, or an
 * inset band hugging its inner edge; and how far either reaches.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkClipOp.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathUtils.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcompose/core/Shadow.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>

#include <algorithm>
#include <cmath>

#include "paint/FillLowering.h"
#include "paint/HeldCoverage.h"
#include "paint/MaterialEffects.h"

namespace sigil::compose {

float Shadow::bleed() const {
  if (inset) return 0;
  float extent = 0;
  if (blur > 0) {
    SkPaint paint;
    paint.setMaskFilter(
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur * 0.5f));
    const SkRect unit = SkRect::MakeWH(1, 1);
    SkRect storage;
    // Skia's mask filter supplies the conservative extent of its kernel.
    const SkRect bounds = paint.computeFastBounds(unit, &storage);
    extent = std::max({-bounds.left(), -bounds.top(), bounds.right() - 1,
                       bounds.bottom() - 1});
  }
  return std::max({std::abs(offset.x), std::abs(offset.y), maximumBind}) +
         extent + std::max(spread, 0.0f);
}

namespace {

/** @p outline grown by @p spread, or shrunk where it is negative: the
 *  outline with a band twice the spread wide added to it or cut out of
 *  it, its corners mitred so a square box keeps square corners, as CSS's
 *  spread does. */
SkPath spreadOutline(const SkPath& outline, float spread) {
  SkPaint band;
  band.setStyle(SkPaint::kStroke_Style);
  band.setStrokeWidth(std::abs(spread) * 2);
  band.setStrokeJoin(SkPaint::kMiter_Join);
  const SkPath stroked = skpathutils::FillPathWithPaint(outline, band);
  return Op(outline, stroked,
            spread > 0 ? kUnion_SkPathOp : kDifference_SkPathOp)
      .value_or(outline);
}

}  // namespace

void Shadow::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  SkPaint p;
  p.setAntiAlias(true);
  if (!detail::layInk(p, resolveFill(color, ctx))) return;
  const SkVector at = {bindOffsetX ? bindOffsetX->value() : offset.x,
                       bindOffsetY ? bindOffsetY->value() : offset.y};
  if (inset) {
    detail::paintInnerShadow(canvas, ctx, p, {at.x(), at.y()}, blur,
                             std::max(spread, 0.0f));
    return;
  }
  // Cast from a silhouette of held parts, each part's blur is held in its
  // own frame and placed where the part stands.
  if (!knockout && spread >= 0) {
    const detail::CoverageInk cast{p, at};
    if (detail::paintHeldCoverage(
            canvas, ctx, {&cast, 1},
            {.sigma = blur > 0 ? blur * 0.5f : 0, .spread = spread}, false))
      return;
  }
  if (blur > 0)
    p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur * 0.5f));
  const SkPath outline = geometry::path::toSk(ctx.outline);
  canvas.save();
  if (knockout) canvas.clipPath(outline, SkClipOp::kDifference, true);
  canvas.translate(at.x(), at.y());
  canvas.drawPath(spread == 0 ? outline : spreadOutline(outline, spread), p);
  canvas.restore();
}

}  // namespace sigil::compose
