/** @file
 * THE VARIABLE-WIDTH BAND: `brush::Ribbon`, a spine walked at a width law
 * and filled.
 *
 * What is here is the WIDTH LAW and the paint. The region itself is
 * SigilGeometry's `path::sweptRegion` — the union of the band's
 * cross-sections, with the join vocabulary a pen has and an offset curve
 * does not — so a milled groove, a ribbon and anything else swept along a
 * spine are one geometry rather than three.
 */

#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Band.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>

#include "paint/FillLowering.h"

namespace sigil::compose::detail {
struct RibbonCache {
  std::optional<Relief> material;
};
}  // namespace sigil::compose::detail

namespace sigil::compose::brush {

bool Ribbon::isRunning() const {
  return fillMaterial ? fillMaterial->isRunning()
                      : compose::detail::paintOf(fill).isRunning();
}

bool Ribbon::readsLighting() const {
  return fillMaterial && material::skia::isLit(*fillMaterial);
}

bool Ribbon::usesWorldSpace() const {
  return fillMaterial ? material::skia::usesWorldSpace(*fillMaterial)
                      : compose::detail::paintOf(fill).usesWorldSpace();
}

float Ribbon::bleed(glm::vec2 size) const {
  return bleed() +
         (fillMaterial ? relief(*fillMaterial, {.depth = 0}).bleed(size) : 0);
}

SkPath Ribbon::band(const SkPath& spine) const {
  // THE GEOMETRY IS SIGILGEOMETRY'S. What is a ribbon's own is the WIDTH
  // LAW — three of them, and the third is why a band is swept rather than
  // zipped from two offset rails: a pen nib's width is a function of the
  // spine's DIRECTION, which no profile keyed on arc length can be.
  const auto law = [this](const geometry::path::SweepStation& at) {
    if (hasProfile()) return width.acrossAt(at.fraction, at.length);
    if (nibAngleDeg >= 0) {
      const float a = std::atan2(at.tangent.y, at.tangent.x) -
                      geometry::path::radians(nibAngleDeg);
      return widthStart *
             (nibContrast + (1 - nibContrast) * std::abs(std::sin(a)));
    }
    return widthStart + (widthEnd - widthStart) * at.fraction;
  };
  return geometry::path::sweptRegion(
      spine, law,
      {.stepPx = step,
       .join = join == geometry::path::Join::Round
                   ? geometry::path::SweepJoin::Round
               : join == geometry::path::Join::Miter
                   ? geometry::path::SweepJoin::Miter
                   : geometry::path::SweepJoin::Bevel,
       .miterLimit = miterLimit});
}

void Ribbon::paint(draw::Pen& pen, const PaintContext& ctx) const {
  if (!fillMaterial && fill.kind == Fill::Kind::None) return;
  SkCanvas& c = *pen.canvas();
  const SkPath region = band(geometry::path::toSk(ctx.outline));
  if (region.isEmpty()) return;
  if (fillMaterial) {
    if (!cache) cache = std::make_shared<detail::RibbonCache>();
    material::Material finish = *fillMaterial;
    if (!finish.surface()) finish.surface({.unlit = true});
    Relief material = relief(std::move(finish), {.depth = 0});
    if (!cache->material || *cache->material != material)
      cache->material = std::move(material);
    PaintContext bandContext = ctx;
    bandContext.outline = geometry::path::fromSk(region);
    bandContext.silhouette = bandContext.outline;
    cache->material->paint(pen, bandContext);
    return;
  }
  SkPaint p;
  p.setAntiAlias(true);
  const Fill band = resolveFill(fill, ctx);
  if (band.kind == Fill::Kind::None) return;
  if (band.kind == Fill::Kind::Color)
    p.setColor4f(material::skia::toSkColor(band.colorValue), nullptr);
  else if (band.kind == Fill::Kind::Paint)
    p.setShader(material::skia::staticShader(compose::detail::paintOf(band)));
  c.drawPath(region, p);
}

Ribbon ribbon(geometry::path::Profile width, material::Material material) {
  Ribbon result;
  result.width = std::move(width);
  result.fillMaterial = std::move(material);
  return result;
}

}  // namespace sigil::compose::brush
