/** @file
 * A unit-reading material answered for the unit its painter paints it
 * into, and the prepared maps held per unit.
 */

#include "UnitSurface.h"

#include <sigilmaterial/skia/Bevel.h>
#include <sigilmaterial/surface/Surface.h>

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

}  // namespace sigil::compose::detail
