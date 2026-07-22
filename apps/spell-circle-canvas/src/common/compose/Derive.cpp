// Derive phase: content whose input is RESOLVED geometry — text exclusions
// (flowAround) plumbed into SigilWeave, and connectors routed between two
// keyed nodes' resolved bounds. Runs after the first layout pass; a changed
// exclusion asks for a bounded second pass (forward-only law, cycle-guarded).

#include "ComposeRuntime.h"

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

/** The derive pass: content whose input is resolved geometry. Returns true
 *  when a text exclusion changed (second layout pass needed). */
bool Composer::Impl::resolveDerived(Instance &inst) {
  bool relayout = false;
  const ElementNode &node = *inst.desc;
  const DeriveData *derive = node.deriveData ? &*node.deriveData : nullptr;

  if (derive && !derive->flowAroundKeys.empty() && inst.paragraph) {
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
      YGNodeMarkDirty(inst.yoga);
      inst.markPaintDirtyUp();
      relayout = true;
    }
  }

  if (derive && !derive->connectFrom.empty() && !derive->connectTo.empty()) {
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
        inst.routedHitPath = expandForHit(inst.connectorPath);
        inst.markPaintDirtyUp();
      }
    }
  }

  if (derive && derive->railAnchors.size() >= 2) {
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

  for (auto &child : inst.children)
    relayout |= resolveDerived(*child);
  return relayout;
}

} // namespace sigil::compose
