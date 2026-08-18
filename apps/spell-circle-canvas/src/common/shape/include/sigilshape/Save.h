#pragma once

/** @file
 * SigilShape save — geometry OUT to the interchange world, the return
 * leg of Import.h. PLY is the carrier, being the one widely read format
 * where arbitrary per-vertex attributes are first-class:
 *
 *  - a Cloud writes positions plus EVERY lane — "normal" as nx/ny/nz,
 *    "uv" as s/t, "tint" as uchar red/green/blue/alpha (so Blender
 *    shows vertex colors on import), scalar lanes under their own
 *    names, other vector lanes as name_x/_y/_z and other color lanes
 *    as name_r/_g/_b/_a. Import.h's PLY reader folds those suffixed
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
 * A typical use: World::readChain() a GPU-cooked pop surface and
 * save::ply() it — compute-shader geometry, attributes and all,
 * opened in Houdini or Blender.
 */

#include <filesystem>
#include <string>

#include "sigilshape/Mesh.h"
#include "sigilshape/Points.h"

namespace sigil::shape::save {

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

/** File conveniences; false when the geometry is empty or the file
 *  cannot be written. */
bool ply(const std::filesystem::path& file, const Cloud& cloud,
         const PlyOptions& options = {});
bool ply(const std::filesystem::path& file, const Mesh& mesh,
         const PlyOptions& options = {});

}  // namespace sigil::shape::save
