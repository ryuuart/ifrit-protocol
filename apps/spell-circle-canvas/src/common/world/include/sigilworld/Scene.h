#pragma once

/** @file
 * SigilWorld scene — SigilCompose's core lesson applied to 3D: DECLARE
 * the scene as a value tree, let a reconciler own the device objects.
 * No kernel import — no layout, no decorations, no timelines — just
 * the part that made compose pleasant: describe, diff, and only what
 * changed touches the GPU.
 *
 *   scene::Node root = scene::group().key("set")
 *     .child(scene::surface(floorMesh, steel).key("floor").at({0,-190,0}))
 *     .child(scene::group().key("hud").at({0,60,0}).rotated(spin)
 *       .child(scene::panel(cardImage, 380, 252).key("left").at({-420,0,0})
 *         ...
 *   scene.render(root);   // add/remove/move against the last render
 *
 * Identity is the KEY PATH (parent keys joined; unkeyed children fall
 * back to their index). A leaf re-uses its device surface when its
 * mesh POINTER and material compare equal — hold meshes in
 * shared_ptrs (or keep the Node tree and rebuild transforms only) and
 * transform-only changes cost setTransform, never re-upload. panel()
 * quads get per-size cached meshes inside the Scene, so panels are
 * stable by construction.
 *
 * render() returns Stats {added, removed, moved, kept} — the same
 * pruning visibility compose's ledgers taught us to demand.
 */

#include "sigilworld/World.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sigil::world::scene {

class Node {
public:
  enum class Kind : uint8_t { Group, Surface, Panel };

  Node &key(std::string value) {
    m_key = std::move(value);
    return *this;
  }
  Node &at(glm::vec3 position) {
    m_position = position;
    return *this;
  }
  Node &rotated(float yawDeg, float pitchDeg = 0, float rollDeg = 0) {
    m_yawDeg = yawDeg;
    m_pitchDeg = pitchDeg;
    m_rollDeg = rollDeg;
    return *this;
  }
  Node &scaled(float scale) {
    m_scale = scale;
    return *this;
  }
  /** Extra local matrix, applied after at/rotated/scaled. */
  Node &transform(const glm::mat4 &m) {
    m_extra = m;
    m_hasExtra = true;
    return *this;
  }
  Node &material(const Material &value) {
    m_material = value;
    return *this;
  }
  Node &child(Node node) {
    m_children.push_back(std::move(node));
    return *this;
  }

  /** The node's local matrix (translate * yaw * pitch * roll * scale
   *  * extra). */
  glm::mat4 localMatrix() const;

private:
  friend Node group();
  friend Node surface(std::shared_ptr<const shape::Mesh>, Material);
  friend Node panel(sk_sp<SkImage>, float, float);
  friend class Scene;

  Kind m_kind = Kind::Group;
  std::string m_key;
  glm::vec3 m_position = {0, 0, 0};
  float m_yawDeg = 0, m_pitchDeg = 0, m_rollDeg = 0;
  float m_scale = 1;
  glm::mat4 m_extra{1.0f};
  bool m_hasExtra = false;
  std::shared_ptr<const shape::Mesh> m_mesh; // Surface
  Material m_material;                       // Surface + Panel
  float m_panelWidth = 0, m_panelHeight = 0; // Panel
  std::vector<Node> m_children;
};

inline Node group() { return Node(); }

/** A mesh + material leaf. Identity for reuse is the mesh POINTER —
 *  share the shared_ptr across renders. */
Node surface(std::shared_ptr<const shape::Mesh> mesh, Material material);
inline Node surface(shape::Mesh mesh, Material material) {
  return surface(std::make_shared<const shape::Mesh>(std::move(mesh)),
                 std::move(material));
}

/** An unlit textured quad — the diegetic UI card. The quad mesh is
 *  cached per size inside the Scene, so panels stay identity-stable. */
Node panel(sk_sp<SkImage> image, float width, float height);

class Scene {
public:
  explicit Scene(World &world) : m_world(world) {}

  struct Stats {
    int added = 0;
    int removed = 0;
    int moved = 0;
    int kept = 0;
  };

  /** Reconcile the declared tree against the last render. */
  Stats render(const Node &root);

  /** Forget everything (removes all scene-owned surfaces). */
  void clear();

private:
  struct Entry {
    uint32_t id = 0;
    const shape::Mesh *mesh = nullptr;
    Material material;
    glm::mat4 world{1.0f};
    bool visited = false;
  };

  World &m_world;
  std::map<std::string, Entry> m_entries;
  std::map<std::pair<float, float>,
           std::shared_ptr<const shape::Mesh>>
      m_quads;
};

} // namespace sigil::world::scene
