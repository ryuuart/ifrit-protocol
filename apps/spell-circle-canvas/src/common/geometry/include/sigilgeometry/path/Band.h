#pragma once

/** @file
 * A path displaced by a width law, and the region between the two rails
 * that displacement produces.
 *
 * `profileOffset` walks one rail; `bandRegion` walks both and closes
 * them, per contour; `sweptRegion` builds neither and unions the band's
 * cross-sections instead, which is the construction a join vocabulary and
 * a direction-keyed width law both need. All three are in the (along,
 * across) frame `Profile` states: `along` is a fraction of total arc
 * length and positive `across` is LEFT of travel, which with y pointing
 * down is outside a clockwise path.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <sigilgeometry/path/Profile.h>

#include <cstdint>
#include <functional>

namespace sigil::geometry::path {

/** Which side of the spine a band occupies. Explicit because the
 *  offset-path lineage has no defensible default beyond "both". */
enum class Formation : uint8_t { Centered, Outward, Inward };

/** Displace a path in its own (along, across) frame — the primitive
 *  behind a relative strand, and exactly the band's frame. A constant
 *  profile delegates to `parallel`, which means the same side.
 *
 *  It is `operations::offset` under a WIDTH LAW rather than a distance, which
 *  is a different axis of generality and not a case of it: the operator
 *  takes one number and this takes a function of arc length, so neither
 *  can be written as the other with a prop. Where the law is constant
 *  the two walk the same rail. */
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

/** ONE STATION OF A SWEPT BAND: where the spine is, which way it heads
 *  there, and how far along its contour that is. A width law reads
 *  whichever of those it is a function of — `fraction` for a taper or a
 *  `Profile`, `tangent` for a pen nib, whose width peaks where the spine
 *  runs perpendicular to the nib. */
struct SweepStation {
  SkPoint position{0, 0};
  SkVector tangent{1, 0};
  float distance = 0;  ///< arc length along this contour
  float fraction = 0;  ///< …as a fraction of `length`
  float length = 0;    ///< the contour's own arc length
};

/** THE FULL WIDTH of a swept band at one station, in px. A non-finite
 *  answer PINCHES the band to the spine rather than poisoning it: Skia
 *  draws none of a path holding one non-finite vertex, so a law that
 *  returns NaN at a single sample would delete the whole mark, silently
 *  and with nothing on screen to say why. */
using SweepWidth = std::function<float(const SweepStation&)>;

/** WHERE A SWEPT BAND'S TWO RAILS MEET at a station two steps share — the
 *  same decision a stroke makes, and named the same three ways: the point
 *  (bevelling past the miter limit), the arc, or the chord. */
enum class SweepJoin : uint8_t { Miter, Round, Bevel };

/** How a band is swept: how finely, and what closes it at a turn.
 *
 *  `stepPx` is the arc length between stations — the shorter it is the
 *  closer the band follows a curve, and the more pieces it is made of.
 *  `miterLimit` is Skia's, in Skia's units: how many half-widths a miter
 *  tip may reach before it is cut back to a bevel. It is the one join
 *  whose reach passes the band's own width. */
struct Sweep {
  float stepPx = 1.0f;
  SweepJoin join = SweepJoin::Miter;
  float miterLimit = 4.0f;
};

/** THE REGION A WIDTH LAW SWEEPS along @p spine, as the UNION of its
 *  cross-sections: one quadrilateral per step, plus a wedge or a disc at
 *  each station two steps share.
 *
 *  This is the OTHER construction of a band, and the difference from
 *  `bandRegion` is what a hard turn does. `bandRegion` walks the two rails
 *  through `profileOffset` and zips them into one closed contour, which
 *  gives a corner `parallel`'s real-vertex repair and no join vocabulary at
 *  all. A sweep never builds a rail: every piece stands alone and the
 *  winding fill unions them, so the inside of a bend is overlap rather
 *  than a crossing — and the outside is whatever `Sweep::join` says, which
 *  is the vocabulary a pen has and an offset curve does not. A calligraphic
 *  nib, whose width is a function of the spine's DIRECTION rather than of
 *  arc length, can only be swept: it is not a profile.
 *
 *  EVERY PIECE IS WOUND THE SAME WAY, which is load-bearing rather than
 *  tidy: under the winding fill a reversed piece laid over another cancels
 *  to zero and punches a hole through exactly the overlap the inside of a
 *  bend is made of. The result carries `kWinding` for the same reason. */
SkPath sweptRegion(const SkPath& spine, const SweepWidth& width,
                   const Sweep& sweep = {});

}  // namespace sigil::geometry::path
