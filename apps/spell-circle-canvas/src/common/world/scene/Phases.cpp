/** @file
 * The declared phases: describe, sample the lanes, derive the
 * placements, extract. Plus the two proofs riding the last of them — the
 * settle that says a node declaring motion is holding still, and the
 * bake that records a still subtree's draw order once.
 */

#include <sigilcore/cache/Cache.h>
#include <sigilgeometry/mesh/curve/Pose.h>
#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmotion/advanced/Held.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilworld/advanced/Light.h>

#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "SceneImpl.h"

namespace sigil::world {

namespace {

namespace gm = ::sigil::geometry::mesh;

/** The surface a node wears: its own, or the first of its per-face
 *  slots. */
const material::Material* surfaceOf(const NodeDescription& node) {
  if (node.material) return &*node.material;
  return node.slots.empty() ? nullptr : &node.slots.front();
}

/** THE SURFACE AT THE BOTTOM OF A STACK. A stack is a base, a top and a
 *  mask that decides where the top shows, and a mask is a program — so
 *  what can be read off a stack without running it is the surface
 *  everything else was laid over. Any other material is itself. */
const material::Material* readableSurface(const material::Material* material) {
  while (material) {
    const material::Material* base = material::under(*material);
    if (base == material) break;
    material = base;
  }
  return material;
}

/** THE COLOUR A BODY IS EXTRACTED WITH: what a pass draws it in where
 *  its recipe carries no program the device can compile, and what a
 *  selection reads. A recipe declaring a four-float `baseColor` is read
 *  for it; anything else takes a neutral grey. */
glm::vec4 baseColorOf(const material::Material* material) {
  constexpr glm::vec4 kDefault{0.8f, 0.8f, 0.85f, 1.0f};
  if (!material) return kDefault;
  const material::Field* field =
      material->recipe().parameters().find("baseColor");
  if (!field || field->floats != 4) return kDefault;
  return material->get<glm::vec4>("baseColor");
}

/** The light, carried by the placement of the node that declared it and
 *  held still for the frame. A direction is not carried the way a point
 *  is: under a non-uniform scale the basis tilts it off the surfaces it
 *  was aimed at, and the normal transform is what keeps the angle. */
PlacedLight placeLight(const LightDescription& light, const glm::mat4& world,
                       const std::string& key) {
  PlacedLight placed{held(light), glm::vec3(world[3]), key};
  const glm::mat3 basis = glm::inverseTranspose(glm::mat3(world));
  const glm::vec3 aim = glm::dot(light.direction, light.direction) > 0.0f
                            ? glm::normalize(light.direction)
                            : glm::vec3(0.0f, -1.0f, 0.0f);
  const glm::vec3 carried = basis * aim;
  placed.light.direction =
      glm::dot(carried, carried) > 0.0f ? glm::normalize(carried) : aim;
  // A rect's width runs along the node's x axis made square to its aim,
  // or its z axis for an aim along x, carried the way a point's offset
  // is; an executor squares it again to the direction a look-at gives.
  glm::vec3 across = glm::vec3(1, 0, 0) - aim * aim.x;
  if (glm::dot(across, across) < 1e-6f)
    across = glm::vec3(0, 0, 1) - aim * aim.z;
  const glm::vec3 turned = glm::mat3(world) * across;
  placed.across = glm::dot(turned, turned) > 0.0f ? glm::normalize(turned)
                                                  : glm::vec3(1, 0, 0);
  return placed;
}

/** The viewpoint, carried the same way. */
geometry::mesh::camera::Camera placeCamera(
    geometry::mesh::camera::Camera camera, const glm::mat4& world) {
  camera.eye = glm::vec3(world * glm::vec4(camera.eye, 1.0f));
  camera.target = glm::vec3(world * glm::vec4(camera.target, 1.0f));
  camera.up = glm::mat3(world) * camera.up;
  return camera;
}

/** THE BAKE'S ONE TIER: a subtree's draw order, recorded while the
 *  subtree is provably standing still and replayed until something in it
 *  moves. The order is entities, not pixels — the components those
 *  entities carry are exactly what a settled subtree does not change. */
struct DrawOrderBake : core::BakeOperations<Scene::Impl::BakeTarget> {
  void take(Scene::Impl::BakeTarget& target) const override {
    target.node->baked.clear();
    target.impl->extractInto(*target.node, target.node->baked,
                             /*recording=*/true);
    target.node->bakeHeld = true;
    target.node->bakeStale = false;
  }
  void replay(Scene::Impl::BakeTarget& target) const override {
    target.into->insert(target.into->end(), target.node->baked.begin(),
                        target.node->baked.end());
  }
  void drop(Scene::Impl::BakeTarget& target) const override {
    target.node->baked.clear();
    target.node->bakeHeld = false;
    target.node->bakeStale = true;
  }
  [[nodiscard]] bool held(
      const Scene::Impl::BakeTarget& target) const override {
    return target.node->bakeHeld;
  }
  // Stateless, so every instance is the same value — which is what
  // lets the seam compare rather than fall back to identity.
  bool operator==(const DrawOrderBake&) const { return true; }
};

std::array<float, 16> scalarsOf(const glm::mat4& world) {
  std::array<float, 16> scalars{};
  std::memcpy(scalars.data(), &world[0][0], sizeof(scalars));
  return scalars;
}

}  // namespace

Scene::Impl::Impl(motion::Engine& t)
    : engine(t), reconciler(*this), bake(DrawOrderBake{}) {}

// ---- describe --------------------------------------------------------------

bool Scene::Impl::phaseDescribe() {
  reconciler.render(root, pending.node());
  stats.reconcile = reconciler.stats();
  rebuildKeyIndex();
  return false;
}

// ---- lanes -----------------------------------------------------------------

void Scene::Impl::sampleLanes(Instance& inst) {
  const NodeDescription& node = *inst.description;
  lanesOf(node, laneScratch);
  const auto read = [this, &inst](Slot slot) {
    const Lane& lane = laneScratch[slot];
    return lane.value ? motion::valueOf(inst.anims[slot].get(), *lane.value)
                      : lane.standing;
  };
  inst.values.translate = {read(kTranslateX), read(kTranslateY),
                           read(kTranslateZ)};
  inst.values.rotateDegrees = {read(kRotateX), read(kRotateY), read(kRotateZ)};
  inst.values.scale = {read(kScaleX), read(kScaleY), read(kScaleZ)};
  inst.values.origin = {read(kOriginX), read(kOriginY), read(kOriginZ)};
  inst.values.axis = node.transform.axis;
  inst.values.axisDegrees = read(kAxisDegrees);
  inst.alongDistance = read(kAlongDistance);
  for (std::unique_ptr<Instance>& child : inst.children) sampleLanes(*child);
}

bool Scene::Impl::phaseLanes() {
  if (root) sampleLanes(*root);
  return false;
}

// ---- derive ----------------------------------------------------------------

void Scene::Impl::deriveInto(Instance& inst, const glm::mat4& parentWorld,
                             bool* changed) {
  const NodeDescription& node = *inst.description;
  glm::mat4 local(1.0f);
  if (node.transform.matrix) {
    local = *node.transform.matrix;
  } else if (node.along) {
    // The curve puts the node in its own moving frame — x across the
    // profile, y its carried up, z along the tangent — and the lanes
    // that are not a placement still apply inside it.
    const gm::curve::Frame3 pose =
        gm::curve::poseAlong(node.along->spline, inst.alongDistance);
    glm::mat4 frame(1.0f);
    frame[0] = glm::vec4(pose.binormal, 0.0f);
    frame[1] = glm::vec4(pose.normal, 0.0f);
    frame[2] = glm::vec4(pose.tangent, 0.0f);
    frame[3] = glm::vec4(pose.position, 1.0f);
    geometry::mesh::Transform inFrame = inst.values;
    inFrame.translate = {0.0f, 0.0f, 0.0f};
    inFrame.axisDegrees = 0.0f;
    local = frame * inFrame.matrix();
  } else {
    local = inst.values.matrix();
  }

  const glm::mat4 world = parentWorld * local;
  if (world != inst.world) {
    inst.world = world;
    *changed = true;
  }
  for (std::unique_ptr<Instance>& child : inst.children)
    deriveInto(*child, world, changed);
}

bool Scene::Impl::phaseDerive() {
  bool changed = false;
  if (root) deriveInto(*root, glm::mat4(1.0f), &changed);
  return changed;
}

void Scene::Impl::rescanMoved() {
  if (root) rescanMoved(*root);
}

void Scene::Impl::rescanMoved(Instance& inst) {
  // The hold's own answer to "is this still the reading I am holding
  // against": it restarts the warmup from the new placement and says
  // whether the node must re-declare. Asking the SETTLE rather than
  // comparing against the previous frame's matrix is what keeps the
  // release and the re-declaration reading the same value.
  if (inst.settle.moved(scalarsOf(inst.world))) {
    inst.released = false;
    staleBakesUp(&inst);
  }
  for (std::unique_ptr<Instance>& child : inst.children) rescanMoved(*child);
}

// ---- the geometry slot's resource -------------------------------------------

void Scene::Impl::ensureResource(Instance& inst) {
  if (!inst.geometryDirty) return;
  // Acquired before the old one is released: two nodes describing one
  // geometry share an entry, and dropping first would retire the very
  // artefact the new reference is about to ask for.
  Resource* next = store.acquire(inst.description->geometry, &stats.cooked);
  store.release(inst.resource);
  inst.resource = next;
  inst.geometryDirty = false;
}

// ---- what a light or a camera looks at
// ---------------------------------------

const Instance* Scene::Impl::findKeyed(std::string_view key) const {
  if (!root || key.empty()) return nullptr;
  std::vector<const Instance*> stack{root.get()};
  while (!stack.empty()) {
    const Instance* inst = stack.back();
    stack.pop_back();
    if (inst->description->key == key) return inst;
    // Children pushed last-first, so the walk visits them in tree order.
    for (auto child = inst->children.rbegin(); child != inst->children.rend();
         ++child)
      stack.push_back(child->get());
  }
  return nullptr;
}

// ---- extract ---------------------------------------------------------------

core::SubtreeVerdict Scene::Impl::foldVolatility(Instance& inst, bool hidden) {
  const NodeDescription& node = *inst.description;
  hidden = hidden || !node.visible;
  // THE CAMERAS IN TREE ORDER, a node's own before its children's: the
  // frame chooses among them once the walk is done.
  // WHERE A LIGHT OR A CAMERA LOOKS, resolved against the placements this
  // frame derived: the keyed node's origin, wherever it stands.
  std::optional<glm::vec3> aim;
  if (!node.lookAt.empty() && (node.camera || node.light)) {
    if (const Instance* target = findKeyed(node.lookAt))
      aim = glm::vec3(target->world[3]);
    else if (error.empty())
      error = std::string(node.camera ? "the camera \"" : "the light \"") +
              node.key + "\" looks at \"" + node.lookAt +
              "\", and no node in the tree answers to that key";
  }
  if (node.camera && !hidden) {
    geometry::mesh::camera::Camera lens =
        placeCamera(node.camera->lens, inst.world);
    if (aim) lens.target = *aim;
    cameras.push_back({.key = node.key,
                       .exposure = node.camera->exposure.value(),
                       .toneCurve = node.camera->toneCurve,
                       .lens = lens});
  }

  core::ChildVolatility childVolatility;
  for (std::unique_ptr<Instance>& child : inst.children)
    childVolatility.add(foldVolatility(*child, hidden));

  lanesOf(node, laneScratch);
  bool movingPlacement = false;
  bool movingContent = false;
  bool opaque = false;
  for (size_t i = 0; i < laneScratch.size(); ++i) {
    const Lane& lane = laneScratch[i];
    if (!lane.value) continue;
    // Whether this lane is MOVING, in SigilMotion's one spelling. It asks
    // whether the held motion is CONNECTED, not whether one was ever
    // started: a lane whose entrance has landed is still, and a lane that
    // could never be told so would hold the whole subtree volatile for
    // the rest of the scene's life.
    if (!motion::isRunning(inst.anims[i].get(), *lane.value)) continue;
    // Every lane drives where the node stands.
    movingPlacement = true;
  }
  if (node.material && node.material->isRunning()) movingContent = true;
  for (const material::Material& slot : node.slots)
    if (slot.isRunning()) movingContent = true;
  if (const Generator* generator = std::get_if<Generator>(&node.geometry))
    if (*generator && !generator->comparable()) {
      // A generator that cannot say whether it is the same generator
      // cooks afresh whenever it is described, and no value comparison
      // can see that coming.
      movingContent = true;
      opaque = true;
    }

  // THE OBSERVE SIDE, once per frame: a placement that resolved
  // identically for long enough stops declaring the motion its bindings
  // are still connected to.
  const std::array<float, 16> scalars = scalarsOf(inst.world);
  const bool stable = inst.haveLastScalars && inst.lastScalars == scalars;
  inst.lastScalars = scalars;
  inst.haveLastScalars = true;
  inst.settle.observe(stable, scalars, kSettleHold);
  inst.released =
      inst.settle.release(kSettleHold, [&scalars] { return scalars; });

  core::NodeVolatility self;
  self.policy = node.cachePolicy;
  self.ownPaint = movingPlacement && !inst.released;
  self.ownContent = movingContent;
  self.memoOpaque = opaque;
  inst.declaration = self;
  inst.verdict = core::foldSubtree(self, childVolatility);

  // Lights are gathered on this walk, which visits every node every
  // frame — a bake replays a draw order, and a light inside one must not
  // go missing with it. A light's colour and strength are read here, so
  // one bound to a live value changes the shading without staling any
  // bake.
  if (node.light && !hidden) {
    PlacedLight placed = placeLight(*node.light, inst.world, node.key);
    // An aimed light travels from where it stands toward what it looks
    // at; a light with no direction of its own is not turned.
    const bool aimable = node.light->kind == LightKind::Directional ||
                         node.light->kind == LightKind::Spot ||
                         node.light->kind == LightKind::Rect ||
                         node.light->kind == LightKind::Disk;
    if (aim && aimable) {
      const glm::vec3 toward = *aim - placed.position;
      if (glm::dot(toward, toward) > 0.0f)
        placed.light.direction = glm::normalize(toward);
    }
    lights.push_back(std::move(placed));
  }
  return inst.verdict;
}

void Scene::Impl::chooseCamera() {
  camera.reset();
  const std::string& named = frame.camera();
  if (!named.empty()) {
    for (const FoundCamera& found : cameras)
      if (found.key == named) {
        camera = found;
        return;
      }
    error = "the frame looks through the camera \"" + named +
            "\", and no camera in the tree answers to that key";
    return;
  }
  if (cameras.size() == 1) {
    camera = cameras.front();
    return;
  }
  if (cameras.size() > 1) {
    std::string keys;
    for (const FoundCamera& found : cameras)
      keys += (keys.empty() ? "\"" : ", \"") + found.key + "\"";
    error = "the tree holds " + std::to_string(cameras.size()) + " cameras (" +
            keys +
            ") and the frame names none of them: name the one to look "
            "through with Frame::camera(key)";
  }
}

void Scene::Impl::writeComponents(Instance& inst) {
  const NodeDescription& node = *inst.description;
  registry.get<component::Placement>(inst.entity).world = inst.world;
  // INSTANCED GEOMETRY IS DRAWN AS IT IS: its stamp, repeated at its
  // instances, both read off the store's entry.
  const geometry::mesh::Instances* instances =
      inst.resource
          ? std::get_if<geometry::mesh::Instances>(&inst.resource->geometry)
          : nullptr;
  if (instances && (instances->stamp.indices.empty() || !instances->size()))
    instances = nullptr;
  const geometry::mesh::Mesh* mesh =
      instances ? &instances->stamp
      : inst.resource && !inst.resource->cooked.mesh.indices.empty()
          ? &inst.resource->cooked.mesh
          : nullptr;
  if (mesh)
    registry.emplace_or_replace<component::Body>(
        inst.entity, mesh, instances, inst.resource->id, node.backface,
        node.facing, node.castShadow, node.receiveShadow);
  else
    registry.remove<component::Body>(inst.entity);
  const material::Material* worn = surfaceOf(node);
  component::Surface& surface = registry.emplace_or_replace<component::Surface>(
      inst.entity, baseColorOf(readableSurface(worn)),
      worn ? std::optional<material::Material>(*worn)
           : std::optional<material::Material>());
  // The colour and the map are read HERE, once, so that an execution
  // reading a body never walks a material tree — and both are read off
  // the same surface, the one at the bottom of whatever was stacked.
  const material::Material* readable =
      surface.material ? readableSurface(&*surface.material) : nullptr;
  surface.texture = readable ? material::surface::map(
                                   *readable, material::surface::kBaseColorSlot)
                             : nullptr;
  // …and so is the answer to whether light reaches it. A surface that is
  // its own light says so once, here, rather than being asked per pass.
  surface.lit = !(readable && material::surface::isUnlit(*readable));
  if (node.collections.empty())
    registry.remove<component::Joined>(inst.entity);
  else
    registry.emplace_or_replace<component::Joined>(inst.entity,
                                                   node.collections);
  registry.emplace_or_replace<component::Named>(inst.entity, node.key,
                                                ancestry);
}

void Scene::Impl::extractInto(Instance& inst, std::vector<entt::entity>& into,
                              bool recording) {
  // A HIDDEN NODE IS NOT IN THE SCENE, and nothing under it is: its
  // subtree contributes no body to the order.
  if (!inst.description->visible) return;
  ensureResource(inst);
  writeComponents(inst);
  ++stats.extracted;
  if (registry.all_of<component::Body>(inst.entity))
    into.push_back(inst.entity);

  // The ancestry a collection reads is the keys standing above the node,
  // which is exactly this walk's own stack.
  ancestry.push_back(inst.description->key);
  for (std::unique_ptr<Instance>& child : inst.children) {
    if (recording) {
      // A bake is one order for the whole settled subtree. Asking a
      // child inside one for an artefact of its own would store the same
      // entities twice and give the invalidation two places to reach.
      extractInto(*child, into, true);
      continue;
    }
    BakeTarget target{this, child.get(), &into};
    const core::BakeState state{
        // `volatileAbove` and not `subtreeVolatile`, because THIS
        // artefact contains the node's own composite: a draw order
        // carries each entity's placement, so a node whose placement
        // moves cannot replay one any more than its ancestor can.
        .cacheable = !child->verdict.volatileAbove,
        .held = child->bakeHeld,
        .stale = child->bakeStale,
    };
    switch (core::runBake(bake, target, state)) {
      case core::BakeAction::Live:
        extractInto(*child, into);
        break;
      case core::BakeAction::Take:
        ++stats.baked;
        break;
      case core::BakeAction::Replay:
        ++stats.replayed;
        break;
    }
  }
  ancestry.pop_back();
}

bool Scene::Impl::phaseExtract() {
  order.clear();
  lights.clear();
  cameras.clear();
  camera.reset();
  ancestry.clear();
  if (!root) return false;
  foldVolatility(*root, false);
  chooseCamera();
  BakeTarget target{this, root.get(), &order};
  const core::BakeState state{
      .cacheable = !root->verdict.volatileAbove,
      .held = root->bakeHeld,
      .stale = root->bakeStale,
  };
  switch (core::runBake(bake, target, state)) {
    case core::BakeAction::Live:
      extractInto(*root, order);
      break;
    case core::BakeAction::Take:
      ++stats.baked;
      break;
    case core::BakeAction::Replay:
      ++stats.replayed;
      break;
  }
  stats.drawn = (int64_t)order.size();
  return false;
}

}  // namespace sigil::world
