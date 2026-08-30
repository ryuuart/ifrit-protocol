/** @file
 * The two phases a frame with passes adds: the ordering, which turns
 * the declarations into steps and gives the resources their surfaces,
 * and the execution, which performs those steps on the runtime the
 * frame carries and takes what the caller asked to read back.
 *
 * Both read the components extract wrote. Neither can reach the Element
 * tree, and that is the point.
 */

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "SceneImpl.h"

namespace sigil::world {

namespace {

/** An empty span of words, for a body carrying no tags. */
constexpr std::span<const std::string> kNoWords;

}  // namespace

Camera Scene::Impl::viewpoint() const {
  return camera ? *camera : frame.camera();
}

void Scene::Impl::collectBodies(const Camera& eye,
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
    const component::Tagged* tagged =
        registry.try_get<component::Tagged>(entity);
    into.push_back(Draw{
        .world = registry.get<component::Placement>(entity).world,
        .mesh = body.mesh,
        .geometry = body.id,
        .baseColor = surface.baseColor,
        .key = named.key,
        .tags = tagged ? std::span<const std::string>(tagged->words) : kNoWords,
        .ancestors = named.ancestors,
        .material = surface.material ? &*surface.material : nullptr,
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
  if (frame.passes().empty()) return false;

  if (frame.extent().isEmpty()) {
    error = "the frame declares passes and no extent to run them at";
    return false;
  }
  plan = graph::build(frame);
  if (!plan) {
    error = plan.error();
    return false;
  }

  targets.extent(frame.extent());
  targets.unbind();
  for (const graph::Resource& resource : plan.resources()) {
    if (resource.kind != graph::Kind::Image) continue;
    targets.bind(resource.name, resource.slot);
  }
  for (const std::string& name : plan.kept()) targets.keep(name);

  collectBodies(viewpoint(), draws);
  view.draws = draws;
  view.lights = lights;
  view.camera = viewpoint();
  view.extent = frame.extent();

  stats.barriers = (int64_t)plan.barriers().size();
  stats.aliased = plan.aliased();
  stats.surfaces = plan.surfaces();
  return false;
}

bool Scene::Impl::phaseExecute() {
  if (frame.passes().empty() || !error.empty()) return false;
  const Runtime& runtime = frame.runtime();
  if (!runtime) {
    error = "the frame carries no executor to perform its passes on";
    return false;
  }
  runtime->beginFrame(targets);
  for (const PassWork& work : plan.steps()) {
    runtime->execute(work, view, targets);
    ++stats.passes;
  }

  for (const Readback& back : frame.readbacks()) {
    if (!back.callback()) continue;
    Readback::Result result;
    result.resource = back.name();
    result.frame = frameIndex;
    const graph::Resource* resource = plan.resource(back.name());
    if (resource && resource->kind == graph::Kind::Points)
      result.points = targets.points(back.name());
    else
      result.image = targets.image(back.name());
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
