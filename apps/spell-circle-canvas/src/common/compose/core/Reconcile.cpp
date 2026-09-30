/** @file
 * What the reconcile walk indexes as it goes: the key index with the
 * borrow lists that ride it, and the order the derived nodes' declared
 * reads imply. The reconciler itself — memo resolution, the prune, keyed and
 * positional matching — is SigilCore's, driven through the host operations
 * in ReconcileHost.cpp.
 */

#include <sigilcore/reconcile/Reads.h>

#include <algorithm>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** The key a retained node answers to: its memo shell's when it was
 *  described through one, else its description's. */
const std::string& keyOf(const Instance& inst) {
  if (inst.memoShell && !inst.memoShell->key.empty())
    return inst.memoShell->key;
  return inst.description->key;
}

/** Where a node stands, for a report an author reads: its key when it has
 *  one, else the child indices that lead to it from its nearest keyed
 *  ancestor — or from the root, when no ancestor carries a key. */
std::string placeOf(const Instance& inst) {
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
  std::string place = keyOf(*node).empty()
                          ? std::string("the root")
                          : "\"" + keyOf(*node) + "\"";
  for (auto index = indices.rbegin(); index != indices.rend(); ++index)
    place += " > child " + std::to_string(*index);
  return place;
}

/** THE LIST NEVER LIES ABOUT ITS ORDER: the arranging operators run during
 *  layout and the adding ones after it, so an arranging one written after
 *  an adding one runs before it whatever the list says. It is reported
 *  rather than reordered, once per place, and the report names the place
 *  so a keyless container can still be found. */
void reportOperatorOrder(const Instance& inst) {
  static std::mutex guard;
  static std::vector<std::string> reported;
  const std::string place = placeOf(inst);
  {
    std::lock_guard lock(guard);
    if (std::find(reported.begin(), reported.end(), place) != reported.end())
      return;
    if (reported.size() >= 16) return;
    reported.push_back(place);
  }
  SkDebugf(
      "[compose] .operators() on %s: an arranging operator is listed after "
      "an adding one. Arranging operators run during layout and adding "
      "operators after it, so the list runs out of the order it is written "
      "in; write the arranging operators first.\n",
      place.c_str());
}

}  // namespace

void Composer::Impl::rebuildKeyIndex() {
  byKey.clear();
  bySlot.clear();
  borrowInstances.clear();
  flowInstances.clear();
  pathMarkInstances.clear();
  threadedInstances.clear();
  addingInstances.clear();
  additionOwners.clear();
  hasDerived = false;
  hasCustomLayout = false;
  hasCenterPins = false;
  // The reconciler fills byKey — a memo shell's key first, else the
  // description's — and the flat borrow lists and the pass gates ride the
  // same walk. Tree order here IS the derive order.
  if (root)
    reconciler.indexKeys(*root, byKey, [this](Instance& inst) {
      if (inst.description->kind == Kind::Slot &&
          !inst.description->key.empty())
        bySlot[inst.description->key] = &inst;
      const ElementNode& node = *inst.description;
      if (node.deriveData) {
        const DeriveData& derive = *node.deriveData;
        if (!derive.flowAroundKeys.empty()) flowInstances.push_back(&inst);
        // A spans::fit() gap and a strand::from() path are the same
        // question — "where did that keyed node land" — so they ride ONE
        // flat list rather than growing a phase each.
        //
        // THIS IS NOT A READ QUESTION, which is why it reads the fields
        // and not the declared reads: it chooses WHICH PASS resolves the
        // node — a borrow, a flow, a chain. A read says what a node waits
        // for; this says what is done to it, and two nodes reading the
        // same key can still be resolved by different passes.
        if (!derive.spanFitKeys.empty() || !derive.borrowedPathKeys.empty())
          borrowInstances.push_back(&inst);
      }
      if (node.arranges()) hasCustomLayout = true;
      if (node.adds()) {
        addingInstances.push_back(&inst);
        bool adding = false;
        for (const Operator& op : node.operatorData->operators) {
          if (op.adds()) adding = true;
          if (adding && op.arranges()) {
            reportOperatorOrder(inst);
            break;
          }
        }
      }
      if (!inst.addedChildren.empty()) additionOwners.push_back(&inst);
      if (node.kind == Kind::Text && node.textData && node.textData->onPath &&
          !node.textData->marks.empty())
        pathMarkInstances.push_back(&inst);
      if (node.kind == Kind::Text && node.textData &&
          !node.textData->threadTo.empty())
        threadedInstances.push_back(&inst);
      if (inst.computed.layout.centerAt) hasCenterPins = true;
    });
  hasDerived = !borrowInstances.empty() || !flowInstances.empty() ||
               !threadedInstances.empty();
  hasAdding = !addingInstances.empty() || !additionOwners.empty();
  orderDerivedByReads();
}

/** THE ORDER THE DECLARED READS IMPLY.
 *
 *  A derived node is one whose answer is a function of another node's
 *  finished answer, and tree order is only the right order to resolve them
 *  in while none of them reads another. One that does — a gate sized from
 *  a box that is itself borrowed, a frame threaded from a frame written
 *  later — is a pass behind for as long as the order is the order it was
 *  written in.
 *
 *  Every derivation DECLARES what it reads, in the statement that writes
 *  it, and this hands those declarations to `core::orderByReads`. Nothing
 *  here infers an edge from which fields a node happens to carry: a
 *  derivation added tomorrow is ordered correctly by the same lines,
 *  because the only thing they know about it is its own declaration.
 *
 *  The order is STABLE: a list whose members read none of each other comes
 *  back exactly as it went in, which is every list on nearly every tree. */
void Composer::Impl::orderDerivedByReads() {
  const auto reorder = [](std::vector<Instance*>& list) {
    if (list.size() < 2) return;
    std::vector<std::string> keys;
    std::vector<std::vector<sigil::core::Read>> reads;
    keys.reserve(list.size());
    reads.reserve(list.size());
    for (const Instance* inst : list) {
      keys.push_back(inst->description ? inst->description->key
                                       : std::string());
      if (inst->description && inst->description->deriveData)
        reads.push_back(inst->description->deriveData->reads);
      else
        reads.emplace_back();  // a node that declares nothing reads nothing
    }
    const std::vector<uint32_t> order = sigil::core::orderByReads(keys, reads);
    std::vector<Instance*> sorted;
    sorted.reserve(list.size());
    for (const uint32_t index : order) sorted.push_back(list[index]);
    list.swap(sorted);
  };
  reorder(flowInstances);
  reorder(borrowInstances);
  reorder(threadedInstances);
}

}  // namespace sigil::compose
