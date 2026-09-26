#pragma once
/** @file
 * What an outline holds, seen only by the path feature's own sources.
 */
#include <include/core/SkPath.h>

#include <mutex>
#include <vector>

#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Outline.h"

namespace sigil::geometry::path {

namespace detail {
/** The Skia path an outline is, and its contours measured the first time
 *  a query asks for a distance along it. The measurement is shared by
 *  every copy of the outline, and taken once. */
struct OutlineBody {
  SkPath path;
  mutable std::once_flag measuredOnce;
  mutable std::vector<Contour> measured;
  mutable float measuredLength = 0;

  const std::vector<Contour>& contours() const {
    std::call_once(measuredOnce, [this] {
      measured = Contour::of(path);
      for (const Contour& contour : measured) measuredLength += contour.length();
    });
    return measured;
  }
};
}  // namespace detail

/** The one door into an outline's body. */
struct OutlineAccess {
  static const SkPath& path(const Outline& outline) {
    return outline.m_body->path;
  }
  static const detail::OutlineBody& body(const Outline& outline) {
    return *outline.m_body;
  }
  static Outline make(SkPath path) {
    auto body = std::make_shared<detail::OutlineBody>();
    body->path = std::move(path);
    return Outline(std::move(body));
  }
  static FillRule ruleOf(SkPathFillType type) {
    return type == SkPathFillType::kEvenOdd ||
                   type == SkPathFillType::kInverseEvenOdd
               ? FillRule::EvenOdd
               : FillRule::NonZero;
  }
};

}  // namespace sigil::geometry::path
