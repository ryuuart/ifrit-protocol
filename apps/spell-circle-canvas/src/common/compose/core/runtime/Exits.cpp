/** @file
 * Exits: a keyed node stating `exit()` that a description leaves out is
 * kept where it stands while every entrance under it plays backwards, and
 * dropped on the frame the last of them lands.
 *
 * THE NODE KEEPS ITS PLACE. The reconciler retires what a list no longer
 * names, so the host hands it a list that still names a leaving node — its
 * own description, standing where it stood — and the node survives the
 * pass untouched, attached to the layout as before: its siblings do not
 * move until it is gone, and then they close over its place at once.
 *
 * WHEN EACH BEGINS. The nodes one reconcile leaves out together are one
 * run played backwards: each entrance ends where the whole run's last one
 * ended, counted from the moment they leave, so the last to arrive is the
 * first to go and the stagger that brought them in takes them out in the
 * reverse order. A node the description names again before its exit lands
 * turns round where it stands.
 */

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** Whether @p inst carries a node the exit machinery has to look at: one
 *  leaving, or one stating `exit()` that the list may have left out. */
bool mayLeave(const Instance& inst) {
  return inst.leaving || inst.description->exits;
}

/** The key @p node is matched by, as the reconciler reads a description:
 *  a memo element's own key, else the description's. */
const std::string& keyNamed(const Element& element) {
  return element.node()->key;
}

/** The keys a list of children names: @p authored, a description's own,
 *  then @p added, what the adding operators keep beside them. */
boost::unordered_flat_set<std::string_view> namedKeys(
    const std::vector<Element>& authored, const std::vector<Element>& added) {
  boost::unordered_flat_set<std::string_view> named;
  for (const Element& child : authored)
    if (!keyNamed(child).empty()) named.insert(keyNamed(child));
  for (const Element& child : added)
    if (!keyNamed(child).empty()) named.insert(keyNamed(child));
  return named;
}

}  // namespace

Composer::Impl::ChildRange Composer::Impl::children(
    const Instance& inst, const Description& description) const {
  ChildRange range{&description->children, &inst.addedChildren, {}};
  // A capture no clock steps has no exit to play: what leaves drops.
  if (liveOnly ||
      std::none_of(inst.children.begin(), inst.children.end(),
                   [](const auto& child) { return child && mayLeave(*child); }))
    return range;
  const boost::unordered_flat_set<std::string_view> named =
      namedKeys(description->children, inst.addedChildren);
  std::optional<std::vector<Element>> merged;
  for (size_t index = 0; index < inst.children.size(); ++index) {
    const Instance& child = *inst.children[index];
    const std::string& key = reconciler.matchKeyOf(child);
    if (key.empty() || named.contains(key) || !mayLeave(child)) continue;
    // A leaving node whose exit has landed is let go here, should no draw
    // have dropped it first.
    if (child.leaving && child.exitLanded()) continue;
    if (!merged) {
      merged.emplace();
      merged->reserve(range.size() + 1);
      merged->insert(merged->end(), description->children.begin(),
                     description->children.end());
      merged->insert(merged->end(), inst.addedChildren.begin(),
                     inst.addedChildren.end());
    }
    // Its own description, so the pass matches it by its key and finds
    // nothing changed; the memo element when it was described through one,
    // since that is what it is matched by. It stands at the index it stood
    // at, among the children before it that stay.
    const Element standing(child.memoShell ? child.memoShell
                                           : child.description);
    merged->insert(
        merged->begin() + (std::ptrdiff_t)std::min(index, merged->size()),
        standing);
  }
  range.merged = std::move(merged);
  return range;
}

bool Composer::Impl::settleExits(
    Instance& parent, std::vector<std::unique_ptr<Instance>>& dropped) {
  if (liveOnly || parent.description->kind == Kind::Slot) return false;
  if (std::none_of(parent.children.begin(), parent.children.end(),
                   [](const auto& child) { return child && mayLeave(*child); }))
    return false;
  const boost::unordered_flat_set<std::string_view> named =
      namedKeys(parent.description->children, parent.addedChildren);
  std::vector<Instance*> beginning;
  for (auto& child : parent.children) {
    const std::string& key = reconciler.matchKeyOf(*child);
    if (key.empty() || named.contains(key)) {
      if (!child->leaving) continue;
      // Named again before its exit landed: every entrance turns round,
      // forward from where it stands.
      child->leaving = false;
      child->exitRuns.clear();
      std::vector<EntranceRun> runs;
      entrancesUnder(*child, child->mountPlace, runs);
      for (const EntranceRun& run : runs) playForwards(run);
      child->markPaintDirtyUp();
      continue;
    }
    if (!child->leaving) beginning.push_back(child.get());
  }
  if (beginning.empty()) return false;

  // ONE RUN, PLAYED BACKWARDS. Each leaving node's own entrances resolve
  // against its place among the nodes leaving with it, and each one's exit
  // waits for as long as the run went on after that entrance ended.
  std::vector<std::vector<EntranceRun>> runs(beginning.size());
  motion::Duration last{};
  for (size_t index = 0; index < beginning.size(); ++index) {
    entrancesUnder(*beginning[index], {index, beginning.size()}, runs[index]);
    for (const EntranceRun& run : runs[index]) last = std::max(last, run.end);
  }
  std::vector<Instance*> nothingToPlay;
  for (size_t index = 0; index < beginning.size(); ++index) {
    Instance& leaving = *beginning[index];
    leaving.leaving = true;
    leaving.exitRuns.clear();
    for (const EntranceRun& run : runs[index])
      if (playBackwards(run, last - run.end))
        leaving.exitRuns.push_back((*run.held)->live);
    if (leaving.exitRuns.empty()) nothingToPlay.push_back(&leaving);
    leaving.markPaintDirtyUp();
  }
  hasLeaving = true;
  volatileDirty = true;
  if (nothingToPlay.empty()) return false;
  // Nothing under it entered, so nothing plays it out: it goes now, as an
  // unkeyed node does.
  std::erase_if(parent.children, [&](std::unique_ptr<Instance>& child) {
    if (std::find(nothingToPlay.begin(), nothingToPlay.end(), child.get()) ==
        nothingToPlay.end())
      return false;
    dropped.push_back(std::move(child));
    return true;
  });
  return true;
}

bool Composer::Impl::dropLandedExits(Instance& parent) {
  bool dropped = false;
  std::vector<std::unique_ptr<Instance>> landed;
  for (auto it = parent.children.begin(); it != parent.children.end();) {
    Instance& child = **it;
    if (child.leaving && child.exitLanded()) {
      landed.push_back(std::move(*it));
      it = parent.children.erase(it);
      continue;
    }
    dropped |= dropLandedExits(child);
    ++it;
  }
  if (landed.empty()) return dropped;
  // Reattached first, so the dropped nodes leave the layout before their
  // own layout nodes are freed.
  reorder(parent, /*structureChanged=*/true);
  landed.clear();
  return true;
}

}  // namespace sigil::compose
