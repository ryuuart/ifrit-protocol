#pragma once

/** @file
 * @ingroup geometry-path
 *
 * The crossing from the two stroke words to Skia's paint. It stands
 * apart from `path/Skia.h` because `SkPaint`'s definition is needed to
 * name its nested enumerations, and a consumer that only says which
 * join it wants should not have to parse a paint.
 */

#include <include/core/SkPaint.h>

#include "sigilgeometry/path/Stroke.h"

namespace sigil::geometry::path {

/** The corner decision as Skia's, for a paint about to stroke. */
inline SkPaint::Join toSk(Join join) {
  switch (join) {
    case Join::Round:
      return SkPaint::kRound_Join;
    case Join::Miter:
      return SkPaint::kMiter_Join;
    case Join::Bevel:
      return SkPaint::kBevel_Join;
  }
  return SkPaint::kRound_Join;
}

/** The end decision as Skia's, on the same terms. */
inline SkPaint::Cap toSk(Cap cap) {
  switch (cap) {
    case Cap::Butt:
      return SkPaint::kButt_Cap;
    case Cap::Round:
      return SkPaint::kRound_Cap;
    case Cap::Square:
      return SkPaint::kSquare_Cap;
  }
  return SkPaint::kButt_Cap;
}

}  // namespace sigil::geometry::path
