#pragma once

/** @file
 * SigilShape model import — files into the Mesh currency.
 *
 * Five formats cover the practical interchange world:
 *  - Wavefront OBJ (+ MTL): the classic text workhorse (tinyobjloader);
 *  - glTF 2.0, .gltf and .glb: the modern standard (cgltf) — node
 *    transforms baked, base-color material honored, buffers reachable
 *    whether embedded (GLB chunk, data: URIs) or external files;
 *  - STL, ascii and binary: the 3D-print staple;
 *  - PLY, ascii and binary little-endian: THE attribute carrier —
 *    every non-conventional vertex property becomes a named lane, and
 *    faceless files are honest point clouds;
 *  - Alembic, .abc Ogawa: the vfx cache — meshes and point clouds at
 *    a chosen time, arbGeomParams as lanes (Alembic library).
 *
 * ATTRIBUTES flow through: glTF's custom _NAME accessors (what Blender
 * and Houdini exporters write) and PLY's extra properties land on the
 * Part as named lanes, and asCloud() pours a part into a shape::Cloud
 * — scatter in Houdini, cook in pops, stamp with points:: here.
 *
 * Import stays ACCESS-agnostic: bytes in, Model out. External
 * references (.mtl libraries, .bin buffers, texture files) are pulled
 * through the caller's Resolver, so the same import runs over a
 * directory (the path overload wires sibling files up automatically),
 * a SigilLoader Hub (res:// mounts or http(s):// behind its offline
 * disk cache), or anything else that maps a relative URI to bytes.
 * Textures are NOT decoded here — a Part carries the encoded bytes
 * (or the unresolved URI); what pixels mean stays SigilImage's
 * concern, exactly as with the loader.
 */

#include "sigilshape/Mesh.h"
#include "sigilshape/Points.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::shape::import {

/** Maps a relative URI ("scene.bin", "textures/wood.png", "duck.mtl")
 *  to its bytes; nullopt when unavailable. Only consulted for formats
 *  with external references. */
using Resolver =
    std::function<std::optional<std::vector<std::byte>>(std::string_view)>;

/** One draw unit of an imported model: a node/shape/material group. */
struct Part {
  std::string name;      ///< node, object or solid name ("" when unnamed)
  Mesh mesh;             ///< model space — node transforms already baked
  /** Material base/diffuse factor; white when the file names none. */
  glm::vec4 baseColor = {1, 1, 1, 1};
  /** Base-color texture reference as the file spells it ("" = none). */
  std::string textureUri;
  /** The texture's ENCODED bytes when reachable — embedded in the file
   *  or pulled through the resolver. Decode via SigilImage. */
  std::vector<std::byte> textureBytes;

  /** Custom per-vertex attributes — the Houdini/Blender lanes glTF
   *  spells as _NAME accessors and PLY as extra properties. Names
   *  arrive verbatim (glTF's leading underscore stripped). Routing by
   *  width: 1 -> scalars, 3 -> vectors, 2 and 4 -> colors (vec4,
   *  zero-padded) — the same lane shapes Cloud speaks. */
  std::map<std::string, std::vector<float>, std::less<>> scalarLanes;
  std::map<std::string, std::vector<glm::vec3>, std::less<>> vectorLanes;
  std::map<std::string, std::vector<glm::vec4>, std::less<>> colorLanes;

  /** This part's vertices as a Cloud: positions, the conventional
   *  lanes ("t" by index, "normal", "uv", "tint" when colors exist),
   *  and EVERY custom lane — the door from imported attributes into
   *  points::instance/panels/drawBillboards and pop seeding. */
  Cloud asCloud() const;
};

struct Model {
  std::vector<Part> parts;

  size_t vertexCount() const;
  size_t triangleCount() const;
  void bounds(glm::vec3 *lo, glm::vec3 *hi) const;

  /** Everything as one mesh: parts appended, any non-white baseColor
   *  baked into the per-vertex color lane (Space's Lit mode and
   *  SigilWorld both multiply it). */
  Mesh merged() const;

  /** The transform that centers the model on the origin and uniformly
   *  scales its largest extent to @p size — models arrive in wildly
   *  different units, this is the "put it on the table" helper. */
  glm::mat4 fitTransform(float size) const;

  /** Every part's asCloud() appended into one Cloud (shared lanes
   *  concatenate, missing ones pad with defaults). */
  Cloud mergedCloud() const;
};

/** Alembic import knobs — which moment of the cache to bake. */
struct AlembicOptions {
  double time = 0; ///< seconds; the NEAREST stored sample is taken
};

/** Import an Alembic (Ogawa) archive from memory: every IPolyMesh
 *  becomes a Part (xforms baked, like glTF), every IPoints a faceless
 *  Part — an honest point cloud like PLY's; arbGeomParams land as
 *  named lanes. Alembic archives are self-contained (no resolver, no
 *  textures). Nearest-sample only, no interpolation. nullopt on
 *  malformed bytes or HDF5-cored archives. */
std::optional<Model> alembic(const void *bytes, size_t size,
                             const AlembicOptions &options = {});

/** Import from memory. @p pathHint names the source ("duck.glb",
 *  a path, a URL path) — its extension picks the format; without a
 *  useful extension the bytes are sniffed (GLB magic, glTF JSON,
 *  Ogawa magic, binary STL's size arithmetic). Returns nullopt on
 *  unknown format or malformed content. */
std::optional<Model> model(const void *bytes, size_t size,
                           std::string_view pathHint,
                           const Resolver &resolve = {});

/** Import a local file; external references resolve against its
 *  directory (the .gltf + .bin + textures layout just works). */
std::optional<Model> model(const std::filesystem::path &file);

} // namespace sigil::shape::import
