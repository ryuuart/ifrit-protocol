#pragma once

/** @file
 * @ingroup geometry-mesh
 *
 * SigilGeometry model decode — files into the Mesh currency.
 *
 * Six formats cover the practical interchange world:
 *  - Wavefront OBJ (+ MTL): the classic text workhorse (tinyobjloader);
 *  - glTF 2.0, .gltf and .glb: the modern standard (cgltf) — node
 *    transforms baked, base-color material honored, buffers reachable
 *    whether embedded (GLB chunk, data: URIs) or external files;
 *  - STL, ascii and binary: the 3D-print staple;
 *  - PLY, ascii and binary little-endian: THE attribute carrier —
 *    every non-conventional vertex property becomes a named lane,
 *    every FACE property a primitive lane on Mesh::primitives, and
 *    faceless files are honest point clouds;
 *  - Alembic, .abc Ogawa: the vfx cache — meshes and point clouds at
 *    a chosen time, arbGeomParams as lanes (Alembic library);
 *  - Houdini .geo (JSON): the SOP network's own save — polygons
 *    unwelded so vertex-class uv and N survive, point attributes as
 *    lanes, primitive attributes on Mesh::primitives, point and primitive
 *    GROUPS as 0/1 lanes under the group's name (a pop mask, ready
 *    made), and a primitive-less file as a point cloud (parsed by
 *    hand; the binary .bgeo and blosc .sc variants are not read).
 *
 * Import stays ACCESS-agnostic: bytes in, Model out. External
 * references (.mtl libraries, .bin buffers, texture files) are pulled
 * through the caller's Resolver, so the same import runs over a
 * directory (the path overload wires sibling files up automatically),
 * a SigilIO Hub (res:// mounts or http(s):// behind its offline
 * disk cache), or anything else that maps a relative URI to bytes.
 * Textures are NOT decoded here — a Part carries the encoded bytes
 * (or the unresolved URI); what pixels mean stays SigilImage's
 * concern, exactly as with SigilIO.
 */

#include <sigilcore/report/Error.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <string_view>

#include "sigilgeometry/mesh/codec/Model.h"

namespace sigil::geometry::mesh::codec::decode {

/** Alembic import knobs — which moment of the cache to bake. */
struct AlembicOptions {
  double time = 0;  ///< seconds; the NEAREST stored sample is taken
};

/** Import an Alembic (Ogawa) archive from memory: every IPolyMesh
 *  becomes a Part (xforms baked, like glTF), every IPoints a faceless
 *  Part — an honest point cloud like PLY's; arbGeomParams land as
 *  named lanes. Alembic archives are self-contained (no resolver, no
 *  textures). Nearest-sample only, no interpolation. The error says
 *  why on malformed bytes or an HDF5-cored archive. */
std::expected<Model, core::Error> alembic(const void* bytes, size_t size,
                                          const AlembicOptions& options = {});

/** Import from memory. @p pathHint names the source ("duck.glb",
 *  a path, a URL path) — its extension picks the format; without a
 *  useful extension the bytes are sniffed (GLB magic, glTF JSON, Ogawa
 *  magic, a .geo's or a PLY's opening line, binary STL's size
 *  arithmetic), so everything the extension table names except OBJ, which
 *  has no signature. The error names the format the bytes were read as
 *  when they do not read as it, or says no format knows them. */
std::expected<Model, core::Error> model(const void* bytes, size_t size,
                                        std::string_view pathHint,
                                        const Resolver& resolve = {});

/** Import a local file; external references resolve against its
 *  directory (the .gltf + .bin + textures layout just works). */
std::expected<Model, core::Error> model(const std::filesystem::path& file);

}  // namespace sigil::geometry::mesh::codec::decode
