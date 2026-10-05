/** @file
 * The frame's named resources: the images an executor answers for, the
 * point sets a compute pass cooks, and the stamped meshes formed from
 * them.
 */

#include <sigilworld/advanced/Skia.h>

#include <iterator>
#include <string>
#include <utility>

namespace sigil::world {

sk_sp<SkImage> Targets::image(std::string_view name) {
  return m_source ? m_source(name) : nullptr;
}

geometry::mesh::Cloud* Targets::points(std::string_view name) {
  return &m_points[std::string(name)];
}

const geometry::mesh::Cloud* Targets::points(std::string_view name) const {
  const auto it = m_points.find(std::string(name));
  return it == m_points.end() ? nullptr : &it->second;
}

const geometry::mesh::Mesh* Targets::stamped(const geometry::mesh::Cloud& cloud,
                                             const geometry::mesh::Mesh& stamp,
                                             uint64_t* key) {
  if (cloud.positions.empty() || stamp.positions.empty()) return nullptr;
  // A FOLD IS A BUCKET AND NEVER AN ANSWER, here as in the resource
  // store: an entry holds the pair it was formed from and is confirmed
  // by value, and a pair that folds onto another's number takes the
  // next free one. The number is also what a device keys its upload by,
  // so two pairs sharing one would hand a set the other's mesh.
  uint64_t folded = stampKey(cloud, stamp);
  while (true) {
    const auto found = m_stamped.find(folded);
    if (found == m_stamped.end()) break;
    if (found->second.cloud == cloud && found->second.stamp == stamp) {
      found->second.used = m_frame;
      if (key) *key = folded;
      return &found->second.mesh;
    }
    ++folded;
  }
  if (key) *key = folded;
  Stamping made;
  // How the stamp rides its points is the point operators' own table —
  // one convention, so a cloud stands its stamps up the same way here
  // and through `pop::cookMesh`.
  made.mesh = cook(Stamped{cloud, stamp}).mesh;
  made.cloud = cloud;
  made.stamp = stamp;
  made.used = m_frame;
  ++m_stampings;
  return &m_stamped.emplace(folded, std::move(made)).first->second.mesh;
}

void Targets::endFrame() {
  // A stamping no pass asked for this frame is let go: the set it came
  // from may have been dropped, and holding its mesh would hold a copy
  // of geometry nothing draws.
  for (auto it = m_stamped.begin(); it != m_stamped.end();)
    it = it->second.used < m_frame ? m_stamped.erase(it) : std::next(it);
  ++m_frame;
}

}  // namespace sigil::world
