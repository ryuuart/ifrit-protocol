/** @file
 * Coverage decorations cast from held parts: a part's cast is drawn into
 * an alpha mask once, in the part's own frame, on the device the mark is
 * drawn on, and each paint samples it where the part now stands.
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
#include <include/gpu/graphite/Surface.h>
#include <sigilcompose/core/Paint.h>
#include <sigilgeometry/advanced/Skia.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <span>
#include <vector>

#include "cache/MarkStates.h"

namespace sigil::compose::detail {

namespace {

std::atomic<size_t> castsOnProcessor{0};
std::atomic<size_t> castsOnDevice{0};

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

/** Where @p cast of @p part reaches, in the part's own frame. */
SkRect castBounds(const SkPath& part, const CoverageCast& cast) {
  const SkPaint paint = castPaint(cast);
  const SkRect shape = part.getBounds().makeOffset(cast.offset);
  SkRect storage;
  return paint.canComputeFastBounds() ? paint.computeFastBounds(shape, &storage)
                                      : shape;
}

HeldCoverageMasks::Mask castMask(const SkPath& part, float density,
                                 const CoverageCast& cast,
                                 skgpu::graphite::Recorder* recorder) {
  HeldCoverageMasks::Mask mask;
  mask.source = part;
  mask.density = density;
  mask.cast = cast;
  mask.recorder = recorder;
  const SkPaint paint = castPaint(cast);
  const SkRect bounds = castBounds(part, cast);
  if (bounds.isEmpty() || !bounds.isFinite()) return mask;
  const float longer = std::max(bounds.width(), bounds.height());
  const float scale = std::min(density > 0 ? density : 1.0f,
                               (float)kMaxMaskRaster / std::max(longer, 1.0f));
  const SkIRect raster =
      SkRect::MakeLTRB(bounds.left() * scale, bounds.top() * scale,
                       bounds.right() * scale, bounds.bottom() * scale)
          .makeOffset(cast.grid)
          .roundOut()
          // An empty pixel all round, so a sample past the last covered
          // one reads nothing rather than the mask's edge carried outward.
          .makeOutset(1, 1);
  const SkImageInfo info = SkImageInfo::MakeA8(std::max(1, raster.width()),
                                               std::max(1, raster.height()));
  // ON THE DEVICE THE MARK IS DRAWN ON, the silhouette and its blur are
  // the device's work and the mask never visits the processor; a device
  // that refuses a one-channel target leaves it to the processor's raster.
  sk_sp<SkSurface> surface =
      recorder ? SkSurfaces::RenderTarget(recorder, info) : nullptr;
  const bool onDevice = surface != nullptr;
  if (!surface) surface = SkSurfaces::Raster(info);
  if (!surface) return mask;
  (onDevice ? castsOnDevice : castsOnProcessor).fetch_add(1);
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.translate((float)-raster.left() + cast.grid.x(),
                   (float)-raster.top() + cast.grid.y());
  canvas.scale(scale, scale);
  canvas.drawPath(part.makeOffset(cast.offset.x(), cast.offset.y()), paint);
  mask.toPart = SkMatrix::Scale(1.0f / scale, 1.0f / scale);
  mask.toPart.preTranslate((float)raster.left() - cast.grid.x(),
                           (float)raster.top() - cast.grid.y());
  // THE REACH IS ONE PIXEL WIDER than the pixels the mask covers: a
  // sample taken between pixels reads its neighbours, so a turned pixel
  // whose centre falls just past the last covered one still reads part of
  // it and is drawn. A device's pixels are not read back, so its reach is
  // the whole raster, whose empty rim reads nothing.
  SkPixmap alpha;
  if (onDevice) {
    mask.reach =
        SkPath::Rect(SkRect::MakeIWH(surface->width(), surface->height()))
            .makeTransform(mask.toPart);
  } else if (surface->peekPixels(&alpha)) {
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

CoverageCastsDrawn coverageCastsDrawn() {
  return {castsOnProcessor.load(), castsOnDevice.load()};
}

const HeldCoverageMasks::Mask& HeldCoverageMasks::maskFor(
    size_t index, const SkPath& part, float density, const CoverageCast& cast,
    skgpu::graphite::Recorder* recorder) {
  if (m_byPart.size() <= index) m_byPart.resize(index + 1);
  std::deque<Mask>& casts = m_byPart[index];
  for (Mask& mask : casts)
    if (mask.cast == cast) {
      if (mask.density != density || mask.recorder != recorder ||
          mask.source != part)
        mask = castMask(part, density, cast, recorder);
      return mask;
    }
  casts.push_back(castMask(part, density, cast, recorder));
  return casts.back();
}

bool paintHeldCoverage(SkCanvas& canvas, const PaintContext& context,
                       std::span<const CoverageInk> inks,
                       const CoverageCast& cast, bool insideOnly) {
  MarkState* state = context.markState;
  if (!state) return false;
  const float density =
      context.bakeDensity > 0 ? context.bakeDensity : context.contentScale;
  const bool composed = state->coverageParts && !state->coverageParts->empty();
  // ANY OTHER OUTLINE IS ONE PART placed where it stands, for a blurred
  // cast held inside it: its band is blurred once and kept while the
  // outline holds, whatever its ink does, and cut by the outline itself
  // where it is drawn, so its edge is the edge a live cast has. A cast
  // outside an outline that is not composed reaches past it, and an
  // unblurred band holds nothing worth keeping: those are the mark's own
  // to draw.
  std::vector<CoveragePart> outlineAlone;
  SkVector grid = {0, 0};
  if (!composed) {
    if (!insideOnly || !(cast.sigma > 0)) return false;
    CoveragePart& whole = outlineAlone.emplace_back();
    whole.outline = geometry::path::toSk(context.outline);
    // An outline too large to hold at the density it is drawn at would be
    // held coarser and sampled up, which a band a few pixels wide does not
    // survive: it is cast live instead.
    const SkRect reach = castBounds(whole.outline, cast);
    if (std::max(reach.width(), reach.height()) *
            (density > 0 ? density : 1.0f) >
        (float)kMaxMaskRaster)
      return false;
    whole.placed = SkMatrix::I();
    // Drawn at its density and between pixels, the band is cut on the
    // pixels it lands on: sampled on its own grid, it is the cast a live
    // blur draws there rather than that cast resampled.
    const SkMatrix& toDevice = canvas.getTotalMatrix();
    if (toDevice.isScaleTranslate() && toDevice.getScaleX() == density &&
        toDevice.getScaleY() == density)
      grid = {toDevice.getTranslateX() - std::floor(toDevice.getTranslateX()),
              toDevice.getTranslateY() - std::floor(toDevice.getTranslateY())};
    if (!context.silhouette.empty())
      whole.inside = geometry::path::toSk(context.silhouette);
    // The masks are cut at this density, so what the composer keeps of the
    // node is drawn again when it changes rather than magnified.
    if (context.contentScaleRead) *context.contentScaleRead = true;
  }
  const std::vector<CoveragePart>& parts =
      composed ? *state->coverageParts : outlineAlone;
  if (!state->coverage) state->coverage = std::make_shared<HeldCoverageMasks>();
  skgpu::graphite::Recorder* const recorder =
      context.recorder ? context.recorder : canvas.recorder();
  // Each ink as a shader carrying its own strength: its shader where it
  // has one, else its colour.
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
  for (size_t i = 0; i < parts.size(); ++i) {
    const CoveragePart& part = parts[i];
    if (part.outline.isEmpty()) continue;
    // AN INK'S CAST AND WHERE IT IS SAMPLED. A part that only slides takes
    // each ink's displacement into its mask, which is then sampled on the
    // grid it was drawn on; a part that turns holds one mask for every
    // ink and samples it displaced, which keeps it while the part turns.
    const bool slides = part.placed.isTranslate();
    struct Cast {
      const HeldCoverageMasks::Mask* mask;
      SkMatrix where;
    };
    const auto castOf = [&](const CoverageInk& ink) {
      CoverageCast displaced = cast;
      if (slides) displaced.offset = ink.offset;
      displaced.grid = grid;
      return Cast{&state->coverage->maskFor(i, part.outline, density, displaced,
                                            recorder),
                  slides ? part.placed : displacedBy(ink.offset, part.placed)};
    };
    if (!insideOnly) {
      for (size_t layer = 0; layer < inks.size(); ++layer) {
        const Cast cast = castOf(inks[layer]);
        if (!cast.mask->image) continue;
        draw(cast.mask->reach.makeTransform(cast.where),
             SkShaders::Blend(SkBlendMode::kSrcIn,
                              sampledAt(*cast.mask, cast.where),
                              inkShaders[layer]));
      }
      continue;
    }
    if (!composed) {
      canvas.save();
      canvas.clipPath(part.inside.isEmpty() ? part.outline : part.inside, true);
      // Each band is drawn as the rectangle it stands in, an edge of which
      // only ever meets the empty rim of its mask: the clip is the one edge
      // drawn, and a device cuts it as it cuts a live cast's.
      for (size_t layer = 0; layer < inks.size(); ++layer) {
        const Cast cast = castOf(inks[layer]);
        if (!cast.mask->image) continue;
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setShader(SkShaders::Blend(SkBlendMode::kSrcIn,
                                         sampledAt(*cast.mask, cast.where),
                                         inkShaders[layer]));
        canvas.drawRect(cast.mask->reach.makeTransform(cast.where).getBounds(),
                        paint);
      }
      canvas.restore();
      continue;
    }
    // INSIDE, the part's own coverage is held as a second mask, and each
    // ink is cut by it and drawn in turn, only where it reaches: one ink
    // over the one before, as a clip to the inside would lay them.
    const HeldCoverageMasks::Mask& inside = state->coverage->maskFor(
        i, part.inside.isEmpty() ? part.outline : part.inside, density, {},
        recorder);
    if (!inside.image) continue;
    const SkPath insideReach = inside.reach.makeTransform(part.placed);
    for (size_t layer = 0; layer < inks.size(); ++layer) {
      const Cast cast = castOf(inks[layer]);
      if (!cast.mask->image) continue;
      draw(insideReach,
           SkShaders::Blend(SkBlendMode::kSrcIn, sampledAt(inside, part.placed),
                            SkShaders::Blend(SkBlendMode::kSrcIn,
                                             sampledAt(*cast.mask, cast.where),
                                             inkShaders[layer])));
    }
  }
  return true;
}

}  // namespace sigil::compose::detail
