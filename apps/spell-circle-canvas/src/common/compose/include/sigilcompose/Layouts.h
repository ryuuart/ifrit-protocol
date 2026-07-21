#pragma once

/** @file
 * SigilCompose layout schemes — free-form placement over the kernel's
 * LayoutScheme seam (layout(scheme)), for compositions that aren't
 * rows and columns: rings, paths, seeded scatter. Children keep their
 * measured sizes; the scheme only decides where they sit. Everything
 * here is ~20 lines of user-writable code — shipped because rings and
 * paths are this codebase's native idiom (spell circles, ring labels).
 *
 *   layout(layouts::Radial{.radiusFraction = 0.8f})
 *       .children(glyphs | std::views::transform(rune));
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

/** Children on a ring — the spell-circle idiom. Child i centers at
 *  startDeg + i·(sweepDeg/n), at radiusFraction of the container's
 *  half-extent (per axis: an oblong container gives an ellipse). A
 *  partial sweep makes fans and arcs. */
struct Radial {
  float radiusFraction = 0.8f;
  float startDeg = -90.0f;
  float sweepDeg = 360.0f;

  std::vector<SkRect> place(const LayoutInput &in) const {
    const size_t n = in.childSizes.size();
    std::vector<SkRect> rects(n);
    if (n == 0)
      return rects;
    const float cx = in.container.width() / 2;
    const float cy = in.container.height() / 2;
    const float rx = cx * radiusFraction, ry = cy * radiusFraction;
    // A full circle spaces n children evenly (endpoint excluded); a
    // partial sweep includes both endpoints.
    const bool full = std::abs(std::abs(sweepDeg) - 360.0f) < 1e-3f;
    const float step =
        n <= 1 ? 0.0f : sweepDeg / (full ? (float)n : (float)(n - 1));
    for (size_t i = 0; i < n; ++i) {
      const float a = (startDeg + step * (float)i) * SK_FloatPI / 180.0f;
      rects[i] = detail::centeredAt(
          {cx + rx * std::cos(a), cy + ry * std::sin(a)}, in.childSizes[i]);
    }
    return rects;
  }
};

/** Children along an arbitrary contour by arc length. The path is a
 *  generator over the container size — any shapes:: outline or your
 *  own — and children center on evenly spaced samples of the
 *  [startFraction, endFraction] stretch of its first contour. */
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

/** Seeded chaotic placement: children scatter over the container on a
 *  jittered grid — deterministic per seed (same seed, same chaos,
 *  fully cacheable), never escaping the container. */
/** The Müller-Brockmann MODULAR grid (Grid Systems in Graphic Design):
 *  columns × rows of modules separated by gutters; each child occupies a
 *  cell SPAN (col, row, colSpan, rowSpan) — spans are given per child in
 *  declaration order, and children beyond the span list auto-flow one cell
 *  each, left→right, top→bottom. Children are SIZED to their span (the
 *  grid disciplines the composition; content answers to the module). Pair
 *  with BaselineGrid inside text cells for the full Swiss discipline. */
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

/** The ATLUS diagonal (REFERENCES.md §1): children stack downward while
 *  marching along the slanted axis — each child's x tracks the same shear
 *  line skewX(skewDeg) leans its verticals to, so a column of skewed cards
 *  reads as ONE oblique battery, not a staircase of accidents. Negative
 *  skewDeg (the P3R ≈ −12°) marches rows leftward as they descend; the
 *  whole run is normalized so nothing lands at negative x. Pair with
 *  `.skewX(skewDeg)` on the children themselves. */
struct Diagonal {
  float skewDeg = -12.0f;
  float gap = 8.0f;

  std::vector<SkRect> place(const LayoutInput &in) const {
    const float k = std::tan(skewDeg * SK_FloatPI / 180.0f);
    std::vector<SkRect> rects(in.childSizes.size());
    float y = 0.0f, minX = 0.0f;
    for (size_t i = 0; i < in.childSizes.size(); ++i) {
      const float x = k * y;
      rects[i] = SkRect::MakeXYWH(x, y, in.childSizes[i].width(),
                                  in.childSizes[i].height());
      minX = std::min(minX, x);
      y += in.childSizes[i].height() + gap;
    }
    for (SkRect &r : rects)
      r.offset(-minX, 0);
    return rects;
  }
};

struct BaselineGrid {
  /** The editorial baseline rhythm (Müller-Brockmann): children stack
   *  vertically at x = 0, and each is shifted DOWN so its anchor — the
   *  first TEXT baseline when the child has one, its bottom edge otherwise
   *  — lands exactly on the next grid line (multiples of `rhythm`, phased
   *  by `offset`). A deterministic post-Yoga quantization: mixed type sizes
   *  share one vertical rhythm, images/rules bottom-align to it, and the
   *  result is cache-stable like any other layout. */
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
