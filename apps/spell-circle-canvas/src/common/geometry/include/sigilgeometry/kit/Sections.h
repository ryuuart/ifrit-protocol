#pragma once

/** @file
 * The stock cross-sections a sweep carries.
 *
 * A section is a plain `path::Polyline` in Skia's y-down 2D space — x
 * runs along the frame's binormal, y against its normal — so a sweep
 * takes any outline that has been flattened, and `profile::fromPath()`
 * is the door from the 2D shape vocabulary. The two here are the shapes
 * anyone would otherwise write out by hand, and they are UNIT shapes:
 * `SweepOptions::scale` sizes them, which is why neither takes a radius
 * or a width.
 */

#include <sigilgeometry/path/Polyline.h>

namespace sigil::geometry::sections {

/** The unit circle, one point per @p sides step around it plus the seam
 *  point DUPLICATED — the contour is open and its first and last points
 *  coincide — because the swept surface's u must run 0 to 1 across the
 *  seam instead of wrapping back onto vertex zero. */
path::Polyline circle(int sides = 12);

/** The unit-width segment across the frame's binormal: two points, open.
 *  Swept, it is a flat band — pair it with `SweepOptions::Normals::Frame`
 *  so the band faces its rail. */
path::Polyline line();

}  // namespace sigil::geometry::sections
