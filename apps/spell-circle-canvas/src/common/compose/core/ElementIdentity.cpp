/** @file
 * A node's identity and retained behaviour — the anchor it hangs off,
 * the key, whether the node answers a hit, the cache policy and bake
 * scale, the node transition, the child stagger, and the children
 * themselves.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <iterator>

#include "ComposeInternal.h"

namespace sigil::compose {

using detail::Kind;

template <class Derived>
Derived& StructureVerbs<Derived>::tether(Tether t) {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  detail::DeriveData& derive = node->deriveData.ensure();
  // LAST-WINS, so the previous tether's reads go with it: a box hangs off
  // exactly one anchor at a time, and one re-tethered would otherwise keep
  // waiting on every node it was ever tethered to. The keys that tether
  // named, and no other Bounds read — a spans gate sized from a node's box
  // is a read this one does not own.
  if (derive.tether) {
    const Tether& was = *derive.tether;
    std::erase_if(derive.reads, [&](const sigil::core::Read& read) {
      if (read.facet != sigil::core::Facet::Bounds) return false;
      if (read.key == was.key) return true;
      for (const Tether& fallback : was.fallbacks)
        if (read.key == fallback.key) return true;
      return false;
    });
  }
  // Every place the box may end up is a node whose finished geometry this
  // one waits for, so every one of them is declared — a fallback that
  // named a node nothing waited for would be resolved a pass late, and
  // the box would flick into it a frame after the anchor moved.
  derive.reads.push_back({t.key, sigil::core::Facet::Bounds});
  for (const Tether& fallback : t.fallbacks)
    derive.reads.push_back({fallback.key, sigil::core::Facet::Bounds});
  derive.tether = std::move(t);
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::hitTestable(bool enabled) {
  declarations()->hitTestable = enabled;
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::key(std::string_view k) {
  // A slot's NAME is its key — one field, two spellings — so this call
  // RENAMES the mount, and renderSlot() on the original name then finds
  // nothing and leaves the slot laying out at zero on its content axis.
  // renderSlot warns about the same trap from the other side; this warning
  // fires where the caller still has BOTH names in hand, which is what
  // makes it actionable.
  detail::ElementNode* node = declarations();
  if (node->kind == Kind::Slot && !node->key.empty() && node->key != k) {
    static thread_local boost::unordered_flat_set<std::string> warned;
    if (warned.insert(node->key + "->" + std::string(k)).second)
      SkDebugf(
          "[compose] .key(\"%.*s\") on slot(\"%s\") RENAMES the slot: "
          "renderSlot(\"%s\") will no longer find it and the mount will "
          "lay out at zero on its content axis. A slot is named once, "
          "by slot().\n",
          (int)k.size(), k.data(), node->key.c_str(), node->key.c_str());
  }
  node->key = std::string(k);
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::cache(Cache c) {
  declarations()->cacheMode = c;
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::cacheScale(float factor) {
  declarations()->bakeScale = std::clamp(factor, 0.1f, 1.0f);
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::transition(motion::Transition t) {
  declarations()->nodeTransition = std::move(t);
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::attributes(Attributes facts) {
  if (facts.empty()) return self();
  declarations()->operatorData.ensure().attributes.merge(facts);
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::operators(std::vector<Operator> list) {
  if (list.empty()) return self();
  std::vector<Operator>& held = declarations()->operatorData.ensure().operators;
  held.insert(held.end(), std::make_move_iterator(list.begin()),
              std::make_move_iterator(list.end()));
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::staggerChildren(
    std::chrono::milliseconds each, motion::Spread::From from) {
  detail::FxData& fx = declarations()->fxData.ensure();
  fx.staggerChildrenMs = (float)each.count();
  fx.staggerFrom = from;
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::children(
    std::initializer_list<Children> runs) {
  for (const Children& run : runs)
    for (const Element& e : run.items) detail::NodeAccess::append(self(), e);
  return self();
}

template class StructureVerbs<Element>;
template class StructureVerbs<Text>;
template class StructureVerbs<Image>;
template class StructureVerbs<Band>;

}  // namespace sigil::compose
