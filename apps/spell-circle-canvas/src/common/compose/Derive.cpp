// Derive phase: content whose input is RESOLVED geometry — text exclusions
// (flowAround) plumbed into SigilWeave, and connectors routed between two
// keyed nodes' resolved bounds. Runs after the first layout pass; a changed
// exclusion asks for a bounded second pass (forward-only law, cycle-guarded).

#include "ComposeRuntime.h"

#include <include/core/SkPathBuilder.h>

namespace sigil::compose {

using namespace detail;

/** The derive pass: content whose input is resolved geometry. Returns true
 *  when a text exclusion changed (second layout pass needed). */
bool Composer::Impl::resolveDerived(Instance &inst) {
  bool relayout = false;
  const ElementNode &node = *inst.desc;

  if (!node.flowAroundKeys.empty() && inst.paragraph) {
    std::vector<SkRect> exclusions;
    const SkRect own = absoluteRect(inst);
    for (const std::string &key : node.flowAroundKeys) {
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

  if (!node.connectFrom.empty() && !node.connectTo.empty()) {
    auto fromIt = byKey.find(node.connectFrom);
    auto toIt = byKey.find(node.connectTo);
    if (fromIt != byKey.end() && toIt != byKey.end()) {
      SkRect own = absoluteRect(inst);
      SkRect from = absoluteRect(*fromIt->second);
      SkRect to = absoluteRect(*toIt->second);
      from.offset(-own.left(), -own.top());
      to.offset(-own.left(), -own.top());
      if (from != inst.connectorFrom || to != inst.connectorTo) {
        inst.connectorFrom = from;
        inst.connectorTo = to;
        if (node.router) {
          inst.connectorPath = node.router(from, to);
        } else {
          SkPathBuilder b;
          b.moveTo(from.centerX(), from.centerY());
          b.lineTo(to.centerX(), to.centerY());
          inst.connectorPath = b.detach();
        }
        inst.markPaintDirtyUp();
      }
    }
  }

  for (auto &child : inst.children)
    relayout |= resolveDerived(*child);
  return relayout;
}

} // namespace sigil::compose
