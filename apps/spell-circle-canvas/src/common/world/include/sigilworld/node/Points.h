#pragma once

/** @file
 * @ingroup world-node
 * POINTS: a set of points with a body — the stamp — standing at each of
 * them, whether the points are given or cooked from a point chain.
 */

#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilworld/advanced/Geometry.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/node/Verbs.h>

#include <optional>

namespace sigil::world {

/** A SET OF POINTS AND THE STAMP STANDING AT EACH. The stamp is oriented
 *  by the points' "normal" lane, scaled by their "size" lane and tinted
 *  by their "tint" lane wherever those lanes exist; it wears the node's
 *  surface. Points with no stamp cook and draw nothing, which is a
 *  legitimate node: the points are what a later pass reads. */
class Points : public Node,
               public PlacementVerbs<Points>,
               public IdentityVerbs<Points>,
               public SurfaceVerbs<Points> {
 public:
  Points() = default;
};

/** How a chain's points are cooked and which of them stand. */
struct PointsOptions {
  /** A WINDOW INTO THE LOOP the chain's first operator carries, so a
   *  moving head tows the points round it. Unset, the whole loop stands.
   *  @trap A moving window is moving GEOMETRY: every distinct pair of
   *  values cooks its own points. */
  std::optional<Window> window;
  /** The executor the chain is cooked on; every one produces the same
   *  points from the same chain. */
  geometry::mesh::pop::Runtime runtime = geometry::mesh::pop::Runtime::cpu();
};

/** @p stamp standing at every point @p chain cooks. */
Points points(geometry::mesh::pop::Chain chain, geometry::mesh::Mesh stamp,
              PointsOptions options = {});
/** @p stamp standing at every point of @p cloud. */
Points points(geometry::mesh::Cloud cloud, geometry::mesh::Mesh stamp);

}  // namespace sigil::world
