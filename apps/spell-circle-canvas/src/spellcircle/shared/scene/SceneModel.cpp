#include "SceneModel.h"
#include "SpellCircle_generated.h"

#include <flatbuffers/flatbuffers.h>

#include <unordered_map>

namespace spellcircle {

namespace {

std::string utf8(const flatbuffers::String *string) {
  return string ? std::string(string->c_str(), string->size()) : std::string();
}

CircleComponent toCircleComponent(const SpellCircle::Circle *circle) {
  if (!circle)
    return {};
  return CircleComponent{
      .name = utf8(circle->name()),
      .centerX = circle->pos() ? circle->pos()->x() : 0.0f,
      .centerY = circle->pos() ? circle->pos()->y() : 0.0f,
      .radius = circle->radius(),
      .textStart = circle->text_start(),
      .active = circle->active(),
  };
}

/**
 * Returns the entity for `point`, creating it on first sight. FlatBuffers is
 * zero-copy, so if the packet embeds the same Point table under more than
 * one Edge/Box field (a vertex shared by several edges, say), every such
 * occurrence decodes to the same `point` pointer — `pointCache` lets those
 * occurrences resolve to one shared PointComponent entity instead of a
 * fresh duplicate per reference.
 */
entt::entity getOrCreatePointEntity(
    entt::registry &registry,
    std::unordered_map<const SpellCircle::Point *, entt::entity> &pointCache,
    const SpellCircle::Point *point) {
  if (!point)
    return entt::null;

  if (const auto cachedPoint = pointCache.find(point);
      cachedPoint != pointCache.end())
    return cachedPoint->second;

  const entt::entity entity = registry.create();
  registry.emplace<PointComponent>(
      entity, PointComponent{
                  .value = utf8(point->value()),
                  .circle = toCircleComponent(point->circle()),
                  .position = point->position(),
              });
  pointCache.emplace(point, entity);
  return entity;
}

} // namespace

bool verifyScenePayload(const void *payload, size_t size) {
  flatbuffers::Verifier verifier(static_cast<const uint8_t *>(payload), size);
  return SpellCircle::VerifySceneBuffer(verifier);
}

SceneStats SceneDocument::decode(const void *payload, size_t size) {
  static_cast<void>(size); // structural bounds were checked by the Verifier
  const auto *scene = SpellCircle::GetScene(payload);
  if (!scene)
    return {};

  m_registry.clear();
  m_sceneWidth = scene->width();
  m_sceneHeight = scene->height();

  SceneStats stats;

  // Shared across edges/boxes so a Point referenced by more than one of them
  // resolves to a single entity rather than being duplicated per reference.
  std::unordered_map<const SpellCircle::Point *, entt::entity> pointCache;

  if (scene->circles()) {
    for (const auto *circle : *scene->circles()) {
      const entt::entity entity = m_registry.create();
      m_registry.emplace<CircleComponent>(entity, toCircleComponent(circle));
      ++stats.circles;
    }
  }

  if (scene->edges()) {
    for (const auto *edge : *scene->edges()) {
      const entt::entity entity = m_registry.create();
      m_registry.emplace<EdgeComponent>(
          entity, EdgeComponent{
                      .first = getOrCreatePointEntity(m_registry, pointCache,
                                                      edge->first()),
                      .second = getOrCreatePointEntity(m_registry, pointCache,
                                                       edge->second()),
                  });
      ++stats.edges;
    }
  }

  if (scene->boxes()) {
    for (const auto *box : *scene->boxes()) {
      const entt::entity entity = m_registry.create();
      m_registry.emplace<BoxComponent>(
          entity, BoxComponent{
                      .value = utf8(box->value()),
                      .point = getOrCreatePointEntity(m_registry, pointCache,
                                                      box->point()),
                      .active = box->active(),
                  });
      ++stats.boxes;
    }
  }

  return stats;
}

void SceneDocument::clear() {
  m_registry.clear();
  m_sceneWidth = 0.0f;
  m_sceneHeight = 0.0f;
}

} // namespace spellcircle
