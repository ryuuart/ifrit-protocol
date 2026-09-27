/** @file
 * The rail and the band at a width law, over the band tier and the
 * offset operator.
 */

#include "sigilgeometry/path/Offset.h"

#include <include/core/SkPath.h>

#include "sigilgeometry/path/Band.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/advanced/Skia.h"

namespace sigil::geometry::path {

Outline offset(const Outline& outline, const Profile& width,
               OffsetOptions options) {
  const SkPath spine = toSk(outline);
  if (!options.region) return fromSk(profileOffset(spine, width));
  return fromSk(operations::offset(
      spine, width.acrossAt(0.0f, 1.0f),
      {.join = options.join, .miterLimit = options.miterLimit}));
}

Outline band(const Outline& spine, const Profile& width, BandOptions options) {
  return fromSk(bandRegion(toSk(spine), width, options.side));
}

}  // namespace sigil::geometry::path
