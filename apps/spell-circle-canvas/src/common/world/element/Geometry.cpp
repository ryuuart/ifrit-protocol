/** @file
 * Cooking a geometry slot, and the cheap signature a store buckets one
 * by.
 */

#include <sigilworld/element/Geometry.h>

#include <cstring>

namespace sigil::world {

namespace {

namespace gm = ::sigil::geometry::mesh;

/** FNV-1a, so a bucket is one function wherever it is computed. */
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t mix(uint64_t hash, uint64_t value) {
  for (unsigned byte = 0; byte < 8; ++byte) {
    hash ^= (value >> (byte * 8)) & 0xffull;
    hash *= kFnvPrime;
  }
  return hash;
}

uint64_t mixText(uint64_t hash, std::string_view text) {
  for (char c : text) {
    hash ^= (uint64_t)(unsigned char)c;
    hash *= kFnvPrime;
  }
  return hash;
}

uint64_t mixMesh(uint64_t hash, const Mesh& mesh) {
  hash = mix(hash, mesh.positions.size());
  hash = mix(hash, mesh.indices.size());
  return mix(hash, mesh.prims.size());
}

uint64_t mixCloud(uint64_t hash, const Cloud& cloud) {
  hash = mix(hash, cloud.size());
  hash = mix(hash, cloud.scalars.size());
  hash = mix(hash, cloud.vectors.size());
  return mix(hash, cloud.colors.size());
}

/** How the stamp rides its points: the conventional lanes every point
 *  operator already writes. The orient lane is "dir" where a chain
 *  produced one and "normal" where a generator did, so a cloud from
 *  either source stands its stamps up without the author naming a lane.
 */
gm::points::InstanceOptions stampOptions(const Cloud& cloud) {
  gm::points::InstanceOptions options;
  options.scaleLane = cloud.scalarIf("size") ? "size" : "";
  options.tintLane = cloud.colorIf("tint") ? "tint" : "";
  if (cloud.vectorIf("dir"))
    options.orientLane = "dir";
  else if (cloud.vectorIf("normal"))
    options.orientLane = "normal";
  return options;
}

Cooked cookPoints(Cloud cloud, const Mesh& stamp) {
  Cooked cooked;
  if (!stamp.positions.empty() && !cloud.positions.empty())
    cooked.mesh = gm::points::instance(cloud, stamp, stampOptions(cloud));
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
  uint64_t hash = mix(kFnvOffset, (uint64_t)geometry.index());
  if (const Mesh* mesh = std::get_if<Mesh>(&geometry))
    return mixMesh(hash, *mesh);
  if (const Stamped* stamped = std::get_if<Stamped>(&geometry))
    return mixMesh(mixCloud(hash, stamped->cloud), stamped->stamp);
  if (const Chained* chained = std::get_if<Chained>(&geometry)) {
    hash = mix(hash, chained->chain.size());
    for (const gm::pop::Op& op : chained->chain)
      hash = mix(hash, (uint64_t)op.index());
    return mixMesh(hash, chained->stamp);
  }
  if (const Generator* generator = std::get_if<Generator>(&geometry))
    if (*generator) return mixText(hash, (*generator)->name());
  return hash;
}

}  // namespace sigil::world
