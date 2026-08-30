#pragma once

/** @file
 * SigilWorld easel — the artist's stage for PLACING things in a World.
 * SigilGeometry's easel (sigilgeometry/easel/Easel.h) covers 2D marks; this one
 * covers the stage: a sun, an eye, registry lights, props, panels,
 * stamps, chains — every call reads like a sentence, defaults are loud, and
 * the whole description reconciles through the Scene layer on
 * commit().
 *
 *   auto stage = easel::stage(world);
 *   stage.sun({-.4f, -.8f, -.5f}, 2.4f)
 *        .light({200, 300, 0}, cyan, 3)
 *        .place(mesh, gold).at({0, 0, 0}).turned(30).key("star")
 *        .panel(image, 380, 252).at({0, 60, 0}).key("hud")
 *        .placeStamps(cloud, quadStamp, glowMat).key("sparks")
 *        .points(pop::on(loop).count(9000).noise(18), quadStamp, glowMat)
 *            .key("comet")
 *        .commit();
 *
 * at()/turned()/sized()/key() style the LAST declared placement — the
 * tail chaining that lets one expression stay one sentence.
 *
 * commit() reconciles against the previous commit and returns
 * scene::Scene::Stats: place()/panel() ride the Scene reconciler
 * (key-path identity; by-value meshes get content-hash identity, so
 * re-declaring the same mesh is a keep, not a re-upload), placeStamps()
 * maps to World::placeStamps/setStamps by key, placeChain() to
 * placeChain/setChain by key (an unchanged chain is a keep; a changed one
 * re-cooks, and World decides whether that is a parameter edit or a rebuild),
 * light()/beam() to LightComponent entities by declaration order. The
 * description is consumed by commit(): re-declare everything you want kept, and
 * KEEP the Stage alive across commits — a fresh Stage forgets what it placed.
 * sun()/sky()/look() are set-level state, applied on commit only when you
 * called them.
 */

#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/pop/Points.h>
#include <sigilgeometry/pop/Pop.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sigilworld/Components.h"
#include "sigilworld/Scene.h"
#include "sigilworld/World.h"

namespace sigil::world::easel {

namespace detail {

inline uint64_t hashBytes(uint64_t h, const void* data, size_t size) {
  const unsigned char* bytes = (const unsigned char*)data;
  for (size_t i = 0; i < size; ++i) {
    h ^= bytes[i];
    h *= 1099511628211ull;  // FNV-1a
  }
  return h;
}

/** Content identity for a by-value mesh — equal content means equal
 *  identity, so re-declared meshes reconcile as kept, not re-added. */
inline uint64_t fingerprint(const geometry::Mesh& mesh) {
  uint64_t h = 1469598103934665603ull;
  h = hashBytes(h, mesh.positions.data(),
                mesh.positions.size() * sizeof(glm::vec3));
  h = hashBytes(h, mesh.normals.data(),
                mesh.normals.size() * sizeof(glm::vec3));
  h = hashBytes(h, mesh.uvs.data(), mesh.uvs.size() * sizeof(glm::vec2));
  h = hashBytes(h, mesh.colors.data(), mesh.colors.size() * sizeof(glm::vec4));
  h = hashBytes(h, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
  return h;
}

/** Content identity for a stamps prop's points AS SEEN through its lanes —
 *  an unchanged cloud skips the instance re-upload entirely. */
inline uint64_t fingerprint(const geometry::Cloud& cloud,
                            const StampLanes& lanes) {
  uint64_t h = 1469598103934665603ull;
  h = hashBytes(h, cloud.positions.data(),
                cloud.positions.size() * sizeof(glm::vec3));
  h = hashBytes(h, &lanes.scale, sizeof(lanes.scale));
  h = hashBytes(h, &lanes.up, sizeof(lanes.up));
  h = hashBytes(h, lanes.scaleLane.data(), lanes.scaleLane.size());
  if (const std::vector<float>* lane = cloud.scalarIf(lanes.scaleLane))
    h = hashBytes(h, lane->data(), lane->size() * sizeof(float));
  h = hashBytes(h, lanes.tintLane.data(), lanes.tintLane.size());
  if (const std::vector<glm::vec4>* lane = cloud.colorIf(lanes.tintLane))
    h = hashBytes(h, lane->data(), lane->size() * sizeof(glm::vec4));
  h = hashBytes(h, lanes.orientLane.data(), lanes.orientLane.size());
  if (const std::vector<glm::vec3>* lane = cloud.vectorIf(lanes.orientLane))
    h = hashBytes(h, lane->data(), lane->size() * sizeof(glm::vec3));
  return h;
}

}  // namespace detail

/** The set: lights, camera and a declared scene, applied together on
 *  commit. Stage is the reach-for layer over World and Scene — it
 *  holds the whole shot as dials, so a sketch can describe the frame
 *  it wants rather than sequencing the calls that build it. */
class Stage {
 public:
  explicit Stage(World& world) : m_world(world), m_scene(world) {}

  // -- the set's light and eye, applied on commit ---------------------------

  Stage& sun(glm::vec3 direction, float intensity = 2.6f,
             glm::vec4 color = {1.0f, 0.96f, 0.9f, 1}) {
    m_lighting.sunDirection = direction;
    m_lighting.sunIntensity = intensity;
    m_lighting.sunColor = color;
    m_lightingDirty = true;
    return *this;
  }
  Stage& sky(glm::vec4 sky, glm::vec4 ground, float ambient = 0.55f) {
    m_lighting.skyColor = sky;
    m_lighting.groundColor = ground;
    m_lighting.ambient = ambient;
    m_lightingDirty = true;
    return *this;
  }
  Stage& look(glm::vec3 eye, glm::vec3 target = {0, 0, 0}, float fovYDeg = 40) {
    m_camera.eye = eye;
    m_camera.target = target;
    m_camera.fovYDeg = fovYDeg;
    m_cameraDirty = true;
    return *this;
  }

  // -- registry lights, reconciled by declaration order ---------------------

  /** A point light hovering at @p position. */
  Stage& light(glm::vec3 position, glm::vec4 color = {1, 1, 1, 1},
               float intensity = 3, float range = 600) {
    LightComponent value;
    value.type = LightComponent::Type::Point;
    value.position = position;
    value.color = color;
    value.intensity = intensity;
    value.range = range;
    m_pendingLights.push_back(value);
    return *this;
  }
  /** A directional light shining along @p direction. */
  Stage& beam(glm::vec3 direction, glm::vec4 color = {1, 1, 1, 1},
              float intensity = 2) {
    LightComponent value;
    value.type = LightComponent::Type::Directional;
    value.direction = direction;
    value.color = color;
    value.intensity = intensity;
    m_pendingLights.push_back(value);
    return *this;
  }

  // -- placements -----------------------------------------------------------

  /** A prop with stable pointer identity — share the shared_ptr across
   *  commits and only transforms are ever touched. */
  Stage& place(std::shared_ptr<const geometry::Mesh> mesh, Material material) {
    Placement p;
    p.kind = Placement::Kind::Prop;
    p.mesh = std::move(mesh);
    p.material = std::move(material);
    m_pending.push_back(std::move(p));
    return *this;
  }
  /** A prop by value — content-hashed, so re-declaring the same mesh
   *  keeps its prop; a changed mesh re-uploads. */
  Stage& place(geometry::Mesh mesh, Material material) {
    Placement p;
    p.kind = Placement::Kind::Prop;
    p.fingerprint = detail::fingerprint(mesh);
    p.value = std::move(mesh);
    p.material = std::move(material);
    m_pending.push_back(std::move(p));
    return *this;
  }
  /** An unlit textured quad — the diegetic UI card. */
  Stage& panel(sk_sp<SkImage> image, float width, float height) {
    Placement p;
    p.kind = Placement::Kind::Panel;
    p.image = std::move(image);
    p.width = width;
    p.height = height;
    m_pending.push_back(std::move(p));
    return *this;
  }
  /** @p stamp instanced at every point of @p cloud in ONE draw
   *  (World::placeStamps). An unchanged cloud is a keep; a changed
   *  one refreshes instances in place (setStamps). */
  Stage& placeStamps(geometry::Cloud cloud, geometry::Mesh stamp,
                     Material material, StampLanes lanes = {}) {
    Placement p;
    p.kind = Placement::Kind::Stamps;
    p.cloud = std::move(cloud);
    p.value = std::move(stamp);
    p.fingerprint = detail::fingerprint(p.value);
    p.material = std::move(material);
    p.lanes = std::move(lanes);
    m_pending.push_back(std::move(p));
    return *this;
  }

  /** A GPU-cooked point chain, @p stamp drawn at every cooked point
   *  (World::placeChain). The chain is compared by value on the next
   *  commit: unchanged is a keep, changed goes to setChain — the
   *  window slide, the deformer amount, the mask, whatever moved. A
   *  chain the GPU executor declines is not placed at all. */
  Stage& placeChain(geometry::pop::Chain chain, geometry::Mesh stamp,
                    Material material) {
    Placement p;
    p.kind = Placement::Kind::Chain;
    p.chain = std::move(chain);
    p.value = std::move(stamp);
    p.fingerprint = detail::fingerprint(p.value);
    p.material = std::move(material);
    m_pending.push_back(std::move(p));
    return *this;
  }

  // -- tail styling: each call shapes the LAST declared placement -----------

  Stage& at(glm::vec3 position) {
    if (!m_pending.empty()) m_pending.back().position = position;
    return *this;
  }
  Stage& turned(float yawDeg, float pitchDeg = 0, float rollDeg = 0) {
    if (!m_pending.empty()) {
      Placement& p = m_pending.back();
      p.yawDeg = yawDeg;
      p.pitchDeg = pitchDeg;
      p.rollDeg = rollDeg;
    }
    return *this;
  }
  Stage& sized(float scale) {
    if (!m_pending.empty()) m_pending.back().scale = scale;
    return *this;
  }
  Stage& key(std::string name) {
    if (!m_pending.empty()) m_pending.back().key = std::move(name);
    return *this;
  }

  // -- the reconcile --------------------------------------------------------

  /** Apply pending sun/sky/look, then reconcile lights, placements,
   *  stamps and chains against the previous commit. Returns the merged Stats
   *  (lights, stamps and chains count in the same added/removed/moved/kept
   *  vocabulary). Consumes the description — re-declare next frame. */
  scene::Scene::Stats commit() {
    if (m_lightingDirty) {
      m_world.setLighting(m_lighting);
      m_lightingDirty = false;
    }
    if (m_cameraDirty) {
      m_world.setCamera(m_camera);
      m_cameraDirty = false;
    }

    scene::Scene::Stats stats;
    reconcileLights(stats);

    // Scene-side placements keep declaration order so unkeyed nodes
    // fall back to stable child indices.
    scene::Node root = scene::group().key("easel");
    int childIndex = 0;
    for (Placement& p : m_pending) {
      if (p.kind == Placement::Kind::Stamps || p.kind == Placement::Kind::Chain)
        continue;
      scene::Node node =
          p.kind == Placement::Kind::Panel
              ? scene::panel(p.image, p.width, p.height)
              : scene::place(resolveMesh(p, childIndex), p.material);
      node.key(p.key)
          .at(p.position)
          .rotated(p.yawDeg, p.pitchDeg, p.rollDeg)
          .scaled(p.scale);
      root.child(std::move(node));
      ++childIndex;
    }
    const scene::Scene::Stats sceneStats = m_scene.render(root);
    stats.added += sceneStats.added;
    stats.removed += sceneStats.removed;
    stats.moved += sceneStats.moved;
    stats.kept += sceneStats.kept;

    reconcileStamps(stats);
    reconcilePoints(stats);

    m_pending.clear();
    m_pendingLights.clear();
    return stats;
  }

  /** Forget everything the stage placed (props, stamps, chains, lights). */
  void clear() {
    m_scene.clear();
    for (auto& [key, entry] : m_stamps) m_world.remove(entry.id);
    m_stamps.clear();
    for (auto& [key, entry] : m_points) m_world.remove(entry.id);
    m_points.clear();
    for (uint32_t id : m_lightIds)
      if (m_world.registry().valid(entity(id)))
        m_world.registry().destroy(entity(id));
    m_lightIds.clear();
    m_meshes.clear();
    m_pending.clear();
    m_pendingLights.clear();
  }

 private:
  struct Placement {
    enum class Kind : uint8_t { Prop, Panel, Stamps, Chain };
    Kind kind = Kind::Prop;
    std::string key;
    glm::vec3 position = {0, 0, 0};
    float yawDeg = 0, pitchDeg = 0, rollDeg = 0;
    float scale = 1;
    std::shared_ptr<const geometry::Mesh> mesh;  // Prop, shared identity
    geometry::Mesh value;                        // Prop by value / stamp
    uint64_t fingerprint = 0;
    Material material;
    sk_sp<SkImage> image;  // Panel
    float width = 0, height = 0;
    geometry::Cloud cloud;       // Stamps
    StampLanes lanes;            // Stamps
    geometry::pop::Chain chain;  // Points
  };
  struct CachedMesh {
    uint64_t fingerprint = 0;
    std::shared_ptr<const geometry::Mesh> mesh;
  };
  struct StampsEntry {
    uint32_t id = 0;
    uint64_t stampFingerprint = 0;
    uint64_t cloudFingerprint = 0;
    Material material;
    glm::mat4 world{1.0f};
    bool visited = false;
  };
  struct PointsEntry {
    uint32_t id = 0;
    uint64_t stampFingerprint = 0;
    geometry::pop::Chain chain;
    Material material;
    glm::mat4 world{1.0f};
    bool visited = false;
  };

  /** By-value meshes reconcile by content hash under the placement's
   *  key (or child index): same content, same shared_ptr, so the Scene
   *  sees stable pointer identity. */
  std::shared_ptr<const geometry::Mesh> resolveMesh(Placement& p,
                                                    int childIndex) {
    if (p.mesh) return p.mesh;
    CachedMesh& cached =
        m_meshes[p.key.empty() ? "#" + std::to_string(childIndex) : p.key];
    if (!cached.mesh || cached.fingerprint != p.fingerprint) {
      cached.mesh = std::make_shared<const geometry::Mesh>(std::move(p.value));
      cached.fingerprint = p.fingerprint;
    }
    return cached.mesh;
  }

  void reconcileLights(scene::Scene::Stats& stats) {
    entt::registry& registry = m_world.registry();
    const size_t oldCount = m_lightIds.size();
    const size_t common = std::min(oldCount, m_pendingLights.size());
    for (size_t i = 0; i < common; ++i) {
      const entt::entity e = entity(m_lightIds[i]);
      if (!registry.valid(e) || !registry.all_of<LightComponent>(e)) {
        m_lightIds[i] = m_world.addLight(m_pendingLights[i]);
        ++stats.added;
        continue;
      }
      LightComponent& live = registry.get<LightComponent>(e);
      if (live == m_pendingLights[i]) {
        ++stats.kept;
      } else {
        live = m_pendingLights[i];
        ++stats.moved;
      }
    }
    // Surplus previous lights leave; extra declared lights arrive.
    for (size_t i = common; i < oldCount; ++i) {
      const entt::entity e = entity(m_lightIds[i]);
      if (registry.valid(e)) registry.destroy(e);
      ++stats.removed;
    }
    m_lightIds.resize(common);
    for (size_t i = common; i < m_pendingLights.size(); ++i) {
      m_lightIds.push_back(m_world.addLight(m_pendingLights[i]));
      ++stats.added;
    }
  }

  void reconcileStamps(scene::Scene::Stats& stats) {
    for (auto& [key, entry] : m_stamps) entry.visited = false;

    int stampsIndex = 0;
    for (Placement& p : m_pending) {
      if (p.kind != Placement::Kind::Stamps) continue;
      const std::string key =
          p.key.empty() ? "~#" + std::to_string(stampsIndex) : p.key;
      ++stampsIndex;
      const uint64_t cloudFp = detail::fingerprint(p.cloud, p.lanes);
      const glm::mat4 world = scene::group()
                                  .at(p.position)
                                  .rotated(p.yawDeg, p.pitchDeg, p.rollDeg)
                                  .scaled(p.scale)
                                  .localMatrix();

      auto it = m_stamps.find(key);
      if (it != m_stamps.end() &&
          it->second.stampFingerprint == p.fingerprint &&
          it->second.material == p.material) {
        StampsEntry& entry = it->second;
        entry.visited = true;
        bool touched = false;
        if (entry.cloudFingerprint != cloudFp) {
          m_world.setStamps(entry.id, p.cloud, p.lanes);
          entry.cloudFingerprint = cloudFp;
          touched = true;
        }
        if (!(entry.world == world)) {
          m_world.setTransform(entry.id, world);
          entry.world = world;
          touched = true;
        }
        touched ? ++stats.moved : ++stats.kept;
        continue;
      }
      if (it != m_stamps.end()) {  // stamp or material changed: rebuild
        m_world.remove(it->second.id);
        m_stamps.erase(it);
        ++stats.removed;
      }
      StampsEntry entry;
      entry.id = m_world.placeStamps(p.value, p.cloud, p.material, p.lanes);
      if (entry.id == 0) continue;
      m_world.setTransform(entry.id, world);
      entry.stampFingerprint = p.fingerprint;
      entry.cloudFingerprint = cloudFp;
      entry.material = p.material;
      entry.world = world;
      entry.visited = true;
      m_stamps.emplace(key, std::move(entry));
      ++stats.added;
    }

    for (auto it = m_stamps.begin(); it != m_stamps.end();) {
      if (!it->second.visited) {
        m_world.remove(it->second.id);
        it = m_stamps.erase(it);
        ++stats.removed;
      } else {
        ++it;
      }
    }
  }

  void reconcilePoints(scene::Scene::Stats& stats) {
    for (auto& [key, entry] : m_points) entry.visited = false;

    int index = 0;
    for (Placement& p : m_pending) {
      if (p.kind != Placement::Kind::Chain) continue;
      const std::string key =
          p.key.empty() ? "~pop#" + std::to_string(index) : p.key;
      ++index;
      const glm::mat4 world = scene::group()
                                  .at(p.position)
                                  .rotated(p.yawDeg, p.pitchDeg, p.rollDeg)
                                  .scaled(p.scale)
                                  .localMatrix();
      auto it = m_points.find(key);
      if (it != m_points.end() &&
          it->second.stampFingerprint == p.fingerprint &&
          it->second.material == p.material) {
        PointsEntry& entry = it->second;
        entry.visited = true;
        bool touched = false;
        if (!(entry.chain == p.chain)) {
          m_world.setChain(entry.id, p.chain);
          entry.chain = p.chain;
          touched = true;
        }
        if (!(entry.world == world)) {
          m_world.setTransform(entry.id, world);
          entry.world = world;
          touched = true;
        }
        touched ? ++stats.moved : ++stats.kept;
        continue;
      }
      if (it != m_points.end()) {  // stamp or material changed: rebuild
        m_world.remove(it->second.id);
        m_points.erase(it);
        ++stats.removed;
      }
      PointsEntry entry;
      entry.id = m_world.placeChain(p.value, p.chain, p.material);
      if (entry.id == 0) continue;  // declined by the executor
      m_world.setTransform(entry.id, world);
      entry.stampFingerprint = p.fingerprint;
      entry.chain = p.chain;
      entry.material = p.material;
      entry.world = world;
      entry.visited = true;
      m_points.emplace(key, std::move(entry));
      ++stats.added;
    }

    for (auto it = m_points.begin(); it != m_points.end();) {
      if (!it->second.visited) {
        m_world.remove(it->second.id);
        it = m_points.erase(it);
        ++stats.removed;
      } else {
        ++it;
      }
    }
  }

  World& m_world;
  scene::Scene m_scene;
  Lighting m_lighting;
  geometry::space::Camera m_camera;
  bool m_lightingDirty = false;
  bool m_cameraDirty = false;
  std::vector<Placement> m_pending;
  std::vector<LightComponent> m_pendingLights;
  std::vector<uint32_t> m_lightIds;
  std::map<std::string, CachedMesh> m_meshes;
  std::map<std::string, StampsEntry> m_stamps;
  std::map<std::string, PointsEntry> m_points;
};

inline Stage stage(World& world) { return Stage(world); }

}  // namespace sigil::world::easel
