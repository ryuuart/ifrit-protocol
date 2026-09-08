/** @file
 * The routes: connectors and rails laid between keyed nodes' resolved
 * bounds, the band spines and stroke gaps borrowed from other keyed nodes,
 * and the hit path a routed element is tested against.
 */

#include <include/core/SkContourMeasure.h>  // the connector's terminal gap
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>

#include "ComposeRuntime.h"
#include "DeriveInternal.h"

namespace sigil::compose {

using namespace detail;

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
        if (derive->router) {
          inst.connectorPath = derive->router(from, to);
        } else {
          SkPathBuilder b;
          b.moveTo(from.centerX(), from.centerY());
          b.lineTo(to.centerX(), to.centerY());
          inst.connectorPath = b.detach();
        }
        // Terminal gap, the connector's spelling of the same knob rail
        // anchors carry: pull each END of the routed path back along
        // itself, clamped like the rail's pullIn below so a short wire
        // keeps a visible run. Applied to the ROUTE rather than to the
        // rects, so it works for any router — straight, orthogonal, arc.
        if (derive->connectorGap > 0 && !inst.connectorPath.isEmpty()) {
          SkPathBuilder trimmed;
          SkContourMeasureIter iter(inst.connectorPath, false);
          bool touched = false;
          while (sk_sp<SkContourMeasure> contour = iter.next()) {
            const float len = contour->length();
            if (len <= 0) continue;
            if (contour->isClosed()) {  // no terminals to pull back
              (void)contour->getSegment(0, len, &trimmed, true);
              continue;
            }
            const float pull = std::min(derive->connectorGap, len * 0.45f);
            (void)contour->getSegment(pull, len - pull, &trimmed, true);
            touched = true;
          }
          if (touched) inst.connectorPath = trimmed.detach();
        }
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
      if (anchor.nodeKey.empty()) {  // a free waypoint, bound to nothing
        pts.push_back(anchor.point);
        continue;
      }
      auto it = byKey.find(anchor.nodeKey);
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
      pts.push_back(
          {target.left() + target.width() * anchor.norm.x() - own.left(),
           target.top() + target.height() * anchor.norm.y() - own.top()});
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
      // Terminal gaps: pull the rail's ends back along their segments,
      // clamped so short segments keep a visible run (and a two-point rail
      // pulled from both ends can't invert).
      auto pullIn = [](SkPoint& end, const SkPoint& next, float gap) {
        if (gap <= 0) return;
        SkVector d = next - end;
        const float len = d.length();
        if (len < 1e-3f) return;
        const float pull = std::min(gap, len * 0.45f);
        d.scale(pull / len);
        end += d;
      };
      pullIn(pts.front(), pts[1], derive->railAnchors.front().gap);
      pullIn(pts.back(), pts[pts.size() - 2], derive->railAnchors.back().gap);

      if (pts != inst.railPoints) {
        inst.railPoints = std::move(pts);
        if (derive->railRouter) {
          inst.connectorPath = derive->railRouter(inst.railPoints);
        } else {
          SkPathBuilder b;  // default: the straight polyline
          b.moveTo(inst.railPoints.front());
          for (size_t i = 1; i < inst.railPoints.size(); ++i)
            b.lineTo(inst.railPoints[i]);
          inst.connectorPath = b.detach();
        }
        inst.routedHitPath = expandForHit(inst.connectorPath);
        inst.markPaintDirtyUp();
      }
    }
  }
}

}  // namespace sigil::compose
