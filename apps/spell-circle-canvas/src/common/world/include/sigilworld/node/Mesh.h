#pragma once

/** @file
 * @ingroup world-node
 * A MESH: one body, the geometry it is — triangles, or one stamp
 * repeated as instanced geometry — and the surface it wears. The one node
 * kind that draws geometry.
 */

#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilworld/advanced/Geometry.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/node/Verbs.h>

namespace sigil::world {

/** ONE BODY: geometry, placed, wearing a surface. */
class Mesh : public Node,
             public PlacementVerbs<Mesh>,
             public IdentityVerbs<Mesh>,
             public SurfaceVerbs<Mesh> {
 public:
  Mesh() = default;
  /** TURNS THE BODY TO FACE THE CAMERA the frame looks through, keeping
   *  where it stands and its scale: `Facing::Camera` square on to the
   *  picture, `Facing::CameraUpright` about the world's up axis only. The
   *  node's own rotation is replaced; the body's +z faces the camera. On
   *  instanced geometry every instance turns where it stands, keeping its
   *  own scale, and the node's placement still carries them all. */
  Mesh& facing(Facing turned) {
    edit().facing = turned;
    return *this;
  }
};

/** A body of @p triangles, lit at the stock surface until it is
 *  `fill`ed. */
Mesh mesh(geometry::mesh::Mesh triangles);
/** A body of @p instances: their stamp drawn once per instance, each copy
 *  placed within the node's placement and carrying its own colour and
 *  attributes to the surface. */
Mesh mesh(geometry::mesh::Instances instances);
/** A body a value builds itself — the escape from the geometry
 *  vocabulary. */
Mesh mesh(Generator generator);

}  // namespace sigil::world
