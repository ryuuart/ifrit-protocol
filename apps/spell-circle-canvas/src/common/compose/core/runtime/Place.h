#pragma once
/** @file
 * Where a retained node stands, spelled for a report an author reads: the
 * key it answers to, or the path of child indices that leads to it from
 * its nearest keyed ancestor.
 */

#include <string>
#include <vector>

#include "Instance.h"

namespace sigil::compose::detail {

/** The key a retained node answers to: its memo shell's when it was
 *  described through one, else its description's. */
inline const std::string& keyOf(const Instance& inst) {
  if (inst.memoShell && !inst.memoShell->key.empty())
    return inst.memoShell->key;
  return inst.description->key;
}

/** Where a node stands, for a report an author reads: its key when it has
 *  one, else the child indices that lead to it from its nearest keyed
 *  ancestor — or from the root, when no ancestor carries a key. */
inline std::string placeOf(const Instance& inst) {
  if (!keyOf(inst).empty()) return "\"" + keyOf(inst) + "\"";
  std::vector<size_t> indices;
  const Instance* node = &inst;
  while (node->parent != nullptr && keyOf(*node).empty()) {
    const auto& siblings = node->parent->children;
    for (size_t index = 0; index < siblings.size(); ++index)
      if (siblings[index].get() == node) {
        indices.push_back(index);
        break;
      }
    node = node->parent;
  }
  std::string place = keyOf(*node).empty() ? std::string("the root")
                                           : "\"" + keyOf(*node) + "\"";
  for (auto index = indices.rbegin(); index != indices.rend(); ++index)
    place += " > child " + std::to_string(*index);
  return place;
}

}  // namespace sigil::compose::detail
