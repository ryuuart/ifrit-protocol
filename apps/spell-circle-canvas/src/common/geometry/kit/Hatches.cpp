/** @file
 * The hatch: the region narrowed by the offset, flattened to rings, run
 * through the lattice once or twice, and the marks joined into one path.
 */

#include "sigilgeometry/kit/Hatches.h"

#include <include/core/SkPathBuilder.h>

#include <vector>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/advanced/Skia.h"

namespace sigil::geometry::shapes {

namespace {

/** The lattice passes of a hatch over rings already narrowed. */
std::vector<path::LatticeMark> passes(std::span<const path::Polyline> rings,
                                      const Hatch& hatch) {
  path::LatticeOptions options{.spacing = hatch.spacing,
                               .angle = hatch.angle,
                               .taper = hatch.taper,
                               .origin = hatch.origin,
                               .maxLines = hatch.maxLines};
  std::vector<path::LatticeMark> marks = path::lattice(rings, options);
  if (hatch.cross) {
    options.angle = hatch.angle + path::kTau * 0.25f;
    const std::vector<path::LatticeMark> across = path::lattice(rings, options);
    marks.insert(marks.end(), across.begin(), across.end());
  }
  return marks;
}

}  // namespace

std::vector<path::LatticeMark> hatchMarks(std::span<const path::Polyline> rings,
                                          const Hatch& hatch) {
  if (hatch.inset == 0) return passes(rings, hatch);
  // The inset is an offset of the region, which is an outline operation:
  // the rings go through the path once and come back narrowed.
  SkPathBuilder region;
  for (const path::Polyline& ring : rings) {
    if (ring.points.empty()) continue;
    region.moveTo(path::toSk(ring.points.front()));
    for (size_t index = 1; index < ring.points.size(); ++index)
      region.lineTo(path::toSk(ring.points[index]));
    region.close();
  }
  SkPath narrowed = region.detach();
  narrowed.setFillType(SkPathFillType::kEvenOdd);
  const std::vector<path::Polyline> inside =
      path::flatten(path::operations::offset(narrowed, -hatch.inset));
  return passes(inside, hatch);
}

path::Outline hatchOutline(const path::Outline& outline, const Hatch& hatch) {
  const SkPath source = path::toSk(outline);
  const SkPath filled = hatch.inset == 0
                            ? source
                            : path::operations::offset(source, -hatch.inset);
  const std::vector<path::Polyline> rings = path::flatten(filled);
  SkPathBuilder out;
  for (const path::LatticeMark& mark : passes(rings, hatch)) {
    out.moveTo(path::toSk(mark.from));
    out.lineTo(path::toSk(mark.to));
  }
  return path::fromSk(out.detach());
}

}  // namespace sigil::geometry::shapes
