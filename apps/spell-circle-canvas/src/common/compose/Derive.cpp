// Derive phase: content whose input is RESOLVED geometry — text exclusions
// (flowAround) plumbed into SigilWeave, and connectors routed between two
// keyed nodes' resolved bounds. Runs after the first layout pass; a changed
// exclusion asks for a bounded second pass (forward-only law, cycle-guarded).

#include "ComposeRuntime.h"

#include <include/core/SkContourMeasure.h> // the connector's terminal gap
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>

#include <algorithm>

namespace sigil::compose {

using namespace detail;

namespace {

/** Routed elements hit near their PATH, not their layout box (an inset(0)
 *  rail must not eclipse the scene): expand the route by a ±6px tolerance
 *  once at derive time; Query.cpp tests containment against it. */
SkPath expandForHit(const SkPath &route) {
  SkPaint p;
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(12.0f);
  p.setStrokeCap(SkPaint::kRound_Cap);
  return skpathutils::FillPathWithPaint(route, p);
}

} // namespace

/** The derive pass over the EDGE STORE's flat lists (rebuilt with the key
 *  index each render): flowAround text nodes first, then routed nodes, both
 *  in tree order — no tree recursion. Returns true when a text exclusion
 *  changed (second layout pass needed). */
bool Composer::Impl::resolveDerived() {
  bool relayout = false;
  for (Instance *inst : flowInstances)
    relayout |= deriveFlow(*inst);
  for (Instance *inst : routedInstances)
    deriveRoute(*inst);
  return relayout;
}

bool Composer::Impl::deriveFlow(Instance &inst) {
  bool relayout = false;
  const DeriveData *derive = &*inst.desc->deriveData;

  if (inst.paragraph) {
    std::vector<SkRect> exclusions;
    const SkRect own = absoluteRect(inst);
    for (const std::string &key : derive->flowAroundKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end())
        continue;
      // Cycle guard: the target must not be this node or a descendant.
      bool cyclic = false;
      for (Instance *p = it->second; p; p = p->parent)
        if (p == &inst) { cyclic = true; break; }
      if (cyclic)
        continue;
      SkRect target = absoluteRect(*it->second);
      target.offset(-own.left(), -own.top());
      exclusions.push_back(target);
    }
    if (exclusions != inst.exclusionsLocal) {
      inst.exclusionsLocal = std::move(exclusions);
      inst.contentRev++;
      if (inst.yoga) // positioned text re-measures via positionedRect
        YGNodeMarkDirty(inst.yoga);
      inst.markPaintDirtyUp();
      relayout = true;
    }
  }

  return relayout;
}

namespace {

/** The shape a laid-out instance actually occupies, in its OWN space —
 *  the same answer the painter builds, so a borrowed spine and the
 *  element it was borrowed from can never disagree. */
SkPath resolvedShapeOf(Instance &inst) {
  const ElementNode &node = *inst.desc;
  const SkRect rect = inst.owner->instanceRect(inst);
  const SkSize size{rect.width(), rect.height()};
  if (node.deriveData && !inst.connectorPath.isEmpty())
    return inst.connectorPath;
  if (node.shapeFn)
    return node.shapeFn(size);
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(size.width(), size.height()));
  return b.detach();
}

/** The forward-only law, as the other derive borrows spell it: the target
 *  must not be this node or anything under it. Borrowing a DESCENDANT's
 *  geometry is the cycle — the child's box depends on this node's, so the
 *  shape would feed itself. (deriveFlow and the rail path carry the same
 *  three lines; this is the third copy of the SAME rule, kept together
 *  with them rather than invented differently.) */
bool borrowIsCyclic(const Instance &inst, Instance *target) {
  for (Instance *p = target; p; p = p->parent)
    if (p == &inst)
      return true;
  return false;
}

} // namespace

void Composer::Impl::deriveRoute(Instance &inst) {
  const DeriveData *derive = &*inst.desc->deriveData;

  // band(around(key)): the spine is another element's resolved shape,
  // moved into this node's local space.
  if (!derive->bandAround.empty()) {
    SkPath spine;
    auto it = byKey.find(derive->bandAround);
    if (it != byKey.end() && !borrowIsCyclic(inst, it->second)) {
      const SkRect own = absoluteRect(inst);
      const SkRect target = absoluteRect(*it->second);
      spine = resolvedShapeOf(*it->second)
                  .makeTransform(SkMatrix::Translate(
                      target.left() - own.left(), target.top() - own.top()));
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
    for (const std::string &key : derive->borrowedPathKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end() || borrowIsCyclic(inst, it->second))
        continue;
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
    for (const std::string &key : derive->spanFitKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end() || borrowIsCyclic(inst, it->second))
        continue;
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
        // Terminal gap — Anchor::gap's spelling on the connector door
        // (§10): pull each END of the routed path back along itself,
        // clamped like the rail's pullIn so a short wire keeps a run.
        // Applied to the ROUTE rather than to the rects so it works for
        // any router, straight or orthogonal or arc.
        if (derive->connectorGap > 0 && !inst.connectorPath.isEmpty()) {
          SkPathBuilder trimmed;
          SkContourMeasureIter iter(inst.connectorPath, false);
          bool touched = false;
          while (sk_sp<SkContourMeasure> contour = iter.next()) {
            const float len = contour->length();
            if (len <= 0)
              continue;
            if (contour->isClosed()) { // no terminals to pull back
              (void)contour->getSegment(0, len, &trimmed, true);
              continue;
            }
            const float pull = std::min(derive->connectorGap, len * 0.45f);
            (void)contour->getSegment(pull, len - pull, &trimmed, true);
            touched = true;
          }
          if (touched)
            inst.connectorPath = trimmed.detach();
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
    for (const Anchor &anchor : derive->railAnchors) {
      auto it = byKey.find(anchor.nodeKey);
      if (it == byKey.end()) {
        resolvedAll = false;
        break;
      }
      bool cyclic = false; // the rail must not thread itself
      for (Instance *p = it->second; p; p = p->parent)
        if (p == &inst) { cyclic = true; break; }
      if (cyclic) {
        resolvedAll = false;
        break;
      }
      const SkRect target = absoluteRect(*it->second);
      pts.push_back({target.left() + target.width() * anchor.norm.x() -
                         own.left(),
                     target.top() + target.height() * anchor.norm.y() -
                         own.top()});
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
      auto pullIn = [](SkPoint &end, const SkPoint &next, float gap) {
        if (gap <= 0)
          return;
        SkVector d = next - end;
        const float len = d.length();
        if (len < 1e-3f)
          return;
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
          SkPathBuilder b; // default: the straight polyline
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

} // namespace sigil::compose
