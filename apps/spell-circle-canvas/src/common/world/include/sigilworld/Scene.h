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

#include <include/core/SkSize.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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

/** A layer stack's pixel DENSITY — the one number that closes panel
 *  sizing (README "Layers at depth"): every image through the same
 *  Stack lands at image_px / pxPerWu world units, so aspect is correct
 *  by construction and two layers' world sizes stay in the exact ratio
 *  of their pixel sizes — the scale an aspect-deriving overload cannot
 *  know. Additive: `panel(image, w, h)` spellings are untouched, and a
 *  Stack panel IS that spelling with the arithmetic done — identical
 *  arithmetic means the reconciler sees the identical surface. */
struct Stack {
  float pxPerWu = 1;

  /** World size of @p image at this density. */
  SkSize size(const SkImage &image) const {
    return SkSize::Make((float)image.width() / pxPerWu,
                        (float)image.height() / pxPerWu);
  }
  /** `panel(image, image->width()/pxPerWu, image->height()/pxPerWu)`. */
  Node panel(sk_sp<SkImage> image) const {
    const SkSize s = image ? size(*image) : SkSize::MakeEmpty();
    return scene::panel(std::move(image), s.width(), s.height());
  }
};

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

  /** PUBLISH IDENTITY — the light door of the Scene × Animation ruling
   *  (2026-08-04; README "Scene and Animation do not meet"): resolve a
   *  key path to the live registry entity behind that node, so an
   *  `Animated*` lane lands on a declared leaf ON PURPOSE instead of
   *  by iterating the registry around the reconciler's back.
   *
   *  The path is the reconciler's OWN identity spelling — the same
   *  bookkeeping key `render()` diffs by: keys joined with '/', so
   *  `group().key("comp").child(panel(...).key("sky"))` answers to
   *  "comp/sky" (the internal form's leading '/' is also accepted).
   *  Unkeyed children carry the reconciler's index fallback in that
   *  spelling too ("#<index>" for the hop, "@" for the unkeyed node
   *  itself) — but key what you intend to animate. Only leaves have an
   *  entity; a group path, an unknown path, or a leaf whose addSurface
   *  failed answers an empty optional, never a throw.
   *
   *  Returns `entt::entity` because that is the currency a lane takes:
   *  `registry().emplace<AnimatedTransform>(*scene.find("comp/sky"))`
   *  composes directly with World::registry() and the Animated*
   *  components. A World surface id is the SAME value one documented
   *  cast away — `(uint32_t)e` / `world::entity(id)`, the bijection in
   *  Components.h — so the setter spelling costs nothing.
   *
   *  LIFETIME: the entity is valid until the next render() that
   *  RECREATES or REMOVES the node. A leaf whose mesh pointer or
   *  material changed is remove+add — `registry.destroy()` of this
   *  entity and every component on it, your lanes included. After such
   *  a render, find() returns the NEW entity: re-attach your lanes. A
   *  kept (or transform-only moved) leaf keeps its entity — and when a
   *  kept leaf's declared placement or material is outranked by a live
   *  Animated* component, render() says so once per node (see the
   *  README section). */
  std::optional<entt::entity> find(std::string_view keyPath) const;

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

  /** The loud half of the light door: a kept/moved leaf whose declared
   *  placement or material a live Animated* component outranks warns
   *  once per key path (the set below; entries reset when the node is
   *  removed or recreated, so a fresh entity warns fresh). */
  void warnIfOutranked(const std::string &path, uint32_t id);

  World &m_world;
  std::map<std::string, Entry> m_entries;
  std::map<std::pair<float, float>,
           std::shared_ptr<const shape::Mesh>>
      m_quads;
  std::set<std::string> m_warnedOutranked;
};

} // namespace sigil::world::scene
