#include "sigilworld/Scene.h"

#include <sigilshape/Mesh.h>

#include <cmath>

namespace sigil::world::scene {

namespace {
constexpr float kDegToRad = (float)M_PI / 180.0f;
}

SkM44 Node::localMatrix() const {
  SkM44 m = SkM44::Translate(m_position.x, m_position.y, m_position.z);
  if (m_yawDeg != 0)
    m.preConcat(SkM44::Rotate({0, 1, 0}, m_yawDeg * kDegToRad));
  if (m_pitchDeg != 0)
    m.preConcat(SkM44::Rotate({1, 0, 0}, m_pitchDeg * kDegToRad));
  if (m_rollDeg != 0)
    m.preConcat(SkM44::Rotate({0, 0, 1}, m_rollDeg * kDegToRad));
  if (m_scale != 1)
    m.preScale(m_scale, m_scale, m_scale);
  if (m_hasExtra)
    m.preConcat(m_extra);
  return m;
}

Node surface(std::shared_ptr<const shape::Mesh> mesh, Material material) {
  Node node;
  node.m_kind = Node::Kind::Surface;
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

Scene::Stats Scene::render(const Node &root) {
  Stats stats;
  for (auto &[key, entry] : m_entries)
    entry.visited = false;

  // Depth-first flatten with accumulated transforms and key paths.
  struct Visit {
    const Node *node;
    SkM44 parent;
    std::string path;
  };
  std::vector<Visit> stack;
  stack.push_back({&root, SkM44(), ""});

  while (!stack.empty()) {
    Visit visit = stack.back();
    stack.pop_back();
    const Node &node = *visit.node;

    SkM44 world = visit.parent;
    world.preConcat(node.localMatrix());
    std::string path = visit.path + "/" +
                       (node.m_key.empty() ? "@" : node.m_key);

    if (node.m_kind != Node::Kind::Group) {
      // Panels resolve their cached quad mesh here.
      const shape::Mesh *mesh = node.m_mesh.get();
      Material material = node.m_material;
      if (node.m_kind == Node::Kind::Panel) {
        std::shared_ptr<const shape::Mesh> &quad =
            m_quads[{node.m_panelWidth, node.m_panelHeight}];
        if (!quad)
          quad = std::make_shared<const shape::Mesh>(
              shape::mesh::quad(node.m_panelWidth, node.m_panelHeight));
        mesh = quad.get();
      }
      if (mesh) {
        auto it = m_entries.find(path);
        if (it != m_entries.end() && it->second.mesh == mesh &&
            it->second.material == material) {
          Entry &entry = it->second;
          entry.visited = true;
          if (!(entry.world == world)) {
            m_world.setTransform(entry.id, world);
            entry.world = world;
            ++stats.moved;
          } else {
            ++stats.kept;
          }
        } else {
          if (it != m_entries.end()) {
            m_world.removeSurface(it->second.id);
            m_entries.erase(it);
            ++stats.removed;
          }
          Entry entry;
          entry.id = m_world.addSurface(*mesh, world, material);
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
      const Node &child = node.m_children[i - 1];
      std::string childPath = path;
      if (child.m_key.empty())
        childPath += "/#" + std::to_string(i - 1);
      stack.push_back({&child, world, std::move(childPath)});
    }
  }

  // Anything not revisited left the description.
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (!it->second.visited) {
      m_world.removeSurface(it->second.id);
      it = m_entries.erase(it);
      ++stats.removed;
    } else {
      ++it;
    }
  }
  return stats;
}

void Scene::clear() {
  for (auto &[key, entry] : m_entries)
    m_world.removeSurface(entry.id);
  m_entries.clear();
}

} // namespace sigil::world::scene
