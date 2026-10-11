/** @file
 * A unit-reading material answered for the unit its painter paints it
 * into, and the prepared maps held per unit.
 */

#include "UnitSurface.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/skia/Bevel.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>

#include <algorithm>
#include <cmath>
#include <glm/matrix.hpp>
#include <iterator>
#include <utility>

#include "GlyphInk.h"

namespace sigil::compose::detail {

namespace {

/** How many units one node keeps prepared maps for: a passage restarting
 *  on each glyph holds one per glyph it paints. */
constexpr size_t kHeldUnits = 256;

}  // namespace

bool UnitShape::operator==(const UnitShape& other) const {
  return density == other.density && direction == other.direction &&
         localToPaint == other.localToPaint &&
         directionMap == other.directionMap && outline == other.outline;
}

std::optional<material::Material> tangentAlong(const SkPath& outline,
                                               const SkPath& band, float reach,
                                               const glm::mat3& toPage,
                                               float density) {
  constexpr double kMaxPixels = 16.0 * 1024 * 1024;
  const SkRect bounds = band.getBounds().makeOutset(2, 2);
  const double across = std::ceil(bounds.width() * density);
  const double down = std::ceil(bounds.height() * density);
  if (bounds.isEmpty() || !bounds.isFinite() || across * down > kMaxPixels)
    return std::nullopt;
  sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::Make(
      (int)across, (int)down, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  if (!surface) return std::nullopt;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SkColor4f{1, 0.5f, 1, 1});
  canvas.scale(density, density);
  canvas.translate(-bounds.left(), -bounds.top());
  const auto colourOf = [&](SkVector tangent) {
    const glm::vec2 page = intoPage({tangent.x(), tangent.y()}, toPage);
    return SkColor4f{page.x * 0.5f + 0.5f, -page.y * 0.5f + 0.5f, 1, 1};
  };
  // Each short piece of the contour twice over. First as a stroke with
  // square ends, which reaches round a corner the contour turns; then as
  // the wedge between the normals at its two ends, which partitions a
  // bending band exactly, so a point across the band reads the tangent of
  // the piece it stands beside rather than of whichever neighbour was
  // drawn last.
  SkPaint stroke;
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(reach);
  stroke.setStrokeCap(SkPaint::kSquare_Cap);
  SkPaint wedge;
  wedge.setAntiAlias(false);
  constexpr float kStep = 1.5f;
  const float half = reach / 2;
  for (const bool wedges : {false, true}) {
    SkContourMeasureIter contours(outline, false);
    while (sk_sp<SkContourMeasure> contour = contours.next()) {
      for (float at = 0; at < contour->length(); at += kStep) {
        SkPoint from, to;
        SkVector tangent, ahead;
        if (!contour->getPosTan(at, &from, &tangent)) continue;
        if (!contour->getPosTan(std::min(at + kStep, contour->length()), &to,
                                &ahead))
          ahead = tangent;
        if (!wedges) {
          stroke.setColor4f(colourOf(tangent));
          canvas.drawLine(from, to, stroke);
          continue;
        }
        const SkVector normal{-tangent.y() * half, tangent.x() * half};
        const SkVector normalAhead{-ahead.y() * half, ahead.x() * half};
        SkPathBuilder piece;
        piece.moveTo(from + normal);
        piece.lineTo(to + normalAhead);
        piece.lineTo(to - normalAhead);
        piece.lineTo(from - normal);
        piece.close();
        wedge.setColor4f(colourOf(tangent));
        canvas.drawPath(piece.detach(), wedge);
      }
    }
  }
  glm::mat3 toLogical(1.0f);
  toLogical[0][0] = toLogical[1][1] = 1.0f / density;
  toLogical[2][0] = bounds.left();
  toLogical[2][1] = bounds.top();
  material::Texture texture(surface->makeImageSnapshot());
  texture.colorSpace(sigil::media::ColorSpace::Data);
  texture.uv(toLogical);
  return material::image(std::move(texture));
}

material::Material resolveUnit(const material::Material& material,
                               const UnitShape& unit) {
  if (!material::readsUnit(material)) return material;
  const material::SurfaceOptions& stated = *material.surface();
  material::surface::PaintedUnit painted;
  if (stated.relief) {
    material::Texture normals = material::skia::reliefNormals(
        unit.outline, *stated.relief, unit.density, unit.localToPaint);
    if (normals.valid()) painted.relief = std::move(normals);
  }
  if (stated.anisotropyAlongUnit)
    painted.direction = unit.directionMap
                            ? *unit.directionMap
                            : material::surface::unitDirection(unit.direction);
  return material::surface::resolveUnit(material, painted);
}

glm::vec2 intoPage(glm::vec2 local, const glm::mat3& localToPage) {
  const glm::vec3 page = localToPage * glm::vec3(local, 0.0f);
  const float length = std::hypot(page.x, page.y);
  return std::isfinite(length) && length > 0
             ? glm::vec2{page.x, page.y} / length
             : local;
}

glm::vec2 longAxisOf(glm::vec2 size, const glm::mat3& localToPage) {
  return intoPage(size.x >= size.y ? glm::vec2{1, 0} : glm::vec2{0, 1},
                  localToPage);
}

const material::skia::SurfaceMaps& UnitSurfaces::mapsFor(
    const material::Material& material, const UnitShape& unit) {
  auto found =
      std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
        return entry.unit == unit && entry.material == material;
      });
  // Least recently painted first: a hit moves to the back.
  if (found != m_entries.end()) {
    std::rotate(found, std::next(found), m_entries.end());
    return *m_entries.back().maps;
  }
  if (m_entries.size() >= kHeldUnits) m_entries.erase(m_entries.begin());
  Entry entry{material, unit, std::nullopt};
  entry.maps.emplace(resolveUnit(material, unit));
  m_entries.push_back(std::move(entry));
  return *m_entries.back().maps;
}

sk_sp<SkShader> TextInk::Surface::shaderOver(const material::FrameData& frame,
                                             const glm::mat3& paintToLocal,
                                             const SkPath& outline,
                                             glm::vec2 direction,
                                             float density) const {
  if (!readsUnit || !units) return shader(frame, paintToLocal);
  UnitShape unit;
  unit.outline = outline;
  unit.localToPaint = glm::inverse(paintToLocal);
  unit.direction = direction;
  unit.density = density;
  return units->mapsFor(*readsUnit, unit).mapShader(map, frame, paintToLocal);
}

sk_sp<SkShader> TextInk::Surface::shaderAcross(const material::FrameData& frame,
                                               const SkPath& outline,
                                               glm::vec2 direction,
                                               float density) const {
  if (!readsUnit || !units)
    return material::skia::shader(inputs.asMap(map), frame);
  UnitShape unit;
  unit.outline = outline;
  unit.direction = direction;
  unit.density = density;
  return material::skia::shader(units->mapsFor(*readsUnit, unit).asMap(map),
                                frame);
}

}  // namespace sigil::compose::detail
