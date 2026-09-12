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

#include <sigilcompose/brush/Ribbons.h>
#include <sigilgeometry/path/Band.h>
#include <sigilgeometry/path/Numeric.h>

#include <cmath>

namespace sigil::compose::brush {

SkPath Ribbon::band(const SkPath& spine) const {
  // THE GEOMETRY IS SIGILGEOMETRY'S. What is a ribbon's own is the WIDTH
  // LAW — three of them, and the third is why a band is swept rather than
  // zipped from two offset rails: a pen nib's width is a function of the
  // spine's DIRECTION, which no profile keyed on arc length can be.
  const auto law = [this](const geometry::path::SweepStation& at) {
    if (hasProfile()) return width.acrossAt(at.fraction, at.length);
    if (nibAngleDeg >= 0) {
      const float a = std::atan2(at.tangent.y(), at.tangent.x()) -
                      geometry::path::radians(nibAngleDeg);
      return widthStart *
             (nibContrast + (1 - nibContrast) * std::abs(std::sin(a)));
    }
    return widthStart + (widthEnd - widthStart) * at.fraction;
  };
  return geometry::path::sweptRegion(
      spine, law,
      {.stepPx = step,
       .join = join == SkPaint::kRound_Join ? geometry::path::SweepJoin::Round
               : join == SkPaint::kMiter_Join
                   ? geometry::path::SweepJoin::Miter
                   : geometry::path::SweepJoin::Bevel,
       .miterLimit = miterLimit});
}

void Ribbon::paint(SkCanvas& c, const PaintContext& ctx) const {
  const SkPath region = band(ctx.outline);
  if (region.isEmpty()) return;
  SkPaint p;
  p.setAntiAlias(true);
  // A material supersedes the fill, and it is resolved through the same
  // body a stroke's does, so a recipe means the same thing on a band as
  // on the outline beside it — unit square, node's box, one clock.
  const Fill band =
      fillMaterial ? resolveFill(*fillMaterial, ctx) : resolveRef(fill, ctx);
  if (band.kind == Fill::Kind::Color)
    p.setColor4f(band.colorValue, nullptr);
  else if (band.kind == Fill::Kind::Shader)
    p.setShader(band.shaderValue);
  c.drawPath(region, p);
}

}  // namespace sigil::compose::brush
