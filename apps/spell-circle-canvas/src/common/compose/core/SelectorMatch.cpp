/** @file
 * Which rules of the sheets in force speak about a node: the sibling
 * index a structural pseudo-class reads, the summary a `:has()` reads,
 * one element against one compound, a chain matched RIGHT TO LEFT with
 * backtracking over the loose combinators and FENCED at the node that
 * applied the sheet, a relative chain searched DOWN from the element a
 * `:has()` stands on, and the order the matches fold in.
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
  return node.cascadeData->role->name;
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

/** Where @p inst stands in its parent's child list, or -1 where it is
 *  counted among no siblings — an element an adding operator attached,
 *  or a named run matched as a virtual child. The index is checked
 *  against the list rather than trusted, so a place left from a pass
 *  that saw a longer list reads nothing past its end. */
int slotOf(const Instance& inst) {
  if (inst.parent == nullptr || inst.place.count == 0) return -1;
  const auto& siblings = inst.parent->children;
  const int slot = inst.place.slot;
  if (slot < 0 || slot >= (int)siblings.size() || siblings[slot].get() != &inst)
    return -1;
  return slot;
}

/** The authored siblings before @p inst, nearest first, handed to @p
 *  visit until it answers true; whether it did. */
template <typename Visit>
bool anySiblingBefore(const Instance& inst, bool nearestOnly, Visit visit) {
  const int slot = slotOf(inst);
  if (slot < 0) return false;
  const auto& siblings = inst.parent->children;
  for (int before = slot - 1; before >= 0; --before) {
    const Instance& sibling = *siblings[before];
    if (sibling.description->added()) continue;
    if (visit(sibling)) return true;
    if (nearestOnly) return false;
  }
  return false;
}

/** WHAT A MATCH READS BESIDE THE TREE: the node that applied the sheet
 *  the selector stands in, past which no compound may reach, and the
 *  names a `:has()` summary is taken over. */
struct MatchContext {
  const Instance* scope = nullptr;
  const HasNames* names = nullptr;
};

bool matchesComplex(const std::vector<Step>& steps, size_t at,
                    const Instance& inst, const MatchContext& context);

/** Whether @p selector speaks about @p inst, reading the WEIGHT of the
 *  alternative that matched — a list weighs what matched it, not its
 *  heaviest alternative. A RELATIVE alternative, one opening with a
 *  relation to a `:has()` anchor, reaches here only as an argument of
 *  `:has()`, which reads it itself; the guard keeps it inert anywhere
 *  else. */
bool matchesSelector(const ElementSelector& selector, const Instance& inst,
                     const MatchContext& context, Specificity* weight) {
  bool any = false;
  for (const ElementSelector& alternative :
       SelectorAccess::alternatives(selector)) {
    const std::vector<Step> steps = SelectorAccess::asSteps(alternative);
    if (steps.empty() || steps.front().combinator != Combinator::Descendant)
      continue;
    if (!matchesComplex(steps, steps.size() - 1, inst, context)) continue;
    any = true;
    if (weight == nullptr) return true;
    *weight = std::max(*weight, alternative.specificity());
  }
  return any;
}

bool matchesAny(const std::vector<ElementSelector>& alternatives,
                const Instance& inst, const MatchContext& context) {
  for (const ElementSelector& one : alternatives)
    if (matchesSelector(one, inst, context, nullptr)) return true;
  return false;
}

bool matchesHas(const std::vector<ElementSelector>& relatives,
                const Instance& anchor, const MatchContext& context);

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
                     const MatchContext& context, int* matchingCount) {
  *matchingCount = 0;
  if (!matchesAny(filter, inst, context)) return 0;
  if (inst.parent == nullptr || &inst == context.scope) {
    *matchingCount = 1;
    return 1;
  }
  int position = 0;
  for (const auto& sibling : inst.parent->children) {
    if (sibling->description->added()) continue;
    if (!matchesAny(filter, *sibling, context)) continue;
    ++*matchingCount;
    if (sibling.get() == &inst) position = *matchingCount;
  }
  return position;
}

bool matchesSimple(const Simple& simple, const Instance& inst,
                   const MatchContext& context) {
  const SiblingPlace place = placeIn(inst, context.scope);
  switch (simple.kind) {
    case SimpleKind::Universal:
      return true;
    case SimpleKind::StyleClass:
      return hasStyleClass(inst, simple.name);
    case SimpleKind::Role:
      return roleOf(inst) == simple.name;
    // A node counted among no siblings — one an adding operator attached,
    // or a named run matched as a virtual child — answers none of these.
    case SimpleKind::FirstChild:
      return place.count > 0 && place.index == 0;
    case SimpleKind::LastChild:
      return place.count > 0 && place.index == place.count - 1;
    case SimpleKind::OnlyChild:
      return place.count == 1;
    case SimpleKind::NthChild:
    case SimpleKind::NthLastChild: {
      const bool fromLast = simple.kind == SimpleKind::NthLastChild;
      if (simple.arguments.empty()) {
        if (place.count == 0) return false;
        const int position =
            fromLast ? place.count - place.index : place.index + 1;
        return countReaches(position, simple.step, simple.offset);
      }
      int matching = 0;
      const int position =
          filteredPosition(inst, simple.arguments, context, &matching);
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
      return inst.parent == nullptr || &inst == context.scope;
    case SimpleKind::Is:
    case SimpleKind::Where:
      return matchesAny(simple.arguments, inst, context);
    case SimpleKind::Not:
      return !matchesAny(simple.arguments, inst, context);
    case SimpleKind::Has:
      return matchesHas(simple.arguments, inst, context);
  }
  return false;
}

bool matchesCompound(const Compound& compound, const Instance& inst,
                     const MatchContext& context) {
  for (const Simple& simple : compound.simples)
    if (!matchesSimple(simple, inst, context)) return false;
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
                    const Instance& inst, const MatchContext& context) {
  if (!matchesCompound(steps[at].compound, inst, context)) return false;
  if (at == 0) return true;
  // The applying node has neither a parent nor a sibling inside the
  // subtree, so every combinator leaving it leaves the sheet's reach.
  if (&inst == context.scope) return false;
  const Instance* const parent = inst.parent;
  switch (steps[at].combinator) {
    case Combinator::Child:
      return parent != nullptr &&
             matchesComplex(steps, at - 1, *parent, context);
    case Combinator::Descendant:
      for (const Instance* above = parent; above != nullptr;
           above = above->parent) {
        if (matchesComplex(steps, at - 1, *above, context)) return true;
        if (above == context.scope) break;
      }
      return false;
    case Combinator::Next:
    case Combinator::Sibling:
      return anySiblingBefore(inst, steps[at].combinator == Combinator::Next,
                              [&](const Instance& sibling) {
                                return matchesComplex(steps, at - 1, sibling,
                                                      context);
                              });
  }
  return false;
}

// ---------------------------------------------------------------------------
// :has()

/** The bits of the names @p inst carries itself: its classes and its
 *  role. */
uint64_t ownNameBits(const Instance& inst, const HasNames& names) {
  const ElementNode& node = *inst.description;
  if (!node.cascadeData) return 0;
  uint64_t bits = 0;
  for (const std::string& carried : node.cascadeData->classes)
    bits |= names.bitOf(true, carried);
  if (node.cascadeData->role)
    bits |= names.bitOf(false, node.cascadeData->role->name);
  return bits;
}

/** The bits a relative chain NEEDS present: every class and role its
 *  compounds state outright. A name with no bit adds nothing, so the
 *  summary can only rule a chain out, never in. */
uint64_t neededBits(const std::vector<Step>& steps, const HasNames& names) {
  uint64_t bits = 0;
  for (const Step& step : steps)
    for (const Simple& simple : step.compound.simples)
      if (simple.kind == SimpleKind::StyleClass ||
          simple.kind == SimpleKind::Role)
        bits |= names.bitOf(simple.kind == SimpleKind::StyleClass, simple.name);
  return bits;
}

/** Whether @p inst stands where @p relation reaches from @p anchor. A
 *  sibling of the node that applied the sheet stands outside what the
 *  sheet sees, so the two sibling relations reach nothing from there. */
bool relationHolds(Combinator relation, const Instance& inst,
                   const Instance& anchor, const MatchContext& context) {
  switch (relation) {
    case Combinator::Child:
      return inst.parent == &anchor;
    case Combinator::Descendant:
      for (const Instance* above = inst.parent; above != nullptr;
           above = above->parent)
        if (above == &anchor) return true;
      return false;
    case Combinator::Next:
      return &anchor != context.scope && inst.parent != nullptr &&
             inst.parent == anchor.parent && inst.place.count > 0 &&
             anchor.place.count > 0 &&
             inst.place.index == anchor.place.index + 1;
    case Combinator::Sibling:
      return &anchor != context.scope && inst.parent != nullptr &&
             inst.parent == anchor.parent && inst.place.count > 0 &&
             anchor.place.count > 0 && anchor.place.index < inst.place.index;
  }
  return false;
}

/** A RELATIVE chain up to and including @p at, against @p inst and the
 *  elements left of it, right to left as `matchesComplex` does — except
 *  that its first compound must stand where the chain's opening relation
 *  reaches from @p anchor, and no walk climbs to the anchor or past it. */
bool matchesRelative(const std::vector<Step>& steps, size_t at,
                     const Instance& inst, const Instance& anchor,
                     const MatchContext& context) {
  if (!matchesCompound(steps[at].compound, inst, context)) return false;
  if (at == 0) return relationHolds(steps[0].combinator, inst, anchor, context);
  const Instance* const parent = inst.parent;
  const auto outside = [&](const Instance* node) {
    return node == nullptr || node == &anchor || node == anchor.parent;
  };
  switch (steps[at].combinator) {
    case Combinator::Child:
      return !outside(parent) &&
             matchesRelative(steps, at - 1, *parent, anchor, context);
    case Combinator::Descendant:
      for (const Instance* above = parent; !outside(above);
           above = above->parent)
        if (matchesRelative(steps, at - 1, *above, anchor, context))
          return true;
      return false;
    case Combinator::Next:
    case Combinator::Sibling:
      return anySiblingBefore(inst, steps[at].combinator == Combinator::Next,
                              [&](const Instance& sibling) {
                                return matchesRelative(steps, at - 1, sibling,
                                                       anchor, context);
                              });
  }
  return false;
}

/** Whether any element of @p region's subtree — @p region itself
 *  included where @p withSelf — is the SUBJECT of a relative chain
 *  reaching back to @p anchor. An element an adding operator attached
 *  is no part of the authored tree, and neither is anything under it. */
bool searchRelative(const std::vector<Step>& steps, const Instance& region,
                    bool withSelf, const Instance& anchor,
                    const MatchContext& context) {
  if (region.description->added()) return false;
  if (withSelf &&
      matchesRelative(steps, steps.size() - 1, region, anchor, context))
    return true;
  for (const auto& child : region.children)
    if (searchRelative(steps, *child, true, anchor, context)) return true;
  return false;
}

/** Whether one relative chain reaches some element from @p anchor. The
 *  summary is asked first: a chain naming one class or one role and
 *  reaching down is answered by the summary alone, and any other chain
 *  is searched for only where the summary holds every name it needs. */
bool matchesOneRelative(const std::vector<Step>& steps, const Instance& anchor,
                        const MatchContext& context) {
  const HasNames& names = *context.names;
  const Combinator relation = steps.front().combinator;
  const uint64_t needed = neededBits(steps, names);
  const bool fresh = names.summaryHolds(anchor, needed);
  if (relation == Combinator::Child || relation == Combinator::Descendant) {
    const uint64_t below = relation == Combinator::Child && steps.size() == 1
                               ? anchor.hasChildNames
                               : anchor.hasSubtreeNames;
    if (fresh && (below & needed) != needed) return false;
    const std::vector<Simple>& only = steps.front().compound.simples;
    const bool oneName = steps.size() == 1 && only.size() == 1 &&
                         (only.front().kind == SimpleKind::StyleClass ||
                          only.front().kind == SimpleKind::Role) &&
                         needed != 0;
    if (fresh && oneName) return true;
    return searchRelative(steps, anchor, false, anchor, context);
  }
  // The sibling relations read the parent's indexed child list.
  if (&anchor == context.scope) return false;
  const int slot = slotOf(anchor);
  if (slot < 0) return false;
  const auto& siblings = anchor.parent->children;
  for (int at = slot + 1; at < (int)siblings.size(); ++at) {
    const Instance& sibling = *siblings[at];
    if (sibling.description->added()) continue;
    const uint64_t held = sibling.hasOwnNames | sibling.hasSubtreeNames;
    const bool ruledOut =
        names.summaryHolds(sibling, needed) && (held & needed) != needed;
    if (!ruledOut && searchRelative(steps, sibling, true, anchor, context))
      return true;
    if (relation == Combinator::Next) return false;
  }
  return false;
}

bool matchesHas(const std::vector<ElementSelector>& relatives,
                const Instance& anchor, const MatchContext& context) {
  if (context.names == nullptr) return false;
  for (const ElementSelector& relative : relatives) {
    const std::vector<Step> steps = SelectorAccess::asSteps(relative);
    if (steps.empty()) continue;
    if (matchesOneRelative(steps, anchor, context)) return true;
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
    const int slot = (int)(&child - parent.children.data());
    if (child->description->added())
      place = SiblingPlace{0, 0, 0, 0, slot};
    else
      place = SiblingPlace{at++, count, 0, 0, slot};
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

void HasNames::add(const SheetBody& sheet) {
  for (const HasName& name : sheet.hasNames)
    if (std::find(m_names.begin(), m_names.end(), name) == m_names.end())
      m_names.push_back(name);
}

bool HasNames::summaryHolds(const Instance& node, uint64_t needed) const {
  if (node.hasSummaryPass != m_pass) return false;
  const uint64_t coveredBits = node.hasSummaryCovers >= 64
                                   ? ~uint64_t{0}
                                   : (uint64_t{1} << node.hasSummaryCovers) - 1;
  return (needed & ~coveredBits) == 0;
}

bool HasNames::summaryCurrent(const Instance& node) const {
  return node.hasSummaryPass == m_pass && node.hasSummaryCovers == covered();
}

uint64_t HasNames::bitOf(bool styleClass, std::string_view name) const {
  const size_t held = std::min<size_t>(m_names.size(), 64);
  for (size_t at = 0; at < held; ++at)
    if (m_names[at].styleClass == styleClass && m_names[at].name == name)
      return uint64_t{1} << at;
  return 0;
}

void summariseForHas(Instance& root, const HasNames& names) {
  indexSiblings(root);
  root.hasOwnNames = ownNameBits(root, names);
  uint64_t children = 0;
  uint64_t subtree = 0;
  for (const auto& child : root.children) {
    summariseForHas(*child, names);
    // An element an adding operator attached is no part of the authored
    // tree, so nothing under it answers its parent's :has() — though its
    // own subtree is summarised and indexed for a :has() standing on it.
    if (child->description->added()) continue;
    children |= child->hasOwnNames;
    subtree |= child->hasOwnNames | child->hasSubtreeNames;
  }
  root.hasChildNames = children;
  root.hasSubtreeNames = subtree;
  root.hasSummaryPass = names.pass();
  root.hasSummaryCovers = names.covered();
}

std::vector<MatchedRule> matchRulesForName(const SheetChain& chain,
                                           const Instance& leaf,
                                           std::string_view name,
                                           const HasNames& names) {
  // One virtual child per thread, re-dressed for each name: a node whose
  // one class is the name, hung under the leaf and counted among none of
  // its siblings.
  struct Virtual {
    Instance child;
    std::shared_ptr<ElementNode> node = std::make_shared<ElementNode>();
  };
  static thread_local Virtual run;
  CascadeData& cascade = run.node->cascadeData.ensure();
  cascade.classes.assign(1, std::string(name));
  run.child.description = run.node;
  run.child.parent = const_cast<Instance*>(&leaf);
  run.child.place = SiblingPlace{0, 0, 0, 0};
  run.child.hasSummaryPass = 0;
  run.child.hasSummaryCovers = 0;
  std::vector<MatchedRule> matched = matchRules(chain, run.child, names);
  run.child.parent = nullptr;
  return matched;
}

std::vector<MatchedRule> matchRules(const SheetChain& chain,
                                    const Instance& inst,
                                    const HasNames& names) {
  std::vector<MatchedRule> matched;
  const ElementNode& node = *inst.description;
  std::vector<uint32_t> candidates;
  for (size_t sheetAt = 0; sheetAt < chain.size(); ++sheetAt) {
    const ScopedSheet& applied = chain[sheetAt];
    const SheetBody* body = SheetAccess::body(*applied.sheet);
    if (body == nullptr) continue;
    // Only the rules filed where this node can be found: under each of
    // its classes, under its role, and among those any element may match.
    candidates.assign(body->anyElement.begin(), body->anyElement.end());
    if (node.cascadeData) {
      for (const std::string& carried : node.cascadeData->classes)
        if (const auto found = body->byClass.find(carried);
            found != body->byClass.end())
          candidates.insert(candidates.end(), found->second.begin(),
                            found->second.end());
      if (node.cascadeData->role)
        if (const auto found = body->byRole.find(node.cascadeData->role->name);
            found != body->byRole.end())
          candidates.insert(candidates.end(), found->second.begin(),
                            found->second.end());
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()),
                     candidates.end());
    const MatchContext context{applied.scope, &names};
    for (const uint32_t ruleAt : candidates) {
      Specificity weight;
      const Rule& rule = body->rules[ruleAt];
      if (!matchesSelector(rule.selector(), inst, context, &weight)) continue;
      matched.push_back(MatchedRule{&rule, weight, (int)sheetAt, (int)ruleAt});
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
