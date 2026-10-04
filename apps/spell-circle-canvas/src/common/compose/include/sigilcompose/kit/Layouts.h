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

#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilgeometry/path/Numeric.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose::layouts {

/** @p rect moved the least distance that puts it inside a box of @p extent
 *  at the origin, so a placement that nudges or scatters never clips a
 *  child away. A rect larger than the box keeps its top-left inside. */
inline geometry::path::Rect heldInside(const geometry::path::Rect& rect,
                                       glm::vec2 extent) {
  const glm::vec2 shift{
      std::max(0.0f, -rect.min.x) - std::max(0.0f, rect.max.x - extent.x),
      std::max(0.0f, -rect.min.y) - std::max(0.0f, rect.max.y - extent.y)};
  return {rect.min + shift, rect.max + shift};
}

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

  void arrange(Arrangement& arrangement) const;
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
 *  The path is a `Shape` over the container's box — any `shapes::`
 *  generator, or a callable — and a container whose children hold still
 *  is memoised above it. */
struct AlongPath {
  Shape path;
  float startFraction = 0.0f;
  float endFraction = 1.0f;
  bool facing = false;

  void arrange(Arrangement& arrangement) const;
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
      const glm::vec2 nudge{dx, dy};
      const geometry::path::Rect moved{child.rect.min + nudge,
                                       child.rect.max + nudge};
      // Held inside the box, so a nudge never clips a child away.
      child.place(heldInside(moved, arrangement.box.size()));
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

  void arrange(Arrangement& arrangement) const {
    const float k = std::tan(skewDeg * geometry::path::kDegToRad);
    float y = 0.0f, minX = 0.0f, maxRight = 0.0f;
    for (Arrangement::Child& child : arrangement.children) {
      const float x = k * y;
      child.place(geometry::path::Rect::of({x, y}, child.size));
      minX = std::min(minX, x);
      maxRight = std::max(maxRight, child.rect.max.x);
      y += child.size.y + gap;
    }
    for (Arrangement::Child& child : arrangement.children) {
      child.rect.min.x -= minX;
      child.rect.max.x -= minX;
    }
    if (anchor == Anchor::End) {
      // Mirror horizontally: each row's right edge rides the shear line.
      const float extent = arrangement.box.width() > 0 ? arrangement.box.width()
                                                       : maxRight - minX;
      for (Arrangement::Child& child : arrangement.children) {
        const float shift = extent - child.rect.max.x - child.rect.min.x;
        child.rect.min.x += shift;
        child.rect.max.x += shift;
      }
    }
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

  void arrange(Arrangement& arrangement) const {
    const float step = std::max(rhythm, 1.0f);
    float flowY = 0.0f;
    for (Arrangement::Child& child : arrangement.children) {
      const float anchor =
          std::isnan(child.baseline) ? child.size.y : child.baseline;
      // Snap the anchor to the next grid line at or below its flow spot.
      const float line =
          offset + step * std::ceil((flowY + anchor - offset) / step - 1e-4f);
      const float top = line - anchor;
      child.place(geometry::path::Rect::of({0, top}, child.size));
      flowY = top + child.size.y + gap;
    }
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

  void arrange(Arrangement& arrangement) const;
};

}  // namespace sigil::compose::layouts
