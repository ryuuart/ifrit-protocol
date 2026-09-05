#pragma once

/** @file
 * The two outlines more than one geometry test binary builds by hand: an
 * axis-aligned rectangle and a square at the origin.
 */

#include <include/core/SkPath.h>
#include <include/core/SkRect.h>

namespace sigil::geometry::test {

/** A square with its top-left corner at the origin. */
inline SkPath square(float size) {
  return SkPath::Rect(SkRect::MakeWH(size, size));
}

/** A rectangle by corner and extent. */
inline SkPath rect(float x, float y, float w, float h) {
  return SkPath::Rect(SkRect::MakeXYWH(x, y, w, h));
}

}  // namespace sigil::geometry::test
