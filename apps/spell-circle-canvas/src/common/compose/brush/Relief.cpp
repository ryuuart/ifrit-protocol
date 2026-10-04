/** @file
 * Outline coverage supplies the shoulder normals; SigilMaterial supplies
 * the colour stack, surface response, lighting and pixel filters.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/brush/Relief.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Bevel.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <vector>

#include "paint/MaterialEffects.h"

namespace sigil::compose {

namespace detail {

struct ReliefEntry {
  geometry::path::Outline outline;
  float density = 0;
  size_t normalPixels = 0;
  material::Material surfaced = material::Color{0, 0, 0, 0};
  std::optional<material::skia::LitSurface> prepared;
  std::optional<material::Lighting> lighting;
  material::Paint paint;
};

struct ReliefCache {
  // A held brush may dress several nodes or standalone outlines. Keep
  // their distinct normal maps apart, with bounded retained raster memory.
  std::vector<ReliefEntry> entries;
};

}  // namespace detail

namespace {

constexpr size_t kMaxNormalPixels = 16 * 1024 * 1024;

float densityOf(const PaintContext& context) {
  const float density =
      context.bakeDensity > 0 ? context.bakeDensity : context.contentScale;
  return std::isfinite(density) && density > 0 && std::isfinite(1.0f / density)
             ? density
             : 1.0f;
}

material::Texture normalOf(const SkPath& outline, ReliefOptions options,
                           float density) {
  if (!std::isfinite(options.shoulder) || options.shoulder <= 0 ||
      !std::isfinite(options.depth) || options.depth == 0)
    return {};
  const float shoulder = options.shoulder * density;
  // The coverage differentiator squares its slopes when normalizing.
  if (!std::isfinite(shoulder) ||
      std::abs(options.depth) > std::sqrt(std::numeric_limits<float>::max()) /
                                    std::max(shoulder, 1.0f))
    return {};
  SkPath scaled = outline.makeTransform(SkMatrix::Scale(density, density));
  if (!scaled.isFinite()) return {};
  SkRect bounds = scaled.computeTightBounds();
  bounds.outset(shoulder + 2, shoulder + 2);
  // A normal bake keeps coverage, height and normal rasters together.
  // Bound that scratch independently of the canvas or outline's size.
  constexpr double kIntLimit = std::numeric_limits<int>::max();
  const double width = std::ceil(bounds.right()) - std::floor(bounds.left());
  const double height = std::ceil(bounds.bottom()) - std::floor(bounds.top());
  if (!bounds.isFinite() || width <= 0 || height <= 0 ||
      width * height > kMaxNormalPixels ||
      std::max({std::abs(static_cast<double>(bounds.left())),
                std::abs(static_cast<double>(bounds.top())),
                std::abs(static_cast<double>(bounds.right())),
                std::abs(static_cast<double>(bounds.bottom()))}) >= kIntLimit)
    return {};
  material::Texture normal = material::skia::bevelNormals(
      scaled, bounds.roundOut(), shoulder, options.depth);
  glm::mat3 toLogical(1.0f);
  toLogical[0][0] = toLogical[1][1] = 1.0f / density;
  normal.uv(toLogical * normal.uv());
  return normal;
}

bool isBeneath(const material::CoverageEffect& step) {
  return step.kind == material::CoverageEffect::Kind::Shadow &&
         !step.shadow.inside;
}

}  // namespace

bool Relief::isRunning() const { return m_material.isRunning(); }

bool Relief::usesWorldSpace() const {
  return material::skia::usesWorldSpace(m_material);
}

float Relief::bleed(glm::vec2 size) const {
  const material::Filter* effects = m_material.effects();
  if (!effects) return 0;
  float bleed = 0;
  for (const material::CoverageEffect& step : effects->coverage())
    bleed = std::max(bleed, detail::CoverageMark{step}.bleed());
  const SkRect input = SkRect::MakeWH(size.x, size.y).makeOutset(bleed, bleed);
  const material::FrameData frame{.resolution = size};
  if (const sk_sp<SkImageFilter> filter = material::skia::resolvedImageFilter(
          effects->withoutCoverage(), &frame)) {
    const SkIRect output = filter->filterBounds(
        input.roundOut(), SkMatrix::I(), SkImageFilter::kForward_MapDirection);
    bleed = std::max({bleed, -static_cast<float>(output.left()),
                      -static_cast<float>(output.top()),
                      static_cast<float>(output.right()) - size.x,
                      static_cast<float>(output.bottom()) - size.y});
  }
  return bleed;
}

bool Relief::readsLighting() const {
  // The relief gives a material that states no surface a default one to
  // carry its shoulder normals, so only a surface stated unlit paints the
  // same under every lighting.
  const material::SurfaceOptions* surface = m_material.surface();
  return !surface || !surface->unlit;
}

void Relief::paint(draw::Pen& pen, const PaintContext& context) const {
  if (context.outline.empty()) return;
  const SkPath outline = geometry::path::toSk(context.outline);
  const float density = densityOf(context);
  if (!m_cache) m_cache = std::make_shared<detail::ReliefCache>();
  auto& entries = m_cache->entries;
  auto found =
      std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.density == density && entry.outline == context.outline;
      });
  // Least recently used first: a hit moves to the back, so a brush that
  // paints more outlines than it retains keeps the ones it repaints.
  if (found != entries.end()) {
    std::rotate(found, std::next(found), entries.end());
    found = std::prev(entries.end());
  } else {
    material::SurfaceOptions surface = m_material.surface()
                                           ? *m_material.surface()
                                           : material::SurfaceOptions{};
    material::Texture normal = normalOf(outline, m_options, density);
    const glm::ivec2 normalSize = normal.source().size();
    const size_t normalPixels =
        static_cast<size_t>(normalSize.x) * normalSize.y;
    if (normal.valid()) {
      material::Material bevel = material::image(std::move(normal));
      surface.normal = surface.normal
                           ? material::surface::blendNormals(
                                 std::move(bevel), *surface.normal,
                                 {.baseDirectX = true,
                                  .detailDirectX = surface.normalDirectX,
                                  .outputDirectX = true})
                           : std::move(bevel);
      surface.normalDirectX = true;
    }
    constexpr size_t kCapacity = 16;
    const auto retainedPixels = [&] {
      size_t pixels = normalPixels;
      for (const auto& entry : entries) pixels += entry.normalPixels;
      return pixels;
    };
    while (!entries.empty() &&
           (entries.size() >= kCapacity || retainedPixels() > kMaxNormalPixels))
      entries.erase(entries.begin());
    entries.emplace_back();
    found = std::prev(entries.end());
    detail::ReliefEntry& cache = *found;
    cache.outline = context.outline;
    cache.density = density;
    cache.normalPixels = normalPixels;
    cache.surfaced = m_material;
    cache.surfaced.surface(surface);
    cache.prepared.emplace(cache.surfaced);
    cache.lighting.reset();
  }
  detail::ReliefEntry& cache = *found;
  const material::Lighting under = material::skia::lightingFor(
      cache.surfaced,
      context.lighting ? *context.lighting : material::Lighting{});
  if (!cache.lighting || *cache.lighting != under) {
    cache.lighting = under;
    cache.paint = cache.prepared->under(under);
  }
  SkCanvas& canvas = *pen.canvas();
  const material::FrameData frame = frameOf(context);
  const material::Filter* effects = m_material.effects();
  SkAutoCanvasRestore restore(&canvas, false);
  if (effects) {
    if (sk_sp<SkImageFilter> pixels = material::skia::resolvedImageFilter(
            effects->withoutCoverage(), &frame)) {
      SkPaint layer;
      layer.setImageFilter(std::move(pixels));
      float coverageBleed = 0;
      for (const material::CoverageEffect& step : effects->coverage())
        coverageBleed =
            std::max(coverageBleed, detail::CoverageMark{step}.bleed());
      const SkRect content =
          outline.getBounds().makeOutset(coverageBleed, coverageBleed);
      canvas.saveLayer(&content, &layer);
    }
    for (const material::CoverageEffect& step : effects->coverage())
      if (isBeneath(step)) detail::CoverageMark{step}.paint(pen, context);
  }
  SkPaint paint;
  paint.setAntiAlias(true);
  if (cache.paint.isSolid())
    paint.setColor4f(material::skia::toSkColor(cache.paint.solidColor()),
                     nullptr);
  else if (sk_sp<SkShader> shader = material::skia::shader(cache.paint, frame))
    paint.setShader(std::move(shader));
  else
    return;
  canvas.drawPath(outline, paint);
  if (effects)
    for (const material::CoverageEffect& step : effects->coverage())
      if (!isBeneath(step)) detail::CoverageMark{step}.paint(pen, context);
}

}  // namespace sigil::compose
