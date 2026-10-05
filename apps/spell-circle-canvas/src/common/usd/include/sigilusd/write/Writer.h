#pragma once

/** @file
 * The Writer: a USD stage built from the values a scene is made of —
 * meshes with their placements and material slots, stamps as point
 * instancers, lights, a camera — and saved as binary crate (`.usdc`, the
 * default), ASCII (`.usda`) or a `.usdz` package, which is an archive of
 * the crate and everything it refers to.
 *
 * Materials go out as UsdPreviewSurface with UsdUVTexture inputs — the
 * metallic-roughness model, slot for slot — and their images are written
 * as PNG files beside the stage, or inside the archive when the stage is
 * a package. A stacked material exports the material
 * at the bottom of the stack; stacking is a live composition, not a thing
 * UsdPreviewSurface can hold, and the depth is noted on the prim as
 * custom metadata rather than baked.
 */

#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilmaterial/core/Material.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/frame/Environment.h>

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
  std::filesystem::path textureDirectory;
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
  /** Opens a stage that will be written to @p file, whose extension
   *  decides the format, under @p options. */
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
  /** Instanced geometry: a UsdGeomPointInstancer with the stamp as its
   *  one prototype, the instances' positions, scales and orientations as
   *  its own, their colours as `displayColor` and `displayOpacity`, and
   *  each named scalar as a per-instance primvar of its name. */
  std::string instances(std::string_view name,
                        const geometry::mesh::Instances& instances,
                        const glm::mat4& model,
                        const material::Material& material,
                        std::string_view parent = "/World");
  /** Stamps: @p stamp standing at every point of @p cloud the point
   *  operators' way — the "size" lane scales, "dir" (or "normal")
   *  orients, "tint" colours — written as `instances` writes it. */
  std::string stamps(std::string_view name, const geometry::mesh::Cloud& cloud,
                     const geometry::mesh::Mesh& stamp, const glm::mat4& model,
                     const material::Material& material,
                     std::string_view parent = "/World");
  /** A light, as a frame keeps it: distant for a directional light,
   *  sphere for a point light or for a spot, whose cone is UsdLux shaping
   *  about the prim's -Z, and an untextured dome for an ambient or a
   *  hemisphere light, the hemisphere's ground colour riding as
   *  `sigil:groundColor` and its up as the dome's +Y. Colours are written
   *  in linear light, as UsdLux states them; a sphere is a point of radius
   *  one whose surface radiance is the light's intensity over pi, which is
   *  what delivers that intensity at a distance of one. The range, which
   *  UsdLux has no word for, rides as `sigil:range`; the light and shadow
   *  links, whose collection terms name collections and keys rather than
   *  this stage's prims, ride as `sigil:lightLink` and `sigil:shadowLink`
   *  custom data. */
  std::string light(std::string_view name, const world::PlacedLight& light,
                    std::string_view parent = "/World");
  /** THE SET'S ENVIRONMENT, as a UsdLuxDomeLight: the panorama written
   *  beside the stage and declared lat-long, the strength on the light's
   *  own attribute, and the environment's orientation — the matrix a
   *  frame carries, taking a world direction into the panorama's frame —
   *  inverted back into the prim's transform.
   *
   *  A panorama holds values above one and no encoder in this tree
   *  writes a floating-point image, so it is divided by its peak,
   *  written as a sixteen-bit PNG, and the peak multiplied into the
   *  light's intensity: the ratios survive, the brightness is right, and
   *  it is right through the standard attribute. The dials UsdLux has no
   *  word for ride as `sigil:` custom data. */
  std::string environmentMap(std::string_view name,
                             const world::Environment& environment,
                             std::string_view parent = "/World");
  /** The camera, as UsdGeomCamera: camera-to-world, the focal length
   *  that gives its vertical field of view against a 24 mm vertical
   *  aperture, the clipping range, and the distance to what it looks at
   *  as the focus distance. */
  std::string camera(std::string_view name,
                     const geometry::mesh::camera::Camera& camera,
                     std::string_view parent = "/World");

  /** Write the stage in the form the path's extension asks for; false
   *  (with @p error) when USD refuses.
   *
   *  `.usdc`, `.usd` and `.usda` are LAYERS, exported onto the path.
   *  `.usdz` is a PACKAGE: a zip archive of a layer and every file that
   *  layer refers to. A root layer cannot be exported onto one, so the
   *  crate is written beside where the package will stand — where the
   *  images this writer already wrote are, and where the stage's
   *  relative asset paths therefore resolve — packaged, and then the
   *  staged crate and the images it took copies of are deleted. What is
   *  left is the one file. */
  bool save(std::string* error = nullptr);

  struct Impl;

 private:
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::usd
