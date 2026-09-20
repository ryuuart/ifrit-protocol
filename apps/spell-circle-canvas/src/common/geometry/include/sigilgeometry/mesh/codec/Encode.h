#pragma once

/** @file
 * @ingroup geometry-mesh
 *
 * SigilGeometry save — geometry OUT to the interchange world, the return
 * leg of the readers in Decode.h. PLY is the carrier, being the one
 * widely read format where arbitrary per-vertex attributes are
 * first-class: a Cloud writes positions plus EVERY lane, a Mesh its
 * vertices and triangles. Ascii is the default because the result is
 * readable and diffable; binary is for a file that has to round-trip
 * exactly or has to be small.
 *
 * Houdini's JSON `.geo` is the second carrier, and the one to reach for
 * when the destination IS Houdini: the same lanes travel under the
 * names that side already knows them by.
 */

#include <filesystem>
#include <string>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/pop/Points.h"

/** THE WRITERS — geometry out to the interchange world, the return leg
 *  of the readers. PLY is the carrier, being the one widely read format
 *  where arbitrary per-vertex attributes are first-class, so a cloud's
 *  every lane survives the round trip; the other formats carry what
 *  they can carry. */
namespace sigil::geometry::mesh::codec::encode {

/** Format choice. Binary keeps the text header but writes rows as raw
 * little-endian bytes — same properties, same order, bit-exact floats
 * on the round trip (ascii's %g is not). */
struct PlyOptions {
  bool binary = false;
};

/** The Cloud as a PLY (faceless — an honest point cloud). Empty
 *  geometry declines with an empty string — a zero-vertex PLY is one
 *  our own importer refuses. */
std::string ply(const Cloud& cloud, const PlyOptions& options = {});

/** The Mesh as a PLY with faces. Empty geometry declines likewise. */
std::string ply(const Mesh& mesh, const PlyOptions& options = {});

/** THE CLOUD AS HOUDINI'S JSON `.geo`: the points, `P`, and every lane
 *  — `normal` as N, `uv` as uv with its v axis flipped back to the
 *  file's convention, `tint` as a four-component Cd so the alpha rides
 *  in it, and every other lane under its own name. Empty geometry
 *  declines with an empty string, as the PLY writer does.
 *  @silent a lane arrived as a GROUP: nothing here can tell it from any
 *  other 0/1 scalar, so it leaves as the attribute it became. */
std::string geo(const Cloud& cloud);

/** THE MESH AS A `.geo` of closed polygons, one per triangle, with the
 *  vertex attributes on the points and every `Mesh::primitives` lane as
 *  a four-component primitive attribute. A mesh with no faces is a
 *  point cloud and is written as one.
 *  @trap It comes back UNWELDED — the format addresses a polygon's
 *  corners through a vertex list, so a cube written with 8 shared
 *  positions returns with 36, every value the same but the count. */
std::string geo(const Mesh& mesh);

/** File conveniences; false when the geometry is empty or the file
 *  cannot be written. */
bool ply(const std::filesystem::path& file, const Cloud& cloud,
         const PlyOptions& options = {});
/** The mesh conveniences, declining on the same terms. */
bool ply(const std::filesystem::path& file, const Mesh& mesh,
         const PlyOptions& options = {});
/** The `.geo` conveniences, declining on the same terms. */
bool geo(const std::filesystem::path& file, const Cloud& cloud);
/** The same for a mesh, whose vertices become the file's points. */
bool geo(const std::filesystem::path& file, const Mesh& mesh);

}  // namespace sigil::geometry::mesh::codec::encode
