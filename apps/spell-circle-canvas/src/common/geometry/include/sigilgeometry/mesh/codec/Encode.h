#pragma once

/** @file
 * SigilGeometry save — geometry OUT to the interchange world, the return
 * leg of the readers in Decode.h. PLY is the carrier, being the one
 * widely read format
 * where arbitrary per-vertex attributes are first-class:
 *
 *  - a Cloud writes positions plus EVERY lane — "normal" as nx/ny/nz,
 *    "uv" as s/t, "tint" as uchar red/green/blue/alpha (so Blender
 *    shows vertex colors on import), scalar lanes under their own
 *    names, other vector lanes as name_x/_y/_z and other color lanes
 *    as name_r/_g/_b/_a. The PLY reader folds those suffixed
 *    triples/quads back into lanes, so a round trip is lossless (up
 *    to the uchar color quantization).
 *  - a Mesh writes vertices (positions/normals/uvs/colors) plus its
 *    triangles.
 *
 * Ascii is the default because the result is readable and diffable.
 * Choose binary (PlyOptions) when the file has to round-trip or has to
 * be small: rows become raw little-endian bytes, so floats survive
 * exactly instead of through a decimal spelling, the file carries no
 * token text, and neither writer nor reader formats or parses numbers.
 *
 * Houdini's JSON `.geo` is the second carrier, and it is the one to reach
 * for when the destination IS Houdini: the same lanes travel, under the
 * names that side already knows them by, with no suffix folding to
 * arrange between the two spellings. It is the exact return leg of the
 * `.geo` reader in Decode.h — everything that reader understands, and
 * nothing it does not.
 *
 * A typical use: World::readChain() a GPU-cooked pop surface and
 * encode::ply() it — compute-shader geometry, attributes and all,
 * opened in Houdini or Blender.
 */

#include <filesystem>
#include <string>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/pop/Points.h"

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

/** The Cloud as Houdini's JSON `.geo`: the points, `P`, and every lane —
 *  `normal` as N, `uv` as uv with its v axis flipped back to the file's
 *  convention, `tint` as a four-component Cd so the alpha rides in it,
 *  and every other lane under its own name at the width that brings it
 *  back as the same kind of lane. Empty geometry declines with an empty
 *  string, as the PLY writer does.
 *
 *  A GROUP IS NOT WRITTEN AS A GROUP. The reader turns a `.geo` group
 *  into a 0/1 scalar lane and nothing here can tell such a lane from any
 *  other scalar, so a lane that arrived as a group leaves as the
 *  attribute it became — which is what round-trips, and what a mask reads
 *  as either way. */
std::string geo(const Cloud& cloud);

/** The Mesh as a `.geo` of closed polygons, one per triangle, with the
 *  vertex attributes on the points and every `Mesh::prims` lane as a
 *  four-component primitive attribute. A mesh with no faces is a point
 *  cloud and is written as one.
 *
 *  IT COMES BACK UNWELDED, and that is the format rather than the writer:
 *  a `.geo` addresses a polygon's corners through a vertex list, and the
 *  reader gives every corner its own mesh vertex so that a per-corner uv
 *  or normal survives a seam. A cube written with 8 shared positions
 *  returns with 36. The positions, the winding and every attribute value
 *  are the same; the vertex COUNT is not. */
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
bool geo(const std::filesystem::path& file, const Mesh& mesh);

}  // namespace sigil::geometry::mesh::codec::encode
