#pragma once

/** @file
 * A DECLARATIVE layer over World: describe the scene as a value tree and
 * let a reconciler own the device objects, so only what changed touches
 * the GPU. There is no layout, no styling and no timeline here — just
 * describe, diff, apply.
 *
 *   scene::Node root = scene::group().key("set")
 *     .child(scene::place(floorMesh, steel).key("floor").at({0,-190,0}))
 *     .child(scene::group().key("hud").at({0,60,0}).rotated(spin)
 *       .child(scene::panel(cardImage, 380, 252).key("left").at({-420,0,0})
 *         ...
 *   scene.render(root);   // add/remove/move against the last render
 *
 * Identity is the KEY PATH: a node's key joined to its parents' keys,
 * with an index fallback for unkeyed children. A leaf reuses its device
 * prop when its mesh POINTER and its material compare equal, so hold
 * meshes in shared_ptrs and a transform-only change costs a setTransform
 * rather than a re-upload. panel() quads get per-size cached meshes
 * inside the Scene, which makes panels identity-stable by construction.
 *
 * render() returns Stats {added, removed, moved, kept}, so a caller can
 * assert that a re-describe cost what it expected to cost.
 */

#include <include/core/SkSize.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "sigilworld/World.h"

namespace sigil::world::scene {

/** One declared node of a scene tree: a group, a prop, or a panel,
 *  with its placement and the children under it. A Node is a VALUE
 *  describing what should exist, not a handle to something that does
 *  — the reconciler is what makes the world match the description. */
class Node {
 public:
  enum class Kind : uint8_t { Group, Prop, Panel };

  Node& key(std::string value) {
    m_key = std::move(value);
    return *this;
  }
  Node& at(glm::vec3 position) {
    m_position = position;
    return *this;
  }
  Node& rotated(float yawDeg, float pitchDeg = 0, float rollDeg = 0) {
    m_yawDeg = yawDeg;
    m_pitchDeg = pitchDeg;
    m_rollDeg = rollDeg;
    return *this;
  }
  Node& scaled(float scale) {
    m_scale = scale;
    return *this;
  }
  /** Extra local matrix, applied after at/rotated/scaled. */
  Node& transform(const glm::mat4& m) {
    m_extra = m;
    m_hasExtra = true;
    return *this;
  }
  Node& material(const Material& value) {
    m_material = value;
    return *this;
  }
  Node& child(Node node) {
    m_children.push_back(std::move(node));
    return *this;
  }

  /** The node's local matrix (translate * yaw * pitch * roll * scale
   *  * extra). */
  glm::mat4 localMatrix() const;

 private:
  friend Node group();
  friend Node place(std::shared_ptr<const geometry::Mesh>, Material);
  friend Node panel(sk_sp<SkImage>, float, float);
  friend class Scene;

  Kind m_kind = Kind::Group;
  std::string m_key;
  glm::vec3 m_position = {0, 0, 0};
  float m_yawDeg = 0, m_pitchDeg = 0, m_rollDeg = 0;
  float m_scale = 1;
  glm::mat4 m_extra{1.0f};
  bool m_hasExtra = false;
  std::shared_ptr<const geometry::Mesh> m_mesh;  // Prop
  Material m_material;                           // Prop + Panel
  float m_panelWidth = 0, m_panelHeight = 0;     // Panel
  std::vector<Node> m_children;
};

inline Node group() { return Node(); }

/** A mesh + material leaf. Identity for reuse is the mesh POINTER, not
 *  its contents, so share one shared_ptr across renders; the overload
 *  below allocates a fresh mesh and therefore a fresh identity every
 *  call. */
Node place(std::shared_ptr<const geometry::Mesh> mesh, Material material);
inline Node place(geometry::Mesh mesh, Material material) {
  return place(std::make_shared<const geometry::Mesh>(std::move(mesh)),
               std::move(material));
}

/** An unlit textured quad — the diegetic UI card. The quad mesh is
 *  cached per size inside the Scene, so panels stay identity-stable. */
Node panel(sk_sp<SkImage> image, float width, float height);

/** A layer stack's pixel DENSITY: one number that sizes every image in
 *  the stack. An image through this Stack lands at image_px / pxPerWu
 *  world units, so each layer's aspect is right by construction and two
 *  layers' world sizes stay in the exact ratio of their pixel sizes —
 *  the relative scale an aspect-deriving overload could not know.
 *
 *  This is arithmetic, not a new node kind: `Stack::panel(image)` builds
 *  exactly the `panel(image, w, h)` a caller would write by hand with
 *  the same numbers, so the reconciler sees the same prop and
 *  swapping one spelling for the other is a keep. */
struct Stack {
  float pxPerWu = 1;

  /** World size of @p image at this density. */
  SkSize size(const SkImage& image) const {
    return SkSize::Make((float)image.width() / pxPerWu,
                        (float)image.height() / pxPerWu);
  }
  /** `panel(image, image->width()/pxPerWu, image->height()/pxPerWu)`. */
  Node panel(sk_sp<SkImage> image) const {
    const SkSize s = image ? size(*image) : SkSize::MakeEmpty();
    return scene::panel(std::move(image), s.width(), s.height());
  }
};

/** Keeps a World matching a declared tree. Each `render()` diffs the
 *  tree it is given against the one before it and applies only the
 *  difference, so a caller can rebuild the whole description every
 *  frame and still pay only for what actually changed. Nodes are
 *  matched across frames by key, which is what lets a prop keep its
 *  identity — and its GPU residency — while it moves. */
class Scene {
 public:
  explicit Scene(World& world) : m_world(world) {}

  /** What the last reconcile did, counted by outcome. `kept` dominating
   *  the others is the sign the tree is being matched rather than
   *  rebuilt. */
  struct Stats {
    int added = 0;
    int removed = 0;
    int moved = 0;
    int kept = 0;
  };

  /** Reconcile the declared tree against the last render. */
  Stats render(const Node& root);

  /** PUBLISH IDENTITY: resolve a key path to the live registry entity
   *  behind that node, so an `Animated*` lane can be attached to a
   *  declared leaf on purpose instead of by iterating the registry
   *  around the reconciler's back.
   *
   *  The path is the reconciler's own identity spelling — the same key
   *  `render()` diffs by. Keys are joined with '/', so
   *  `group().key("comp").child(panel(...).key("sky"))` answers to
   *  "comp/sky"; a leading '/' is accepted too. Unkeyed children carry
   *  the index fallback in that spelling ("#<index>" for the hop, "@"
   *  for the unkeyed node itself), but key anything you intend to
   *  animate. Only leaves have an entity: a group path, an unknown path,
   *  or a leaf whose prop creation failed answers an empty optional,
   *  never a throw.
   *
   *  Returns `entt::entity` because that is what the Animated*
   *  components take:
   *  `registry().emplace<AnimatedTransform>(*scene.find("comp/sky"))`.
   *  A World prop id is the same value one cast away (Components.h),
   *  so the imperative setters are equally reachable.
   *
   *  LIFETIME: the entity stays valid until the next render() that
   *  RECREATES or REMOVES that node. A leaf whose mesh pointer or
   *  material changed is remove-and-add, which destroys this entity and
   *  every component on it — your lanes included — so call find() again
   *  after such a render and re-attach. A kept leaf, including one moved
   *  by transform only, keeps its entity. When a kept leaf's declared
   *  placement or material is outranked by a live Animated* component,
   *  render() warns once for that node. */
  std::optional<entt::entity> find(std::string_view keyPath) const;

  /** Forget everything (removes all scene-owned props). */
  void clear();

 private:
  struct Entry {
    uint32_t id = 0;
    const geometry::Mesh* mesh = nullptr;
    Material material;
    glm::mat4 world{1.0f};
    bool visited = false;
  };

  /** Warn once per key path when a kept or moved leaf's declared
   *  placement or material is outranked by a live Animated* component
   *  on the same entity — silently losing a declared value is the
   *  failure this makes visible. The set below remembers who has been
   *  warned; its entries clear when the node is removed or recreated,
   *  so a fresh entity warns afresh. */
  void warnIfOutranked(const std::string& path, uint32_t id);

  World& m_world;
  std::map<std::string, Entry> m_entries;
  std::map<std::pair<float, float>, std::shared_ptr<const geometry::Mesh>>
      m_quads;
  std::set<std::string> m_warnedOutranked;
};

}  // namespace sigil::world::scene
