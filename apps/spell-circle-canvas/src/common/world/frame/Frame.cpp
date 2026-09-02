/** @file
 * The frame value: the scene, the passes, the readbacks, and the few
 * dials that say where the picture lands.
 */

#include <sigilworld/frame/Frame.h>

#include <utility>

namespace sigil::world {

Readback readback(std::string name) { return Readback(std::move(name)); }

Frame& Frame::scene(Element root) {
  m_scene = std::move(root);
  return *this;
}

Frame& Frame::extent(SkISize size) {
  m_extent = size;
  return *this;
}

Frame& Frame::camera(geometry::mesh::camera::Camera c) {
  m_camera = c;
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

Frame& Frame::present(std::string name) {
  m_present = std::move(name);
  return *this;
}

}  // namespace sigil::world
