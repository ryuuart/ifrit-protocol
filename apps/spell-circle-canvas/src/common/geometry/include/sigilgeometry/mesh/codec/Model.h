#pragma once

/** @file
 * SigilGeometry's imported-model types — what every reader produces and
 * every consumer holds: a Part (one draw unit: a mesh in model space,
 * its material factors and texture references, its custom attributes as
 * named lanes), a Model (the parts, and the counts, bounds, merge and
 * fit that only make sense across all of them), and the Resolver a
 * reader consults for a file's external references.
 *
 * ATTRIBUTES flow through: glTF's custom _NAME accessors (what Blender
 * and Houdini exporters write), PLY's extra properties and .geo's
 * point class land on the Part as named lanes, and asCloud() pours a
 * part into a geometry::mesh::Cloud — scatter in Houdini, cook in pops, stamp
 * with points:: here.
 */

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/pop/Points.h"

namespace sigil::geometry::mesh::codec::decode {

/** Maps a relative URI ("scene.bin", "textures/wood.png", "duck.mtl")
 *  to its bytes; nullopt when unavailable. Only consulted for formats
 *  with external references. */
using Resolver =
    std::function<std::optional<std::vector<std::byte>>(std::string_view)>;

/** One draw unit of an imported model: a node/shape/material group. */
struct Part {
  std::string name;  ///< node, object or solid name ("" when unnamed)
  Mesh mesh;         ///< model space — node transforms already baked
  /** Material base/diffuse factor; white when the file names none. */
  glm::vec4 baseColor = {1, 1, 1, 1};
  /** Base-color texture reference as the file spells it ("" = none). */
  std::string textureUri;
  /** The texture's ENCODED bytes when reachable — embedded in the file
   *  or pulled through the resolver. Decode via SigilImage. */
  std::vector<std::byte> textureBytes;

  /** The rest of a metallic-roughness material, as glTF carries it:
   *  the scalar factors, and the other maps keyed by USAGE word —
   *  "normal" (OpenGL convention, green up the image), "orm" (glTF's
   *  metallicRoughness image: roughness in G, metallic in B, and by
   *  convention occlusion in R), "occlusion" (its own image, or the
   *  same bytes as "orm" when the file packs them), "emissive". Each
   *  entry holds the encoded bytes when reachable and the URI as the
   *  file spells it. Formats without a material model leave this
   *  empty. These are the words SigilWorld's texture-set door reads. */
  struct TextureRef {
    std::string uri;
    std::vector<std::byte> bytes;
  };
  std::map<std::string, TextureRef> textures;
  float metallic = 1;   ///< glTF's factor default; multiplies the map
  float roughness = 1;  ///< likewise
  glm::vec4 emissive = {0, 0, 0, 1};
  /** glTF's transmission and index of refraction extensions (0 and 1.5
   *  when absent), and its alpha mode: `alphaCutoff` above 0 is MASK
   *  (cut out below it), 0 with a base alpha or texture alpha is
   *  BLEND, and `opaque` says the file declared OPAQUE regardless. */
  float transmission = 0;
  float ior = 1.5f;
  float alphaCutoff = 0;
  bool opaque = true;
  /** The file's material SLOT this part wears (glTF: the material's
   *  index in the file; -1 when the part names none). The same number
   *  is written across the part's `mesh.prims["Material"]` lane, so a
   *  merged model keeps per-triangle slots. Houdini's `.geo` writes the
   *  lane from `shop_materialpath` (its string table's index) and
   *  leaves this -1. */
  int materialIndex = -1;

  /** Custom per-vertex attributes — the Houdini/Blender lanes glTF
   *  spells as _NAME accessors and PLY as extra properties. Names
   *  arrive verbatim (glTF's leading underscore stripped). Routing by
   *  width: 1 -> scalars, 3 -> vectors, 2 and 4 -> colors (vec4,
   *  zero-padded) — the same lane shapes Cloud speaks.
   *
   *  These three are the POINT class, one value per VERTEX. Per-face
   *  attributes are a different cardinality and live in a different
   *  container: `mesh.prims` (see below). */
  std::map<std::string, std::vector<float>, std::less<>> scalarLanes;
  std::map<std::string, std::vector<glm::vec3>, std::less<>> vectorLanes;
  std::map<std::string, std::vector<glm::vec4>, std::less<>> colorLanes;

  /** Per-PRIMITIVE attributes need no member of their own: they land
   *  in `mesh.prims`, the Mesh currency's primitive-lane container,
   *  which is triangleCount()-sized by definition — so a per-face lane
   *  can never be read as a per-vertex one, and Model::merged()
   *  carries them through Mesh::append for free. PLY face properties
   *  are the source (the read leg of encode::ply's per-face write):
   *  `name_r/_g/_b/_a` folds to the vec4 lane `name` (alpha defaults
   *  to 1), `name_x/_y/_z` to `name` with w = 0, conventional
   *  `red/green/blue/alpha` to "Color", and any lone scalar to `.x`
   *  (the "Id" convention). A polygon that fan-triangulates replicates
   *  its value across every triangle it produced. asCloud() is
   *  point-class and does NOT carry them. */

  /** This part's vertices as a Cloud: positions, the conventional
   *  lanes ("t" by index, "normal", "uv", "tint" when colors exist),
   *  and EVERY custom lane — the door from imported attributes into
   *  points::instance/panels/drawBillboards and pop seeding. */
  Cloud asCloud() const;
};

/** A whole imported file: its parts, and the operations that only make
 *  sense across all of them at once — the combined counts and bounds,
 *  one merged mesh, and the transform that fits the model to a size
 *  the rest of a scene can work with. */
struct Model {
  std::vector<Part> parts;

  size_t vertexCount() const;
  size_t triangleCount() const;
  void bounds(glm::vec3* lo, glm::vec3* hi) const;

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

  /** How many material slots the parts name: max `materialIndex` + 1
   *  (0 when none does). `merged()` keeps the "Material" lane, so a
   *  merged mesh placed with that many slots wears them per face. */
  int materialSlotCount() const;
};

}  // namespace sigil::geometry::mesh::codec::decode
