#pragma once

/** @file
 * @ingroup world-node
 * What a node is made of: the three shapes a geometry slot takes — a
 * formed mesh, instanced geometry, or a generator — and the cook that
 * turns any of them into the triangles a draw uses. There is no kind field
 * anywhere in this library: the slot's value type IS the kind.
 */

#include <sigilcore/comparable/Erased.h>
#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Points.h>

#include <cstdint>
#include <string>
#include <variant>

namespace sigil::world {

/** The escape from the geometry vocabulary: a value that builds its own
 *  mesh. */
class GeneratorOperations {
 public:
  GeneratorOperations() = default;
  GeneratorOperations(const GeneratorOperations&) = default;
  GeneratorOperations(GeneratorOperations&&) = default;
  GeneratorOperations& operator=(const GeneratorOperations&) = default;
  GeneratorOperations& operator=(GeneratorOperations&&) = default;
  virtual ~GeneratorOperations() = default;

  /** What this generator is called, in a message that names it. */
  [[nodiscard]] virtual std::string name() const = 0;
  /** Build the geometry. Called once per distinct generator value, and
   *  never again while that value is in the tree. */
  [[nodiscard]] virtual geometry::mesh::Mesh cook() const = 0;
};

/** A generator carried as a comparable value. A model with `==` declares
 *  its own identity, so two nodes describing the same generator share
 *  one cooked mesh; a model without one compares equal to nothing but
 *  its own copies, and therefore cooks again whenever it is described
 *  afresh. */
using Generator = core::Erased<GeneratorOperations>;

/** THE GEOMETRY SLOT. An empty slot draws nothing — a node that is only
 *  a placement for its children, an emitter or a viewpoint. Instanced
 *  geometry is drawn as its stamp repeated once per instance. */
using Geometry = std::variant<std::monostate, geometry::mesh::Mesh,
                              geometry::mesh::Instances, Generator>;

/** What a geometry slot cooks to: the triangles a draw uses, which may be
 *  empty. Instanced geometry is already what is drawn, so it cooks to
 *  nothing and is read off the slot itself. */
struct Cooked {
  geometry::mesh::Mesh mesh;
};

/** Evaluate @p geometry. A Mesh is already cooked; instanced geometry and
 *  an empty slot cook to nothing; a Generator is asked. */
Cooked cook(const Geometry& geometry);

/** THE NUMBER TWO STAMPINGS AGREE ON — the stamps a pass draws over
 *  the points a compute pass cooked — when they are the same stamping:
 *  a fold over the VALUES of @p cloud and @p stamp, not over their
 *  shapes and not over their addresses. A cloud that has not moved must
 *  answer with the same number and a cloud that has moved must not,
 *  which is why this reads the bytes: it costs one pass over the cloud
 *  against forming its whole stamped mesh. */
uint64_t stampKey(const geometry::mesh::Cloud& cloud,
                  const geometry::mesh::Mesh& stamp);

/** A cheap signature over @p geometry: equal values always share one,
 *  unequal values usually do not. It reads counts and kinds rather than
 *  contents, so it is a BUCKET and never an answer — the store that
 *  carries it decides membership with `==`. */
uint64_t signature(const Geometry& geometry);

}  // namespace sigil::world
