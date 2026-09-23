#pragma once

/** @file
 * Internal to the kernel — matching the rules of the sheets in force
 * at a node against that node: where a node stands among its siblings,
 * the chain of sheets in force, and the rules that matched in the
 * order they fold.
 */

#include <sigilcompose/core/StyleSheet.h>

#include <cstdint>
#include <string_view>
#include <vector>

#include "SheetBody.h"

namespace sigil::compose::detail {

struct Instance;

/** WHERE A NODE STANDS AMONG ITS SIBLINGS, counted once when its
 *  parent's children are resolved: by position, and by ROLE, which is
 *  what a type is here. A node with no role has no type, so `typeCount`
 *  is zero on it and no of-type pseudo-class matches it. */
struct SiblingPlace {
  int index = 0;      ///< 0-based position among the parent's children
  int count = 1;      ///< how many children the parent has
  int typeIndex = 0;  ///< 0-based position among the siblings of its role
  int typeCount = 0;  ///< how many of the parent's children share that role
};

/** ONE SHEET IN FORCE, and the node that applied it. That node is the
 *  ROOT OF EVERYTHING THE SHEET SEES: every compound of every selector
 *  of the sheet must match it or an element below it, so nothing above
 *  it and nothing beside it can satisfy any part of one of its rules. */
struct ScopedSheet {
  const StyleSheet* sheet = nullptr;
  const Instance* scope = nullptr;
};

/** THE SHEETS IN FORCE AT A NODE, in the order they were applied down
 *  the tree: every sheet an ancestor applied, then the ones this node
 *  applies. Built from the root down, so a LATER entry is applied by a
 *  nearer node or applied later at the same node — which is exactly the
 *  order the cascade breaks a tie of equal weight in. */
using SheetChain = std::vector<ScopedSheet>;

/** ONE RULE THAT MATCHED A NODE, and everything the fold order reads:
 *  the weight of the ALTERNATIVE that matched, where its sheet stands
 *  in the chain, and where the rule stands in that sheet. */
struct MatchedRule {
  const Rule* rule = nullptr;
  Specificity weight;
  int chainOrder = 0;
  int sourceOrder = 0;
};

/** THE NAMES THE `:has()` ARGUMENTS IN FORCE TEST FOR, each given one
 *  bit of a node's summary. Built afresh every cascade pass, from the
 *  sheets that use `:has()` as the pass meets them; bits are only ever
 *  added within a pass, so a summary taken earlier in it stays right for
 *  the names it covers. A name past the sixty-fourth has no bit, and a
 *  test that needs it searches rather than trusting a summary. */
class HasNames {
 public:
  /** Gives every name @p sheet's `:has()` arguments test for a bit;
   *  whether any name was new. */
  bool add(const SheetBody& sheet);
  /** The bit of @p name, or none. */
  [[nodiscard]] uint64_t bitOf(bool styleClass, std::string_view name) const;
  /** Forgets every name, for the cascade pass numbered @p pass. */
  void begin(uint32_t pass) {
    m_names.clear();
    m_pass = pass;
  }
  /** The pass a summary must be stamped with to be read. */
  [[nodiscard]] uint32_t pass() const { return m_pass; }

 private:
  std::vector<HasName> m_names;
  uint32_t m_pass = 0;
};

/** WHAT A NODE'S SUBTREE HOLDS, for `:has()`: one bit per name of @p
 *  names, over the node itself, its children and everything under it,
 *  written bottom up over @p root's subtree and stamped with @p pass. */
void summariseForHas(Instance& root, const HasNames& names, uint32_t pass);

/** Writes every child of @p parent its place among its siblings. Runs
 *  once per parent per cascade pass, so a structural pseudo-class is a
 *  lookup rather than a walk. */
void indexSiblings(Instance& parent);

/** The tree's root stands alone: the only child of nothing, and the
 *  only one of its role where it carries one. */
void indexRoot(Instance& root);

/** Whether any rule of @p chain names the style class @p name in its
 *  selector, matched or not — which is what makes the name a
 *  registered one rather than a misprint. */
[[nodiscard]] bool namesStyleClass(const SheetChain& chain,
                                   std::string_view name);

/** The rules of @p chain that speak about @p inst, WEAKEST FIRST, so
 *  folding them in order leaves the strongest standing: by weight, then
 *  by the nearer and later sheet, then by the later rule. Each sheet is
 *  matched inside the subtree of the node that applied it, and no
 *  further; only the rules filed under @p inst's classes, its role or
 *  any element are tried. @p names reads the `:has()` summaries. */
[[nodiscard]] std::vector<MatchedRule> matchRules(const SheetChain& chain,
                                                  const Instance& inst,
                                                  const HasNames& names);

}  // namespace sigil::compose::detail
