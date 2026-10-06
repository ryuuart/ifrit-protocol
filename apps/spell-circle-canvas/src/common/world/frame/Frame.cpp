/** @file
 * The frame value: the scene, its environment and camera, the passes,
 * the readbacks, and the few dials that say where the picture lands; and
 * an environment's options read once.
 */

#include <sigilgeometry/mesh/Transform.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/frame/Frame.h>

#include <glm/matrix.hpp>
#include <utility>

namespace sigil::world {

Readback readback(std::string name) { return Readback(std::move(name)); }

Environment environmentOf(const EnvironmentMap& map,
                          const EnvironmentOptions& options) {
  Environment out;
  out.map = map;
  out.next = options.next;
  out.intensity = options.intensity.value();
  out.diffuse = options.diffuse.value();
  out.specular = options.specular.value();
  out.roughnessBias = options.roughnessBias.value();
  out.crossfade = options.crossfade.value();
  out.shown = options.shown.value();
  out.blur = options.blur.value();
  out.groundRadius = options.groundRadius;
  // THE TURN ORIENTS THE PANORAMA. A panorama is sampled by a direction,
  // so only a rotation means anything to it, and the inverse of that
  // rotation is what carries a world direction into the panorama's own
  // frame. It is formed the way a node's placement is, so a turn of the
  // environment and a turn of a node are the same matrix.
  geometry::mesh::Transform turn;
  turn.rotateDegrees = {0.0f, options.rotation.value(), 0.0f};
  out.orientation = glm::inverse(glm::mat3(turn.matrix()));
  return out;
}

Frame& Frame::scene(Node root) {
  m_scene = std::move(root);
  return *this;
}

Frame& Frame::extent(glm::ivec2 size) {
  m_extent = size;
  return *this;
}

Frame& Frame::viewOffset(ViewOffset offset) {
  m_viewOffset = offset;
  return *this;
}

Frame& Frame::environment(EnvironmentMap map, EnvironmentOptions options) {
  m_environmentMap = std::move(map);
  m_environmentOptions = std::move(options);
  return *this;
}

Frame& Frame::camera(std::string key) {
  m_cameraKey = std::move(key);
  return *this;
}

Frame& Frame::defaultCamera(geometry::mesh::camera::Camera lens) {
  m_defaultCamera = lens;
  return *this;
}

Frame& Frame::cameraOverride(geometry::mesh::camera::Camera lens) {
  m_cameraOverride = lens;
  return *this;
}

Frame& Frame::pass(Pass p) {
  m_passes.push_back(std::move(p));
  return *this;
}

Frame& Frame::readback(Readback r) {
  m_readbacks.push_back(std::move(r));
  return *this;
}

Frame& Frame::runtime(Runtime r) {
  m_runtime = std::move(r);
  return *this;
}

Frame& Frame::standIns(bool drawStandIns) {
  m_standIns = drawStandIns;
  return *this;
}

Frame& Frame::timeStages(bool timed) {
  m_timeStages = timed;
  return *this;
}

Frame& Frame::present(std::string name) {
  m_present = std::move(name);
  return *this;
}

}  // namespace sigil::world
