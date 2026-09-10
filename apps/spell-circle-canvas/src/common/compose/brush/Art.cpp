/** @file
 * `brush::Art`: an element baked once and stretched along the outline as
 * a single image, which is what a border rule drawn as a picture of a
 * rule wants — the art is authored horizontally and the band carries it
 * round every turn of the boundary.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <sigilcompose/brush/Ribbons.h>

#include <cmath>
#include <utility>
#include <vector>

#include "BakedArt.h"

namespace sigil::compose::brush {

void Art::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (!ctx.fonts) return;
  if (!cache->image || !bakedFromNode(cache->bakedFor, art.node())) {
    cache->bakedFor = art.node();
    cache->image = nullptr;
    // Consult the instance-side store before doing any raster work.
    if (ctx.stamps) {
      if (const StampCache::Entry* hit = ctx.stamps->get(art.node());
          hit && hit->image) {
        cache->image = hit->image;
        cache->artSize = hit->artSize;
      }
    }
  }
  if (!cache->image) {
    // Shell box: snapshot() and intrinsicSize() size by the root's CHILDREN
    // and ignore the root's own dimensions.
    const SkSize sz = intrinsicSize(box().child(art), *ctx.fonts);
    if (sz.isEmpty()) return;
    sk_sp<SkPicture> pic = snapshot(box().child(art), *ctx.fonts);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        std::max(1, (int)std::ceil(sz.width() * 2.0f)),
        std::max(1, (int)std::ceil(sz.height() * 2.0f))));
    if (!pic || !surface) return;
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    surface->getCanvas()->scale(2.0f, 2.0f);
    surface->getCanvas()->drawPicture(pic);
    cache->image = surface->makeImageSnapshot();
    cache->artSize = sz;
    if (ctx.stamps && cache->image)
      ctx.stamps->put(art.node(), {nullptr, cache->image, cache->artSize});
  }
  if (!cache->image) return;

  const float texW = (float)cache->image->width();
  const float texH = (float)cache->image->height();
  const float half = 0.5f * (height > 0 ? height : cache->artSize.height());
  SkPaint p;
  p.setAntiAlias(true);
  p.setShader(
      cache->image->makeShader(SkTileMode::kClamp, SkTileMode::kClamp,
                               SkSamplingOptions(SkFilterMode::kLinear)));

  SkContourMeasureIter iter(ctx.outline, false);
  std::vector<SkPoint> positions, texs;
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float length = contour->length();
    if (length < 1.0f) continue;
    const int stations =
        std::max(2, (int)std::ceil(length / std::max(1.0f, stationPx)));
    positions.clear();
    texs.clear();
    positions.reserve((size_t)(stations + 1) * 2);
    texs.reserve((size_t)(stations + 1) * 2);
    for (int i = 0; i <= stations; ++i) {
      const float f = (float)i / (float)stations;
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(length * f, &pos, &tan)) continue;
      const SkVector normal{-tan.fY, tan.fX};
      positions.push_back(pos + normal * half);
      positions.push_back(pos - normal * half);
      texs.push_back({texW * f, 0.0f});
      texs.push_back({texW * f, texH});
    }
    if (positions.size() < 4) continue;
    c.drawVertices(SkVertices::MakeCopy(SkVertices::kTriangleStrip_VertexMode,
                                        (int)positions.size(), positions.data(),
                                        texs.data(), nullptr),
                   SkBlendMode::kModulate, p);
  }
}

Art artAlong(Element art, float height, float stationPx) {
  Art b;
  b.art = std::move(art);
  b.height = height;
  b.stationPx = stationPx;
  b.bleedPx = std::max(32.0f, height);
  return b;
}

Ribbon ribbon(geometry::path::Profile width, Fill fill) {
  Ribbon r;
  r.width = std::move(width);
  r.fill = std::move(fill);
  return r;
}

}  // namespace sigil::compose::brush
