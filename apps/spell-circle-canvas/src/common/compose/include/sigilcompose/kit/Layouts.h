#pragma once

/** @file
 * @ingroup compose-kit
 *
 * Free-form child placement: rings, paths, sheared stacks, baseline
 * rhythms and seeded jitter. Each is an arranging operator or a
 * placement scheme applied with `Element::operators` or `layout()`,
 * placing measured child boxes within a container. Track-based
 * arrangements use Grid.
 */

#include <include/core/SkContourMeasure.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilgeometry/path/Numeric.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose::layouts {

/** Children on a ring. Child i centers at startDeg + i·(sweepDeg/n), at
 *  `radiusFraction` of the container's half-extent — applied per axis, so
 *  an oblong container gives an ellipse rather than a circle. A partial
 *  sweep makes fans and arcs.
 *
 *  Angles are degrees, clockwise on screen (y grows downward), and the
 *  default −90 puts the first child at twelve o'clock.
 *
 *  A FULL-TURN sweep excludes the endpoint — n children divide the circle
 *  into n equal steps, and the last does not land on top of the first. A
 *  PARTIAL sweep includes both ends, so the first child sits at startDeg
 *  and the last at startDeg + sweepDeg.
 *
 *  BY A FACT RATHER THAN BY INDEX: name a `lane`, and each child stands
 *  at the fraction its own fact under that name makes of `divisions` —
 *  the child stating `3` under "hour" at three of twelve, whatever
 *  order the children were written in and whichever hours are missing.
 *  `divisions` of zero is the number of children. A child stating no
 *  such fact stands at its index.
 *
 *  `facing` turns each child to stand along its radius, its top toward
 *  the rim — a numeral on a dial, a petal — as a paint-only turn over
 *  whatever rotation the child states for itself. */
struct Radial {
  float radiusFraction = 0.8f;
  float startDeg = -90.0f;
  float sweepDeg = 360.0f;
  /** Per-child radius: a fraction per index, overriding `radiusFraction`
   *  where present — an orbit diagram's bands, a skill wheel's tiers. May
   *  be shorter than the child list; the tail falls back to
   *  `radiusFraction`. */
  std::vector<float> radiusAt;
  std::string lane;
  float divisions = 0.0f;
  bool facing = false;
  bool operator==(const Radial&) const = default;

  void arrange(Arrangement& arrangement) const {
    const size_t n = arrangement.children.size();
    if (n == 0) return;
    const float cx = arrangement.box.width() / 2;
    const float cy = arrangement.box.height() / 2;
    auto frac = [&](size_t i) {
      return i < radiusAt.size() ? radiusAt[i] : radiusFraction;
    };
    // A full circle spaces n children evenly (endpoint excluded); a
    // partial sweep includes both endpoints. The test is made in degrees,
    // the unit the author stated the sweep in.
    const bool closed = std::abs(std::abs(sweepDeg) - 360.0f) < 1e-3f;
    const geometry::arrange::Turn turn = closed
                                             ? geometry::arrange::Turn::Closed
                                             : geometry::arrange::Turn::Open;
    const float start = startDeg * geometry::path::kDegToRad;
    const float sweep = sweepDeg * geometry::path::kDegToRad;
    const float count = divisions > 0 ? divisions : (float)n;
    for (size_t i = 0; i < n; ++i) {
      Arrangement::Child& child = arrangement.children[i];
      const float r = frac(i);
      const std::optional<float> fact =
          lane.empty() ? std::nullopt : child.number(lane);
      float angle;
      if (fact) {
        // A fact's fraction of the sweep: a closed ring divides into
        // `count` steps, an open fan into `count - 1` so the last fact
        // reaches the far end exactly as the last index does.
        const float steps = closed ? count : std::max(count - 1.0f, 1.0f);
        angle = start + sweep * (*fact / steps);
      } else {
        angle = geometry::arrange::along(start, sweep, i, n, turn);
      }
      child.centreAt(
          geometry::arrange::onEllipse({cx, cy}, {cx * r, cy * r}, angle));
      // Standing along the radius: a child at twelve o'clock (−90°) is
      // upright, one at three o'clock turned a quarter clockwise.
      if (facing) child.turn(angle * geometry::path::kRadToDeg + 90.0f);
    }
  }
};

/** Children along an arbitrary contour by arc length. The path is a
 *  generator over the container size — any shapes:: outline or your own,
 *  naming the size or leaving it unnamed — and children center on evenly
 *  spaced samples of the [startFraction, endFraction] stretch of it.
 *
 *  ONLY THE FIRST CONTOUR IS USED. A generator returning several subpaths
 *  places children on the first one and silently ignores the rest; give
 *  each contour its own layout node if you want children on all of them.
 *
 *  A closed contour walked end to end excludes the duplicate endpoint, so
 *  the last child does not land on the first. Any other stretch, and any
 *  open contour, includes both ends.
 *
 *  `facing` turns each child to lie along the contour where it stands —
 *  its x axis on the tangent, so a word set along a curve reads along
 *  it — as a paint-only turn over the child's own rotation.
 *
 *  The path is a callable and carries no equality, so an `AlongPath` is
 *  the escape hatch that never prunes; a container whose children hold
 *  still is memoised above it. */
struct AlongPath {
  core::Callable<SkPath(SkSize)> path;
  float startFraction = 0.0f;
  float endFraction = 1.0f;
  bool facing = false;

  void arrange(Arrangement& arrangement) const {
    const size_t n = arrangement.children.size();
    if (n == 0 || !path) return;
    const SkPath resolved =
        path({arrangement.box.width(), arrangement.box.height()});
    SkContourMeasureIter iter(resolved, false);
    sk_sp<SkContourMeasure> contour = iter.next();
    if (!contour) return;
    const float length = contour->length();
    const float d0 = length * startFraction;
    const float d1 = length * endFraction;
    // Closed stretches exclude the duplicate endpoint; open ones hit
    // both ends. Arc length divides among n children exactly as an angle
    // does around a ring, so the same run arithmetic answers both.
    const bool loop =
        contour->isClosed() && startFraction == 0.0f && endFraction == 1.0f;
    const geometry::arrange::Turn turn =
        loop ? geometry::arrange::Turn::Closed : geometry::arrange::Turn::Open;
    for (size_t i = 0; i < n; ++i) {
      SkPoint pos;
      SkVector tangent;
      if (!contour->getPosTan(geometry::arrange::along(d0, d1 - d0, i, n, turn),
                              &pos, &tangent))
        continue;
      arrangement.children[i].centreAt(pos);
      if (facing)
        arrangement.children[i].turn(std::atan2(tangent.y(), tangent.x()) *
                                     geometry::path::kRadToDeg);
    }
  }
};

/** EVERY CHILD NUDGED FROM WHERE IT STANDS by a seeded offset of up to
 *  `amount` px on each axis — deterministic per seed, so the same chaos
 *  every frame and a cacheable one — and held inside the box. It moves
 *  what the operator before it in the list placed, so a ring, a path
 *  or a grid is jittered by listing this after it. */
struct Jitter {
  uint32_t seed = 1;
  float amount = 8.0f;
  bool operator==(const Jitter&) const = default;

  void arrange(Arrangement& arrangement) const {
    for (size_t i = 0; i < arrangement.children.size(); ++i) {
      Arrangement::Child& child = arrangement.children[i];
      const float dx = core::noise::hash(seed, (uint32_t)(i * 2)) * amount;
      const float dy = core::noise::hash(seed, (uint32_t)(i * 2 + 1)) * amount;
      SkRect moved = child.rect.makeOffset(dx, dy);
      // Held inside the box, so a nudge never clips a child away.
      moved.offset(std::max(0.0f, -moved.left()) -
                       std::max(0.0f, moved.right() - arrangement.box.width()),
                   std::max(0.0f, -moved.top()) -
                       std::max(0.0f, moved.bottom() - arrangement.box.height()));
      child.place(moved);
    }
  }
};

/** The sheared stack: children stack downward while marching along a
 *  slanted axis. Each child's x tracks the same shear line that
 *  `skewX(skewDeg)` leans a node's verticals to, so a column of skewed
 *  cards reads as one oblique block rather than a staircase. Negative
 *  skewDeg marches rows leftward as they descend; the whole run is
 *  normalized so nothing lands at negative x. Pair with `.skewX(skewDeg)`
 *  on the children themselves, or the boxes stay upright while their
 *  positions slant. */
struct Diagonal {
  float skewDeg = -12.0f;
  float gap = 8.0f;
  /** Start (default) marches LEFT edges along the shear line; End mirrors
   *  the battery so RIGHT edges ride it (right-anchored menus) — aligned
   *  to the container's right when it has a width, else to the run's own
   *  extent. */
  enum class Anchor : uint8_t { Start, End } anchor = Anchor::Start;

  std::vector<SkRect> place(const LayoutInput& in) const {
    const float k = std::tan(skewDeg * geometry::path::kDegToRad);
    std::vector<SkRect> rects(in.childSizes.size());
    float y = 0.0f, minX = 0.0f, maxRight = 0.0f;
    for (size_t i = 0; i < in.childSizes.size(); ++i) {
      const float x = k * y;
      rects[i] = SkRect::MakeXYWH(x, y, in.childSizes[i].width(),
                                  in.childSizes[i].height());
      minX = std::min(minX, x);
      maxRight = std::max(maxRight, rects[i].right());
      y += in.childSizes[i].height() + gap;
    }
    for (SkRect& r : rects) r.offset(-minX, 0);
    if (anchor == Anchor::End) {
      // Mirror horizontally: each row's RIGHT edge rides the shear line.
      const float extent =
          in.container.width() > 0 ? in.container.width() : maxRight - minX;
      for (SkRect& r : rects) r.offsetTo(extent - r.right(), r.top());
    }
    return rects;
  }
};

/** The editorial baseline rhythm: children stack vertically at x = 0,
 *  and each is shifted DOWN so its anchor — the first TEXT baseline when
 *  the child has one, its bottom edge otherwise — lands exactly on the
 *  next grid line (multiples of `rhythm`, phased by `offset`). A
 *  deterministic quantization applied after Yoga has measured: mixed type
 *  sizes share one vertical rhythm, images and rules bottom-align to it,
 *  and the placement is a pure function of the sizes, so the node caches
 *  like any other static layout. */
struct BaselineGrid {
  float rhythm = 24.0f;  ///< distance between grid lines
  float offset = 0.0f;   ///< grid phase
  float gap = 0.0f;      ///< extra space between children before snapping

  std::vector<SkRect> place(const LayoutInput& in) const {
    std::vector<SkRect> rects(in.childSizes.size());
    const float step = std::max(rhythm, 1.0f);
    float flowY = 0.0f;
    for (size_t i = 0; i < in.childSizes.size(); ++i) {
      const SkSize size = in.childSizes[i];
      const float anchor =
          (i < in.childBaselines.size() && !std::isnan(in.childBaselines[i]))
              ? in.childBaselines[i]
              : size.height();
      // Snap the anchor to the next grid line at or below its flow spot.
      const float line =
          offset + step * std::ceil((flowY + anchor - offset) / step - 1e-4f);
      const float top = line - anchor;
      rects[i] = SkRect::MakeXYWH(0, top, size.width(), size.height());
      flowY = top + size.height() + gap;
    }
    return rects;
  }
};

/** Seeded chaotic placement: children sit on a JITTERED GRID over the
 *  container — deterministic per seed (same seed, same chaos, fully
 *  cacheable), never escaping the container.
 *
 *  Named for the grid it deviates from, not for the scattering: a brush
 *  that instances art along a path is `brush::Scatter`, and the two answer
 *  different questions. */
struct Jittered {
  uint32_t seed = 1;
  float jitter = 0.6f;  ///< 0 = regular grid, 1 = up to half a cell off

  std::vector<SkRect> place(const LayoutInput& in) const {
    const size_t n = in.childSizes.size();
    std::vector<SkRect> rects(n);
    if (n == 0) return rects;
    const int cols = (int)std::ceil(std::sqrt((float)n));
    const int rows = (int)std::ceil((float)n / (float)cols);
    // The regular grid the jitter is measured against is the same grid a
    // modular layout lays down: gapless modules filling the container.
    const SkSize module =
        geometry::arrange::moduleSize(in.container, cols, rows, {0, 0});
    for (size_t i = 0; i < n; ++i) {
      const float jx = core::noise::hash(seed, (uint32_t)(i * 2)) * jitter *
                       module.width() / 2;
      const float jy = core::noise::hash(seed, (uint32_t)(i * 2 + 1)) * jitter *
                       module.height() / 2;
      const SkPoint cell = geometry::arrange::cellRect(
                               geometry::arrange::cellAt(i, cols), module)
                               .center();
      SkRect r = geometry::path::centred({cell.fX + jx, cell.fY + jy},
                                         in.childSizes[i]);
      // Clamp into the container so jitter never clips children away.
      r.offset(std::max(0.0f, -r.left()) -
                   std::max(0.0f, r.right() - in.container.width()),
               std::max(0.0f, -r.top()) -
                   std::max(0.0f, r.bottom() - in.container.height()));
      rects[i] = r;
    }
    return rects;
  }
};

}  // namespace sigil::compose::layouts
