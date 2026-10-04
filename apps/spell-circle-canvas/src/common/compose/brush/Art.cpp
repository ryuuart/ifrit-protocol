/** @file
 * `brush::Art`: an element baked once and stretched along the outline as
 * a single image, which is what a border rule drawn as a picture of a
 * rule wants — the art is authored horizontally and the band carries it
 * round every turn of the boundary.
 */

#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Measure.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilmotion/clock/Engine.h>

#include <cmath>
#include <utility>
#include <vector>

#include "BakedArt.h"
#include "cache/StampCache.h"
#include "paint/BakeFormat.h"

namespace sigil::compose::brush {

void Art::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& c = *pen.canvas();
  if (!ctx.fonts) return;
  const SkImageInfo actual = c.imageInfo();
  const SkImageInfo info = detail::pixelBakeInfo(
      actual.colorType() == kUnknown_SkColorType ? ctx.destination : actual,
      {1, 1});
  if (!cache->image || !bakedFromNode(cache->bakedFor, art.node()) ||
      cache->image->imageInfo().colorInfo() != info.colorInfo()) {
    cache->bakedFor = art.node();
    cache->image = nullptr;
    // Consult the instance-side store before doing any raster work.
    if (ctx.stamps) {
      if (const StampCache::Entry* hit = ctx.stamps->get(art.node());
          hit && hit->image &&
          hit->image->imageInfo().colorInfo() == info.colorInfo()) {
        cache->image = hit->image;
        cache->artSize = hit->artSize;
      }
    }
  }
  if (!cache->image) {
    // The shell includes the art's declared dimensions in its intrinsic size.
    const SkSize sz = intrinsicSize(box().children({art}), *ctx.fonts);
    if (sz.isEmpty()) return;
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        info.makeWH(std::max(1, (int)std::ceil(sz.width() * 2.0f)),
                    std::max(1, (int)std::ceil(sz.height() * 2.0f))));
    if (!surface) return;
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    surface->getCanvas()->scale(2.0f, 2.0f);
    motion::Engine engine;
    Composer composer(engine, *ctx.fonts);
    composer.setSize({sz.width(), sz.height()});
    composer.setAutoTexturePromotion(false);
    composer.render(
        box().width(sz.width()).height(sz.height()).children({art}));
    composer.draw(*surface->getCanvas());
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

  std::vector<SkPoint> positions, texs;
  for (const geometry::path::Contour& contour :
       geometry::path::Contour::of(ctx.outline)) {
    const float length = contour.length();
    if (length < 1.0f) continue;
    const int stations =
        std::max(2, (int)std::ceil(length / std::max(1.0f, stationPx)));
    positions.clear();
    texs.clear();
    positions.reserve((size_t)(stations + 1) * 2);
    texs.reserve((size_t)(stations + 1) * 2);
    for (int i = 0; i <= stations; ++i) {
      const float f = (float)i / (float)stations;
      const auto sample = contour.at(length * f);
      if (!sample) continue;
      const SkPoint pos = geometry::path::toSk(sample->position);
      const SkVector normal{-sample->tangent.y, sample->tangent.x};
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
