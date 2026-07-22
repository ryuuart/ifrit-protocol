#pragma once

/** @file
 * SigilCompose KIT — the figure's own coordinate system, as a value.
 *
 * ## What the kit tier is
 *
 * Everything under `sigilcompose/kit/` sits ON TOP of the library and
 * changes none of it. No new `ElementNode` state, no new reconciler
 * equality, no new paint path — every kit component is a free function or
 * an aggregate over the shipped API. That inverts the risk that governs
 * `<sigilcompose/Layouts.h>` and `EXTRACT.md`: a wrong entry in the core
 * API is permanent, so the bar there is brutal, while a wrong kit
 * component is simply not `#include`d by the next study and costs nothing.
 *
 * The bar here is therefore: **three or more real hand-rolls in the
 * corpus, cited.** Not "would be nice". Two candidates were dropped for
 * failing it; see `kit/README` in the extraction report.
 *
 * ## Frame — five hand-rolls, all five lines, two conventions
 *
 * | site | spelling | 0° at | sense | radius |
 * |---|---|---|---|---|
 * | `sigillum_aemeth.cpp:273` | `P(thDeg, rNorm)` | 12 o'clock | CW | normalised |
 * | `nightingale_coxcomb.cpp:194` | `polar(c, r, bearingDeg)` | 12 o'clock | CW | px |
 * | `chladni_tab1.cpp:167` | `polar(c, r, bearingDeg)` | 12 o'clock | CW | px |
 * | `eva_magi_interior.cpp:1211` | `polar(r, deg)` | 3 o'clock | CW | px |
 * | `chaucer_astrolabe.cpp:391,394` | `eclPoint`/`ringPoint` | 3 o'clock | CW | fixed |
 *
 * Five files, two conventions, and `chladni`'s body is byte-identical to
 * `nightingale`'s. **The convention is the reason a value beats a
 * function**: two of these plates measure clockwise from noon because
 * their sources do (Dee's "procede toward thy right hand"; Nightingale's
 * 1858 coxcomb starts at the top), and three measure from due east because
 * Skia does. Written as a bare helper, that difference is a sign flip a
 * reader has to reverse-engineer at every call site. Written as a `Frame`,
 * it is one field.
 *
 * ### Why this is not `layouts::Radial`, and why Radial is not wrong
 *
 * `Radial` distributes N children **evenly around a container** and hands
 * back rects. That is a placement POLICY and it works at both of its call
 * sites (`ScenesFlourish.h:225`, `ScenesOrganic.h:71`). `Frame` decides
 * nothing: it converts `(angle, radius)` — numbers *you* measured off a
 * reference plate — into a point, an SkRect, or an arc-length fraction.
 * It is the peer of `util::centred`, not of a layout scheme. The two most
 * radial studies in the corpus called `Radial` zero times and hand-wrote
 * this instead, which is the evidence, not an indictment.
 *
 * ### The conversions are the point
 *
 * `sigillum_aemeth.cpp:279` is the scar this header exists to close:
 *
 *     float frac(float thDeg) { return fmod((thDeg - 90) / 360 + 4, 1); }
 *     // θ → the arc-length fraction of shapes::circle(), whose contour
 *     //   starts at due EAST and runs clockwise.
 *
 * A library convention (where `shapes::circle()`'s contour begins) leaking
 * into user arithmetic, in a file that computes angles hundreds of times.
 * `Frame::fraction()` and `Frame::skiaDeg()` are that arithmetic, once,
 * with the convention in the value rather than in a comment.
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
  North, ///< 12 o'clock — every engraved plate in the corpus.
};

/** Which way a frame's angles increase, **as seen on screen**. Screen y is
 *  down, so `CW` is the direction that *looks* clockwise. */
enum class Sense { CW, CCW };

/** A figure's own polar coordinate system: a centre, a radius that `r = 1`
 *  lands on, and the convention flags.
 *
 *  Designated-init on purpose. `gallery/GalleryCore.h:35` shipped
 *  `styleAt(float size, SkColor colour)` and sixteen gallery scenes wrote
 *  their own type helper anyway, because a positional signature cannot
 *  grow. Every aggregate in this kit is `Frame{.centre = …}`.
 *
 *      const kit::Frame fig{.centre = {kRR, kRR}, .radius = kR};  // North/CW
 *      g.child(util::disc(fig.at(126.0f, 0.72f), 6.0f).fill(ink));
 *
 *  28 bytes, trivially copyable, holds no Element and no node state. */
struct Frame {
  SkPoint centre{0, 0};
  /** The px radius that `rNorm = 1` maps to. Authoring in normalised
   *  radius is what lets a plate be re-scaled by editing one number —
   *  `sigillum_aemeth` carries ~60 `constexpr float r…` fractions against
   *  one `kR`. */
  float radius = 1.0f;
  Zero zero = Zero::North;
  Sense sense = Sense::CW;
  /** An extra origin offset in SCREEN degrees (+ = clockwise on screen),
   *  applied after `zero`. Normally 0. It exists because a scanned plate
   *  is rarely square to its own axes — `chevreul_circle.cpp:52` measures
   *  its source as "rotated 3.2° in this scan" — and because an index ring
   *  offset half a division is otherwise a `+ 4.5f` at every call site
   *  (`sigillum_aemeth.cpp:1930`: `th = i * 9.0f - 4.5f`). */
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
   *  **due-east** extreme and advances the way screen-clockwise runs
   *  (`Shapes.h:138`, `Shapes.h:154-176`). The fraction is therefore the
   *  screen angle over 360, wrapped into [0, 1).
   *
   *  **@p baseline is the direction of the PATH, not of this frame, and
   *  the two are independent.** Measured: `shapes::circle(kCCW)` also
   *  starts due east — `startIndex` is 1 either way — and then runs the
   *  other way round, so at f = 0.25 it is at 12 o'clock where the kCW
   *  contour is at 6. A frame whose `sense` is CCW on a baseline that is
   *  still kCW is a perfectly ordinary thing to want (Chevreul's limb
   *  numbers anticlockwise on a clockwise ring), so conflating them puts
   *  every label half a turn out. This defaulted to the frame's own sense
   *  in the first draft and the test caught it.
   *
   *  **Only exact on a circle.** A circle's arc length is proportional to
   *  its angle; an ellipse's is not, so on a non-square box this drifts.
   *  Every ring inscription in the corpus is square (`util::disc`,
   *  `Frame::box()`), which is why the hand-rolled versions got away with
   *  the same approximation — but say it out loud rather than let a label
   *  slide. */
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
   *  `rNorm = 1` is `radius`. This is `sigillum_aemeth.cpp:273`. */
  SkPoint at(float deg, float rNorm = 1.0f) const {
    return px(deg, rNorm * radius);
  }
  /** `(angle, PX radius)` → a point. This is `nightingale_coxcomb.cpp:194`
   *  and `chladni_tab1.cpp:167`, whose plates measure radii in px off the
   *  scan rather than as fractions of one figure radius. */
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
// Grid — the unit map. The OTHER five-line function every study writes.

/** Author in the artefact's own units; multiply once.
 *
 *  Four independent hand-rolls, and the last two are why this is a struct
 *  and not a `float g(float)`:
 *
 *  - `thaumonomicon.cpp:198` `constexpr float g(float)` — scale only.
 *  - `xcom_battlescape.cpp:214` `constexpr float n(float v) { return v * PX; }`
 *    — scale only, `PX = 4`.
 *  - `astral_tome.cpp:326-328` `g`/`gx`/`gy` — scale **plus an origin**,
 *    because the artefact's box is not at the canvas origin.
 *  - `vagrant_story_target.cpp:216-218` `snapG`/`snapX`/`snapY` — scale
 *    plus **snapping**, and that file carries TWO simultaneous grids: a
 *    4 px geometry grid and a 2.5 px text grid.
 *
 *  So the component needs scale, origin and snap, and must allow more than
 *  one grid alive at a time — which a free function in a namespace cannot
 *  do without inventing a second name. As a value:
 *
 *      const kit::Grid geo{.scale = 4.0f}, type{.scale = 2.5f};
 *      box().rect(geo.rect(12, 8, 40, 16))
 *           .child(text(u8"HIT", ts).at({type.x(13), type.y(9)}));
 *
 *  **This is a chore, not a choice** — it converts units you already
 *  measured. It ships with no warning.
 *
 *  Note `s()` takes no origin and `x()`/`y()` do. A width is not a
 *  position; adding the origin to one is the bug this split prevents. */
struct Grid {
  /** Canvas px per artefact unit. */
  float scale = 1.0f;
  /** Where artefact (0, 0) lands on the canvas. */
  SkPoint origin{0, 0};
  /** Snap the RESULT to a multiple of this many canvas px (0 = off). Note
   *  `vagrant_story_target` snaps to its own grid pitch, not to 1. */
  float snap = 0.0f;

  bool operator==(const Grid &) const = default;

  float snapped(float v) const {
    return snap > 0 ? std::round(v / snap) * snap : v;
  }
  /** A LENGTH in artefact units → px. */
  float s(float units) const { return snapped(units * scale); }
  /** An X position. */
  float x(float units) const { return snapped(origin.fX + units * scale); }
  /** A Y position. */
  float y(float units) const { return snapped(origin.fY + units * scale); }
  SkPoint at(SkPoint units) const { return {x(units.fX), y(units.fY)}; }
  SkRect rect(float ux, float uy, float uw, float uh) const {
    return SkRect::MakeXYWH(x(ux), y(uy), s(uw), s(uh));
  }
  /** The artefact-unit rect as canvas px, corner-by-corner — so a snapped
   *  grid keeps both edges on the grid rather than only the near one. */
  SkRect rect(const SkRect &units) const {
    return SkRect::MakeLTRB(x(units.fLeft), y(units.fTop), x(units.fRight),
                            y(units.fBottom));
  }
  /** A polyline in artefact units → canvas px. This is
   *  `thunder_fulu.cpp:547 place(median, origin, sx, sy)` with one scale
   *  where that file has two; for the anisotropic case build two Grids and
   *  read `x` from one and `y` from the other, which is what the two-scale
   *  signature was silently doing anyway. */
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
