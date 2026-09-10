/** @file
 * THE VARIABLE-WIDTH BAND: `brush::Ribbon`, a spine walked at a width law
 * and filled.
 *
 * The region is the UNION of its cross-sections — one quadrilateral per
 * step, plus a wedge or a disc at each station two steps meet on — rather
 * than one contour that would cross itself wherever the spine turns hard.
 * Every piece is wound the same way, because under the winding fill a
 * reversed piece laid over another cancels to nothing and punches a hole
 * through exactly the overlap the inside of a bend is made of.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Decorations.h>  // PathSample
#include <sigilcompose/brush/Ribbons.h>
#include <sigilgeometry/path/Numeric.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose::brush {

namespace {

/** Add one convex piece of the band, wound the way every other piece is.
 *
 *  EVERY sub-polygon of a band must wind the same way. Under the winding
 *  fill a reversed piece laid over another cancels to 0 and punches a
 *  hole where the two overlap, which is precisely the overlap the inside
 *  of a bend is made of. Cheap to enforce here, and impossible to see
 *  coming from the picture.
 *
 *  Which way a ring of points is wound is SigilGeometry's question and it
 *  is asked over the stack: at most four points per piece, and one piece
 *  per step of every ribbon in the frame. */
void addBandPiece(SkPathBuilder& b, const SkPoint* pts, size_t n) {
  if (n < 3) return;
  std::array<glm::vec2, 4> ring{};
  for (size_t i = 0; i < n && i < ring.size(); ++i)
    ring[i] = geometry::path::fromSk(pts[i]);
  b.moveTo(pts[0]);
  if (geometry::path::signedArea(std::span(ring).first(n)) > 0)
    for (size_t i = n; i-- > 1;) b.lineTo(pts[i]);
  else
    for (size_t i = 1; i < n; ++i) b.lineTo(pts[i]);
  b.close();
}

}  // namespace

SkPath Ribbon::band(const SkPath& spine) const {
  SkPathBuilder band;
  const float stride = std::max(step, 0.5f);
  const float limit = std::max(miterLimit, 1.0f);
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    // The stations: where the spine is, and how wide the band is there.
    std::vector<SkPoint> pos;
    std::vector<float> half;
    for (float d = 0;; d += stride) {
      const float at = std::min(d, len);
      SkPoint here;
      SkVector tan;
      if (!contour->getPosTan(at, &here, &tan)) break;
      const PathSample s{here, tan, at, len > 0 ? at / len : 0};
      float w;
      if (hasProfile()) {
        w = width.acrossAt(s.fraction, len);
      } else if (nibAngleDeg >= 0) {
        const float a =
            std::atan2(tan.y(), tan.x()) - geometry::path::radians(nibAngleDeg);
        w = widthStart *
            (nibContrast + (1 - nibContrast) * std::abs(std::sin(a)));
      } else {
        w = widthStart + (widthEnd - widthStart) * s.fraction;
      }
      // A NON-FINITE WIDTH PINCHES TO THE SPINE rather than poisoning the
      // band. Skia draws NONE of a path holding one non-finite vertex, so
      // a law that returns NaN at a single sample would delete the whole
      // mark, silently and with nothing on screen to say why; a local
      // pinch fails where the law failed.
      if (!std::isfinite(w)) w = 0.0f;
      pos.push_back(here);
      half.push_back(w * 0.5f);
      if (at >= len) break;
    }
    if (pos.size() < 2) continue;

    // One quadrilateral per step, each wound the same way, so the band is
    // the UNION of its cross-sections rather than one contour that
    // crosses itself where the spine turns hard.
    std::vector<SkVector> normal(pos.size() - 1);
    for (size_t i = 0; i + 1 < pos.size(); ++i) {
      const SkVector e{pos[i + 1].x() - pos[i].x(),
                       pos[i + 1].y() - pos[i].y()};
      const float L = std::hypot(e.x(), e.y());
      if (L < 1e-4f) {
        normal[i] = {0, 0};
        continue;
      }
      const SkVector n{-e.y() / L, e.x() / L};
      normal[i] = n;
      const SkPoint quad[4]{
          {pos[i].x() + n.x() * half[i], pos[i].y() + n.y() * half[i]},
          {pos[i + 1].x() + n.x() * half[i + 1],
           pos[i + 1].y() + n.y() * half[i + 1]},
          {pos[i + 1].x() - n.x() * half[i + 1],
           pos[i + 1].y() - n.y() * half[i + 1]},
          {pos[i].x() - n.x() * half[i], pos[i].y() - n.y() * half[i]}};
      addBandPiece(band, quad, 4);
    }

    // The joins, at every station two steps meet on. Both sides are
    // emitted and the winding fill absorbs the one on the inside of the
    // turn, which is cheaper and steadier than deciding which side is
    // outside from a cross product that vanishes on a straight run.
    for (size_t i = 1; i + 1 < pos.size(); ++i) {
      const SkVector& n0 = normal[i - 1];
      const SkVector& n1 = normal[i];
      const float turn = n0.x() * n1.y() - n0.y() * n1.x();
      const float h = half[i];
      // Below about a tenth of a pixel of opening there is nothing to
      // close, which is every station of a smoothly sampled curve.
      if (h <= 0 || std::abs(turn) * h < 0.1f) continue;
      if (join == SkPaint::kRound_Join) {
        band.addCircle(pos[i].x(), pos[i].y(), h, SkPathDirection::kCCW);
        continue;
      }
      for (int side = -1; side <= 1; side += 2) {
        const float s = (float)side;
        const SkPoint a{pos[i].x() + n0.x() * h * s,
                        pos[i].y() + n0.y() * h * s};
        const SkPoint b{pos[i].x() + n1.x() * h * s,
                        pos[i].y() + n1.y() * h * s};
        if (join == SkPaint::kMiter_Join) {
          // The rails meet where the bisector carries them. `cos` is the
          // half-angle's cosine, and 1/cos is the reach in half-widths —
          // Skia's miter limit, in the same units Skia states it in.
          SkVector m{n0.x() + n1.x(), n0.y() + n1.y()};
          const float mag = std::hypot(m.x(), m.y());
          const float cosHalf = mag * 0.5f;
          if (mag > 1e-4f && cosHalf > 1e-3f && 1.0f / cosHalf <= limit) {
            const float reach = h / cosHalf;
            const SkPoint tip{pos[i].x() + m.x() / mag * reach * s,
                              pos[i].y() + m.y() / mag * reach * s};
            const SkPoint wedge[4]{pos[i], a, tip, b};
            addBandPiece(band, wedge, 4);
            continue;
          }
        }
        const SkPoint wedge[3]{pos[i], a, b};
        addBandPiece(band, wedge, 3);
      }
    }
  }
  SkPath path = band.detach();
  path.setFillType(SkPathFillType::kWinding);
  return path;
}

void Ribbon::paint(SkCanvas& c, const PaintContext& ctx) const {
  const SkPath region = band(ctx.outline);
  if (region.isEmpty()) return;
  SkPaint p;
  p.setAntiAlias(true);
  // A material supersedes the fill, and it is resolved through the same
  // body a stroke's does, so a recipe means the same thing on a band as
  // on the outline beside it — unit square, node's box, one clock.
  const Fill band = fillMaterial ? resolveFill(*fillMaterial, ctx) : fill;
  if (band.kind == Fill::Kind::Color)
    p.setColor4f(band.colorValue, nullptr);
  else if (band.kind == Fill::Kind::Shader)
    p.setShader(band.shaderValue);
  c.drawPath(region, p);
}

}  // namespace sigil::compose::brush
