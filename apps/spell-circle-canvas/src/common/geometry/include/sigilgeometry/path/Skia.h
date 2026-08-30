#pragma once
/** @file
 * The two conversions between the geometry library's numeric currency
 * (glm vectors) and Skia's point type. Every geometry signature takes
 * and returns glm; Skia types appear only where the object IS a Skia
 * path, image or canvas. These live in one place so a caller drawing a
 * geometry result never spells the swizzle itself.
 */
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

#include <glm/vec2.hpp>

namespace sigil::geometry::path {

inline SkPoint toSk(glm::vec2 v) { return {v.x, v.y}; }
inline glm::vec2 fromSk(SkPoint p) { return {p.fX, p.fY}; }
inline glm::vec2 fromSk(SkSize s) { return {s.width(), s.height()}; }

/** The rect's centre as a vector. */
inline glm::vec2 centre(const SkRect& r) { return {r.centerX(), r.centerY()}; }

}  // namespace sigil::geometry::path
