#include "sigilworld/Scene.h"

#include <include/core/SkTypes.h>  // SkDebugf — the outrank diagnostic
#include <sigilgeometry/Mesh.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilworld/Animation.h"
#include "sigilworld/Components.h"

namespace sigil::world::scene {

namespace {
constexpr float kDegToRad = (float)M_PI / 180.0f;
}

glm::mat4 Node::localMatrix() const {
  glm::mat4 m = glm::translate(glm::mat4(1.0f), m_position);
  if (m_yawDeg != 0)
    m = glm::rotate(m, m_yawDeg * kDegToRad, glm::vec3{0, 1, 0});
  if (m_pitchDeg != 0)
    m = glm::rotate(m, m_pitchDeg * kDegToRad, glm::vec3{1, 0, 0});
  if (m_rollDeg != 0)
    m = glm::rotate(m, m_rollDeg * kDegToRad, glm::vec3{0, 0, 1});
  if (m_scale != 1) m = glm::scale(m, glm::vec3{m_scale});
  if (m_hasExtra) m *= m_extra;
  return m;
}

Node place(std::shared_ptr<const geometry::Mesh> mesh, Material material) {
  Node node;
  node.m_kind = Node::Kind::Prop;
  node.m_mesh = std::move(mesh);
  node.m_material = std::move(material);
  return node;
}

Node panel(sk_sp<SkImage> image, float width, float height) {
  Node node;
  node.m_kind = Node::Kind::Panel;
  node.m_panelWidth = width;
  node.m_panelHeight = height;
  node.m_material.unlit = true;
  node.m_material.texture = std::move(image);
  return node;
}

Scene::Stats Scene::render(const Node& root) {
  Stats stats;
  for (auto& [key, entry] : m_entries) entry.visited = false;

  // Depth-first flatten with accumulated transforms and key paths.
  struct Visit {
    const Node* node;
    glm::mat4 parent{1.0f};
    std::string path;
  };
  std::vector<Visit> stack;
  stack.push_back({&root, glm::mat4(1.0f), ""});

  while (!stack.empty()) {
    Visit visit = stack.back();
    stack.pop_back();
    const Node& node = *visit.node;

    const glm::mat4 world = visit.parent * node.localMatrix();
    std::string path =
        visit.path + "/" + (node.m_key.empty() ? "@" : node.m_key);

    if (node.m_kind != Node::Kind::Group) {
      // Panels resolve their cached quad mesh here.
      const geometry::Mesh* mesh = node.m_mesh.get();
      Material material = node.m_material;
      if (node.m_kind == Node::Kind::Panel) {
        std::shared_ptr<const geometry::Mesh>& quad =
            m_quads[{node.m_panelWidth, node.m_panelHeight}];
        if (!quad)
          quad = std::make_shared<const geometry::Mesh>(
              geometry::mesh::quad(node.m_panelWidth, node.m_panelHeight));
        mesh = quad.get();
      }
      if (mesh) {
        auto it = m_entries.find(path);
        if (it != m_entries.end() && it->second.mesh == mesh &&
            it->second.material == material) {
          Entry& entry = it->second;
          entry.visited = true;
          if (!(entry.world == world)) {
            m_world.setTransform(entry.id, world);
            entry.world = world;
            ++stats.moved;
          } else {
            ++stats.kept;
          }
          // The kept entity may carry a live Animated* that outranks
          // what this description just said — say so, once, loudly.
          warnIfOutranked(path, entry.id);
        } else {
          if (it != m_entries.end()) {
            m_world.remove(it->second.id);
            m_entries.erase(it);
            m_warnedOutranked.erase(path);
            ++stats.removed;
          }
          Entry entry;
          entry.id = m_world.place(*mesh, world, material);
          entry.mesh = mesh;
          entry.material = std::move(material);
          entry.world = world;
          entry.visited = true;
          if (entry.id != 0) {
            m_entries.emplace(path, std::move(entry));
            ++stats.added;
          }
        }
      }
    }

    // Children in reverse so unkeyed indices read in declaration order.
    for (size_t i = node.m_children.size(); i > 0; --i) {
      const Node& child = node.m_children[i - 1];
      std::string childPath = path;
      if (child.m_key.empty()) childPath += "/#" + std::to_string(i - 1);
      stack.push_back({&child, world, std::move(childPath)});
    }
  }

  // Anything not revisited left the description.
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (!it->second.visited) {
      m_world.remove(it->second.id);
      m_warnedOutranked.erase(it->first);
      it = m_entries.erase(it);
      ++stats.removed;
    } else {
      ++it;
    }
  }
  return stats;
}

std::optional<entt::entity> Scene::find(std::string_view keyPath) const {
  if (keyPath.empty()) return std::nullopt;
  // The internal bookkeeping key always starts with '/'; accept the
  // caller's "comp/sky" as the same path. No parallel resolution — this
  // IS the map render() diffs by.
  std::string path;
  path.reserve(keyPath.size() + 1);
  if (keyPath.front() != '/') path += '/';
  path += keyPath;
  const auto it = m_entries.find(path);
  if (it == m_entries.end()) return std::nullopt;
  return entity(it->second.id);
}

void Scene::warnIfOutranked(const std::string& path, uint32_t id) {
  entt::registry& reg = m_world.registry();
  const entt::entity e = entity(id);
  // O(1) per kept leaf: two component probes, no registry scan.
  const bool transformOutranked = reg.any_of<AnimatedTransform>(e);
  const AnimatedMaterial* mat = reg.try_get<AnimatedMaterial>(e);
  const bool materialOutranked =
      mat && (mat->opacity || mat->emissiveStrength || mat->uvOffsetX ||
              mat->uvOffsetY || mat->uvScaleX || mat->uvScaleY);
  if (!transformOutranked && !materialOutranked) return;
  if (!m_warnedOutranked.insert(path).second)
    return;  // once per node, not per frame
  const char* outranker =
      transformOutranked
          ? (materialOutranked ? "AnimatedTransform + AnimatedMaterial"
                               : "AnimatedTransform")
          : "AnimatedMaterial";
  SkDebugf(
      "[world] scene node \"%s\" was kept, but a live %s OUTRANKS "
      "what it declares: resolveAnimation() rewrites that state "
      "after every render(), so the surface follows the lane while "
      "Stats reports kept/moved. Drop the lane, or own the node's "
      "motion deliberately through Scene::find() — and re-attach "
      "after a recreate, which destroys the entity and the lane "
      "with it. See world/README.md, \"Scene and Animation do not "
      "meet\".\n",
      path.c_str(), outranker);
}

void Scene::clear() {
  for (auto& [key, entry] : m_entries) m_world.remove(entry.id);
  m_entries.clear();
  m_warnedOutranked.clear();
}

}  // namespace sigil::world::scene
