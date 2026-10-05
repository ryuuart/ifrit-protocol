/** @file
 * The node handle's copy-on-write, the factories that start each kind,
 * and the lowering a filled surface goes through. Nothing here cooks
 * geometry, resolves a material or touches a clock — that is the Scene's
 * side of the seam.
 */

#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/node/Camera.h>
#include <sigilworld/node/Group.h>
#include <sigilworld/node/Mesh.h>
#include <sigilworld/node/Null.h>
#include <sigilworld/node/Verbs.h>

#include <utility>

namespace sigil::world {

Node::Node() : m_node(std::make_shared<NodeDescription>()) {}

Node::Node(std::shared_ptr<NodeDescription> description)
    : m_node(std::move(description)) {}

NodeDescription& Node::edit() {
  if (!m_node)
    m_node = std::make_shared<NodeDescription>();
  else if (m_node.use_count() != 1)
    m_node = std::make_shared<NodeDescription>(*m_node);
  return *m_node;
}

material::Material worn(const material::Material& material) {
  if (material.hasProgram() || material.surface())
    return material::surface::lower(material);
  return material::surface::lower(
      material::from(material).surface(material::SurfaceOptions{}));
}

Group group(std::initializer_list<Children> children) {
  Group out;
  std::vector<Node>& into = out.edit().children;
  for (const Children& run : children)
    into.insert(into.end(), run.nodes.begin(), run.nodes.end());
  return out;
}

Mesh mesh(geometry::mesh::Mesh triangles) {
  Mesh out;
  out.edit().geometry = std::move(triangles);
  return out;
}

Mesh mesh(geometry::mesh::Instances instances) {
  Mesh out;
  out.edit().geometry = std::move(instances);
  return out;
}

Null null() { return Null(); }

Mesh mesh(Generator generator) {
  Mesh out;
  out.edit().geometry = std::move(generator);
  return out;
}

Camera camera(geometry::mesh::camera::Camera lens, CameraOptions options) {
  Camera out;
  out.edit().camera = CameraDescription{.lens = lens,
                                        .exposure = std::move(options.exposure),
                                        .toneCurve = options.toneCurve};
  return out;
}

const CameraDescription& Camera::description() const {
  static const CameraDescription none;
  const NodeDescription* described = node().get();
  return described && described->camera ? *described->camera : none;
}

Node detail::makeMemo(
    std::any properties,
    std::function<bool(const std::any&, const std::any&)> equal,
    std::function<Node(const std::any&)> invoke) {
  Node out;
  out.edit().memo = Memo{std::move(properties), std::move(equal),
                         std::move(invoke), core::environment::capture()};
  return out;
}

}  // namespace sigil::world
