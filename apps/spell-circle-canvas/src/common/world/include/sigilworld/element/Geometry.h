#pragma once

/** @file
 * What a node is made of: the four shapes a geometry slot takes — a
 * formed mesh, a cloud with the body stamped at every point, a point
 * chain with the runtime that cooks it, or a generator that builds its
 * own — and the cook that turns any of them into the points and
 * triangles a draw uses.
 *
 * There is no kind field anywhere in this library. The slot's value type
 * IS the kind: a node holding a Mesh and a node holding a Chained are
 * told apart by what they hold, so a node that changes from one to the
 * other resolves new resources and keeps its identity, its handle and
 * its lanes.
 */

#include <sigilcore/reconcile/Erased.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>

#include <cstdint>
#include <string>
#include <variant>

namespace sigil::world {

// The geometry currency, under the names a tree spells it. These are
// SigilGeometry's own types reached by a shorter word, not second copies
// of them: `world::Mesh` and `geometry::mesh::Mesh` name one entity.
using Mesh = geometry::mesh::Mesh;
using Cloud = geometry::mesh::Cloud;
using Chain = geometry::mesh::pop::Chain;
/** The executor a point chain cooks on, carried as a comparable value. */
using PopRuntime = geometry::mesh::pop::Runtime;

/** Points, and the body standing at each of them. The stamp is oriented
 *  by the cloud's "normal" lane, scaled by its "size" lane and tinted by
 *  its "tint" lane wherever those lanes exist — the names every point
 *  operator already writes. A cloud with no stamp cooks its points and
 *  draws nothing, which is a legitimate node: the points are what a
 *  later pass reads. */
struct Stamped {
  Cloud cloud;
  Mesh stamp;

  bool operator==(const Stamped&) const = default;
};

/** A point chain and the runtime that cooks it. The chain DESCRIBES the
 *  points; the runtime evaluates them, and every runtime is required to
 *  produce the same cloud from the same chain. The stamp is the body at
 *  each cooked point, on the same terms as Stamped's. */
struct Chained {
  Chain chain;
  PopRuntime runtime = PopRuntime::cpu();
  Mesh stamp;

  bool operator==(const Chained&) const = default;
};

/** The escape from the geometry vocabulary: a value that builds its own
 *  mesh. */
class GeneratorOps {
 public:
  GeneratorOps() = default;
  GeneratorOps(const GeneratorOps&) = default;
  GeneratorOps(GeneratorOps&&) = default;
  GeneratorOps& operator=(const GeneratorOps&) = default;
  GeneratorOps& operator=(GeneratorOps&&) = default;
  virtual ~GeneratorOps() = default;

  /** What this generator is called, in a message that names it. */
  [[nodiscard]] virtual std::string name() const = 0;
  /** Build the geometry. Called once per distinct generator value, and
   *  never again while that value is in the tree. */
  [[nodiscard]] virtual Mesh cook() const = 0;
};

/** A generator carried as a comparable value. A model with `==` declares
 *  its own identity, so two nodes describing the same generator share
 *  one cooked mesh; a model without one compares equal to nothing but
 *  its own copies, and therefore cooks again whenever it is described
 *  afresh. */
using Generator = core::Erased<GeneratorOps>;

/** THE GEOMETRY SLOT. An empty slot draws nothing — a node that is only
 *  a placement for its children, an emitter or a viewpoint. */
using Geometry =
    std::variant<std::monostate, Mesh, Stamped, Chained, Generator>;

/** What a geometry slot cooks to: the points it produced, when it
 *  produced any, and the triangles a draw uses. Both may be empty. */
struct Cooked {
  Cloud cloud;
  Mesh mesh;
};

/** Evaluate @p geometry. A Mesh is already cooked; a Stamped instances
 *  its stamp over its cloud; a Chained cooks its chain on its runtime
 *  and then instances; a Generator is asked. */
Cooked cook(const Geometry& geometry);

/** A cheap signature over @p geometry: equal values always share one,
 *  unequal values usually do not. It reads counts and kinds rather than
 *  contents, so it is a BUCKET and never an answer — the store that
 *  carries it decides membership with `==`. */
uint64_t signature(const Geometry& geometry);

}  // namespace sigil::world
