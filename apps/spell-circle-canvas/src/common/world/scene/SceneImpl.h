#pragma once

/** @file
 * The retained side, from the inside: the components an entity carries,
 * the node the reconciler drives, and the host that owns both.
 *
 * EnTT appears here and nowhere in a public header. There is no registry
 * accessor and no write path through one: components are written by the
 * extract phase and read by the draw, and everything else goes through
 * the reconciler or a bound value.
 */

#include <sigilcore/cache/Cache.h>
#include <sigilcore/reconcile/Memo.h>
#include <sigilcore/reconcile/Node.h>
#include <sigilcore/reconcile/Reconciler.h>
#include <sigilworld/advanced/Lanes.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/advanced/Runtime.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/advanced/TextureProducer.h>
#include <sigilworld/scene/Scene.h>

#include <array>
#include <entt/entity/registry.hpp>
#include <functional>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Resources.h"

namespace sigil::motion {
class Engine;
}

namespace sigil::world {

/** WHAT AN ENTITY CARRIES. Written by extract, read by the draw, and by
 *  nothing else — which is what makes "execution never reads the node
 *  tree" a property of the code rather than a promise. */
namespace component {

/** Where the node stands, in world space. */
struct Placement {
  glm::mat4 world{1.0f};
};

/** The triangles to draw, and which cooked artefact they are. The mesh
 *  belongs to the resource store, whose entries hold their address for
 *  their whole life; the id names the artefact across its life and is
 *  never given to another, which an address cannot promise. */
struct Body {
  const geometry::mesh::Mesh* mesh = nullptr;
  /** The instances that mesh is repeated at, when the slot holds
   *  instanced geometry — the mesh is then its stamp — and null for a
   *  body drawn once. Both belong to the store's entry. */
  const geometry::mesh::Instances* instances = nullptr;
  uint64_t id = 0;
  material::Backface backface = material::Backface::Hidden;
  /** The primitive lane whose colour multiplies each triangle, or empty. */
  std::string primitiveColor;
  /** Whether the body turns to face the camera, which only the frame's
   *  viewpoint can settle — so it is applied where the draws are
   *  collected, not here. */
  Facing facing = Facing::None;
  /** Whether it blocks the lights that cast shadows, and whether their
   *  shadows darken it. */
  bool castShadow = true;
  bool receiveShadow = true;
};

/** What the surface is: the colour a tier with no compiler to run a
 *  recipe's body can read, and the material itself. */
struct Surface {
  glm::vec4 baseColor{0.8f, 0.8f, 0.85f, 1.0f};
  std::optional<::sigil::material::Material> material;
  /** Every per-face slot, where the node wears more than one. */
  std::vector<::sigil::material::Material> slots;
  /** The base-colour map that material carries, resolved once at
   *  extract. It addresses a leaf the material above holds, so it stands
   *  exactly as long as this component does. */
  const ::sigil::material::Texture* texture = nullptr;
  /** Do the frame's lights reach this surface? False for one that is
   *  its own light. Read off the same surface the colour and the map are
   *  read off — the one at the bottom of whatever was stacked. */
  bool lit = true;
};

/** The collections this node joined. */
struct Joined {
  std::vector<std::string> collections;
};

/** What a collection term addresses this node by: its own key, and the
 *  keys from the root down to its parent. Extract writes them, so a pass
 *  narrowing on a subtree never reaches for the description. */
struct Named {
  std::string key;
  std::vector<std::string> ancestors;
};

}  // namespace component

/** ONE RETAINED NODE. The key, the lanes and their motions, the entity
 *  and the resource reference — the three lifetimes, met in one place
 *  and each free of the others. */
struct Instance : core::Node<Instance, std::shared_ptr<NodeDescription>> {
  entt::entity entity = entt::null;
  /** One motion slot per fixed lane row, so a lane keeps its meaning
   *  across a patch that changed what the node holds. */
  std::array<std::unique_ptr<motion::HeldMotion>, kLaneCount> anims;
  /** Where this node stood among the siblings that mounted with it — the
   *  place every staggered tween on it resolves against. */
  motion::Place mountPlace;

  /** What the lanes resolved to this frame. */
  geometry::mesh::Transform values;
  float alongDistance = 0.0f;

  /** The artefact this node's geometry slot resolved. */
  Resource* resource = nullptr;
  /** The description's slot changed and the artefact has not caught up
   *  yet — resolution is extract's. */
  bool geometryDirty = true;

  glm::mat4 world{1.0f};

  core::NodeVolatility declaration;
  core::SubtreeVerdict verdict;
  /** The proof that a node DECLARING motion is nevertheless holding
   *  still: sixteen floats of placement, observed once per frame. */
  core::Settle<std::array<float, 16>> settle;
  bool released = false;
  /** The previous frame's placement, which is what makes "this node's
   *  reading stayed exact" answerable at all: a Settle only remembers
   *  the readings it was told were stable. */
  std::array<float, 16> lastScalars{};
  bool haveLastScalars = false;

  /** THE BAKE: this subtree's draw order, recorded once while it is
   *  provably standing still. */
  std::vector<entt::entity> baked;
  bool bakeHeld = false;
  bool bakeStale = true;
};

/** How long a node's placement must resolve identically before the
 *  settle releases the volatility its bindings declare. */
inline constexpr int kSettleHold = 3;
/** The cap on the converging phase group. */
inline constexpr int kConvergeRounds = 8;

/** THE HOST. It implements the ReconcileHost operations on itself and
 *  holds the reconciler over its own node and description types. */
/** The surface at the bottom of whatever @p material stacks — the one a
 *  body's colour, map and lighting are read off — or null for none. */
const material::Material* readableSurface(const material::Material* material);
/** The base colour @p material states, or the stock grey. */
glm::vec4 baseColorOf(const material::Material* material);

struct Scene::Impl {
  using Description = std::shared_ptr<NodeDescription>;

  /** What one bake decision acts on: the node whose subtree is being
   *  decided, the host that walks it, and the order the result lands
   *  in. */
  struct BakeTarget {
    Impl* impl = nullptr;
    Instance* node = nullptr;
    std::vector<entt::entity>* into = nullptr;
  };

  explicit Impl(motion::Engine& t);

  motion::Engine& engine;
  core::Reconciler<Impl, Instance, Description> reconciler;
  std::unique_ptr<Instance> root;
  entt::registry registry;
  ResourceStore store;
  SceneStats stats;

  /** What `render()` was handed, held for the describe phase — the
   *  phase list is declared over member functions, and this is the one
   *  argument describe needs. */
  Node pending;

  core::Reconciler<Impl, Instance, Description>::KeyIndex byKey;
  /** The extracted draw order, in tree order. */
  std::vector<entt::entity> order;
  std::vector<PlacedLight> lights;
  /** ONE CAMERA THE TREE HOLDS, in tree order: its key, how the picture
   *  it takes is read, and its lens carried by its node's placement. */
  struct FoundCamera {
    std::string key;
    float exposure = 1.0f;
    ToneCurve toneCurve = ToneCurve::Reinhard;
    geometry::mesh::camera::Camera lens;
  };
  std::vector<FoundCamera> cameras;
  /** The camera the frame looks through, where the tree holds one the
   *  rule can choose. */
  std::optional<FoundCamera> camera;

  std::vector<Lane> laneScratch;
  std::vector<Lane> prevLaneScratch;
  /** The keys standing above the node extract is writing, held across
   *  the walk so a node's ancestry costs a push and a pop. */
  std::vector<std::string> ancestry;

  // ---- the frame the last render was handed ----
  Frame frame;
  graph::Plan plan;
  Targets targets;
  /** The bodies the last extract left, sorted back to front, and what a
   *  pass sees of them. */
  std::vector<Draw> draws;
  View view;
  /** What the texture passes restated the bodies wearing their textures
   *  as, this frame; a restated body in `draws` points into it. Sized once per
   * frame before anything is restated, so no pointer into it moves. */
  std::vector<material::Material> restatedMaterials;
  /** The readbacks taken at the end of the last frame, handed over at
   *  the start of the next one. */
  std::vector<
      std::pair<Readback::Result, std::function<void(const Readback::Result&)>>>
      captured;
  uint64_t frameIndex = 0;
  std::string error;

  /** The bake seam's one tier: the decision is SigilCore's, and these
   *  are the operations that carry it out. */
  core::Bake<BakeTarget> bake;

  // ---- the reconciler's host (Host.cpp) ----
  static const std::string& keyOf(const Description& description) {
    return description->key;
  }
  static bool equal(const Description& a, const Description& b) {
    return propertiesEqual(*a, *b);
  }
  static bool reconcilesChildren(const Description&) { return true; }
  static const std::vector<Node>& children(const Instance&,
                                           const Description& description) {
    return description->children;
  }
  static const Description& descriptionOf(const Node& child) {
    return child.node();
  }
  static const Memo* memoOf(const Description& description) {
    return description->memo ? &*description->memo : nullptr;
  }
  static Description produce(const Memo& memo) {
    return memo.invoke(memo.properties).node();
  }

  std::unique_ptr<Instance> create(const Description& description,
                                   Instance* parent, size_t ordinal,
                                   size_t count);
  void onPatched(Instance& inst, const NodeDescription* prev,
                 const NodeDescription& next);
  void reorder(Instance& parent, bool structureChanged);
  /** False, always: nothing a node retains is welded to what its slots
   *  hold. A geometry slot that changes its value type resolves new
   *  resources in place, and the entity and the lanes survive. */
  static bool remountRequired(const Instance&, const Instance&) {
    return false;
  }
  void invalidate(Instance& inst);
  void destroy(std::unique_ptr<Instance> inst, uint64_t frame);
  /** Hands back everything @p inst and its subtree hold — the entities
   *  and the resource references — before the node itself goes. */
  void retire(Instance& inst);

  // ---- the declared phases (Phases.cpp) ----
  bool phaseDescribe();
  bool phaseLanes();
  bool phaseDerive();
  bool phaseExtract();
  bool phaseGraph();
  bool phaseExecute();

  /** THE HOLD'S RESCAN SIDE, run between the converging rounds: every
   *  node whose placement now differs from the reading its hold is
   *  against re-declares, before extract reads a single artefact.
   *
   *  It visits EVERY node, not only the ones the proof released, because
   *  a bake here is decided on declarations alone: a node with no lane of
   *  its own declares no placement motion and takes an artefact whether
   *  or not its hold has warmed up, and an ancestor's lane can move it
   *  afterwards. Skipping it would replay that artefact — the entities
   *  AND the placement recorded with them — where the node used to
   *  stand. */
  void rescanMoved();
  void rescanMoved(Instance& inst);

  /** Resolves @p inst's geometry slot against the store when it changed,
   *  dropping whatever it held. */
  void ensureResource(Instance& inst);
  /** WHERE THE NODE KEYED @p key STANDS this frame, the first in tree
   *  order; null when no node answers to it. */
  [[nodiscard]] const Instance* findKeyed(std::string_view key) const;
  /** Every bake from @p inst up to the root goes stale. */
  void staleBakesUp(Instance* inst);
  void rebuildKeyIndex();

  void sampleLanes(Instance& inst);
  void deriveInto(Instance& inst, const glm::mat4& parentWorld, bool* changed);
  /** @p hidden is whether a node above @p inst is hidden, which hides
   *  its lights and cameras as well as its bodies. */
  core::SubtreeVerdict foldVolatility(Instance& inst, bool hidden);
  /** The camera the frame looks through, by the rule: the one the frame
   *  names, else the only one the tree holds. Writes what stops the frame
   *  when the rule cannot choose. */
  void chooseCamera();
  /** What stops the frame when a body wears a texture whose pass is not
   *  on it, naming both; empty when every such texture has its pass. */
  [[nodiscard]] std::string missingTexturePass() const;
  /** Writes @p inst's subtree into @p into. @p recording is true while
   *  a bake is being taken: the artefact is ONE order for the whole
   *  settled subtree, so nothing inside it takes an artefact of its
   *  own. */
  void extractInto(Instance& inst, std::vector<entt::entity>& into,
                   bool recording = false);
  void writeComponents(Instance& inst);

  /** The lens the passes execute from: the frame's override, else the
   *  chosen camera's, else the frame's default. */
  [[nodiscard]] geometry::mesh::camera::Camera viewpoint() const;
  /** The extracted bodies, sorted back to front by view depth from
   *  @p camera — stably, so two at one depth stand in tree order. It is
   *  the one place components become the values a draw reads. */
  /** Every body in the view wearing a texture @p producer paints, drawn
   *  this frame wearing what the producer restates its material as, and
   *  said to be repainted where @p painted says the producer painted. */
  void restate(const TextureProducerOperations& producer, bool painted);
  void collectBodies(const geometry::mesh::camera::Camera& camera,
                     std::vector<Draw>& into) const;
  /** Hands over what the frame before read back. */
  void deliverReadbacks();
};

/** The scene's state, for the draw declared beside the renderer it draws
 *  on. */
struct SceneAccess {
  static Scene::Impl& of(Scene& scene) { return *scene.m_impl; }
};

}  // namespace sigil::world
