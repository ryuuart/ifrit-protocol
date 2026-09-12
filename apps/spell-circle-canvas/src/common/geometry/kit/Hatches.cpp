/** @file
 * The hatch: the outline narrowed by the offset, flattened to rings, run
 * through the lattice, and the marks joined into one path.
 */

#include "sigilgeometry/kit/Hatches.h"

#include <include/core/SkPathBuilder.h>

#include <vector>

#include "sigilgeometry/path/Lattice.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::shapes {

SkPath hatchOutline(const SkPath& outline, const Hatch& hatch) {
  const SkPath filled = hatch.inset == 0
                            ? outline
                            : path::operations::offset(outline, -hatch.inset);
  const std::vector<path::Polyline> rings = path::flatten(filled);
  const std::vector<path::LatticeMark> marks =
      path::lattice(rings, {.spacing = hatch.spacing,
                            .angle = hatch.angle,
                            .taper = hatch.taper,
                            .origin = hatch.origin,
                            .maxLines = hatch.maxLines});
  SkPathBuilder out;
  for (const path::LatticeMark& mark : marks) {
    out.moveTo(path::toSk(mark.from));
    out.lineTo(path::toSk(mark.to));
  }
  return out.detach();
}

}  // namespace sigil::geometry::shapes
