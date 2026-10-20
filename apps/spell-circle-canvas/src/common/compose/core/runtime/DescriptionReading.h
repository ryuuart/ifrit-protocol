#pragma once

/** @file
 * Internal to the kernel — how the reconciler reads a description: its key,
 * whether two are equal, whether it reconciles its own children and the
 * range of children it does, the description a child element carries, its
 * memo and what the memo produces.
 */

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

/** THE RECONCILER'S READING OF A DESCRIPTION: the half of the composer's
 *  host operations that reads a description and holds no state, so the
 *  reconciler reaches each one through the composer by its own name. */
struct DescriptionReading {
  using Description = std::shared_ptr<ElementNode>;
  static const std::string& keyOf(const Description& description) {
    return description->key;
  }
  static bool equal(const Description& a, const Description& b) {
    return propertiesEqual(*a, *b);
  }
  /** Slot content is owned by renderSlot(), not the description. */
  static bool reconcilesChildren(const Description& description) {
    return description->kind != Kind::Slot;
  }
  /** THE CHILDREN A NODE RECONCILES: the description's, then what the
   *  adding operators kept under the node — one range, so one pass
   *  mounts, patches and retires both by key. Where a child the list no
   *  longer names is still leaving, the range is one list instead, with
   *  that child's own description standing where it stood, so the pass
   *  keeps it in its place. */
  struct ChildRange {
    const std::vector<Element>* authored = nullptr;
    const std::vector<Element>* added = nullptr;
    std::optional<std::vector<Element>> merged;
    struct Iterator {
      const ChildRange* range = nullptr;
      size_t at = 0;
      const Element& operator*() const {
        if (range->merged) return (*range->merged)[at];
        const size_t authored = range->authored->size();
        return at < authored ? (*range->authored)[at]
                             : (*range->added)[at - authored];
      }
      Iterator& operator++() {
        ++at;
        return *this;
      }
      bool operator!=(const Iterator& other) const { return at != other.at; }
    };
    size_t size() const {
      return merged ? merged->size() : authored->size() + added->size();
    }
    Iterator begin() const { return {this, 0}; }
    Iterator end() const { return {this, size()}; }
  };
  static const Description& descriptionOf(const Element& child) {
    return child.node();
  }
  static const MemoData* memoOf(const Description& description) {
    return description->memoData ? &*description->memoData : nullptr;
  }
  static Description produce(const MemoData& memo) {
    return memo.invoke(memo.properties).node();
  }
};

}  // namespace sigil::compose::detail
