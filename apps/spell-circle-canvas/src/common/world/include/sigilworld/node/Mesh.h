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

#include <string>
#include <utility>

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
  /** TINTS EACH TRIANGLE BY ITS OWN COLOUR: the primitive lane named
   *  @p lane — one colour per triangle, conventionally `"Color"` — is
   *  multiplied into every triangle's colour as a vertex colour is, so two
   *  triangles that share their corners still differ. A lane the mesh does
   *  not carry, or one not sized to its triangles, tints nothing. Instanced
   *  geometry reads its stamp's lane. */
  Mesh& primitiveColor(std::string lane = "Color") {
    edit().primitiveColor = std::move(lane);
    return *this;
  }
  /** WHETHER THE BODY CASTS SHADOWS from the lights that cast them. Every
   *  body that writes depth does unless it says otherwise; a light's
   *  shadow link narrows it further. */
  Mesh& castShadow(bool casts) {
    edit().castShadow = casts;
    return *this;
  }
  /** WHETHER SHADOWS DARKEN THE BODY. Every lit body is darkened by them
   *  unless it says otherwise. */
  Mesh& receiveShadow(bool receives) {
    edit().receiveShadow = receives;
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

/** The geometry slot's reading of a solid value: the value is the
 *  generator's identity and its `mesh()` is the cook, so two descriptions
 *  naming equal values share one cooked mesh and a value that stands
 *  unchanged is formed once. */
template <geometry::mesh::Solid Value>
class SolidGenerator final : public GeneratorOperations {
 public:
  explicit SolidGenerator(Value solid) : m_solid(std::move(solid)) {}
  [[nodiscard]] std::string name() const override { return "solid"; }
  [[nodiscard]] geometry::mesh::Mesh cook() const override {
    return m_solid.mesh();
  }
  bool operator==(const SolidGenerator& other) const {
    return m_solid == other.m_solid;
  }

 private:
  Value m_solid;
};

/** A body of a solid spelled as a value — `geometry::solids::Torus`,
 *  `Superellipsoid`, a consumer's own `geometry::mesh::Solid` — whose
 *  geometry slot holds the value and forms its mesh once while the value
 *  stands unchanged, where a formed mesh handed over is compared triangle
 *  for triangle on every description. */
template <geometry::mesh::Solid Value>
Mesh mesh(Value solid) {
  return mesh(Generator(SolidGenerator<Value>(std::move(solid))));
}

}  // namespace sigil::world
