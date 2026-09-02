#pragma once

/** @file
 * A path displaced by a width law, and the region between the two rails
 * that displacement produces.
 *
 * `profileOffset` walks one rail; `bandRegion` walks both and closes
 * them, per contour. Both are in the (along, across) frame `Profile`
 * states: `along` is a fraction of total arc length and positive
 * `across` is LEFT of travel, which with y pointing down is outside a
 * clockwise path.
 */

#include <include/core/SkPath.h>
#include <sigilgeometry/path/Profile.h>

#include <cstdint>

namespace sigil::geometry::path {

/** Which side of the spine a band occupies. Explicit because the
 *  offset-path lineage has no defensible default beyond "both". */
enum class Formation : uint8_t { Centered, Outward, Inward };

/** Displace a path in its own (along, across) frame — the primitive
 *  behind a relative strand, and exactly the band's frame. A constant
 *  profile delegates to `parallel`, which means the same side. */
SkPath profileOffset(const SkPath& spine, const Profile& profile);

/** THE REGION a band occupies: the spine walked at both profile rails,
 *  per contour, through `profileOffset` — so corners get `parallel`'s
 *  real-vertex repair (arc outside a turn, miter inside) instead of the
 *  sample-and-displace spur a naive walk leaves on the inside of every
 *  rectangle.
 *
 *  Public because a varying-width MARK along a spine IS this region: a
 *  milled groove, or a ribbon, is this band filled. Sharing one geometry
 *  keeps the corner repair from being reimplemented per consumer. */
SkPath bandRegion(const SkPath& spine, const Profile& width,
                  Formation formation = Formation::Centered);

}  // namespace sigil::geometry::path
