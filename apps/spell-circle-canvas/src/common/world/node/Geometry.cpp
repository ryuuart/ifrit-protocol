/** @file
 * Cooking a geometry slot, and the cheap signature a store buckets one
 * by.
 */

#include <sigilcore/compute/Hash.h>
#include <sigilworld/advanced/Geometry.h>

#include <cstring>
#include <vector>

namespace sigil::world {

namespace {

namespace gm = ::sigil::geometry::mesh;

// The fold is SigilCoreCompute's, so a bucket is one number wherever it
// is computed.
using core::hash::fnv1a;
using core::hash::kFnvOffset;

uint64_t mixMesh(uint64_t hash, const geometry::mesh::Mesh& mesh) {
  hash = fnv1a(hash, mesh.positions.size());
  hash = fnv1a(hash, mesh.indices.size());
  return fnv1a(hash, mesh.primitives.size());
}

/** Every float of a lane folded in, read as its bits so that two values
 *  differing in their last place are two values. */
uint64_t mixFloats(uint64_t hash, const float* values, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    uint32_t bits = 0;
    std::memcpy(&bits, values + i, sizeof(bits));
    hash = fnv1a(hash, (uint64_t)bits);
  }
  return hash;
}

template <class T>
uint64_t mixLane(uint64_t hash, const std::vector<T>& lane) {
  hash = fnv1a(hash, lane.size());
  return mixFloats(hash, reinterpret_cast<const float*>(lane.data()),
                   lane.size() * (sizeof(T) / sizeof(float)));
}

}  // namespace

uint64_t stampKey(const geometry::mesh::Cloud& cloud,
                  const geometry::mesh::Mesh& stamp) {
  uint64_t hash = fnv1a(kFnvOffset, cloud.size());
  hash = mixLane(hash, cloud.positions);
  // The named lanes fold with their names, so a lane renamed is a
  // different stamping — it is, since the table that stands a stamp up
  // reads lanes by name.
  for (const auto& [name, values] : cloud.scalars)
    hash = mixLane(fnv1a(hash, name), values);
  for (const auto& [name, values] : cloud.vectors)
    hash = mixLane(fnv1a(hash, name), values);
  for (const auto& [name, values] : cloud.colors)
    hash = mixLane(fnv1a(hash, name), values);
  hash = mixLane(hash, stamp.positions);
  hash = mixLane(hash, stamp.normals);
  hash = mixLane(hash, stamp.uvs);
  hash = mixLane(hash, stamp.colors);
  hash = fnv1a(hash, stamp.indices.size());
  for (uint32_t index : stamp.indices) hash = fnv1a(hash, (uint64_t)index);
  return hash;
}

Cooked cook(const Geometry& geometry) {
  if (const geometry::mesh::Mesh* mesh =
          std::get_if<geometry::mesh::Mesh>(&geometry))
    return {*mesh};
  if (const Generator* generator = std::get_if<Generator>(&geometry))
    if (*generator) return {(*generator)->cook()};
  return {};
}

uint64_t signature(const Geometry& geometry) {
  uint64_t hash = fnv1a(kFnvOffset, (uint64_t)geometry.index());
  if (const geometry::mesh::Mesh* mesh =
          std::get_if<geometry::mesh::Mesh>(&geometry))
    return mixMesh(hash, *mesh);
  if (const gm::Instances* instances = std::get_if<gm::Instances>(&geometry)) {
    hash = fnv1a(hash, instances->size());
    hash = fnv1a(hash, instances->attributes.size());
    return mixMesh(hash, instances->stamp);
  }
  if (const Generator* generator = std::get_if<Generator>(&geometry))
    if (*generator) return fnv1a(hash, (*generator)->name());
  return hash;
}

}  // namespace sigil::world
