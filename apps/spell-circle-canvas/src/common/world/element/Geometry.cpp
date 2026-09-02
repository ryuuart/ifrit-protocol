/** @file
 * Cooking a geometry slot, and the cheap signature a store buckets one
 * by.
 */

#include <sigilcore/compute/Hash.h>
#include <sigilworld/element/Geometry.h>

#include <cstring>

namespace sigil::world {

namespace {

namespace gm = ::sigil::geometry::mesh;

// The fold is SigilCoreCompute's, so a bucket is one number wherever it
// is computed.
using core::hash::fnv1a;
using core::hash::kFnvOffset;

uint64_t mixMesh(uint64_t hash, const Mesh& mesh) {
  hash = fnv1a(hash, mesh.positions.size());
  hash = fnv1a(hash, mesh.indices.size());
  return fnv1a(hash, mesh.prims.size());
}

uint64_t mixCloud(uint64_t hash, const Cloud& cloud) {
  hash = fnv1a(hash, cloud.size());
  hash = fnv1a(hash, cloud.scalars.size());
  hash = fnv1a(hash, cloud.vectors.size());
  return fnv1a(hash, cloud.colors.size());
}

Cooked cookPoints(Cloud cloud, const Mesh& stamp) {
  Cooked cooked;
  // How the stamp rides its points is the point operators' own table —
  // one convention, so a cloud stands its stamps up the same way here
  // and through `pop::cookMesh`.
  if (!stamp.positions.empty() && !cloud.positions.empty())
    cooked.mesh =
        gm::points::instance(cloud, stamp, gm::points::stampOptions(cloud));
  cooked.cloud = std::move(cloud);
  return cooked;
}

}  // namespace

Cooked cook(const Geometry& geometry) {
  if (const Mesh* mesh = std::get_if<Mesh>(&geometry)) return {{}, *mesh};
  if (const Stamped* stamped = std::get_if<Stamped>(&geometry))
    return cookPoints(stamped->cloud, stamped->stamp);
  if (const Chained* chained = std::get_if<Chained>(&geometry)) {
    if (chained->chain.empty()) return {};
    return cookPoints(gm::pop::cook(chained->chain, chained->runtime),
                      chained->stamp);
  }
  if (const Generator* generator = std::get_if<Generator>(&geometry))
    if (*generator) return {{}, (*generator)->cook()};
  return {};
}

uint64_t signature(const Geometry& geometry) {
  uint64_t hash = fnv1a(kFnvOffset, (uint64_t)geometry.index());
  if (const Mesh* mesh = std::get_if<Mesh>(&geometry))
    return mixMesh(hash, *mesh);
  if (const Stamped* stamped = std::get_if<Stamped>(&geometry))
    return mixMesh(mixCloud(hash, stamped->cloud), stamped->stamp);
  if (const Chained* chained = std::get_if<Chained>(&geometry)) {
    hash = fnv1a(hash, chained->chain.size());
    for (const gm::pop::Op& op : chained->chain)
      hash = fnv1a(hash, (uint64_t)op.index());
    return mixMesh(hash, chained->stamp);
  }
  if (const Generator* generator = std::get_if<Generator>(&geometry))
    if (*generator) return fnv1a(hash, (*generator)->name());
  return hash;
}

}  // namespace sigil::world
