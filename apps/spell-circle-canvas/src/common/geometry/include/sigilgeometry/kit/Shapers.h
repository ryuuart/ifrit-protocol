#pragma once

/** @file
 * The stock values over two of this library's seams: `shapers::`, what
 * bends one continuous mark, and the oscillating width law a strand that
 * trades sides is written as (`path::profile::wave`).
 *
 * Each is a comparable struct with its seam's required member, so a
 * caller's own value is indistinguishable from these at the call site,
 * which is the point of a seam. Nothing here decides anything a caller
 * did not ask for.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/effects/SkDiscretePathEffect.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Ops.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilgeometry/path/Shaper.h>

#include <cmath>
#include <vector>

namespace sigil::geometry::shapers {

/** A smooth oscillation across the mark — the wave every wavy rule,
 *  scalloped frame and ribbon edge is made of. Also the BRAID primitive:
 *  strands that oscillate trade sides, and where they trade sides they
 *  cross, which is what a braid is made of.
 *
 *  **As a PROFILE it is ZERO-MEAN**, which makes it a CENTRELINE — a strand
 *  path that swings either side of the boundary. It is NOT a band width: a
 *  band asks its profile for a width and this one goes negative half the
 *  time, which inverts the rails wherever it does. An undulating band is
 *  the composition — a positive offset PLUS an oscillation:
 *
 *      struct Undulating {                     // width 20, wobbling by 6
 *        shapers::Wave wobble{6, 40};
 *        bool operator==(const Undulating &) const = default;
 *        float max() const { return 20.0f + wobble.max(); }
 *        float across(float a) const { return 20.0f + wobble.across(a); }
 *      };
 *      path::bandRegion(spine, path::Profile(Undulating{}));
 *
 *  `wavelength` is PX, and the profile seam is asked in FRACTIONS of arc
 *  length. There is no contour length available at that seam to convert
 *  with, so the PROFILE reading treats `wavelength` as px-per-cycle on a
 *  nominal 1000 px contour: on a spine much shorter or much longer than
 *  that, the wavelength you get is not the one you asked for. That is also
 *  why `strands::braid` takes its own phase count instead of deriving one.
 *  The SHAPER reading (`shape()`) has a real path and is exact.
 *
 *  The third member is `phase`, not a zigzag flag — the cornered
 *  oscillation is `Zigzag` below, a separate value. */
struct Wave {
  float amplitude = 4.0f, wavelength = 24.0f, phase = 0.0f;
  bool operator==(const Wave&) const = default;
  float bleed() const { return std::abs(amplitude); }
  float max() const { return std::abs(amplitude); }
  /** As a PROFILE: the same value read as a width across a spine, which
   *  is what makes a braid strand and a wavy band one vocabulary. */
  float across(float along) const {
    return amplitude * std::sin(2.0f * 3.14159265f *
                                (along / wavelengthFraction() + phase));
  }
  /** As a SHAPER: displace the path itself. */
  SkPath shape(const SkPath& p) const {
    return path::displace(p, amplitude, wavelength, false);
  }

 private:
  /** See the note on the struct: px per cycle on a nominal 1000 px contour. */
  float wavelengthFraction() const {
    return wavelength > 0 ? wavelength / 1000.0f : 0.024f;
  }
};

/** A hand-drawn wobble: the mark resampled into short segments, each
 *  pushed off true by a seeded amount (the rough.js line).
 *
 *  ONE pass of SkDiscretePathEffect. The sketchy double-line those tools
 *  draw is TWO passes — full and half deviation at different seeds — so it
 *  is two brush layers or two restyles here, never one call. */
struct Jitter {
  float segLength = 8.0f, deviation = 2.0f;
  uint32_t seed = 7;
  bool operator==(const Jitter&) const = default;
  float bleed() const { return deviation * 2.0f; }
  SkPath shape(const SkPath& p) const {
    SkPathBuilder out;
    // HAIRLINE rec is required: under a fill rec SkDiscretePathEffect
    // force-CLOSES open contours, so an open mark gains a return chord
    // from its end back to its start — which then jitters away from the
    // real run and draws as a second, phantom line.
    SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);
    if (sk_sp<SkPathEffect> fx =
            SkDiscretePathEffect::Make(segLength, deviation, seed);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  }
};

/** A parallel displacement — the rail. Parallels never cross, which is
 *  why `layers` plus this is the double or triple line and a braid needs
 *  Wave instead.
 *
 *  **Positive is LEFT of travel.** That is the library-wide sign
 *  convention for an across-the-path offset, and it agrees exactly with
 *  `path::profile::offset(px)`; anything added here must match it,
 * because a disagreement mirrors a drawing rather than erroring. */
struct Offset {
  float px = 0.0f;
  float step = 4.0f;
  bool operator==(const Offset&) const = default;
  float bleed() const { return std::abs(px); }
  SkPath shape(const SkPath& p) const {
    return path::parallel(p, px, step);
  }
};

/** ROUND EVERY CORNER of the mark (SkCornerPathEffect). Not
 *  `shapes::rounded()`, which rounds an OUTLINE GENERATOR's result: this
 *  rounds whatever path the brush pipeline is carrying, so it softens a
 *  displaced zigzag or an offset rail, not just a silhouette. */
struct Rounded {
  float radius = 6.0f;
  bool operator==(const Rounded&) const = default;
  SkPath shape(const SkPath& p) const {
    SkPathBuilder out;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  }
};

/** CUT EVERY CORNER of the mark at 45° — Rounded's
 *  machined sibling, and the treatment SkCornerPathEffect cannot give you
 *  because it only rounds. Not `shapes::chamfered()`, which cuts an
 *  OUTLINE GENERATOR's box: this cuts whatever polyline the brush pipeline
 *  is carrying — a routed wire, a displaced zigzag, an offset rail.
 *
 *  A contour containing any curve segment passes through COMPLETELY
 *  UNTOUCHED, so this is a silent no-op over a curved mark. */
struct Chamfer {
  float cut = 6.0f;
  bool operator==(const Chamfer&) const = default;
  SkPath shape(const SkPath& p) const {
    return path::ops::chamferCorners(p, cut);
  }
};

/** THE BOXY DISPLACEMENT: a square wave across the mark — battlements,
 *  the Greek meander key, a stepped circuit trace. Wave's sibling; it has
 *  no profile reading, only a shaper one. */
struct Square {
  float amplitude = 5.0f, wavelength = 32.0f;
  bool operator==(const Square&) const = default;
  float bleed() const { return std::abs(amplitude); }
  SkPath shape(const SkPath& p) const {
    return path::ops::displaceSquare(p, amplitude, wavelength);
  }
};

/** THE SAME OSCILLATION WITH CORNERS: `Wave` sampled as straight runs
 *  between its extremes rather than a curve — the drawn zigzag, the
 *  saw edge, the seismograph line.
 *
 *  Its own value rather than a `zigzag` flag on `Wave`, because `Wave` is
 *  ALSO read as a profile through `across()`, and a flag the profile
 *  reading had to ignore would be a silent asymmetry between the two
 *  readings of one value. */
struct Zigzag {
  float amplitude = 4.0f, wavelength = 24.0f;
  bool operator==(const Zigzag&) const = default;
  float bleed() const { return std::abs(amplitude); }
  SkPath shape(const SkPath& p) const {
    return path::displace(p, amplitude, wavelength, true);
  }
};

inline Wave wave(float amplitude, float wavelength, float phase = 0.0f) {
  return Wave{amplitude, wavelength, phase};
}
inline Zigzag zigzag(float amplitude = 4.0f, float wavelength = 24.0f) {
  return Zigzag{amplitude, wavelength};
}
inline Rounded rounded(float radius = 6.0f) { return Rounded{radius}; }
inline Chamfer chamfered(float cut = 6.0f) { return Chamfer{cut}; }
inline Square square(float amplitude = 5.0f, float wavelength = 32.0f) {
  return Square{amplitude, wavelength};
}
inline Jitter jitter(float segLength = 8.0f, float deviation = 2.0f,
                     uint32_t seed = 7) {
  return Jitter{segLength, deviation, seed};
}
inline Offset offset(float px, float step = 4.0f) { return Offset{px, step}; }

}  // namespace sigil::geometry::shapers

// ---------------------------------------------------------------------------
// The width laws that oscillate — the profile seam's stock beyond
// `profile::self` and `profile::offset`, which are the two the seam
// itself ships. They land in the seam's own namespace, one library
// directory down, because that is where a caller looks for a width law.

namespace sigil::geometry::path::profile {

/** The wave as a PROFILE value (`across`/`max`): a strand that trades
 *  sides. The seam itself ships only `path::profile::self()` and
 *  `path::profile::offset()`; everything that oscillates lives here.
 *
 *  ZERO-MEAN, so this is a strand CENTRELINE and not a band width — as a
 *  width it goes negative half the time and inverts the band's rails.
 *  `Wave` above states the composition an undulating band wants
 *  instead. */
inline Profile wave(float amplitude, float wavelength,
                    float phase = 0.0f) {
  return Profile(shapers::Wave{amplitude, wavelength, phase});
}

}  // namespace sigil::geometry::path::profile
