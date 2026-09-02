#pragma once

/** @file
 * The Writer: a USD stage built from the values a scene is made of —
 * meshes with their placements and material slots, stamps as point
 * instancers, lights, a camera — and saved as binary crate (`.usdc`, the
 * default), ASCII (`.usda`) or a `.usdz` package.
 *
 * Materials go out as UsdPreviewSurface with UsdUVTexture inputs — the
 * metallic-roughness model, slot for slot — and their images are written
 * as PNG files beside the stage. A stacked material exports the material
 * at the bottom of the stack; stacking is a live composition, not a thing
 * UsdPreviewSurface can hold, and the depth is noted on the prim as
 * custom metadata rather than baked.
 */

#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilmaterial/core/Material.h>
#include <sigilworld/element/Environment.h>
#include <sigilworld/light/Light.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::usd {

/** Stage-wide settings for a `Writer`: where its textures land beside
 *  the stage file, and the unit scale recorded so a consumer reads the
 *  geometry at the size it was authored. */
struct WriteOptions {
  /** Where the materials' images are written, relative to the stage
   *  file; empty = "<stem>_textures". PNG. */
  std::filesystem::path textureDir;
  /** USD's stage metadata. Meshes here are authored in whatever units
   *  the caller used; metersPerUnit tells a consumer how to read them
   *  (0.01 = centimetres, the DCC default). */
  double metersPerUnit = 0.01;
};

/** Builds one stage. Every call adds a prim under /World (or under a
 *  parent path you name); names are sanitized to valid identifiers and
 *  made unique. save() writes the file the path's extension asks for. */
class Writer {
 public:
  explicit Writer(const std::filesystem::path& file, WriteOptions options = {});
  ~Writer();
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  /** A mesh as a UsdGeomMesh: points, normals, uv (`st`), the colour
   *  lane as `displayColor`, every prim lane as a uniform primvar, and
   *  the "Material" lane as GeomSubsets bound to @p slots (one material
   *  = the whole mesh bound). Returns the prim path. */
  std::string mesh(std::string_view name, const geometry::mesh::Mesh& mesh,
                   const glm::mat4& model,
                   const std::vector<material::Material>& slots,
                   std::string_view parent = "/World");
  /** The one-material spelling: no GeomSubsets, the binding covers the
   *  whole mesh. */
  std::string mesh(std::string_view name, const geometry::mesh::Mesh& mesh,
                   const glm::mat4& model, const material::Material& material,
                   std::string_view parent = "/World") {
    return this->mesh(name, mesh, model,
                      std::vector<material::Material>{material}, parent);
  }
  /** Stamps: a UsdGeomPointInstancer over @p cloud's positions with the
   *  stamp mesh as its one prototype; the "size" lane scales, "dir" (or
   *  "normal") orients, "tint" lands as a per-instance primvar. */
  std::string stamps(std::string_view name, const geometry::mesh::Cloud& cloud,
                     const geometry::mesh::Mesh& stamp, const glm::mat4& model,
                     const material::Material& material,
                     std::string_view parent = "/World");
  /** An emitter: distant for a sun, sphere for a point light or for a
   *  spot, whose cone is UsdLux shaping about the prim's -Z. The range,
   *  which UsdLux has no word for, rides as `sigil:range`. */
  std::string light(std::string_view name, const world::light::Light& light,
                    std::string_view parent = "/World");
  /** THE SET'S ENVIRONMENT MAP, as a UsdLuxDomeLight: the panorama
   *  written beside the stage and declared lat-long, the strength and
   *  the tint on the light's own attributes, and @p orientation — the
   *  matrix a frame carries, taking a world direction into the
   *  panorama's frame — inverted back into the prim's transform.
   *
   *  A panorama holds values above one and no encoder in this tree
   *  writes a floating-point image, so it is divided by its peak,
   *  written as a sixteen-bit PNG, and the peak multiplied into the
   *  light's intensity: the ratios survive, the brightness is right, and
   *  it is right through the standard attribute. The dials UsdLux has no
   *  word for ride as `sigil:` custom data. */
  std::string environmentMap(std::string_view name,
                             const world::Environment& environment,
                             const glm::mat3& orientation = glm::mat3(1.0f),
                             std::string_view parent = "/World");
  /** The camera, as UsdGeomCamera: camera-to-world, the focal length
   *  that gives its vertical field of view against a 24 mm vertical
   *  aperture, the clipping range, and the distance to what it looks at
   *  as the focus distance. */
  std::string camera(std::string_view name,
                     const geometry::mesh::camera::Camera& camera,
                     std::string_view parent = "/World");

  /** Write the stage; false (with @p error) when USD refuses. */
  bool save(std::string* error = nullptr);

  struct Impl;

 private:
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::usd
