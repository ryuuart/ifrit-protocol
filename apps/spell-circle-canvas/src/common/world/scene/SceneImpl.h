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
#include <sigilcore/reconcile/Reconcile.h>
#include <sigilworld/element/Lanes.h>
#include <sigilworld/element/Node.h>
#include <sigilworld/frame/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <array>
#include <entt/entity/registry.hpp>
#include <functional>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Resources.h"

namespace sigil::motion {
class Ticker;
}

namespace sigil::world {

/** WHAT AN ENTITY CARRIES. Written by extract, read by the draw, and by
 *  nothing else — which is what makes "execution never reads the Element
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
  const Mesh* mesh = nullptr;
  uint64_t id = 0;
};

/** What the surface is: the colour a tier with no compiler to run a
 *  recipe's body can read, and the material itself, for a selector
 *  asking which surface a body wears. */
struct Surface {
  glm::vec4 baseColor{0.8f, 0.8f, 0.85f, 1.0f};
  std::optional<::sigil::material::Material> material;
  /** The base-colour map that material carries, resolved once at
   *  extract. It addresses a leaf the material above holds, so it stands
   *  exactly as long as this component does. */
  const ::sigil::material::Texture* texture = nullptr;
  /** Do the frame's emitters reach this surface? False for one that is
   *  its own light. Read off the same surface the colour and the map are
   *  read off — the one at the bottom of whatever was stacked. */
  bool lit = true;
};

/** The words this node answers to. */
struct Tagged {
  std::vector<std::string> words;
};

/** What a selector addresses this node by: its own key, and the keys
 *  from the root down to its parent. Extract writes them, so a pass
 *  narrowing on a key never reaches for the description. */
struct Named {
  std::string key;
  std::vector<std::string> ancestors;
};

}  // namespace component

/** ONE RETAINED NODE. The key, the lanes and their motions, the entity
 *  and the resource reference — the three lifetimes, met in one place
 *  and each free of the others. */
struct Instance : core::Node<Instance, std::shared_ptr<ElementNode>> {
  entt::entity entity = entt::null;
  /** One motion slot per fixed lane row, so a lane keeps its meaning
   *  across a patch that changed what the node holds. */
  std::array<std::unique_ptr<motion::AnimatedFloat>, kLaneCount> anims;

  /** What the lanes resolved to this frame. */
  TransformValues values;
  float alongDistance = 0.0f;
  float windowHead = 1.0f;
  float windowSpan = 1.0f;
  /** What the emitter's dials resolved to, for a node carrying one:
   *  the strength it shines at and the colour it shines in. */
  float intensity = 1.0f;
  glm::vec3 emission{1.0f, 1.0f, 1.0f};
  /** …and what an environment map's own dials resolved to, for a node
   *  placing one. Its strength and tint are the two rows above: a
   *  panorama placed in a set is an emitter of a kind. */
  float envDiffuse = 1.0f;
  float envSpecular = 1.0f;
  float envRoughness = 0.0f;
  float envCrossfade = 0.0f;
  float backdrop = 0.0f;
  float backdropBlur = 0.0f;

  /** The artefact this node's geometry slot resolved, and the window
   *  values it was resolved at — a moving window is moving geometry, so
   *  it resolves again whenever those change. */
  Resource* resource = nullptr;
  float resolvedHead = 0.0f;
  float resolvedSpan = 0.0f;
  /** The description's slot changed and the artefact has not caught up
   *  yet — resolution is extract's, so that a window sampled this frame
   *  is already in hand when the slot is cooked. */
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
struct Scene::Impl {
  using Desc = std::shared_ptr<ElementNode>;

  /** What one bake decision acts on: the node whose subtree is being
   *  decided, the host that walks it, and the order the result lands
   *  in. */
  struct BakeTarget {
    Impl* impl = nullptr;
    Instance* node = nullptr;
    std::vector<entt::entity>* into = nullptr;
  };

  explicit Impl(motion::Ticker& t);

  motion::Ticker& ticker;
  core::Reconciler<Impl, Instance, Desc> reconciler;
  std::unique_ptr<Instance> root;
  entt::registry registry;
  ResourceStore store;
  SceneStats stats;

  /** What `render()` was handed, held for the describe phase — the
   *  phase list is declared over member functions, and this is the one
   *  argument describe needs. */
  Element pending;

  std::unordered_map<std::string, Instance*> byKey;
  /** The extracted draw order, in tree order. */
  std::vector<entt::entity> order;
  std::vector<Light> lights;
  /** The one environment map the frame described, with its dials
   *  resolved and the node's placement folded into its orientation. A
   *  frame holds one; a second is a warning naming both keys. */
  Environment environment;
  glm::mat3 environmentOrientation{1.0f};
  std::string environmentKey;
  std::optional<Camera> camera;

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
  static const std::string& keyOf(const Desc& desc) { return desc->key; }
  static bool equal(const Desc& a, const Desc& b) { return propsEqual(*a, *b); }
  static bool reconcilesChildren(const Desc&) { return true; }
  static const std::vector<Element>& children(const Desc& desc) {
    return desc->children;
  }
  static const Desc& descOf(const Element& child) { return child.node(); }
  static const Memo* memoOf(const Desc& desc) {
    return desc->memo ? &*desc->memo : nullptr;
  }
  static Desc produce(const Memo& memo) {
    return memo.invoke(memo.props).node();
  }

  std::unique_ptr<Instance> create(const Desc& desc, Instance* parent,
                                   size_t ordinal, size_t count);
  void onPatched(Instance& inst, const ElementNode* prev,
                 const ElementNode& next);
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

  /** Resolves @p inst's geometry slot against the store, dropping
   *  whatever it held. @p geometry is the slot with this frame's window
   *  applied. */
  void ensureResource(Instance& inst);
  void resolveResource(Instance& inst, Geometry geometry);
  /** The node's geometry slot with its window values written into it. */
  Geometry effectiveGeometry(const Instance& inst) const;
  /** Every bake from @p inst up to the root goes stale. */
  void staleBakesUp(Instance* inst);
  void rebuildKeyIndex();

  void sampleLanes(Instance& inst);
  void deriveInto(Instance& inst, const glm::mat4& parentWorld, bool* changed);
  core::SubtreeVerdict foldVolatility(Instance& inst);
  /** Writes @p inst's subtree into @p into. @p recording is true while
   *  a bake is being taken: the artefact is ONE order for the whole
   *  settled subtree, so nothing inside it takes an artefact of its
   *  own. */
  void extractInto(Instance& inst, std::vector<entt::entity>& into,
                   bool recording = false);
  void writeComponents(Instance& inst);

  /** The viewpoint the passes execute from: the tree's, or the frame's
   *  where the tree declared none. */
  [[nodiscard]] Camera viewpoint() const;
  /** The extracted bodies, sorted back to front by view depth from
   *  @p camera — stably, so two at one depth stand in tree order. It is
   *  the one place components become the values a draw reads. */
  void collectBodies(const Camera& camera, std::vector<Draw>& into) const;
  /** Hands over what the frame before read back. */
  void deliverReadbacks();
};

}  // namespace sigil::world
