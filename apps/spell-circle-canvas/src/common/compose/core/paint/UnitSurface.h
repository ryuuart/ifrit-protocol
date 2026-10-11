#pragma once

/** @file
 * THE UNIT A LIT MATERIAL IS PAINTED INTO, read by the painter at paint
 * time: the outline it covers, the way it runs and where the material is
 * sampled over it, answering the unit sources a material states — a
 * relief cut from the unit's coverage, a brushing along its direction —
 * so the material never holds an element. Held per unit, so a still unit
 * prepares its maps once.
 */

#include <include/core/SkPath.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/SurfaceMaps.h>

#include <cstddef>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <optional>
#include <vector>

namespace sigil::compose::detail {

/** One unit as its painter knows it. */
struct UnitShape {
  /** What the unit covers, in the node's logical pixels. */
  SkPath outline;
  /** The node's logical pixels into the space the material is sampled
   *  in: identity for a fill on the node's own box, the inverse of the
   *  unit square's placement for a unit ink, the node's place in the root
   *  for a paint laid over the canvas. */
  glm::mat3 localToPaint{1.0f};
  /** The way the unit runs, in the page the maps are painted on, y down. */
  glm::vec2 direction{1, 0};
  /** A painted direction along the unit, in place of `direction` — the
   *  tangent along a stroke. */
  std::optional<material::Material> directionMap;
  /** Device pixels to each logical pixel the relief is rasterised at. */
  float density = 1;
  bool operator==(const UnitShape& other) const;
};

/** THE TANGENT ALONG @p outline as a painted direction for a unit, over
 *  @p band in the node's logical pixels: each short piece of the contour
 *  paints the way it runs in the page — @p toPage carries the node there —
 *  across @p reach, at @p density device pixels to each logical pixel.
 *  Empty where the band is empty or too large to paint. */
std::optional<material::Material> tangentAlong(const SkPath& outline,
                                               const SkPath& band, float reach,
                                               const glm::mat3& toPage,
                                               float density);

/** @p material with its unit sources answered by @p unit. */
material::Material resolveUnit(const material::Material& material,
                               const UnitShape& unit);

/** @p local, a direction in the node, as a unit direction in the page
 *  @p localToPage carries the node into. */
glm::vec2 intoPage(glm::vec2 local, const glm::mat3& localToPage);

/** The direction of a box @p size across, as its long axis — across where
 *  it is at least as wide as it is tall — carried into the page through
 *  @p localToPage, the node's logical pixels into the page. */
glm::vec2 longAxisOf(glm::vec2 size, const glm::mat3& localToPage);

/** THE SURFACE MAPS OF A UNIT-READING MATERIAL, one prepared set per unit
 *  it was painted into, the most recently painted kept. */
class UnitSurfaces {
 public:
  /** The maps of @p material resolved for @p unit. */
  const material::skia::SurfaceMaps& mapsFor(const material::Material& material,
                                             const UnitShape& unit);

 private:
  struct Entry {
    material::Material material;
    UnitShape unit;
    std::optional<material::skia::SurfaceMaps> maps;
  };
  std::vector<Entry> m_entries;
};

}  // namespace sigil::compose::detail
