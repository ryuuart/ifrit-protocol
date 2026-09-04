/** @file
 * The Scene's public face: one frame through the declared phases, and
 * the handful of readings a caller takes off the result.
 */

#include <sigilcore/reconcile/Phases.h>

#include <span>
#include <string>
#include <utility>

#include "SceneImpl.h"

namespace sigil::world {

namespace {

/** The declared phase list. `describe` reconciles the tree the author
 *  handed over; `lanes` samples every binding once; `derive` resolves
 *  placements, and converges because a node's placement is read by
 *  everything under it; `extract` is the one crossing from the value
 *  tree into the retained state a draw reads; `graph` turns the frame's
 *  declarations into an order and gives its resources surfaces; and
 *  `execute` performs that order. The last two do nothing for a frame
 *  that declared no passes. */
constexpr core::Phase<Scene::Impl> kPhases[] = {
    {"describe", &Scene::Impl::phaseDescribe, false},
    {"lanes", &Scene::Impl::phaseLanes, false},
    {"derive", &Scene::Impl::phaseDerive, true},
    {"extract", &Scene::Impl::phaseExtract, false},
    {"graph", &Scene::Impl::phaseGraph, false},
    {"execute", &Scene::Impl::phaseExecute, false},
};

}  // namespace

Scene::Scene(motion::Ticker& ticker) : m_impl(std::make_unique<Impl>(ticker)) {}
Scene::Scene(Scene&&) noexcept = default;
Scene& Scene::operator=(Scene&&) noexcept = default;
Scene::~Scene() = default;

void Scene::render(const Frame& frame) {
  Impl& impl = *m_impl;
  impl.stats.reset();
  impl.error.clear();
  impl.frame = frame;
  impl.pending = frame.scene();
  impl.stats.rounds = core::runPhases(
      impl, std::span<const core::Phase<Impl>>(kPhases), kConvergeRounds,
      // What settles between rounds is the stability hold: `derive` has
      // just written new placements, and a node whose placement moved
      // must re-declare here, while the phase list is still ahead of
      // `extract`. A round that changed nothing moved no placement, so
      // the runner not calling this is the same answer as calling it.
      [&impl] { impl.rescanMoved(); });
  impl.stats.resources = (int64_t)impl.store.size();
  ++impl.frameIndex;
}

std::optional<geometry::mesh::camera::Camera> Scene::camera() const {
  return m_impl->camera;
}

std::vector<Light> Scene::lights() const { return m_impl->lights; }

uint64_t Scene::handleOf(std::string_view key) const {
  const auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end()) return 0;
  // Offset by one so that zero can mean "no such node" without
  // colliding with the first entity EnTT hands out.
  return (uint64_t)entt::to_integral(it->second->entity) + 1;
}

std::optional<glm::mat4> Scene::transformOf(std::string_view key) const {
  const auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end()) return std::nullopt;
  return it->second->world;
}

int Scene::referencesOf(std::string_view key) const {
  const auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end() || !it->second->resource) return 0;
  return it->second->resource->references;
}

const graph::Plan& Scene::plan() const { return m_impl->plan; }

Targets& Scene::targets() { return m_impl->targets; }

const std::string& Scene::error() const { return m_impl->error; }

const SceneStats& Scene::stats() const { return m_impl->stats; }

}  // namespace sigil::world
