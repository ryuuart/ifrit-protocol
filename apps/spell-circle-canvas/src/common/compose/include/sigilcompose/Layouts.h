#pragma once

/** @file
 * SigilCompose layout schemes — free-form placement over the kernel's
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
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Shapes.h"

#include <include/core/SkContourMeasure.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::compose::layouts {

namespace detail {
inline SkRect centeredAt(SkPoint center, SkSize size) {
  return SkRect::MakeXYWH(center.x() - size.width() / 2,
                          center.y() - size.height() / 2, size.width(),
                          size.height());
}
} // namespace detail

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

  std::vector<SkRect> place(const LayoutInput &in) const {
    const size_t n = in.childSizes.size();
    std::vector<SkRect> rects(n);
    if (n == 0)
      return rects;
    const float cx = in.container.width() / 2;
    const float cy = in.container.height() / 2;
    auto frac = [&](size_t i) {
      return i < radiusAt.size() ? radiusAt[i] : radiusFraction;
    };
    // A full circle spaces n children evenly (endpoint excluded); a
    // partial sweep includes both endpoints.
    const bool full = std::abs(std::abs(sweepDeg) - 360.0f) < 1e-3f;
    const float step =
        n <= 1 ? 0.0f : sweepDeg / (full ? (float)n : (float)(n - 1));
    for (size_t i = 0; i < n; ++i) {
      const float a = (startDeg + step * (float)i) * SK_FloatPI / 180.0f;
      const float rx = cx * frac(i), ry = cy * frac(i);
      rects[i] = detail::centeredAt(
          {cx + rx * std::cos(a), cy + ry * std::sin(a)}, in.childSizes[i]);
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

  std::vector<SkRect> place(const LayoutInput &in) const {
    const size_t n = in.childSizes.size();
    std::vector<SkRect> rects(n);
    if (n == 0 || !path)
      return rects;
    const SkPath resolved = path(in.container);
    SkContourMeasureIter iter(resolved, false);
    sk_sp<SkContourMeasure> contour = iter.next();
    if (!contour)
      return rects;
    const float length = contour->length();
    const float d0 = length * startFraction;
    const float d1 = length * endFraction;
    // Closed stretches exclude the duplicate endpoint; open ones hit
    // both ends.
    const bool loop = resolved.isLastContourClosed() &&
                      startFraction == 0.0f && endFraction == 1.0f;
    const float step =
        n <= 1 ? 0.0f : (d1 - d0) / (loop ? (float)n : (float)(n - 1));
    for (size_t i = 0; i < n; ++i) {
      SkPoint pos;
      if (contour->getPosTan(d0 + step * (float)i, &pos, nullptr))
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

  struct Span {
    int col = 0, row = 0, colSpan = 1, rowSpan = 1;
    bool operator==(const Span &) const = default;
  };
  std::vector<Span> spans; // per-child; missing entries auto-flow

  std::vector<SkRect> place(const LayoutInput &in) const {
    const int cols = std::max(columns, 1), rws = std::max(rows, 1);
    const float cw =
        (in.container.width() - gutter * (float)(cols - 1)) / (float)cols;
    const float rh =
        (in.container.height() - gutter * (float)(rws - 1)) / (float)rws;
    std::vector<SkRect> rects(in.childSizes.size());
    for (size_t i = 0; i < in.childSizes.size(); ++i) {
      Span s;
      if (i < spans.size()) {
        s = spans[i];
      } else { // auto-flow the overflow, one module each
        const size_t k = i - spans.size();
        s.col = (int)(k % (size_t)cols);
        s.row = (int)(k / (size_t)cols);
      }
      s.colSpan = std::max(s.colSpan, 1);
      s.rowSpan = std::max(s.rowSpan, 1);
      rects[i] = SkRect::MakeXYWH(
          (cw + gutter) * (float)s.col, (rh + gutter) * (float)s.row,
          cw * (float)s.colSpan + gutter * (float)(s.colSpan - 1),
          rh * (float)s.rowSpan + gutter * (float)(s.rowSpan - 1));
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

  std::vector<SkRect> place(const LayoutInput &in) const {
    const float k = std::tan(skewDeg * SK_FloatPI / 180.0f);
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
    for (SkRect &r : rects)
      r.offset(-minX, 0);
    if (anchor == Anchor::End) {
      // Mirror horizontally: each row's RIGHT edge rides the shear line.
      const float extent =
          in.container.width() > 0 ? in.container.width() : maxRight - minX;
      for (SkRect &r : rects)
        r.offsetTo(extent - r.right(), r.top());
    }
    return rects;
  }
};

struct BaselineGrid {
  /** The editorial baseline rhythm: children stack vertically at x = 0,
   *  and each is shifted DOWN so its anchor — the first TEXT baseline when
   *  the child has one, its bottom edge otherwise — lands exactly on the
   *  next grid line (multiples of `rhythm`, phased by `offset`). A
   *  deterministic quantization applied after Yoga has measured: mixed type
   *  sizes share one vertical rhythm, images and rules bottom-align to it,
   *  and the placement is a pure function of the sizes, so the node caches
   *  like any other static layout. */
  float rhythm = 24.0f;
  float offset = 0.0f; // grid phase
  float gap = 0.0f;    // extra space between children before snapping

  std::vector<SkRect> place(const LayoutInput &in) const {
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
  float jitter = 0.6f; // 0 = regular grid, 1 = up to half a cell off

  std::vector<SkRect> place(const LayoutInput &in) const {
    const size_t n = in.childSizes.size();
    std::vector<SkRect> rects(n);
    if (n == 0)
      return rects;
    const int cols = (int)std::ceil(std::sqrt((float)n));
    const int rows = (int)std::ceil((float)n / (float)cols);
    const float cw = in.container.width() / (float)cols;
    const float ch = in.container.height() / (float)rows;
    for (size_t i = 0; i < n; ++i) {
      const int cx = (int)i % cols, cy = (int)i / cols;
      const float jx =
          shapes::detail::hashNoise(seed, (uint32_t)(i * 2)) * jitter * cw /
          2;
      const float jy =
          shapes::detail::hashNoise(seed, (uint32_t)(i * 2 + 1)) * jitter *
          ch / 2;
      SkPoint center{cw * ((float)cx + 0.5f) + jx,
                     ch * ((float)cy + 0.5f) + jy};
      SkRect r = detail::centeredAt(center, in.childSizes[i]);
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

} // namespace sigil::compose::layouts
