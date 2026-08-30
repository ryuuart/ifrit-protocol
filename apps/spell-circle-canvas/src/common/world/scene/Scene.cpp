/** @file
 * The Scene's public face: one frame through the declared phases, and
 * the handful of readings a caller takes off the result.
 */

#include <sigilcore/reconcile/Phases.h>

#include <span>
#include <utility>

#include "SceneImpl.h"

namespace sigil::world {

namespace {

/** The declared phase list. `describe` reconciles the tree the author
 *  handed over; `lanes` samples every binding once; `derive` resolves
 *  placements, and converges because a node's placement is read by
 *  everything under it; `extract` is the one crossing from the value
 *  tree into the retained state a draw reads. */
constexpr core::Phase<Scene::Impl> kPhases[] = {
    {"describe", &Scene::Impl::phaseDescribe, false},
    {"lanes", &Scene::Impl::phaseLanes, false},
    {"derive", &Scene::Impl::phaseDerive, true},
    {"extract", &Scene::Impl::phaseExtract, false},
};

}  // namespace

Scene::Scene(motion::Ticker& ticker) : m_impl(std::make_unique<Impl>(ticker)) {}
Scene::Scene(Scene&&) noexcept = default;
Scene& Scene::operator=(Scene&&) noexcept = default;
Scene::~Scene() = default;

void Scene::render(const Element& root) {
  Impl& impl = *m_impl;
  impl.stats.reset();
  impl.pending = root;
  impl.stats.rounds = core::runPhases(
      impl, std::span<const core::Phase<Impl>>(kPhases), kConvergeRounds,
      // Nothing settles between rounds: derive is the only converging
      // pass, it is idempotent, and it reads only its own output and its
      // parent's.
      [] {});
  impl.stats.resources = (int64_t)impl.store.size();
}

std::optional<Camera> Scene::camera() const { return m_impl->camera; }

std::vector<Light> Scene::lights() const { return m_impl->lights; }

uint64_t Scene::handleOf(std::string_view key) const {
  const auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end()) return 0;
  // Offset by one so that zero can mean "no such node" without
  // colliding with the first entity EnTT hands out.
  return (uint64_t)entt::to_integral(it->second->entity) + 1;
}

std::optional<glm::mat4> Scene::transformOf(std::string_view key) const {
  const auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end()) return std::nullopt;
  return it->second->world;
}

int Scene::referencesOf(std::string_view key) const {
  const auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end() || !it->second->resource) return 0;
  return it->second->resource->references;
}

const SceneStats& Scene::stats() const { return m_impl->stats; }

}  // namespace sigil::world
