/** @file
 * The routes: connectors and rails laid between keyed nodes' resolved
 * bounds, the band spines and stroke gaps borrowed from other keyed nodes,
 * and the hit path a routed element is tested against.
 */

#include <include/core/SkContourMeasure.h>  // the connector's terminal gap
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <variant>

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
  // Terminal gap, the connector's spelling of the same knob rail anchors
  // carry: pull each END of the routed path back along itself, clamped
  // like the rail's pullIn so a short wire keeps a visible run. Applied to
  // the ROUTE rather than to the rects, so it works for any router —
  // straight, orthogonal, arc.
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

void Composer::Impl::deriveRoute(Instance& inst) {
  const DeriveData* derive = &*inst.description->deriveData;

  // band(around(key)): the spine is another element's resolved shape,
  // moved into this node's local space.
  if (!derive->bandAround.empty()) {
    SkPath spine;
    auto it = byKey.find(derive->bandAround);
    if (it != byKey.end() && !borrowIsCyclic(inst, it->second)) {
      const SkRect own = absoluteRect(inst);
      const SkRect target = absoluteRect(*it->second);
      spine = resolvedShapeOf(*it->second)
                  .makeTransform(SkMatrix::Translate(target.left() - own.left(),
                                                     target.top() - own.top()));
    }
    if (spine != inst.bandSpine) {
      inst.bandSpine = std::move(spine);
      inst.markPaintDirtyUp();
    }
  }

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

  if (!derive->connectFrom.empty() && !derive->connectTo.empty()) {
    auto fromIt = byKey.find(derive->connectFrom);
    auto toIt = byKey.find(derive->connectTo);
    if (fromIt != byKey.end() && toIt != byKey.end()) {
      SkRect own = absoluteRect(inst);
      SkRect from = absoluteRect(*fromIt->second);
      SkRect to = absoluteRect(*toIt->second);
      from.offset(-own.left(), -own.top());
      to.offset(-own.left(), -own.top());
      if (from != inst.connectorFrom || to != inst.connectorTo) {
        inst.connectorFrom = from;
        inst.connectorTo = to;
        inst.connectorPath =
            routeBetween(derive->router, from, to, derive->connectorGap);
        inst.routedHitPath = expandForHit(inst.connectorPath);
        inst.markPaintDirtyUp();
      }
    }
  }

  if (derive->railAnchors.size() >= 2) {
    std::vector<SkPoint> pts;
    pts.reserve(derive->railAnchors.size());
    const SkRect own = absoluteRect(inst);
    bool resolvedAll = true;
    for (const Anchor& anchor : derive->railAnchors) {
      if (const Anchor::FreePoint* free =
              std::get_if<Anchor::FreePoint>(&anchor.where)) {
        pts.push_back(free->point);  // bound to nothing
        continue;
      }
      const Anchor::OnNode& on = std::get<Anchor::OnNode>(anchor.where);
      auto it = byKey.find(on.key);
      if (it == byKey.end()) {
        resolvedAll = false;
        break;
      }
      bool cyclic = false;  // the rail must not thread itself
      for (Instance* p = it->second; p; p = p->parent)
        if (p == &inst) {
          cyclic = true;
          break;
        }
      if (cyclic) {
        resolvedAll = false;
        break;
      }
      const SkRect target = absoluteRect(*it->second);
      pts.push_back({target.left() + target.width() * on.norm.x() - own.left(),
                     target.top() + target.height() * on.norm.y() - own.top()});
    }
    if (!resolvedAll) {
      // An anchor vanished (station unmounted) or went cyclic: the rail
      // goes with it — a stale path pointing at ghosts must not replay.
      if (!inst.connectorPath.isEmpty()) {
        inst.connectorPath.reset();
        inst.routedHitPath.reset();
        inst.railPoints.clear();
        inst.markPaintDirtyUp();
      }
    } else if (pts.size() >= 2) {
      if (pts != inst.railPoints) {
        inst.railPoints = std::move(pts);
        inst.connectorPath =
            routeAlong(derive->railRouter, inst.railPoints,
                       derive->railAnchors.front().gap,
                       derive->railAnchors.back().gap);
        inst.routedHitPath = expandForHit(inst.connectorPath);
        inst.markPaintDirtyUp();
      }
    }
  }
}

}  // namespace sigil::compose
