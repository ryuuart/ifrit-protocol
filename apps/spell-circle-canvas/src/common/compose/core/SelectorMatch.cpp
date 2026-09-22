/** @file
 * Which rules of the sheets in force speak about a node: the sibling
 * index a structural pseudo-class reads, one element against one
 * compound, a chain matched RIGHT TO LEFT with backtracking over the
 * loose combinators and FENCED at the node that applied the sheet, and
 * the order the matches fold in.
 */

#include "SelectorMatch.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Instance.h"
#include "SelectorInternal.h"

namespace sigil::compose::detail {
namespace {

/** The node's ROLE, which is what a type selector selects here, or
 *  empty where it has none. */
std::string_view roleOf(const Instance& inst) {
  const ElementNode& node = *inst.description;
  if (!node.cascadeData || !node.cascadeData->role) return {};
  return node.cascadeData->role->name();
}

bool hasStyleClass(const Instance& inst, std::string_view name) {
  const ElementNode& node = *inst.description;
  if (!node.cascadeData) return false;
  for (const std::string& carried : node.cascadeData->classes)
    if (carried == name) return true;
  return false;
}

/** CSS's an+b over a 1-BASED position: the position is `step * n +
 *  offset` for some n at or above zero. A step of zero names the one
 *  position `offset`. */
bool countReaches(int position, int step, int offset) {
  const int fromOffset = position - offset;
  if (step == 0) return fromOffset == 0;
  return fromOffset % step == 0 && fromOffset / step >= 0;
}

bool matchesComplex(const std::vector<Step>& steps, size_t at,
                    const Instance& inst, const Instance* scope);

/** Whether @p selector speaks about @p inst, reading the WEIGHT of the
 *  alternative that matched — a list weighs what matched it, not its
 *  heaviest alternative. @p scope is the node that applied the sheet
 *  the selector stands in, past which no compound may reach. */
bool matchesSelector(const ElementSelector& selector, const Instance& inst,
                     const Instance* scope, Specificity* weight) {
  bool any = false;
  for (const ElementSelector& alternative :
       SelectorAccess::alternatives(selector)) {
    const std::vector<Step> steps = SelectorAccess::asSteps(alternative);
    if (steps.empty()) continue;
    if (!matchesComplex(steps, steps.size() - 1, inst, scope)) continue;
    any = true;
    if (weight == nullptr) return true;
    *weight = std::max(*weight, alternative.specificity());
  }
  return any;
}

bool matchesAny(const std::vector<ElementSelector>& alternatives,
                const Instance& inst, const Instance* scope) {
  for (const ElementSelector& one : alternatives)
    if (matchesSelector(one, inst, scope, nullptr)) return true;
  return false;
}

/** WHERE A NODE STANDS AMONG ITS SIBLINGS AS ITS SHEET SEES IT. The
 *  node that applied the sheet is the root of the only tree that sheet
 *  sees: its own siblings stand outside it, so it counts as the only
 *  child of nothing, exactly as the tree's root does. */
SiblingPlace placeIn(const Instance& inst, const Instance* scope) {
  if (&inst != scope) return inst.place;
  return SiblingPlace{0, 1, 0, roleOf(inst).empty() ? 0 : 1};
}

/** The 1-based position of @p inst among the siblings matching @p
 *  filter, counted from the first, or zero where @p inst does not match
 *  the filter itself — CSS's `of S`. */
int filteredPosition(const Instance& inst,
                     const std::vector<ElementSelector>& filter,
                     const Instance* scope, int* matchingCount) {
  *matchingCount = 0;
  if (!matchesAny(filter, inst, scope)) return 0;
  if (inst.parent == nullptr || &inst == scope) {
    *matchingCount = 1;
    return 1;
  }
  int position = 0;
  for (const auto& sibling : inst.parent->children) {
    if (!matchesAny(filter, *sibling, scope)) continue;
    ++*matchingCount;
    if (sibling.get() == &inst) position = *matchingCount;
  }
  return position;
}

bool matchesSimple(const Simple& simple, const Instance& inst,
                   const Instance* scope) {
  const SiblingPlace place = placeIn(inst, scope);
  switch (simple.kind) {
    case SimpleKind::Universal:
      return true;
    case SimpleKind::StyleClass:
      return hasStyleClass(inst, simple.name);
    case SimpleKind::Role:
      return roleOf(inst) == simple.name;
    case SimpleKind::FirstChild:
      return place.index == 0;
    case SimpleKind::LastChild:
      return place.index == place.count - 1;
    case SimpleKind::OnlyChild:
      return place.count == 1;
    case SimpleKind::NthChild:
    case SimpleKind::NthLastChild: {
      const bool fromLast = simple.kind == SimpleKind::NthLastChild;
      if (simple.arguments.empty()) {
        const int position =
            fromLast ? place.count - place.index : place.index + 1;
        return countReaches(position, simple.step, simple.offset);
      }
      int matching = 0;
      const int position =
          filteredPosition(inst, simple.arguments, scope, &matching);
      if (position == 0) return false;
      return countReaches(fromLast ? matching - position + 1 : position,
                          simple.step, simple.offset);
    }
    case SimpleKind::FirstOfType:
      return place.typeCount > 0 && place.typeIndex == 0;
    case SimpleKind::LastOfType:
      return place.typeCount > 0 && place.typeIndex == place.typeCount - 1;
    case SimpleKind::OnlyOfType:
      return place.typeCount == 1;
    case SimpleKind::NthOfType:
      return place.typeCount > 0 &&
             countReaches(place.typeIndex + 1, simple.step, simple.offset);
    case SimpleKind::NthLastOfType:
      return place.typeCount > 0 &&
             countReaches(place.typeCount - place.typeIndex, simple.step,
                          simple.offset);
    case SimpleKind::Empty:
      return inst.children.empty();
    case SimpleKind::Root:
      // The root of the tree the sheet sees, which is the node that
      // applied it where that node is not the tree's own root.
      return inst.parent == nullptr || &inst == scope;
    case SimpleKind::Is:
    case SimpleKind::Where:
      return matchesAny(simple.arguments, inst, scope);
    case SimpleKind::Not:
      return !matchesAny(simple.arguments, inst, scope);
  }
  return false;
}

bool matchesCompound(const Compound& compound, const Instance& inst,
                     const Instance* scope) {
  for (const Simple& simple : compound.simples)
    if (!matchesSimple(simple, inst, scope)) return false;
  return true;
}

/** @p steps up to and including @p at against @p inst and its
 *  ancestors, right to left. The loose combinators — a descendant and
 *  a later sibling — try every candidate and BACK TRACK, because a
 *  nearer one matching the compound does not mean the rest of the
 *  chain matches above it. Every walk stops at @p scope, the node that
 *  applied the sheet: a compound satisfied above it or beside it would
 *  be read outside the only subtree that sheet sees. */
bool matchesComplex(const std::vector<Step>& steps, size_t at,
                    const Instance& inst, const Instance* scope) {
  if (!matchesCompound(steps[at].compound, inst, scope)) return false;
  if (at == 0) return true;
  // The applying node has neither a parent nor a sibling inside the
  // subtree, so every combinator leaving it leaves the sheet's reach.
  if (&inst == scope) return false;
  const Instance* const parent = inst.parent;
  switch (steps[at].combinator) {
    case Combinator::Child:
      return parent != nullptr && matchesComplex(steps, at - 1, *parent, scope);
    case Combinator::Descendant:
      for (const Instance* above = parent; above != nullptr;
           above = above->parent) {
        if (matchesComplex(steps, at - 1, *above, scope)) return true;
        if (above == scope) break;
      }
      return false;
    case Combinator::Next:
      if (parent == nullptr || inst.place.index == 0) return false;
      return matchesComplex(steps, at - 1,
                            *parent->children[inst.place.index - 1], scope);
    case Combinator::Sibling:
      if (parent == nullptr) return false;
      for (int before = inst.place.index - 1; before >= 0; --before)
        if (matchesComplex(steps, at - 1, *parent->children[before], scope))
          return true;
      return false;
  }
  return false;
}

}  // namespace

void indexSiblings(Instance& parent) {
  // A node an adding operator attached is not a sibling of the authored
  // children: it is counted among none of them, and answers to no
  // structural pseudo-class itself.
  int count = 0;
  for (const auto& child : parent.children)
    if (!child->description->added()) ++count;
  int at = 0;
  for (const auto& child : parent.children) {
    SiblingPlace& place = child->place;
    if (child->description->added())
      place = SiblingPlace{0, 0, 0, 0};
    else
      place = SiblingPlace{at++, count, 0, 0};
  }
  // Then by role, which is what a type is. Roles are few per parent, so
  // the running tally is a scanned list and a parent whose children name
  // no role allocates nothing.
  struct RoleTally {
    std::string_view name;
    int total = 0;
    int seen = 0;
  };
  std::vector<RoleTally> tallies;
  const int held = (int)parent.children.size();
  for (int at = 0; at < held; ++at) {
    if (parent.children[at]->description->added()) continue;
    const std::string_view role = roleOf(*parent.children[at]);
    if (role.empty()) continue;
    const auto found = std::find_if(
        tallies.begin(), tallies.end(),
        [role](const RoleTally& tally) { return tally.name == role; });
    if (found == tallies.end())
      tallies.push_back(RoleTally{role, 1, 0});
    else
      ++found->total;
  }
  if (tallies.empty()) return;
  for (int at = 0; at < held; ++at) {
    if (parent.children[at]->description->added()) continue;
    const std::string_view role = roleOf(*parent.children[at]);
    if (role.empty()) continue;
    const auto found = std::find_if(
        tallies.begin(), tallies.end(),
        [role](const RoleTally& tally) { return tally.name == role; });
    SiblingPlace& place = parent.children[at]->place;
    place.typeIndex = found->seen++;
    place.typeCount = found->total;
  }
}

void indexRoot(Instance& root) {
  root.place = SiblingPlace{0, 1, 0, roleOf(root).empty() ? 0 : 1};
}

namespace {

/** Whether @p selector names the style class @p name anywhere in it —
 *  in a compound of the chain, or inside the arguments of `:is`,
 *  `:where`, `:not` or an `of` filter. */
bool selectorNamesClass(const ElementSelector& selector,
                        std::string_view name) {
  for (const ElementSelector& alternative :
       SelectorAccess::alternatives(selector))
    for (const Step& step : SelectorAccess::asSteps(alternative))
      for (const Simple& simple : step.compound.simples) {
        if (simple.kind == SimpleKind::StyleClass && simple.name == name)
          return true;
        for (const ElementSelector& argument : simple.arguments)
          if (selectorNamesClass(argument, name)) return true;
      }
  return false;
}

}  // namespace

bool namesStyleClass(const SheetChain& chain, std::string_view name) {
  for (const ScopedSheet& applied : chain)
    for (const Rule& rule : applied.sheet->rules())
      if (selectorNamesClass(rule.selector(), name)) return true;
  return false;
}

std::vector<MatchedRule> matchRules(const SheetChain& chain,
                                    const Instance& inst) {
  std::vector<MatchedRule> matched;
  for (size_t sheetAt = 0; sheetAt < chain.size(); ++sheetAt) {
    const ScopedSheet& applied = chain[sheetAt];
    const std::vector<Rule>& rules = applied.sheet->rules();
    for (size_t ruleAt = 0; ruleAt < rules.size(); ++ruleAt) {
      Specificity weight;
      if (!matchesSelector(rules[ruleAt].selector(), inst, applied.scope,
                           &weight))
        continue;
      matched.push_back(MatchedRule{&rules[ruleAt], weight, (int)sheetAt,
                                    (int)ruleAt});
    }
  }
  // Weakest first: CSS's order, with scope proximity and application
  // order both read off the chain, which was built from the root down.
  std::stable_sort(matched.begin(), matched.end(),
                   [](const MatchedRule& left, const MatchedRule& right) {
                     if (!(left.weight == right.weight))
                       return left.weight < right.weight;
                     if (left.chainOrder != right.chainOrder)
                       return left.chainOrder < right.chainOrder;
                     return left.sourceOrder < right.sourceOrder;
                   });
  return matched;
}

}  // namespace sigil::compose::detail
