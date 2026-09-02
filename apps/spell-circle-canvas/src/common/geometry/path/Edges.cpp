/** @file
 * The edge arithmetic: walking an outline's contours and cutting them
 * where the edge they face changes, and the concentric copy.
 */

#include "sigilgeometry/path/Edges.h"

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>

#include "sigilgeometry/path/Numeric.h"

namespace sigil::geometry::path {

SkPath edges(const SkPath& outline, Edge mask, float step) {
  const SkRect bounds = outline.getBounds();
  const float cx = bounds.centerX(), cy = bounds.centerY();
  const float hw = std::max(bounds.width() / 2, 1.0f);
  const float hh = std::max(bounds.height() / 2, 1.0f);
  auto classify = [&](SkPoint p) {
    const float nx = (p.x() - cx) / hw, ny = (p.y() - cy) / hh;
    if (std::abs(nx) > std::abs(ny)) return nx > 0 ? Edge::Right : Edge::Left;
    return ny > 0 ? Edge::Bottom : Edge::Top;
  };

  SkPathBuilder out;
  SkContourMeasureIter iter(outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float length = contour->length();
    if (length <= 0) continue;
    const int samples = std::max(8, (int)std::ceil(length / step));
    float runStart = 0.0f;
    SkPoint pos;
    if (!contour->getPosTan(0, &pos, nullptr)) continue;
    Edge runEdge = classify(pos);
    auto flushRun = [&](float endD) {
      if (has(mask, runEdge) && endD > runStart) {
        SkPathBuilder segment;
        if (contour->getSegment(runStart, endD, &segment, true))
          out.addPath(segment.detach());
      }
    };
    for (int i = 1; i <= samples; ++i) {
      const float d = length * (float)i / (float)samples;
      if (!contour->getPosTan(std::min(d, length), &pos, nullptr)) continue;
      const Edge e = classify(pos);
      if (e != runEdge) {
        // The boundary lies between the previous sample and this one;
        // narrow the bracket rather than taking either sample, or every
        // run boundary sits up to one step away from the real corner.
        const float at =
            bisect(length * (float)(i - 1) / (float)samples, d, [&](float mid) {
              SkPoint mp;
              return contour->getPosTan(mid, &mp, nullptr) &&
                     classify(mp) == runEdge;
            });
        flushRun(at);
        runStart = at;
        runEdge = e;
      }
    }
    flushRun(length);
  }
  return out.detach();
}

SkPath insetOutline(const SkPath& outline, float px) {
  if (px == 0) return outline;
  SkPaint offset;
  offset.setStyle(SkPaint::kStroke_Style);  // the RING, not the grown shape
  offset.setStrokeWidth(std::abs(px) * 2.0f);
  offset.setStrokeJoin(SkPaint::kMiter_Join);
  // The stroke-and-fill of the outline is the RING of width 2|px|
  // straddling it. Subtracting that ring shrinks the silhouette;
  // unioning it grows the silhouette by the same amount.
  const SkPath ring = skpathutils::FillPathWithPaint(outline, offset);
  SkPath result;
  if (Op(outline, ring,
         px > 0 ? SkPathOp::kDifference_SkPathOp : SkPathOp::kUnion_SkPathOp,
         &result))
    return result;
  return outline;
}

}  // namespace sigil::geometry::path
