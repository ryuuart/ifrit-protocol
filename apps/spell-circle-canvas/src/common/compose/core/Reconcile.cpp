/** @file
 * What the reconcile walk indexes as it goes: the key index with the edge
 * store that rides it, and the order the derived nodes' declared reads
 * imply. The reconciler itself — memo resolution, the prune, keyed and
 * positional matching — is SigilCore's, driven through the host operations
 * in ReconcileHost.cpp.
 */

#include <sigilcore/reconcile/Reads.h>

#include <algorithm>
#include <span>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

void Composer::Impl::rebuildKeyIndex() {
  byKey.clear();
  bySlot.clear();
  routedInstances.clear();
  flowInstances.clear();
  tetheredInstances.clear();
  pathMarkInstances.clear();
  threadedInstances.clear();
  routesByAnchor.clear();
  hasDerived = false;
  hasCustomLayout = false;
  hasCenterPins = false;
  // The reconciler fills byKey — a memo shell's key first, else the
  // description's — and the edge store (flat derive lists + anchor
  // back-index) and the pass gates ride the same walk. Tree order here IS
  // the derive order.
  if (root)
    reconciler.indexKeys(*root, byKey, [this](Instance& inst) {
      if (inst.description->kind == Kind::Slot &&
          !inst.description->key.empty())
        bySlot[inst.description->key] = &inst;
      const ElementNode& node = *inst.description;
      if (node.deriveData) {
        const DeriveData& derive = *node.deriveData;
        if (!derive.flowAroundKeys.empty()) flowInstances.push_back(&inst);
        if (derive.tether) tetheredInstances.push_back(&inst);
        const bool isConnector =
            !derive.connectFrom.empty() && !derive.connectTo.empty();
        const bool isRail = derive.railAnchors.size() >= 2;
        // A borrowed band spine and a spans::fit() gap are the same kind
        // of question a connector asks — "where did that keyed node land"
        // — so they ride the SAME flat derive list rather than growing a
        // phase.
        //
        // THESE THREE ARE NOT READ QUESTIONS, which is why they read the
        // fields and not the declared reads: they choose WHICH PASS
        // resolves the node — a route, a flow, a chain — and which anchors
        // a route is re-run from when one of them moves. A read says what
        // a node waits for; this says what is done to it, and two nodes
        // reading the same key can still be resolved by different passes.
        const bool isBorrowed = !derive.bandAround.empty() ||
                                !derive.spanFitKeys.empty() ||
                                !derive.borrowedPathKeys.empty();
        if (isBorrowed && !isConnector && !isRail)
          routedInstances.push_back(&inst);
        if (isConnector || isRail) {
          routedInstances.push_back(&inst);
          if (isConnector) {
            routesByAnchor[derive.connectFrom].push_back(&inst);
            if (derive.connectTo != derive.connectFrom)
              routesByAnchor[derive.connectTo].push_back(&inst);
          }
          for (const Anchor& anchor : derive.railAnchors) {
            // A waypoint that names no node is a free point: nothing
            // resolves it, so it belongs under no anchor's key.
            if (anchor.nodeKey.empty()) continue;
            auto& at = routesByAnchor[anchor.nodeKey];
            if (at.empty() || at.back() != &inst)  // rails revisit anchors
              at.push_back(&inst);
          }
        }
        if (derive.placeFn) hasCustomLayout = true;
      }
      if (node.kind == Kind::Text && node.textData && node.textData->onPath &&
          !node.textData->marks.empty())
        pathMarkInstances.push_back(&inst);
      if (node.kind == Kind::Text && node.textData &&
          !node.textData->threadTo.empty())
        threadedInstances.push_back(&inst);
      if (node.layout.centerAt) hasCenterPins = true;
    });
  hasDerived = !routedInstances.empty() || !flowInstances.empty() ||
               !threadedInstances.empty() || !tetheredInstances.empty();
  orderDerivedByReads();
}

/** THE ORDER THE DECLARED READS IMPLY.
 *
 *  A derived node is one whose answer is a function of another node's
 *  finished answer, and tree order is only the right order to resolve them
 *  in while none of them reads another. One that does — a rail anchored on
 *  a connector's own box, a frame threaded from a frame written later — is
 *  a pass behind for as long as the order is the order it was written in.
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
  reorder(tetheredInstances);
  reorder(routedInstances);
  reorder(threadedInstances);
}

}  // namespace sigil::compose
