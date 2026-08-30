#pragma once

/** @file
 * TRANSITIONAL. The path tier's names, also reachable without naming
 * the tier, so a caller still spelling `sigil::geometry::Contour`
 * compiles while it moves to `sigil::geometry::path::Contour`. Delete
 * this file, and the include of it in Contour.h, once no caller needs
 * it.
 */

namespace sigil::geometry {
namespace path {}
using namespace path;
}  // namespace sigil::geometry
