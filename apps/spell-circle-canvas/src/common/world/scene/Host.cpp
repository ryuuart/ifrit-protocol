/** @file
 * The host half of the reconcile pass: what the Scene does when a node
 * mounts, when its description changed, when a parent's children moved,
 * and when a node leaves the tree.
 *
 * The three lifetimes meet here. A patch retargets the LANES, which
 * belong to the node; it marks the geometry slot for RESOLUTION, which
 * belongs to the store; and it stales the bakes above, which belong to
 * the frame. None of the three ever ends another.
 */

#include <sigilmotion/clock/Ticker.h>

#include <span>
#include <utility>

#include "SceneImpl.h"

namespace sigil::world {

std::unique_ptr<Instance> Scene::Impl::create(const Desc& desc,
                                              Instance* parent, size_t,
                                              size_t) {
  auto inst = std::make_unique<Instance>();
  inst->parent = parent;
  inst->entity = registry.create();
  registry.emplace<component::Placement>(inst->entity);
  // The first patch is the mount: it plays the entrances, marks the
  // geometry slot for resolution and walks the children.
  reconciler.patch(*inst, desc);
  return inst;
}

void Scene::Impl::onPatched(Instance& inst, const ElementNode* prev,
                            const ElementNode& next) {
  lanesOf(next, laneScratch);
  if (prev) {
    lanesOf(*prev, prevLaneScratch);
    motion::retargetSlots<LaneFamily>(
        ticker,
        std::span<std::unique_ptr<motion::AnimatedFloat>>(inst.anims),
        std::span<const Lane>(prevLaneScratch),
        std::span<const Lane>(laneScratch), next.nodeTransition);
  } else {
    for (const Lane& lane : laneScratch)
      if (lane.value)
        motion::mountEntrance(ticker, inst.anims[lane.slot.index], *lane.value,
                              0.0f);
  }

  // The geometry slot's value type is the node's kind, so a change of
  // kind is a change of this field and nothing else: the entity and the
  // lanes above have already survived it.
  if (!prev || !(prev->geometry == next.geometry) ||
      prev->window.has_value() != next.window.has_value())
    inst.geometryDirty = true;

  staleBakesUp(&inst);
}

void Scene::Impl::reorder(Instance& parent, bool structureChanged) {
  // A mount, an unmount or a move changes what this subtree draws even
  // when every surviving child is identical, and the structural prune
  // must not swallow that.
  if (structureChanged) staleBakesUp(&parent);
}

void Scene::Impl::invalidate(Instance& inst) { staleBakesUp(&inst); }

void Scene::Impl::retire(Instance& inst) {
  for (std::unique_ptr<Instance>& child : inst.children)
    if (child) retire(*child);
  store.release(inst.resource);
  inst.resource = nullptr;
  if (registry.valid(inst.entity)) registry.destroy(inst.entity);
  inst.entity = entt::null;
}

void Scene::Impl::destroy(std::unique_ptr<Instance> inst, uint64_t) {
  retire(*inst);
  inst.reset();
}

void Scene::Impl::staleBakesUp(Instance* inst) {
  for (Instance* up = inst; up; up = up->parent) up->bakeStale = true;
}

void Scene::Impl::rebuildKeyIndex() {
  byKey.clear();
  stats.nodes = 0;
  if (!root) return;
  reconciler.indexKeys(*root, byKey, [this](Instance&) { ++stats.nodes; });
}

}  // namespace sigil::world
