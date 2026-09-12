#pragma once

/** @file
 * WHICH WAY A PIECE LEAVES THE NODE IT STARTS AT, and arrives at the one
 * it ends at. Both the offset that moves a source's own nodes and the
 * corner treatment that cuts a selection read a piece's direction, and
 * they must read it the same way for a treated corner to sit where an
 * offset one does. Private to the path operators.
 */

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "sigilgeometry/path/Segments.h"

namespace sigil::geometry::path::operations {

/** The first chord of @p piece that is not degenerate, as a unit
 *  vector. */
inline glm::vec2 leavingAlong(const Segment& piece) {
  for (int i = 1; i < piece.size(); ++i) {
    const glm::vec2 chord = piece.points[(size_t)i] - piece.points[0];
    if (glm::length(chord) > 1e-9f) return chord / glm::length(chord);
  }
  return {1, 0};
}

/** The last chord of @p piece that is not degenerate, as a unit
 *  vector. */
inline glm::vec2 arrivingAlong(const Segment& piece) {
  const int last = piece.size() - 1;
  for (int i = last - 1; i >= 0; --i) {
    const glm::vec2 chord =
        piece.points[(size_t)last] - piece.points[(size_t)i];
    if (glm::length(chord) > 1e-9f) return chord / glm::length(chord);
  }
  return {1, 0};
}

}  // namespace sigil::geometry::path::operations
