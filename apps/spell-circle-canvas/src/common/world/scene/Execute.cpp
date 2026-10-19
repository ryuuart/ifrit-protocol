/** @file
 * The two phases a frame with passes adds: the ordering, which turns
 * the declarations into steps and gives the resources their surfaces,
 * and the execution, which performs those steps on the runtime the
 * frame carries and takes what the caller asked to read back.
 *
 * Both read the components extract wrote. Neither can reach the node
 * tree, and that is the point.
 */

#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilworld/advanced/TextureProducer.h>

#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <string>
#include <utility>
#include <vector>

#include "SceneImpl.h"

namespace sigil::world {

namespace {

/** An empty span of names, for a body that joined no collection. */
constexpr std::span<const std::string> kNoNames;

/** Calls @p visit with every texture @p material samples, its slots'
 *  materials included. */
template <class Visit>
void eachTexture(const material::Material& material, Visit&& visit) {
  for (const auto& [name, slot] : material.slots()) {
    if (slot.material) eachTexture(*slot.material, visit);
    if (const auto* texture =
            dynamic_cast<const material::Texture*>(slot.leaf.get()))
      visit(*texture);
  }
}

}  // namespace

void Scene::Impl::restate(const TextureProducerOperations& producer) {
  // `view.draws` is a window onto `draws`, so a body restated here is the
  // body the view draws.
  for (Draw& body : draws) {
    if (!body.material) continue;
    bool wears = false;
    eachTexture(*body.material, [&](const material::Texture& texture) {
      wears = wears || producer.paints(texture);
    });
    if (!wears) continue;
    std::optional<material::Material> restated =
        producer.restated(*body.material);
    // The vector was sized for every body, so a restatement never moves
    // one made before it.
    if (!restated || restatedMaterials.size() == restatedMaterials.capacity())
      continue;
    restatedMaterials.push_back(std::move(*restated));
    body.material = &restatedMaterials.back();
    // …and everything extraction read off the surface is read again off
    // the one restated, so the body draws exactly as if the frame had
    // described it so: its colour, its map and whether light reaches it.
    const material::Material* readable = readableSurface(body.material);
    body.baseColor = baseColorOf(readable);
    body.texture = readable ? material::surface::map(
                                  *readable, material::surface::kBaseColorSlot)
                            : nullptr;
    body.lit = !(readable && material::surface::isUnlit(*readable));
  }
}

geometry::mesh::camera::Camera Scene::Impl::viewpoint() const {
  if (frame.cameraOverride()) return *frame.cameraOverride();
  return camera ? camera->lens : frame.defaultCamera();
}

std::string Scene::Impl::missingTexturePass() const {
  const auto unpainted = [&](const material::Texture& picture) {
    const std::string pass = producingPass(picture);
    if (pass.empty()) return std::string();
    for (const Pass& candidate : frame.passes())
      if (candidate.stage() == Stage::Texture && candidate.producer() &&
          candidate.producer()->paints(picture))
        return std::string();
    return pass;
  };
  for (const auto& [pictures, kind] :
       {std::pair{frame.overlays(), "an overlay"},
        std::pair{frame.backdrops(), "a backdrop"}})
    for (const material::Texture& picture : pictures)
      if (const std::string pass = unpainted(picture); !pass.empty())
        return std::string(kind) + " is a texture the pass \"" + pass +
               "\" paints, and that pass is not on the frame: add it with "
               "Frame::pass";
  for (const Draw& body : draws) {
    if (!body.material) continue;
    std::string missing;
    eachTexture(*body.material, [&](const material::Texture& texture) {
      if (!missing.empty()) return;
      const std::string pass = producingPass(texture);
      if (pass.empty()) return;
      for (const Pass& candidate : frame.passes())
        if (candidate.stage() == Stage::Texture && candidate.producer() &&
            candidate.producer()->paints(texture))
          return;
      missing = pass;
    });
    if (!missing.empty())
      return "the body \"" + std::string(body.key) +
             "\" wears a texture the pass \"" + missing +
             "\" paints, and that pass is not on the frame: add it with "
             "Frame::pass";
  }
  return {};
}

namespace {

/** @p world TURNED TO FACE @p eye as @p facing says: where it stands and
 *  its scale along each of its axes kept, its rotation replaced so its +z
 *  faces the camera — square on to the picture, or about the world's up
 *  axis only. */
glm::mat4 faced(const glm::mat4& world, Facing facing,
                const geometry::mesh::camera::Camera& eye) {
  if (facing == Facing::None) return world;
  const glm::vec3 at(world[3]);
  const glm::vec3 scale{glm::length(glm::vec3(world[0])),
                        glm::length(glm::vec3(world[1])),
                        glm::length(glm::vec3(world[2]))};
  glm::vec3 right, up, toward;
  if (facing == Facing::Camera) {
    // The camera's own axes: the view is rigid, so its transpose carries
    // the picture's right, up and backward into the world.
    const glm::mat3 axes = glm::transpose(glm::mat3(eye.view()));
    right = axes[0];
    up = axes[1];
    toward = axes[2];
  } else {
    up = {0.0f, 1.0f, 0.0f};
    toward = eye.eye - at;
    toward.y = 0.0f;
    // Straight above or below, the camera's own backward direction on the
    // ground decides which way the body stands turned.
    if (glm::dot(toward, toward) < 1e-8f) {
      toward = eye.eye - eye.target;
      toward.y = 0.0f;
    }
    toward = glm::dot(toward, toward) > 1e-8f ? glm::normalize(toward)
                                              : glm::vec3(0.0f, 0.0f, 1.0f);
    right = glm::cross(up, toward);
  }
  glm::mat4 out(1.0f);
  out[0] = glm::vec4(right * scale.x, 0.0f);
  out[1] = glm::vec4(up * scale.y, 0.0f);
  out[2] = glm::vec4(toward * scale.z, 0.0f);
  out[3] = world[3];
  return out;
}

}  // namespace

void Scene::Impl::collectBodies(const geometry::mesh::camera::Camera& eye,
                                std::vector<Draw>& into) const {
  into.clear();
  // Back to front by view depth, so a nearer body covers a farther one
  // without a depth buffer. Stable, because two bodies at one depth must
  // land in tree order rather than in whichever order a sort happened to
  // leave them — a byte-identity gate reads the difference.
  const glm::mat4 view = eye.view();
  std::vector<std::pair<float, entt::entity>> sorted;
  sorted.reserve(order.size());
  for (entt::entity entity : order) {
    const glm::mat4& world = registry.get<component::Placement>(entity).world;
    const glm::vec4 centre = view * world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    sorted.emplace_back(centre.z, entity);
  }
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const std::pair<float, entt::entity>& a,
                      const std::pair<float, entt::entity>& b) {
                     return a.first < b.first;
                   });

  into.reserve(sorted.size());
  for (const auto& [depth, entity] : sorted) {
    const component::Body& body = registry.get<component::Body>(entity);
    if (!body.mesh) continue;
    const component::Surface& surface =
        registry.get<component::Surface>(entity);
    const component::Named& named = registry.get<component::Named>(entity);
    const component::Joined* joined =
        registry.try_get<component::Joined>(entity);
    // A body drawn once turns as a whole; instanced geometry turns each
    // instance where it stands, which only the device's vertex stage
    // reaches.
    const glm::mat4& placed = registry.get<component::Placement>(entity).world;
    into.push_back(Draw{
        .world = body.instances ? placed : faced(placed, body.facing, eye),
        .mesh = body.mesh,
        .instances = body.instances,
        .facing = body.instances ? body.facing : Facing::None,
        .backface = body.backface,
        .primitiveColor = body.primitiveColor,
        .geometry = body.id,
        .baseColor = surface.baseColor,
        .key = named.key,
        .collections = joined
                           ? std::span<const std::string>(joined->collections)
                           : kNoNames,
        .ancestors = named.ancestors,
        .material = surface.material ? &*surface.material : nullptr,
        .slots = surface.slots,
        .lit = surface.lit,
        .texture = surface.texture,
        .castShadow = body.castShadow,
        .receiveShadow = body.receiveShadow,
    });
  }
}

void Scene::Impl::deliverReadbacks() {
  // Taken at the end of one frame and handed over during the next,
  // which is what reading a device's memory costs — so a caller cannot
  // write code that only works where there is no device.
  for (auto& [result, callback] : captured)
    if (callback) callback(result);
  captured.clear();
}

bool Scene::Impl::phaseGraph() {
  deliverReadbacks();
  plan = graph::Plan();
  view = View{};
  draws.clear();
  if (frame.passes().empty() || !error.empty()) return false;

  if (frame.extent().x <= 0 || frame.extent().y <= 0) {
    error = "the frame declares passes and no extent to run them at";
    return false;
  }
  plan = graph::build(frame);
  if (!plan) {
    error = plan.error();
    return false;
  }

  targets.extent(frame.extent());

  collectBodies(viewpoint(), draws);
  if (std::string missing = missingTexturePass(); !missing.empty()) {
    error = std::move(missing);
    return false;
  }
  view.draws = draws;
  view.lights = lights;
  view.environment =
      environmentOf(frame.environmentMap(), frame.environmentOptions());
  view.camera = viewpoint();
  view.exposure = camera ? camera->exposure : 1.0f;
  view.toneCurve = camera ? camera->toneCurve : ToneCurve::Reinhard;
  view.extent = frame.extent();
  view.offset = frame.viewOffset();
  view.time = engine.elapsed();
  view.standIns = frame.standIns();
  view.timeStages = frame.timeStages();
  view.backdrops = frame.backdrops();

  // THE SHADOW STAGE a lit geometry pass opens with: one map per light
  // that casts, and none at all in a frame where nothing does.
  bool paints = false;
  for (const PassWork& work : plan.steps())
    paints = paints || (work.pass && work.pass->stage() == Stage::Geometry &&
                        !work.pass->body());
  stats.shadowMaps =
      paints
          ? (int64_t)shadowCasters(std::span<const PlacedLight>(lights)).size()
          : 0;
  stats.barriers = (int64_t)plan.barriers().size();
  stats.aliased = plan.aliased();
  stats.surfaces = plan.surfaces();
  return false;
}

bool Scene::Impl::phaseExecute() {
  if (frame.passes().empty() || !error.empty()) return false;
  const Runtime& runtime = frame.runtime();
  if (!runtime) {
    error =
        "the frame declares passes and carries no runtime to perform them "
        "on: a pass is drawn on a device";
    return false;
  }
  runtime->beginFrame(targets);
  restatedMaterials.clear();
  restatedMaterials.reserve(draws.size());
  for (const PassWork& work : plan.steps()) {
    ++stats.passes;
    // A TEXTURE PASS PAINTS WITH ITS OWN PRODUCER, at the frame's time,
    // and nothing of it is an executor's. One that found nothing changed
    // is counted, so skipped work is as visible as work done.
    if (work.pass->stage() == Stage::Texture) {
      if (!work.pass->producer()) continue;
      if (work.pass->producer()->produce(engine.elapsed()))
        ++stats.texturesPainted;
      else
        ++stats.texturesSkipped;
      restate(*work.pass->producer());
      continue;
    }
    runtime->execute(work, view, targets);
  }

  stats.shadowMapsDrawn = targets.takeShadowMapsDrawn();
  stats.programsPending = targets.takeProgramsPending();
  targets.takeDeviceStages(stats);
  for (const Readback& back : frame.readbacks()) {
    if (!back.callback()) continue;
    Readback::Result result;
    result.resource = back.name();
    result.frame = frameIndex;
    const graph::Resource* resource = plan.resource(back.name());
    if (resource && resource->kind == graph::ResourceKind::Points)
      result.points = targets.points(back.name());
    else
      result.image = targets.image(back.name());
    if (result.image) ++stats.readbacks;
    captured.emplace_back(std::move(result), back.callback());
  }
  targets.endFrame();
  // Last, because it is what turns this frame's resources into what the
  // next frame's `previous()` names: everything read back above still
  // wanted THIS frame's reading.
  runtime->endFrame(targets);
  return false;
}

}  // namespace sigil::world
