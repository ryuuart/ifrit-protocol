/** @file
 * Where a route runs and where it ends — between two rects, and along a
 * run of stops — and the geometry one node borrows off another's settled
 * answer: the paths a decoration reads and the boxes a stroke gap is
 * sized from.
 */

#include <sigilgeometry/path/Contour.h>  // the terminal gap
#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/path/Skia.h>

#include <glm/geometric.hpp>

#include <algorithm>

#include "ComposeRuntime.h"
#include "DeriveInternal.h"

namespace sigil::compose {

using namespace detail;

geometry::path::Outline routeBetween(const Router& router,
                                     const geometry::path::Rect& from,
                                     const geometry::path::Rect& to,
                                     float gap) {
  SkPath path;
  if (router) {
    path = geometry::path::toSk(router(from, to));
  } else {
    SkPathBuilder b;
    b.moveTo(geometry::path::toSk(from.centre()));
    b.lineTo(geometry::path::toSk(to.centre()));
    path = b.detach();
  }
  // The terminal gap, the same knob a run of stops carries on its ends:
  // pull each END of the routed path back along itself, clamped so a short
  // wire keeps a visible run. Applied to the ROUTE rather than to the
  // rects, so it works for any router — straight, orthogonal, arc.
  if (gap > 0 && !path.isEmpty()) {
    SkPathBuilder trimmed;
    bool touched = false;
    for (const geometry::path::Contour& contour :
         geometry::path::Contour::of(path)) {
      const float len = contour.length();
      if (contour.closed()) {  // no terminals to pull back
        contour.appendSegment(trimmed, 0, len);
        continue;
      }
      const float pull = std::min(gap, len * 0.45f);
      contour.appendSegment(trimmed, pull, len - pull);
      touched = true;
    }
    if (touched) path = trimmed.detach();
  }
  return geometry::path::fromSk(std::move(path));
}

geometry::path::Outline routeAlong(const RailRouter& router,
                                   std::span<const glm::vec2> stops,
                                   float gapStart, float gapEnd) {
  if (stops.size() < 2) return {};
  std::vector<glm::vec2> points(stops.begin(), stops.end());
  // Terminal gaps: pull the run's ends back along their own segments,
  // clamped so a short segment keeps a visible run (and a two-point run
  // pulled from both ends cannot invert).
  const auto pullIn = [](glm::vec2& end, glm::vec2 next, float gap) {
    if (gap <= 0) return;
    const glm::vec2 direction = next - end;
    const float length = glm::length(direction);
    if (length < 1e-3f) return;
    const float pull = std::min(gap, length * 0.45f);
    end += direction * (pull / length);
  };
  pullIn(points.front(), points[1], gapStart);
  pullIn(points.back(), points[points.size() - 2], gapEnd);
  if (router) return router(points);
  SkPathBuilder b;  // default: the straight polyline
  b.moveTo(geometry::path::toSk(points.front()));
  for (size_t i = 1; i < points.size(); ++i)
    b.lineTo(geometry::path::toSk(points[i]));
  return geometry::path::fromSk(b.detach());
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
