#pragma once

/** @file
 * The layout schemes — free-form placement over the kernel's
 * LayoutScheme seam (`layout(scheme)`), for compositions that are not rows
 * and columns. Six schemes live here: `Radial` (a ring or fan), `AlongPath`
 * (arc-length placement on any contour), `ModularGrid` (columns × rows of
 * modules), `Diagonal` (a sheared stack), `BaselineGrid` (a vertical type
 * rhythm) and `Scatter` (seeded jitter).
 *
 *   layout(layouts::Radial{.radiusFraction = 0.8f})
 *       .children(glyphs | std::views::transform(rune));
 *
 * A scheme returns one rect per child from the container size and the
 * children's measured sizes — except `ModularGrid`, which sizes children to
 * their cell span. Placement is pure arithmetic over `LayoutInput`, so the
 * result is deterministic and the node caches like any other static content.
 *
 * A scheme belongs here when the placement is a FUNCTION an author would
 * otherwise write out — a ring, a modular grid, a baseline rhythm. It does
 * not belong here when the placement IS the design decision: a generator
 * that produces arrangements "in the family" of a hand-chosen one produces
 * the single thing the design is not.
 *
 * WHAT A SCHEME MAY NOT SPELL ITSELF. The two arithmetics a placement
 * catalog keeps arriving at — where item i of n falls on a ring, and which
 * cell of a grid of modules it occupies — belong to nothing here and are
 * SigilGeometry's, in `<sigilgeometry/path/Arrange.h>`. `Radial`,
 * `AlongPath`, `ModularGrid` and `Scatter` step through those bodies; the
 * pool fillers of `<sigilcompose/kit/Placers.h>` step through the same
 * ones. A scheme that re-derived a ring here would round its own way, and
 * the same ring drawn two ways would differ by a pixel with nothing in
 * either file to say why. What a scheme owns is the DECISION on top: which
 * radius per child, where the anchor of a box is, what closes the run.
 */

#include <include/core/SkContourMeasure.h>
#include <sigilcompose/kit/Silhouettes.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Numeric.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose::layouts {

namespace detail {
inline SkRect centeredAt(SkPoint center, SkSize size) {
  return SkRect::MakeXYWH(center.x() - size.width() / 2,
                          center.y() - size.height() / 2, size.width(),
                          size.height());
}
}  // namespace detail

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
 *  and the last at startDeg + sweepDeg. */
struct Radial {
  float radiusFraction = 0.8f;
  float startDeg = -90.0f;
  float sweepDeg = 360.0f;
  /** Per-child radius: a fraction per index, overriding `radiusFraction`
   *  where present — an orbit diagram's bands, a skill wheel's tiers. May
   *  be shorter than the child list; the tail falls back to
   *  `radiusFraction`. Participates in equality like every field. */
  std::vector<float> radiusAt;

  std::vector<SkRect> place(const LayoutInput& in) const {
    const size_t n = in.childSizes.size();
    std::vector<SkRect> rects(n);
    if (n == 0) return rects;
    const float cx = in.container.width() / 2;
    const float cy = in.container.height() / 2;
    auto frac = [&](size_t i) {
      return i < radiusAt.size() ? radiusAt[i] : radiusFraction;
    };
    // A full circle spaces n children evenly (endpoint excluded); a
    // partial sweep includes both endpoints. The test is made in degrees,
    // the unit the author stated the sweep in.
    const geometry::arrange::Turn turn =
        std::abs(std::abs(sweepDeg) - 360.0f) < 1e-3f
            ? geometry::arrange::Turn::Closed
            : geometry::arrange::Turn::Open;
    const float start = startDeg * geometry::path::kDegToRad;
    const float sweep = sweepDeg * geometry::path::kDegToRad;
    for (size_t i = 0; i < n; ++i) {
      const float r = frac(i);
      rects[i] = detail::centeredAt(
          geometry::arrange::onRing(i, n, {cx, cy}, {cx * r, cy * r}, start,
                                    sweep, turn),
          in.childSizes[i]);
    }
    return rects;
  }
};

/** Children along an arbitrary contour by arc length. The path is a
 *  generator over the container size — any shapes:: outline or your own —
 *  and children center on evenly spaced samples of the
 *  [startFraction, endFraction] stretch of it.
 *
 *  ONLY THE FIRST CONTOUR IS USED. A generator returning several subpaths
 *  places children on the first one and silently ignores the rest; give
 *  each contour its own layout node if you want children on all of them.
 *
 *  A closed contour walked end to end excludes the duplicate endpoint, so
 *  the last child does not land on the first. Any other stretch, and any
 *  open contour, includes both ends. */
struct AlongPath {
  std::function<SkPath(SkSize)> path;
  float startFraction = 0.0f;
  float endFraction = 1.0f;

  std::vector<SkRect> place(const LayoutInput& in) const {
    const size_t n = in.childSizes.size();
    std::vector<SkRect> rects(n);
    if (n == 0 || !path) return rects;
    const SkPath resolved = path(in.container);
    SkContourMeasureIter iter(resolved, false);
    sk_sp<SkContourMeasure> contour = iter.next();
    if (!contour) return rects;
    const float length = contour->length();
    const float d0 = length * startFraction;
    const float d1 = length * endFraction;
    // Closed stretches exclude the duplicate endpoint; open ones hit
    // both ends. Arc length divides among n children exactly as an angle
    // does around a ring, so the same run arithmetic answers both.
    const bool loop = resolved.isLastContourClosed() && startFraction == 0.0f &&
                      endFraction == 1.0f;
    const geometry::arrange::Turn turn =
        loop ? geometry::arrange::Turn::Closed : geometry::arrange::Turn::Open;
    for (size_t i = 0; i < n; ++i) {
      SkPoint pos;
      if (contour->getPosTan(geometry::arrange::along(d0, d1 - d0, i, n, turn),
                             &pos, nullptr))
        rects[i] = detail::centeredAt(pos, in.childSizes[i]);
    }
    return rects;
  }
};

/** The modular grid: columns × rows of equal modules separated by gutters,
 *  each child occupying a cell SPAN (col, row, colSpan, rowSpan). Spans are
 *  given per child in declaration order, and children beyond the span list
 *  auto-flow one cell each, left→right then top→bottom. Children are SIZED
 *  to their span rather than to their content. Pair with BaselineGrid
 *  inside text cells to put the type on a shared vertical rhythm.
 *
 *  TWO SHARP EDGES, both silent:
 *
 *  - Auto-flow counts from cell 0, not from the end of the explicit spans.
 *    Give spans for the first three children of eight and the fourth child
 *    starts again at (0, 0), landing on top of whatever was explicitly
 *    placed there. Either span every child or span none of them.
 *  - `col` and `row` are NOT clamped to the grid. A value at or past
 *    `columns`/`rows`, or a negative one, places the child outside the
 *    container, where it is drawn or clipped according to the parent's own
 *    settings rather than reported as an error. */
struct ModularGrid {
  int columns = 4;
  int rows = 6;
  float gutter = 12.0f;

  /** One child's cell and how many cells it covers. Children without a
   *  span auto-flow into the next free cell. */
  struct Span {
    int col = 0, row = 0, colSpan = 1, rowSpan = 1;
    bool operator==(const Span&) const = default;
  };
  std::vector<Span> spans;  // per-child; missing entries auto-flow

  std::vector<SkRect> place(const LayoutInput& in) const {
    const int cols = std::max(columns, 1);
    const SkSize gap{gutter, gutter};
    const SkSize module = geometry::arrange::moduleSize(in.container, cols,
                                                        std::max(rows, 1), gap);
    std::vector<SkRect> rects(in.childSizes.size());
    for (size_t i = 0; i < in.childSizes.size(); ++i) {
      Span s;
      if (i < spans.size()) {
        s = spans[i];
      } else {  // auto-flow the overflow, one module each
        const geometry::arrange::Cell cell =
            geometry::arrange::cellAt(i - spans.size(), cols);
        s.col = cell.column;
        s.row = cell.row;
      }
      rects[i] = geometry::arrange::cellRect({s.col, s.row}, module, gap,
                                             {0, 0}, s.colSpan, s.rowSpan);
    }
    return rects;
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
  float offset = 0.0f;   // grid phase
  float gap = 0.0f;      // extra space between children before snapping

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

/** Seeded chaotic placement: children scatter over the container on a
 *  jittered grid — deterministic per seed (same seed, same chaos,
 *  fully cacheable), never escaping the container. */
struct Scatter {
  uint32_t seed = 1;
  float jitter = 0.6f;  // 0 = regular grid, 1 = up to half a cell off

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
      SkRect r =
          detail::centeredAt({cell.fX + jx, cell.fY + jy}, in.childSizes[i]);
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
