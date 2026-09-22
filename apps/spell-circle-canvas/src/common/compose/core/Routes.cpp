/** @file
 * Where a route runs and where it ends — between two rects, and along a
 * run of stops — and the geometry one node borrows off another's settled
 * answer: the paths a decoration reads and the boxes a stroke gap is
 * sized from.
 */

#include <include/core/SkContourMeasure.h>  // the terminal gap
#include <include/core/SkPathBuilder.h>

#include <algorithm>

#include "ComposeRuntime.h"
#include "DeriveInternal.h"

namespace sigil::compose {

using namespace detail;

SkPath routeBetween(const Router& router, const SkRect& from, const SkRect& to,
                    float gap) {
  SkPath path;
  if (router) {
    path = router(from, to);
  } else {
    SkPathBuilder b;
    b.moveTo(from.centerX(), from.centerY());
    b.lineTo(to.centerX(), to.centerY());
    path = b.detach();
  }
  // The terminal gap, the same knob a run of stops carries on its ends:
  // pull each END of the routed path back along itself, clamped so a short
  // wire keeps a visible run. Applied to the ROUTE rather than to the
  // rects, so it works for any router — straight, orthogonal, arc.
  if (gap > 0 && !path.isEmpty()) {
    SkPathBuilder trimmed;
    SkContourMeasureIter iter(path, false);
    bool touched = false;
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      if (len <= 0) continue;
      if (contour->isClosed()) {  // no terminals to pull back
        (void)contour->getSegment(0, len, &trimmed, true);
        continue;
      }
      const float pull = std::min(gap, len * 0.45f);
      (void)contour->getSegment(pull, len - pull, &trimmed, true);
      touched = true;
    }
    if (touched) path = trimmed.detach();
  }
  return path;
}

SkPath routeAlong(const RailRouter& router, std::span<const SkPoint> stops,
                  float gapStart, float gapEnd) {
  if (stops.size() < 2) return SkPath();
  std::vector<SkPoint> pts(stops.begin(), stops.end());
  // Terminal gaps: pull the run's ends back along their own segments,
  // clamped so a short segment keeps a visible run (and a two-point run
  // pulled from both ends cannot invert).
  const auto pullIn = [](SkPoint& end, const SkPoint& next, float gap) {
    if (gap <= 0) return;
    SkVector d = next - end;
    const float len = d.length();
    if (len < 1e-3f) return;
    const float pull = std::min(gap, len * 0.45f);
    d.scale(pull / len);
    end += d;
  };
  pullIn(pts.front(), pts[1], gapStart);
  pullIn(pts.back(), pts[pts.size() - 2], gapEnd);
  if (router) return router(pts);
  SkPathBuilder b;  // default: the straight polyline
  b.moveTo(pts.front());
  for (size_t i = 1; i < pts.size(); ++i) b.lineTo(pts[i]);
  return b.detach();
}

void Composer::Impl::deriveBorrows(Instance& inst) {
  const DeriveData* derive = &*inst.description->deriveData;

  // strand::from(key): the keyed PATHS a decoration borrows, in this
  // node's local space. Same walk, same cycle guard as every other borrow.
  if (!derive->borrowedPathKeys.empty()) {
    std::vector<std::pair<std::string, SkPath>> paths;
    paths.reserve(derive->borrowedPathKeys.size());
    const SkRect own = absoluteRect(inst);
    for (const std::string& key : derive->borrowedPathKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end() || borrowIsCyclic(inst, it->second)) continue;
      const SkRect target = absoluteRect(*it->second);
      paths.emplace_back(
          key, resolvedShapeOf(*it->second)
                   .makeTransform(SkMatrix::Translate(
                       target.left() - own.left(), target.top() - own.top())));
    }
    if (paths != inst.borrowedPaths) {
      inst.borrowedPaths = std::move(paths);
      inst.markPaintDirtyUp();
    }
  }

  // spans::fit(key): the boxes a stroke pass sizes its gap from.
  if (!derive->spanFitKeys.empty()) {
    std::vector<std::pair<std::string, SkRect>> rects;
    rects.reserve(derive->spanFitKeys.size());
    const SkRect own = absoluteRect(inst);
    for (const std::string& key : derive->spanFitKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end() || borrowIsCyclic(inst, it->second)) continue;
      SkRect target = absoluteRect(*it->second);
      target.offset(-own.left(), -own.top());
      rects.emplace_back(key, target);
    }
    if (rects != inst.spanFitRects) {
      inst.spanFitRects = std::move(rects);
      inst.markPaintDirtyUp();
    }
  }
}

}  // namespace sigil::compose
