/** @file
 * The declared phases: describe, sample the lanes, derive the
 * placements, extract. Plus the two proofs riding the last of them — the
 * settle that says a node declaring motion is holding still, and the
 * bake that records a still subtree's draw order once.
 */

#include <sigilcore/cache/Cache.h>
#include <sigilgeometry/mesh/curve/Pose.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/clock/Ticker.h>

#include <cstdio>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <optional>
#include <utility>
#include <variant>

#include "SceneImpl.h"

namespace sigil::world {

namespace {

namespace gm = ::sigil::geometry::mesh;

/** The surface a node wears: its own, or the first of its per-face
 *  slots. */
const material::Material* surfaceOf(const ElementNode& node) {
  if (node.material) return &*node.material;
  return node.slots.empty() ? nullptr : &node.slots.front();
}

/** THE SURFACE AT THE BOTTOM OF A STACK. A stack is a base, a top and a
 *  mask that decides where the top shows, and a mask is a program — so
 *  what a tier with no compiler can honestly state about a stack is the
 *  surface everything else was laid over. Any other material is
 *  itself. */
const material::Material* readableSurface(const material::Material* material) {
  while (material) {
    const material::Material* base = material::under(*material);
    if (base == material) break;
    material = base;
  }
  return material;
}

/** What the CPU tier shades a surface with. A recipe declaring a
 *  four-float `baseColor` is read for it; anything else takes the mesh
 *  painter's own default, because a recipe's body is a program and the
 *  CPU tier has no compiler to run one. */
glm::vec4 baseColorOf(const material::Material* material) {
  constexpr glm::vec4 kDefault{0.8f, 0.8f, 0.85f, 1.0f};
  if (!material) return kDefault;
  const material::Field* field = material->recipe().params().find("baseColor");
  if (!field || field->floats != 4) return kDefault;
  return material->get<glm::vec4>("baseColor");
}

/** The emitter, carried by the placement of the node that declared it. */
Light placeLight(Light light, const glm::mat4& world) {
  const glm::mat3 basis(world);
  light.direction = basis * light.direction;
  if (glm::dot(light.direction, light.direction) > 0.0f)
    light.direction = glm::normalize(light.direction);
  const glm::vec4 position = world * glm::vec4(light.position, 1.0f);
  light.position = glm::vec3(position);
  return light;
}

/** The viewpoint, carried the same way. */
Camera placeCamera(Camera camera, const glm::mat4& world) {
  camera.eye = glm::vec3(world * glm::vec4(camera.eye, 1.0f));
  camera.target = glm::vec3(world * glm::vec4(camera.target, 1.0f));
  camera.up = glm::mat3(world) * camera.up;
  return camera;
}

/** THE BAKE'S ONE TIER: a subtree's draw order, recorded while the
 *  subtree is provably standing still and replayed until something in it
 *  moves. The order is entities, not pixels — the components those
 *  entities carry are exactly what a settled subtree does not change. */
struct DrawOrderBake : core::BakeOps<Scene::Impl::BakeTarget> {
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

Scene::Impl::Impl(motion::Ticker& t)
    : ticker(t), reconciler(*this), bake(DrawOrderBake{}) {}

// ---- describe --------------------------------------------------------------

bool Scene::Impl::phaseDescribe() {
  reconciler.render(root, pending.node());
  stats.reconcile = reconciler.stats();
  rebuildKeyIndex();
  return false;
}

// ---- lanes -----------------------------------------------------------------

void Scene::Impl::sampleLanes(Instance& inst) {
  const ElementNode& node = *inst.desc;
  lanesOf(node, laneScratch);
  const auto read = [this, &inst](Slot slot) {
    const Lane& lane = laneScratch[slot];
    return lane.value
               ? motion::resolveFloatAt(inst.anims[slot].get(), *lane.value)
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
  inst.windowHead = read(kWindowHead);
  inst.windowSpan = read(kWindowSpan);
  inst.intensity = read(kIntensity);
  inst.emission = {read(kEmissionRed), read(kEmissionGreen),
                   read(kEmissionBlue)};
  inst.envDiffuse = read(kEnvironmentDiffuse);
  inst.envSpecular = read(kEnvironmentSpecular);
  inst.envRoughness = read(kEnvironmentRoughness);
  inst.envCrossfade = read(kEnvironmentCrossfade);
  inst.backdrop = read(kBackdrop);
  inst.backdropBlur = read(kBackdropBlur);
  for (std::unique_ptr<Instance>& child : inst.children) sampleLanes(*child);
}

bool Scene::Impl::phaseLanes() {
  if (root) sampleLanes(*root);
  return false;
}

// ---- derive ----------------------------------------------------------------

void Scene::Impl::deriveInto(Instance& inst, const glm::mat4& parentWorld,
                             bool* changed) {
  const ElementNode& node = *inst.desc;
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
    TransformValues inFrame = inst.values;
    inFrame.translate = {0.0f, 0.0f, 0.0f};
    inFrame.axisDegrees = 0.0f;
    local = frame * localMatrix(inFrame);
  } else {
    local = localMatrix(inst.values);
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

Geometry Scene::Impl::effectiveGeometry(const Instance& inst) const {
  const ElementNode& node = *inst.desc;
  if (!node.window) return node.geometry;
  const Chained* chained = std::get_if<Chained>(&node.geometry);
  if (!chained || chained->chain.empty()) return node.geometry;
  Chained windowed = *chained;
  gm::pop::setField(windowed.chain.front(), "head", inst.windowHead);
  gm::pop::setField(windowed.chain.front(), "span", inst.windowSpan);
  return windowed;
}

void Scene::Impl::resolveResource(Instance& inst, Geometry geometry) {
  // Acquired before the old one is released: two nodes describing one
  // geometry share an entry, and dropping first would retire the very
  // artefact the new reference is about to ask for.
  Resource* next = store.acquire(std::move(geometry), &stats.cooked);
  store.release(inst.resource);
  inst.resource = next;
  inst.resolvedHead = inst.windowHead;
  inst.resolvedSpan = inst.windowSpan;
  inst.geometryDirty = false;
}

void Scene::Impl::ensureResource(Instance& inst) {
  // A window only reaches the geometry of a slot that holds a chain, so
  // only such a slot re-resolves when the window's values move. Asking a
  // slot the window cannot address would compare its whole contents
  // against the store on every frame of a motion that does not touch it.
  const bool windowMoved =
      inst.desc->window &&
      std::holds_alternative<Chained>(inst.desc->geometry) &&
      (inst.resolvedHead != inst.windowHead ||
       inst.resolvedSpan != inst.windowSpan);
  if (!inst.geometryDirty && !windowMoved) return;
  resolveResource(inst, effectiveGeometry(inst));
}

// ---- extract ---------------------------------------------------------------

core::SubtreeVerdict Scene::Impl::foldVolatility(Instance& inst) {
  core::ChildVolatility childVolatility;
  for (std::unique_ptr<Instance>& child : inst.children)
    childVolatility.add(foldVolatility(*child));

  const ElementNode& node = *inst.desc;
  lanesOf(node, laneScratch);
  bool movingPlacement = false;
  bool movingContent = false;
  bool opaque = false;
  for (size_t i = 0; i < laneScratch.size(); ++i) {
    const Lane& lane = laneScratch[i];
    if (!lane.value) continue;
    const bool live = lane.value->binding() != nullptr ||
                      (inst.anims[i] && inst.anims[i]->started);
    if (!live) continue;
    // A window drives what the node IS MADE OF; the emitter's dials
    // drive what the frame is LIT BY and nothing about any node; every
    // other lane drives only where the node stands.
    if (i == kWindowHead || i == kWindowSpan) {
      movingContent = true;
    } else if (i == kIntensity || i == kEmissionRed || i == kEmissionGreen ||
               i == kEmissionBlue || i == kEnvironmentDiffuse ||
               i == kEnvironmentSpecular || i == kEnvironmentRoughness ||
               i == kEnvironmentCrossfade || i == kBackdrop ||
               i == kBackdropBlur) {
      // The emitters are gathered on the walk below, which visits every
      // node every frame, and a bake holds a draw order and no light —
      // so a lamp that ramps stales nothing.
      continue;
    } else {
      movingPlacement = true;
    }
  }
  if (node.material && node.material->isAnimated()) movingContent = true;
  for (const material::Material& slot : node.slots)
    if (slot.isAnimated()) movingContent = true;
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

  // Emitters and viewpoints are gathered on this walk, which visits
  // every node every frame — a bake replays a draw order, and a light
  // inside one must not go missing with it.
  if (node.light) {
    Light emitter = *node.light;
    emitter.intensity = inst.intensity;
    emitter.color = {inst.emission.r, inst.emission.g, inst.emission.b,
                     emitter.color.a};
    lights.push_back(placeLight(emitter, inst.world));
  }
  if (node.environment) {
    if (environment.valid()) {
      // A SECOND SKY IS NOT A CHOICE THE FRAME CAN MAKE. The first in
      // tree order shades, and both keys are named, because a silent
      // no-op would be a set lit by whichever node happened to come
      // last and no way to see which.
      fprintf(stderr,
              "[world] two environment maps in one frame: \"%s\" shades "
              "and \"%s\" is ignored\n",
              environmentKey.c_str(), inst.desc->key.c_str());
    } else {
      environment = *node.environment;
      environment.intensity = inst.intensity;
      environment.tint = inst.emission;
      environment.diffuse = inst.envDiffuse;
      environment.specular = inst.envSpecular;
      environment.roughnessBias = inst.envRoughness;
      environment.crossfade = inst.envCrossfade;
      environment.backdrop.intensity = inst.backdrop;
      environment.backdrop.blur = inst.backdropBlur;
      // THE NODE'S TRANSFORM ORIENTS THE SKY. A panorama is sampled by
      // a direction, so only the rotation of the placement means
      // anything to it; the inverse of that rotation is what carries a
      // world-space direction into the panorama's own frame.
      environmentOrientation = glm::inverse(glm::mat3(inst.world));
      environmentKey = inst.desc->key;
    }
  }
  if (node.camera && !camera) camera = placeCamera(*node.camera, inst.world);
  return inst.verdict;
}

void Scene::Impl::writeComponents(Instance& inst) {
  const ElementNode& node = *inst.desc;
  registry.get<component::Placement>(inst.entity).world = inst.world;
  const Mesh* mesh =
      inst.resource && !inst.resource->cooked.mesh.indices.empty()
          ? &inst.resource->cooked.mesh
          : nullptr;
  if (mesh)
    registry.emplace_or_replace<component::Body>(inst.entity, mesh,
                                                 inst.resource->id);
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
  surface.texture =
      readable ? material::kit::map(*readable, material::kit::kBaseColorSlot)
               : nullptr;
  // …and so is the answer to whether light reaches it. A surface that is
  // its own light says so once, here, rather than being asked per pass.
  surface.lit = !(readable && material::kit::isUnlit(*readable));
  if (node.tags.empty())
    registry.remove<component::Tagged>(inst.entity);
  else
    registry.emplace_or_replace<component::Tagged>(inst.entity, node.tags);
  registry.emplace_or_replace<component::Named>(inst.entity, node.key,
                                                ancestry);
}

void Scene::Impl::extractInto(Instance& inst, std::vector<entt::entity>& into,
                              bool recording) {
  ensureResource(inst);
  writeComponents(inst);
  ++stats.extracted;
  if (registry.all_of<component::Body>(inst.entity))
    into.push_back(inst.entity);

  // The ancestry a selector reads is the keys standing above the node,
  // which is exactly this walk's own stack.
  ancestry.push_back(inst.desc->key);
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
  environment = {};
  environmentOrientation = glm::mat3(1.0f);
  environmentKey.clear();
  camera.reset();
  ancestry.clear();
  if (!root) return false;
  foldVolatility(*root);
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
