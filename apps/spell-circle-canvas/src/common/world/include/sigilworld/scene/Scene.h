#pragma once

/** @file
 * The retained side of a 3D scene: the tree an Element description is
 * reconciled onto, the store that cooks a geometry slot once per
 * distinct value, the phases that settle it, and the draw that reads
 * what those phases extracted.
 */

#include <sigilgeometry/mesh/render/Runtime.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/scene/Stats.h>

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

class SkCanvas;

namespace sigil::motion {
class Ticker;
}

namespace sigil::world {

namespace render = ::sigil::geometry::mesh::render;

/** A 3D SCENE, retained.
 *
 *  An author builds a fresh Element tree every frame and hands it to
 *  `render()`; the Scene reconciles it onto what it already holds, so
 *  only what changed is touched. THREE LIFETIMES run underneath and none
 *  of them is the others':
 *
 *   - a NODE — its key, its lanes, the motions in flight on them and its
 *     entity — lives as long as its key is in the tree;
 *   - a RESOURCE — what a geometry slot cooked to — lives in a
 *     content-keyed store, reference-counted, shared by every node that
 *     describes the same geometry, and dropped when the last of them
 *     lets go;
 *   - an EXTRACTED FRAME lives for one draw.
 *
 *  So a node whose geometry slot changes resolves a new resource and
 *  drops the old one while its entity and its running motions stand.
 *  Nothing is welded to what a slot holds, which is why there is no kind
 *  field and why nothing here ever remounts.
 *
 *  There are exactly two write paths: `render()`, and the live values a
 *  description's lanes are bound to. Nothing writes onto a retained node
 *  from outside. */
class Scene {
 public:
  /** @p ticker drives the lanes' transitions; the Scene neither owns it
   *  nor steps it. */
  explicit Scene(motion::Ticker& ticker);
  Scene(Scene&&) noexcept;
  Scene& operator=(Scene&&) noexcept;
  ~Scene();

  /** ONE FRAME: describe, sample the lanes, derive the placements, and
   *  extract. Nothing is drawn here. */
  void render(const Element& root);

  /** Draw what the last `render()` extracted, from @p camera, on
   *  @p runtime. Execution reads the extracted state and never the
   *  Element tree. */
  void draw(SkCanvas& canvas, const Camera& camera,
            const render::Runtime& runtime = render::Runtime::cpu());
  /** …and from the viewpoint the tree declared, if it declared one. A
   *  tree with no `camera()` in it draws from the default Camera. */
  void draw(SkCanvas& canvas,
            const render::Runtime& runtime = render::Runtime::cpu());

  /** The viewpoint the tree declared, carried by its node's placement —
   *  the first one in tree order when there are several. */
  [[nodiscard]] std::optional<Camera> camera() const;
  /** The emitters the tree declared, each carried by its node's
   *  placement. */
  [[nodiscard]] std::vector<Light> lights() const;

  /** THE HOST HANDLE of the node addressed by @p key, opaque and
   *  non-zero; 0 when no node answers to that key. It is the pin on a
   *  node's identity: a handle that survives a describe is the same
   *  node, with the same lanes and the same motions on them. */
  [[nodiscard]] uint64_t handleOf(std::string_view key) const;
  /** The world placement of the node addressed by @p key. */
  [[nodiscard]] std::optional<glm::mat4> transformOf(
      std::string_view key) const;
  /** How many nodes hold a reference to the cooked artefact the node
   *  addressed by @p key resolved — 0 when it resolved none. Two nodes
   *  describing one geometry share one artefact and answer 2. */
  [[nodiscard]] int referencesOf(std::string_view key) const;

  /** What the last frame did. */
  [[nodiscard]] const SceneStats& stats() const;

  /** @private the retained side itself — the host, the entity store and
   *  the phases. Declared here so this library's own translation units
   *  can define and name it; nothing outside can reach one. */
  struct Impl;

 private:
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::world
