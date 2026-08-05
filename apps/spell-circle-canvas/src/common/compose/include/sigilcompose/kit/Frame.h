#pragma once

/** @file
 * SigilCompose KIT — the figure's own coordinate system, as a value.
 *
 * ## Frame
 *
 * A `Frame` converts `(angle, radius)` — numbers measured off a reference
 * drawing — into a point, an SkRect, or an arc-length fraction, in the
 * angle convention that drawing uses.
 *
 * **The convention is the reason this is a value and not a function.**
 * Engraved and statistical plates commonly measure clockwise from twelve
 * o'clock; Skia measures from due east. Written as a bare `polar()`
 * helper, that difference is a sign flip and a −90 that every call site
 * repeats and every reader has to reverse-engineer. Written as a `Frame`,
 * it is one field set once, and every conversion below respects it.
 *
 * ### Why this is not `layouts::Radial`
 *
 * `Radial` distributes N children evenly around a container and hands back
 * rects: it is a placement POLICY and it decides where things go. `Frame`
 * decides nothing — you supply the angle and the radius. It is the peer of
 * `util::centred`, not of a layout scheme, and the two compose fine.
 *
 * ### The conversions are the point
 *
 * The arithmetic this exists to hold is of this shape:
 *
 *     float frac(float thDeg) { return fmod((thDeg - 90) / 360 + 4, 1); }
 *     // θ → the arc-length fraction of shapes::circle(), whose contour
 *     //   starts at due EAST and runs clockwise.
 *
 * That is a library convention — where `shapes::circle()`'s contour begins
 * — leaking into a caller's arithmetic, at every site that places a label
 * on a ring. `Frame::fraction()` and `Frame::skiaDeg()` are that
 * arithmetic written once, with the convention carried in the value rather
 * than in a comment beside each copy.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Util.h"

#include <include/core/SkMatrix.h>
#include <include/core/SkPathTypes.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <cmath>
#include <vector>

namespace sigil::compose::kit {

/** Where a frame's 0° points. */
enum class Zero {
  East,  ///< 3 o'clock — Skia's own convention, and `shapes::arc`'s.
  North, ///< 12 o'clock — the engraver's and the statistical plate's.
};

/** Which way a frame's angles increase, **as seen on screen**. Screen y is
 *  down, so `CW` is the direction that *looks* clockwise. */
enum class Sense { CW, CCW };

/** A figure's own polar coordinate system: a centre, a radius that `r = 1`
 *  lands on, and the convention flags.
 *
 *  An aggregate, meant for designated initialisation — a positional
 *  constructor could not gain a field later without breaking every call
 *  site, and the convention flags are exactly the fields a caller wants to
 *  name.
 *
 *      const kit::Frame fig{.centre = {kRR, kRR}, .radius = kR};  // North/CW
 *      g.child(util::disc(fig.at(126.0f, 0.72f), 6.0f).fill(ink));
 *
 *  Trivially copyable; holds no Element and no node state. */
struct Frame {
  SkPoint centre{0, 0};
  /** The px radius that `rNorm = 1` maps to. Authoring the rest of a
   *  figure in normalised radius is what lets the whole plate be rescaled
   *  by editing this one number. */
  float radius = 1.0f;
  Zero zero = Zero::North;
  Sense sense = Sense::CW;
  /** An extra origin offset in SCREEN degrees (+ = clockwise on screen),
   *  applied after `zero`. Normally 0. Two things want it: a scanned
   *  source that is not square to its own axes, and an index ring offset
   *  by half a division, which otherwise becomes a stray constant added at
   *  every call site. */
  float originDeg = 0.0f;

  bool operator==(const Frame &) const = default;

  // ---- angles ------------------------------------------------------------

  /** This frame's @p deg as a SCREEN angle: degrees from +x, increasing in
   *  the direction that looks clockwise. That is exactly what Skia's
   *  `addArc`, `shapes::arc()` and `shapes::sector()` take, so
   *
   *      shapes::sector(fig.skiaDeg(hourStart), fig.skiaSweep(30.0f))
   *
   *  reads in the plate's units and draws in Skia's. */
  constexpr float skiaDeg(float deg) const {
    const float base = zero == Zero::North ? -90.0f : 0.0f;
    return base + originDeg + (sense == Sense::CW ? deg : -deg);
  }
  /** A SWEEP in this frame's units as a Skia sweep — sign only, no origin.
   *  Kept separate from skiaDeg() because adding the origin twice is the
   *  classic bug when an arc is spelled as two absolute angles. */
  constexpr float skiaSweep(float sweepDeg) const {
    return sense == Sense::CW ? sweepDeg : -sweepDeg;
  }
  /** This frame's @p deg in radians about +x, ready for `std::cos`/`sin`
   *  against a screen-space point. */
  float radians(float deg) const {
    return skiaDeg(deg) * 0.01745329251994329577f;
  }

  /** @p deg as the arc-length fraction of a circular baseline — the value
   *  `TextPath::at` wants.
   *
   *  `shapes::circle()` is `SkPathBuilder::addOval(rect)` with direction
   *  kCW and `startIndex` 1, so its contour starts at the oval's
   *  **due-east** extreme and advances the way screen-clockwise runs. The
   *  fraction is therefore the screen angle over 360, wrapped into [0, 1).
   *
   *  **@p baseline is the direction of the PATH, not of this frame, and
   *  the two are independent.** `shapes::circle(kCCW)` also starts due
   *  east — `startIndex` is 1 either way — and then runs the other way
   *  round, so at f = 0.25 it sits at 12 o'clock where the kCW contour
   *  sits at 6. A frame whose `sense` is CCW on a baseline that is still
   *  kCW is an ordinary thing to want (numbers running anticlockwise
   *  around a clockwise ring), so this argument must not default to the
   *  frame's own sense: conflating them puts every label half a turn out.
   *
   *  **Only exact on a circle.** A circle's arc length is proportional to
   *  its angle; an ellipse's is not, so on a non-square box the result
   *  drifts from the true arc-length fraction. Keep ring inscriptions on a
   *  square box (`util::disc`, `Frame::box()`). */
  float fraction(float deg,
                 SkPathDirection baseline = SkPathDirection::kCW) const {
    const float screen =
        baseline == SkPathDirection::kCW ? skiaDeg(deg) : -skiaDeg(deg);
    const float f = std::fmod(screen / 360.0f, 1.0f);
    return f < 0 ? f + 1.0f : f;
  }
  /** The inverse of fraction(): an arc-length fraction back into this
   *  frame's degrees — for labelling a ring that was already placed by
   *  fraction, or for reading a hit test back out in plate units. */
  constexpr float degOf(float frac,
                        SkPathDirection baseline = SkPathDirection::kCW) const {
    const float base = zero == Zero::North ? -90.0f : 0.0f;
    const float screen =
        baseline == SkPathDirection::kCW ? frac * 360.0f : -frac * 360.0f;
    const float rel = screen - base - originDeg;
    return sense == Sense::CW ? rel : -rel;
  }

  // ---- points ------------------------------------------------------------

  /** `(angle, NORMALISED radius)` → a point in the frame's parent space.
   *  `rNorm = 1` is `radius`. */
  SkPoint at(float deg, float rNorm = 1.0f) const {
    return px(deg, rNorm * radius);
  }
  /** `(angle, PX radius)` → a point, for a figure whose radii were
   *  measured in pixels rather than as fractions of one figure radius. */
  SkPoint px(float deg, float rPx) const {
    const float a = radians(deg);
    return {centre.fX + rPx * std::cos(a), centre.fY + rPx * std::sin(a)};
  }
  /** The unit vector pointing out along @p deg — the direction a tick, a
   *  leader or a radial label runs. */
  SkVector dir(float deg) const {
    const float a = radians(deg);
    return {std::cos(a), std::sin(a)};
  }

  // ---- boxes -------------------------------------------------------------

  /** The square box of radius `rNorm` about the centre — the frame every
   *  `shapes::` generator inscribes itself in. Hand it to `Element::rect`,
   *  inset it, union it. */
  SkRect box(float rNorm = 1.0f) const {
    return util::centred(centre, 2 * rNorm * radius, 2 * rNorm * radius);
  }
  /** `util::disc` at this frame: an Element sized and centred for a
   *  `shapes::circle()`/`sector()`/`arc()` outline at `rNorm`. */
  Element disc(float rNorm = 1.0f) const {
    return util::disc(centre, rNorm * radius);
  }

  // ---- derived frames ----------------------------------------------------

  /** A concentric frame at @p k of this radius, same centre and
   *  conventions — the inner limb, the cell band, the hub. Saves the
   *  four-field restatement, which is where a convention gets silently
   *  dropped. */
  constexpr Frame scaled(float k) const {
    return {centre, radius * k, zero, sense, originDeg};
  }
  /** The same frame about a different centre — a satellite figure that
   *  inherits the plate's angle convention. */
  constexpr Frame about(SkPoint c) const {
    return {c, radius, zero, sense, originDeg};
  }
  /** The same frame with its zero turned by @p deg IN THIS FRAME'S SENSE.
   *  Composes: `f.turned(4.5f).turned(-4.5f) == f`. */
  constexpr Frame turned(float deg) const {
    return {centre, radius, zero, sense,
            originDeg + (sense == Sense::CW ? deg : -deg)};
  }
};

// ---------------------------------------------------------------------------
// Grid — the unit map.

/** Author in the artefact's own units; multiply once.
 *
 *  A value rather than a `float g(float)` for two reasons. It needs three
 *  things — scale, origin and snap — because an artefact's box is rarely
 *  at the canvas origin and a pixel-art plate wants its positions on a
 *  pitch. And more than one grid has to be alive at once: a plate at a 4 px
 *  geometry pitch carrying a readout on a 2.5 px text pitch is ordinary,
 *  and a free function cannot do that without a second name.
 *
 *      const kit::Grid geo{.scale = 4.0f}, type{.scale = 2.5f};
 *      box().rect(geo.rect(12, 8, 40, 16))
 *           .child(text(u8"HIT", ts).at({type.x(13), type.y(9)}));
 *
 *  Note `s()` takes no origin and `x()`/`y()` do. A width is not a
 *  position; adding the origin to one is the bug this split prevents. */
struct Grid {
  /** Canvas px per artefact unit. */
  float scale = 1.0f;
  /** Where artefact (0, 0) lands on the canvas. */
  SkPoint origin{0, 0};
  /** Snap the RESULT to a multiple of this many canvas px (0 = off). This
   *  is a canvas-px pitch, not a unit count: snapping to the grid's own
   *  pitch and snapping to the device pixel are different values. */
  float snap = 0.0f;

  bool operator==(const Grid &) const = default;

  /** Rounds half away from zero, like `std::round`, but CONSTEXPR — which
   *  `std::round` is not before C++23. That is why it is hand-rolled: a
   *  unit map typically feeds `constexpr` canvas constants (a canvas width
   *  declared as so many artefact units), and a helper that cannot run at
   *  compile time cannot be used for those. */
  constexpr float snapped(float v) const {
    if (!(snap > 0))
      return v;
    const float q = v / snap;
    return snap * (float)(long long)(q + (q < 0 ? -0.5f : 0.5f));
  }
  /** A LENGTH in artefact units → px. */
  constexpr float s(float units) const { return snapped(units * scale); }
  /** An X position. */
  constexpr float x(float units) const {
    return snapped(origin.fX + units * scale);
  }
  /** A Y position. */
  constexpr float y(float units) const {
    return snapped(origin.fY + units * scale);
  }
  constexpr SkPoint at(SkPoint units) const { return {x(units.fX), y(units.fY)}; }
  SkRect rect(float ux, float uy, float uw, float uh) const {
    return SkRect::MakeXYWH(x(ux), y(uy), s(uw), s(uh));
  }
  /** The artefact-unit rect as canvas px, corner-by-corner — so a snapped
   *  grid keeps both edges on the grid rather than only the near one. */
  SkRect rect(const SkRect &units) const {
    return SkRect::MakeLTRB(x(units.fLeft), y(units.fTop), x(units.fRight),
                            y(units.fBottom));
  }
  /** A polyline in artefact units → canvas px. One scale for both axes:
   *  for the anisotropic case build two Grids and read `x` from one and
   *  `y` from the other. */
  std::vector<SkPoint> map(const std::vector<SkPoint> &units) const {
    std::vector<SkPoint> out;
    out.reserve(units.size());
    for (const SkPoint &p : units)
      out.push_back(at(p));
    return out;
  }
  /** The affine matrix, for handing a whole SkPath through in one go. NOT
   *  snapped — a matrix cannot round per-point, and pretending otherwise
   *  is how a "snapped" plate ends up half on the grid. */
  SkMatrix matrix() const {
    return SkMatrix::Translate(origin.fX, origin.fY).preScale(scale, scale);
  }
  /** A grid at @p k of this one's scale, same origin and snap — the nested
   *  unit system (a plate at 4 px/unit carrying a readout at 2.5). */
  constexpr Grid scaled(float k) const { return {scale * k, origin, snap}; }
};

} // namespace sigil::compose::kit
