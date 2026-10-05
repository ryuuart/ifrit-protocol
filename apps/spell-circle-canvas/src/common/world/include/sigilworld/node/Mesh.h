#pragma once

/** @file
 * @ingroup world-node
 * A MESH: one body, the triangles it is, and the surface they wear.
 */

#include <sigilgeometry/mesh/Mesh.h>
#include <sigilworld/advanced/Geometry.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/node/Verbs.h>

namespace sigil::world {

/** ONE BODY: triangles, placed, wearing a surface. */
class Mesh : public Node,
             public PlacementVerbs<Mesh>,
             public IdentityVerbs<Mesh>,
             public SurfaceVerbs<Mesh> {
 public:
  Mesh() = default;
};

/** A body of @p triangles, lit at the stock surface until it is
 *  `fill`ed. */
Mesh mesh(geometry::mesh::Mesh triangles);
/** A body a value builds itself — the escape from the geometry
 *  vocabulary. */
Mesh mesh(Generator generator);

}  // namespace sigil::world
