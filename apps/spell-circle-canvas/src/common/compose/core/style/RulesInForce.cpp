/** @file
 * The rules in force at a node: the selector sheets its ancestors and it
 * apply, the rules of those sheets that match it, and what they leave on
 * the instance — the transition, the layer of carried properties, the
 * marks laid under its own.
 */

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "CascadePass.h"
#include "RuleMarks.h"
#include "SelectorMatch.h"
#include "SheetBody.h"
#include "paint/PaintInternal.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

RulesInForce resolveRulesInForce(Composer::Impl& impl, Instance& inst,
                                 const SheetChain& parentSheets,
                                 SheetChain& extended, bool first) {
  const ElementNode& node = *inst.description;
  const CascadeData* const cascade =
      node.cascadeData ? &*node.cascadeData : nullptr;
  RulesInForce inForce;
  // THE SELECTOR SHEETS IN FORCE HERE: the ones the ancestors applied,
  // then the ones this node applies, which speak about this node too.
  // Each carries the node that applied it, because that node bounds
  // what its sheet can see. The chain is extended only where a node
  // applies one, so a tree that applies none shares the root's empty
  // chain all the way down.
  const SheetChain* sheets = &parentSheets;
  if (cascade != nullptr && !cascade->appliedSheets.empty()) {
    extended = parentSheets;
    bool anyHas = false;
    for (const StyleSheet& applied : cascade->appliedSheets) {
      extended.push_back(ScopedSheet{&applied, &inst});
      const SheetBody* body = SheetAccess::body(applied);
      if (body == nullptr || !body->usesHas) continue;
      anyHas = true;
      impl.hasNames.add(*body);
    }
    // A :has() LOOKS DOWN, at a subtree the pass has not reached yet, so
    // what it asks about is summarised here, bottom up — only under a
    // node applying a sheet that uses one, since a :has() can see nothing
    // outside the subtree its sheet was applied at. A summary an ancestor
    // took this pass is taken again where a sheet since gave a name a bit,
    // because that summary says nothing about the new name.
    if (anyHas && !impl.hasNames.summaryCurrent(inst))
      summariseForHas(inst, impl.hasNames);
    sheets = &extended;
  }
  // The rules that speak about this node, weakest first. A rule's
  // SUBJECT is this node, which lies in the applying node's subtree by
  // construction; every other compound of its selector is matched
  // inside that subtree too, so nothing above or beside the applying
  // node can satisfy one.
  inForce.matched = sheets->empty() ? std::vector<MatchedRule>{}
                                    : matchRules(*sheets, inst, impl.hasNames);
  const std::vector<MatchedRule>& matched = inForce.matched;
  // The strongest matched rule stating a transition gives the node the
  // one its own verb has not; the matches run weakest first.
  const Rule* transitionRule = nullptr;
  for (const MatchedRule& one : matched)
    if (one.rule->transition()) transitionRule = one.rule;
  if (transitionRule != nullptr)
    inst.ruleTransition = transitionRule->transition();
  else if (inst.ruleTransition)
    inst.ruleTransition.reset();
  // WHAT THE MATCHED RULES STATE about the carried properties, folded
  // into the layer that stands between the parent's answers and this
  // node's own verbs. It is rebuilt here because a rule can move while
  // this node's own declarations stand still — a class toggled, a sheet
  // applied above — and a node whose declarations did not move never
  // reaches the patch.
  //
  // Only where the rules themselves changed: a pass that matches the rules
  // it matched last time — the same sheet, the same classes — keeps the
  // layers it built then, so a tree re-described every frame under one
  // sheet does not rebuild a layer per matched node per frame.
  const bool sameRules = std::equal(
      matched.begin(), matched.end(), inst.ruleLayerSource.begin(),
      inst.ruleLayerSource.end(),
      [](const MatchedRule& one, const std::shared_ptr<ElementNode>& held) {
        return one.rule->node().get() == held.get();
      });
  bool layerMoved = false;
  bool fillMaterialMoved = false;
  if (!sameRules) {
    inst.ruleLayerSource.clear();
    for (const MatchedRule& one : matched)
      inst.ruleLayerSource.push_back(one.rule->node());
    std::unique_ptr<const RuleLayer> layer;
    if (!matched.empty()) {
      std::vector<const Rule*> rules;
      rules.reserve(matched.size());
      for (const MatchedRule& one : matched) rules.push_back(one.rule);
      layer = ruleLayerOf(rules);
    }
    layerMoved = !ruleLayerEqual(inst.ruleLayer.get(), layer.get());
    if (layerMoved) {
      fillMaterialMoved =
          !ruleFillMaterialEqual(inst.ruleLayer.get(), layer.get());
      inst.ruleLayer = std::move(layer);
    }
    // Set, never cleared here, for the reason the ink's box below is.
    if (ruleFillUsesWorldSpace(inst.ruleLayer.get()))
      inst.hasWorldSpaceMaterial = true;
  }
  // Prepared inputs belong to the effective surfaced fill. Dropping or
  // replacing that fill releases them even when the node does not paint.
  if (inst.surfaceMapMaterial) {
    const MaterialData* fill = fillSlotOf(inst);
    if (!fill || !fill->surfaced ||
        *fill->surfaced != *inst.surfaceMapMaterial) {
      inst.surfaceMapFill.reset();
      inst.surfaceMapFillInputs.reset();
      inst.surfaceMapMaterial.reset();
    }
  }
  // THE MARKS THE MATCHED RULES STATE, laid under the node's own on the
  // description the paint phase reads — again where the rules moved, and
  // where a patch swapped the description the last dressing was made from.
  if ((!matched.empty() || inst.dressed) &&
      (!sameRules || inst.dressedFrom != inst.description)) {
    std::vector<const Rule*> rules;
    rules.reserve(matched.size());
    for (const MatchedRule& one : matched) rules.push_back(one.rule);
    std::shared_ptr<const ElementNode> dressed =
        rules.empty() ? nullptr : dressedWithRules(node, rules);
    const bool dressingMoved = dressed != nullptr || inst.dressed != nullptr;
    inst.dressed = std::move(dressed);
    inst.dressedFrom = inst.description;
    if (!first && dressingMoved) {
      inst.markPaintDirtyUp();
      impl.contentDirty = true;
      impl.volatileDirty = true;
    }
  }
  inForce.sheets = sheets;
  inForce.sameRules = sameRules;
  inForce.layerMoved = layerMoved;
  inForce.fillMaterialMoved = fillMaterialMoved;
  return inForce;
}

}  // namespace sigil::compose
