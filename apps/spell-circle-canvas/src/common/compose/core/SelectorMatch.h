#pragma once

/** @file
 * Internal to the kernel — matching the rules of the sheets in force
 * at a node against that node: where a node stands among its siblings,
 * the chain of sheets in force, and the rules that matched in the
 * order they fold.
 */

#include <sigilcompose/core/StyleSheet.h>

#include <string_view>
#include <vector>

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
 *  further. */
[[nodiscard]] std::vector<MatchedRule> matchRules(const SheetChain& chain,
                                                  const Instance& inst);

}  // namespace sigil::compose::detail
