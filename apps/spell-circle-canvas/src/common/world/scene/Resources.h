#pragma once

/** @file
 * The content-keyed resource store: a geometry slot cooked once per
 * distinct value, shared by every node that describes it, and dropped
 * when the last of them lets go.
 */

#include <sigilworld/element/Geometry.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sigil::world {

/** ONE COOKED ARTEFACT and the nodes holding it.
 *
 *  The entry's ADDRESS is stable for its whole life, so a node holds a
 *  pointer rather than an index and a store that grew underneath it
 *  cannot move what it is drawing. */
struct Resource {
  /** THE KEY: the geometry value this artefact was cooked from. Two
   *  nodes match one entry when their geometry compares equal. */
  Geometry geometry;
  /** A cheap signature over the key, so the lookup compares contents
   *  only against the handful of entries that could match. */
  uint64_t bucket = 0;
  Cooked cooked;
  int references = 0;
};

/** THE STORE. Keyed by the geometry VALUE, not by a hash of it: the
 *  signature buckets a lookup and `operator==` decides it, so two
 *  different geometries can never be served one artefact however their
 *  bytes happen to fold together. */
class ResourceStore {
 public:
  /** The artefact for @p geometry, cooking it when no entry holds it,
   *  and one more reference to it either way. Adds one to @p cooked when
   *  it cooked. Null for an empty slot, which resolves nothing. */
  Resource* acquire(Geometry geometry, int64_t* cooked);
  /** One fewer reference; the entry is dropped when the last one goes.
   *  Null is a no-op, so a node that resolved nothing releases nothing.
   */
  void release(Resource* resource);

  /** How many distinct artefacts are held. */
  [[nodiscard]] size_t size() const { return m_entries.size(); }

 private:
  std::vector<std::unique_ptr<Resource>> m_entries;
};

}  // namespace sigil::world
