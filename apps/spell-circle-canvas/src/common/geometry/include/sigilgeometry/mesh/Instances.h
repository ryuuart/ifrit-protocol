#pragma once

/** @file
 * @ingroup geometry-mesh
 *
 * INSTANCED GEOMETRY: one stamp mesh and, per instance, where a copy of it
 * stands — a position, an orientation and a scale — and what that copy
 * carries — a colour, a texture window and a few named scalars such as an
 * age, a phase or a cell. It is geometry like a mesh is: the operation
 * that makes it from points stands with the point operators, a renderer
 * draws it as one mesh repeated, and a scene format writes it as its point
 * instancer.
 */

#include <boost/container/map.hpp>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"

namespace sigil::geometry::mesh {

/** How many named scalars one instance carries: four, which is one
 *  four-float lane on a device. */
inline constexpr size_t kInstanceAttributes = 4;

/** ONE MESH, REPEATED: the stamp, and per instance its placement and its
 *  attributes. Every lane beside `positions` is either empty — every
 *  instance takes the neutral value — or one value per instance:
 *
 *  - an orientation turns the stamp about its own origin (identity when
 *    empty);
 *  - a scale scales it along its own axes before the turn (one when
 *    empty);
 *  - a colour multiplies the stamp's own colour (white when empty);
 *  - a texture window `{uOffset, vOffset, uScale, vScale}` remaps the
 *    stamp's uv for this copy, which is how one sheet gives each copy a
 *    different cell (the whole sheet when empty);
 *  - each named attribute is one scalar per instance, which a material
 *    reads by its name. At most `kInstanceAttributes` names are carried;
 *    the ones past that are not drawn. */
struct Instances {
  Mesh stamp;
  std::vector<glm::vec3> positions;
  std::vector<glm::quat> orientations;
  std::vector<glm::vec3> scales;
  std::vector<glm::vec4> colors;
  std::vector<glm::vec4> textureWindows;
  boost::container::map<std::string, std::vector<float>, std::less<>>
      attributes;

  /** How many copies of the stamp there are. */
  [[nodiscard]] size_t size() const { return positions.size(); }

  /** Instance @p index's placement as a matrix: translation times
   *  orientation times scale, the neutral value standing in for a lane
   *  that is empty. */
  [[nodiscard]] glm::mat4 placement(size_t index) const;

  /** The named attribute @p name, or null when no instance carries it. */
  [[nodiscard]] const std::vector<float>* attributeIf(
      std::string_view name) const;

  /** Value equality, lane for lane. */
  bool operator==(const Instances&) const = default;
};

}  // namespace sigil::geometry::mesh
