/** @file
 * Coverage decorations cast from held parts: a part's cast is drawn into
 * an alpha mask once, in the part's own frame, and each paint samples it
 * where the part now stands.
 */

#include "HeldCoverage.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Paint.h>

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include "cache/MarkStates.h"

namespace sigil::compose::detail {

namespace {

/** The ceiling on a mask's raster along its longer side, the same bound
 *  the silhouette's own trace keeps: past it the mask is drawn coarser and
 *  sampled up, which a blurred cast tolerates best of anything. */
constexpr int kMaxMaskRaster = 2048;

/** The paint a cast is drawn into its mask with. */
SkPaint castPaint(const CoverageCast& cast) {
  SkPaint paint;
  paint.setAntiAlias(true);
  if (cast.bandWidth > 0) {
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(cast.bandWidth);
  } else if (cast.spread > 0) {
    paint.setStyle(SkPaint::kStrokeAndFill_Style);
    paint.setStrokeWidth(cast.spread * 2);
  }
  if (cast.sigma > 0)
    paint.setMaskFilter(
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, cast.sigma));
  return paint;
}

HeldCoverageMasks::Mask castMask(const SkPath& part, float density,
                                 const CoverageCast& cast) {
  HeldCoverageMasks::Mask mask;
  mask.source = part;
  mask.density = density;
  mask.cast = cast;
  const SkPaint paint = castPaint(cast);
  SkRect storage;
  const SkRect bounds =
      paint.canComputeFastBounds()
          ? paint.computeFastBounds(part.getBounds(), &storage)
          : part.getBounds();
  if (bounds.isEmpty() || !bounds.isFinite()) return mask;
  const float longer = std::max(bounds.width(), bounds.height());
  const float scale = std::min(density > 0 ? density : 1.0f,
                               (float)kMaxMaskRaster / std::max(longer, 1.0f));
  const SkIRect raster =
      SkRect::MakeLTRB(bounds.left() * scale, bounds.top() * scale,
                       bounds.right() * scale, bounds.bottom() * scale)
          .roundOut()
          // An empty pixel all round, so a sample past the last covered
          // one reads nothing rather than the mask's edge carried outward.
          .makeOutset(1, 1);
  const sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeA8(
      std::max(1, raster.width()), std::max(1, raster.height())));
  if (!surface) return mask;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.translate((float)-raster.left(), (float)-raster.top());
  canvas.scale(scale, scale);
  canvas.drawPath(part, paint);
  mask.toPart = SkMatrix::Scale(1.0f / scale, 1.0f / scale);
  mask.toPart.preTranslate((float)raster.left(), (float)raster.top());
  // THE REACH IS ONE PIXEL WIDER than the pixels the mask covers: a
  // sample taken between pixels reads its neighbours, so a turned pixel
  // whose centre falls just past the last covered one still reads part of
  // it and is drawn.
  SkPixmap alpha;
  if (surface->peekPixels(&alpha)) {
    const SkRegion covered = regionOfCoveredPixels(alpha, 1);
    std::vector<SkIRect> grown;
    for (SkRegion::Iterator rects(covered); !rects.done(); rects.next())
      grown.push_back(rects.rect().makeOutset(1, 1));
    SkRegion reach;
    reach.setRects(grown.data(), (int)grown.size());
    mask.reach = reach.getBoundaryPath().makeTransform(mask.toPart);
  }
  mask.image = surface->makeImageSnapshot();
  return mask;
}

}  // namespace

const HeldCoverageMasks::Mask& HeldCoverageMasks::maskFor(
    size_t index, const SkPath& part, float density, const CoverageCast& cast) {
  if (m_byPart.size() <= index) m_byPart.resize(index + 1);
  std::vector<Mask>& casts = m_byPart[index];
  for (Mask& mask : casts)
    if (mask.cast == cast) {
      if (mask.density != density || mask.source != part)
        mask = castMask(part, density, cast);
      return mask;
    }
  casts.push_back(castMask(part, density, cast));
  return casts.back();
}

bool paintHeldCoverage(SkCanvas& canvas, const PaintContext& context,
                       std::span<const CoverageInk> inks,
                       const CoverageCast& cast, bool insideOnly) {
  MarkState* state = context.markState;
  if (!state || !state->coverageParts || state->coverageParts->empty())
    return false;
  if (!state->coverage) state->coverage = std::make_shared<HeldCoverageMasks>();
  const float density =
      context.bakeDensity > 0 ? context.bakeDensity : context.contentScale;
  // Each ink as a shader carrying its own strength: its shader where it
  // has one, else its colour, so inks of different strengths can be laid
  // one over another inside one draw.
  std::vector<sk_sp<SkShader>> inkShaders;
  for (const CoverageInk& layer : inks) {
    const float alpha = layer.ink.getAlphaf();
    sk_sp<SkShader> shader = layer.ink.refShader();
    if (!shader)
      shader = SkShaders::Color(layer.ink.getColor4f(), nullptr);
    else if (alpha < 1)
      shader = SkShaders::Blend(SkBlendMode::kDstIn, std::move(shader),
                                SkShaders::Color({0, 0, 0, alpha}, nullptr));
    inkShaders.push_back(std::move(shader));
  }
  const SkSamplingOptions sampling(SkFilterMode::kLinear);
  const auto sampledAt = [&](const HeldCoverageMasks::Mask& mask,
                             const SkMatrix& where) {
    SkMatrix toNode = where;
    toNode.preConcat(mask.toPart);
    return mask.image->makeShader(SkTileMode::kClamp, SkTileMode::kClamp,
                                  sampling, &toNode);
  };
  const auto displacedBy = [](SkVector offset, const SkMatrix& placed) {
    SkMatrix displaced = SkMatrix::Translate(offset.x(), offset.y());
    displaced.preConcat(placed);
    return displaced;
  };
  // EVERY CAST IS DRAWN THROUGH A REGION, without an edge of its own: the
  // masks carry every edge, so what is drawn is a run of whole pixels the
  // shader shades rather than a path whose every step is its own
  // antialiased blit.
  const auto draw = [&](const SkPath& reach, sk_sp<SkShader> shader) {
    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setShader(std::move(shader));
    canvas.drawPath(reach, paint);
  };
  const std::vector<CoveragePart>& parts = *state->coverageParts;
  for (size_t i = 0; i < parts.size(); ++i) {
    const CoveragePart& part = parts[i];
    if (part.outline.isEmpty()) continue;
    const HeldCoverageMasks::Mask& mask =
        state->coverage->maskFor(i, part.outline, density, cast);
    if (!mask.image) continue;
    if (!insideOnly) {
      for (size_t layer = 0; layer < inks.size(); ++layer) {
        const SkMatrix displaced = displacedBy(inks[layer].offset, part.placed);
        draw(mask.reach.makeTransform(displaced),
             SkShaders::Blend(SkBlendMode::kSrcIn, sampledAt(mask, displaced),
                              inkShaders[layer]));
      }
      continue;
    }
    // INSIDE, the part's own coverage is held as a second mask: every ink
    // is laid over the ones before it, the whole is cut by that mask, and
    // it is drawn once, only where the mask reaches.
    const HeldCoverageMasks::Mask& inside =
        state->coverage->maskFor(i, part.outline, density, {});
    if (!inside.image) continue;
    sk_sp<SkShader> laid;
    for (size_t layer = 0; layer < inks.size(); ++layer) {
      sk_sp<SkShader> term = SkShaders::Blend(
          SkBlendMode::kSrcIn,
          sampledAt(mask, displacedBy(inks[layer].offset, part.placed)),
          inkShaders[layer]);
      laid = laid ? SkShaders::Blend(SkBlendMode::kSrcOver, std::move(laid),
                                     std::move(term))
                  : std::move(term);
    }
    draw(inside.reach.makeTransform(part.placed),
         SkShaders::Blend(SkBlendMode::kSrcIn, sampledAt(inside, part.placed),
                          std::move(laid)));
  }
  return true;
}

}  // namespace sigil::compose::detail
