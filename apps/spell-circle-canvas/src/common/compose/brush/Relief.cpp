/** @file
 * The relief a material states, painted over the outline a decoration is
 * given: the outline is the unit the relief is cut from, answered as a
 * fill or an ink answers its own unit; SigilMaterial supplies the colour
 * stack, the surface maps and pixel filters.
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
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SurfaceMaps.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <vector>

#include "UnitPaint.h"
#include "paint/MaterialEffects.h"
#include "paint/SurfaceMap.h"
#include "paint/UnitSurface.h"

namespace sigil::compose {

namespace {

float densityOf(const PaintContext& context) {
  const float density =
      context.bakeDensity > 0 ? context.bakeDensity : context.contentScale;
  return std::isfinite(density) && density > 0 && std::isfinite(1.0f / density)
             ? density
             : 1.0f;
}

bool isBeneath(const material::CoverageEffect& step) {
  return step.kind == material::CoverageEffect::Kind::Shadow &&
         !step.shadow.inset;
}

/** @p drawn over @p outline between @p material's coverage effects, under
 *  its pixel effects, marked as a surface map where @p asMap. */
void paintOver(const material::Material& material, draw::Pen& pen,
               const PaintContext& context, const SkPath& outline,
               const material::Paint& drawn, bool asMap) {
  SkCanvas& canvas = *pen.canvas();
  const material::FrameData frame = frameOf(context);
  const material::Filter* effects = material.effects();
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
  if (drawn.isSolid())
    paint.setColor4f(material::skia::toSkColor(drawn.solidColor()), nullptr);
  else if (sk_sp<SkShader> shader = material::skia::shader(drawn, frame))
    paint.setShader(std::move(shader));
  else
    return;
  if (asMap) detail::markSurfaceMap(paint);
  canvas.drawPath(outline, paint);
  if (effects)
    for (const material::CoverageEffect& step : effects->coverage())
      if (!isBeneath(step)) detail::CoverageMark{step}.paint(pen, context);
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

void Relief::paint(draw::Pen& pen, const PaintContext& context) const {
  if (context.outline.empty()) return;
  if (!m_units) m_units = std::make_shared<detail::UnitSurfaces>();
  detail::paintUnit(m_material, *m_units, pen, context,
                    geometry::path::toSk(context.outline), std::nullopt);
}

void detail::paintUnit(const material::Material& material, UnitSurfaces& units,
                       draw::Pen& pen, const PaintContext& context,
                       const SkPath& outline,
                       const std::optional<material::Material>& direction) {
  // Only a surface map reads the unit: every other draw paints the
  // material's colours flat over the outline.
  if (!context.surfaceMap) {
    paintOver(material, pen, context, outline, material::skia::paint(material),
              false);
    return;
  }
  UnitShape unit;
  unit.outline = outline;
  const SkRect bounds = outline.getBounds();
  unit.direction =
      longAxisOf({bounds.width(), bounds.height()}, context.toRoot.matrix);
  unit.directionMap = direction;
  unit.density = densityOf(context);
  // The unit paints the map of its surface — the relief's normals among
  // them. A surface stated unlit is its own colour in every map.
  const bool asMap = material.surface() != nullptr;
  paintOver(material, pen, context, outline,
            asMap ? units.mapsFor(material, unit).asMap(*context.surfaceMap)
                  : material::skia::paint(material),
            asMap);
}

}  // namespace sigil::compose
