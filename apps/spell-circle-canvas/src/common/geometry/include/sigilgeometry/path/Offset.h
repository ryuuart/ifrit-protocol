#pragma once
/** @file
 * @ingroup geometry-path
 *
 * AN OUTLINE AT A WIDTH: the rail a width law away from a spine, and the
 * band between the spine's two rails. The width is a `Profile` — a
 * number, stops along the spine, or any comparable law.
 */
#include <cstdint>

#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Profile.h"
#include "sigilgeometry/path/Stroke.h"

namespace sigil::geometry::path {

/** Which side of the spine a band occupies. Explicit because the
 *  offset-path lineage has no defensible default beyond "both". */
enum class Formation : uint8_t { Center, Inner, Outer };

/** The dials of `offset()`. */
struct OffsetOptions {
  /** How a region offset meets itself at a corner. */
  Join join = Join::Round;
  /** How many widths a mitred corner may stand from its vertex. */
  float miterLimit = 4.0f;
  /** Grow or shrink the REGION the outline bounds, by the law's width at
   *  the start of the spine, rather than walk the one rail beside it —
   *  a bolder silhouette or an inset frame, where a rail is a line. */
  bool region = false;
  bool operator==(const OffsetOptions&) const = default;
};

/** The rail @p width away from @p outline, positive to the LEFT of travel
 *  (outside a clockwise figure) — or, with `region`, the figure grown or
 *  shrunk. A constant width is a parallel; a law narrows and widens the
 *  rail along the spine. */
Outline offset(const Outline& outline, const Profile& width,
               OffsetOptions options = {});

/** The dials of `band()`. */
struct BandOptions {
  Formation side = Formation::Center;
  bool operator==(const BandOptions&) const = default;
};

/** The closed region between @p spine and its rail @p width away —
 *  straddling the spine, or wholly inside or outside it. */
Outline band(const Outline& spine, const Profile& width, BandOptions options = {});

}  // namespace sigil::geometry::path
