#pragma once

/** @file
 * A mesh read by its FACES rather than by its triangles, and the
 * rotation that turns one of them to face a direction.
 *
 * A triangle is what a mesh is drawn from; a face is what a solid is
 * made of, and the two are only the same thing when every face happens
 * to be a triangle. The `"Id"` primitive lane is the seam between them:
 * `.x` is the face a triangle belongs to, which is what a generator that
 * fans a pentagon into three triangles writes. A mesh with no such lane
 * has one face per triangle, so every reader here works on any mesh.
 *
 * `edges()` is the same reading one step further: an edge of the SOLID
 * is where two faces meet, so a chord a fan drew across the inside of one
 * face is not an edge at all, and neither reader can be written from the
 * index buffer alone.
 *
 * `faceUp()` is the pose of a placed solid — "which rotation puts face N
 * up" — and it is a rotation about the centre, so a solid built around
 * the origin stays where it is while the chosen face turns.
 */

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"

namespace sigil::geometry::mesh {

/** How many faces the mesh has: one more than the largest `"Id"` a
 *  triangle carries, or the triangle count where the lane is absent. */
size_t faceCount(const Mesh& mesh);

/** The outward unit normal of face @p face — its triangles' geometric
 *  normals summed by area, so a fanned polygon answers its own plane and
 *  a face that is not quite flat answers where most of it points. Faces
 *  outside the count answer +y. */
glm::vec3 faceNormal(const Mesh& mesh, size_t face);

/** The centroid of face @p face: the mean of the positions its triangles
 *  stand on, each counted once however many triangles share it. Where a
 *  numeral, a decal or a label on that face is centred. */
glm::vec3 faceCentroid(const Mesh& mesh, size_t face);

/** The face directly across from @p face — the one whose centroid is
 *  this one's negated, within @p tolerance of the mesh's own extent.
 *  Nothing where the solid has no face there, which is every odd-faced
 *  solid: a tetrahedron answers a vertex across from each of its faces
 *  and this answers nothing.
 *
 *  Measured from the centroids rather than assumed from an index, so it
 *  holds for a mesh whose faces were emitted in any order. */
std::optional<size_t> opposedFace(const Mesh& mesh, size_t face,
                                  float tolerance = 1e-3f);

/** Where two faces meet: the corners the edge runs between, and the two
 *  faces on it. `opposite` is `kNoFace` on a border edge, which is what a
 *  sheet or an open shell has all round it.
 *
 *  The corners are index into the mesh's positions, so an edge is only
 *  shared where the corner IS: on a mesh whose faces each carry their own
 *  corners, every edge is a border edge, and the reading that wants
 *  neighbours wants the welded form. */
struct Edge {
  uint32_t from = 0, to = 0;  ///< from < to
  size_t face = 0;
  size_t opposite = 0;
};

/** The value `Edge::opposite` carries where nothing is across. */
inline constexpr size_t kNoFace = (size_t)-1;

/** Every edge of the mesh's faces, in the order they are first met.
 *  A chord INSIDE one face — the seam a fan leaves across a pentagon —
 *  is not an edge and is not returned, which is what makes the count of
 *  this the E in V - E + F. */
std::vector<Edge> edges(const Mesh& mesh);

/** The rotation that lands face @p face's outward normal on @p up — the
 *  shortest one, about the axis perpendicular to both, so nothing else
 *  about the attitude is decided and a caller that wants a tilt or a
 *  spin composes it on top.
 *
 *  @p up is a direction and not a name: +y stands the face up on a
 *  ground plane, +z turns it square to a viewer looking down that axis.
 *  A face already pointing along @p up answers the identity; one
 *  pointing exactly against it turns half a turn about an arbitrary
 *  perpendicular, because every such rotation lands it. */
glm::mat4 faceUp(const Mesh& mesh, size_t face, glm::vec3 up = {0, 1, 0});

}  // namespace sigil::geometry::mesh
