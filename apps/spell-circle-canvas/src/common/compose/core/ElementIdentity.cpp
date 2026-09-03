/** @file
 * Element's identity and retained behaviour — the key, whether the node
 * answers a hit, the cache policy and bake scale, the node transition,
 * the child stagger, and the children themselves.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic

#include <algorithm>
#include <set>

#include "ComposeInternal.h"

namespace sigil::compose {

using detail::Kind;

Element& Element::hitTestable(bool enabled) {
  m_node->hitTestable = enabled;
  return *this;
}

Element& Element::key(std::string_view k) {
  // A slot's NAME is its key — one field, two spellings — so this call
  // RENAMES the mount, and renderSlot() on the original name then finds
  // nothing and leaves the slot laying out at zero on its content axis.
  // renderSlot warns about the same trap from the other side; this warning
  // fires where the caller still has BOTH names in hand, which is what
  // makes it actionable.
  if (m_node->kind == Kind::Slot && !m_node->key.empty() && m_node->key != k) {
    static std::set<std::string> warned;  // once per rename, not per frame
    if (warned.insert(m_node->key + "->" + std::string(k)).second)
      SkDebugf(
          "[compose] .key(\"%.*s\") on slot(\"%s\") RENAMES the slot: "
          "renderSlot(\"%s\") will no longer find it and the mount will "
          "lay out at zero on its content axis. A slot is named once, "
          "by slot().\n",
          (int)k.size(), k.data(), m_node->key.c_str(), m_node->key.c_str());
  }
  m_node->key = std::string(k);
  return *this;
}

Element& Element::cache(Cache c) {
  m_node->cacheMode = c;
  return *this;
}

Element& Element::bakeScale(float factor) {
  m_node->bakeScale = std::clamp(factor, 0.1f, 1.0f);
  return *this;
}

Element& Element::transition(motion::Transition t) {
  m_node->nodeTransition = std::move(t);
  return *this;
}

Element& Element::staggerChildren(std::chrono::milliseconds each,
                                  motion::Spread::From from) {
  detail::FxData& fx = m_node->fxData.ensure();
  fx.staggerChildrenMs = (float)each.count();
  fx.staggerFrom = from;
  return *this;
}

Element& Element::child(Element e) {
  m_node->children.push_back(std::move(e));
  return *this;
}

}  // namespace sigil::compose
