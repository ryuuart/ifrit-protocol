#pragma once

/** @file
 * SigilUsd — the world's data in and out of USD.
 *
 * Two doors over OpenUSD, both value-driven:
 *  - a Writer that builds a stage from the values a scene is made of —
 *    meshes with their placements and material slots, stamps as point
 *    instancers, lights, a camera — and saves it as binary crate
 *    (`.usdc`, the default), ASCII (`.usda`) or a `.usdz` package;
 *  - a reader that pours a USD stage's meshes, point instancers and
 *    materials into shape::import::Model, the same currency every other
 *    format lands in.
 *
 * Materials go out as UsdPreviewSurface with UsdUVTexture inputs — the
 * shading model is the one this renderer shades with, slot for slot —
 * and their images are written as PNG files beside the stage. A layered
 * material exports its base; layers are this renderer's live
 * composition, not a thing UsdPreviewSurface can hold, and are noted on
 * the prim as custom metadata rather than baked.
 *
 * Namespace sigil::usd, target SigilUsd. Links SigilWorld (Material,
 * Lighting) and SigilShape (Mesh, Cloud, import::Model) publicly and
 * OpenUSD privately; SigilWorld and SigilShape do not know it exists.
 */

#include <sigilshape/Import.h>
#include <sigilshape/Mesh.h>
#include <sigilshape/Points.h>
#include <sigilworld/Components.h>
#include <sigilworld/World.h>

#include <filesystem>
#include <memory>
#include <optional>
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
  std::string mesh(std::string_view name, const shape::Mesh& mesh,
                   const glm::mat4& model,
                   const std::vector<world::Material>& slots,
                   std::string_view parent = "/World");
  std::string mesh(std::string_view name, const shape::Mesh& mesh,
                   const glm::mat4& model, const world::Material& material,
                   std::string_view parent = "/World") {
    return this->mesh(name, mesh, model, std::vector<world::Material>{material},
                      parent);
  }
  /** Stamps: a UsdGeomPointInstancer over @p cloud's positions with the
   *  stamp mesh as its one prototype; the "size" lane scales, "dir" (or
   *  "normal") orients, "tint" lands as a per-instance primvar. */
  std::string stamps(std::string_view name, const shape::Cloud& cloud,
                     const shape::Mesh& stamp, const glm::mat4& model,
                     const world::Material& material,
                     std::string_view parent = "/World");
  /** A light: distant (the sun) or sphere (a point light). */
  std::string light(std::string_view name, const world::LightComponent& light,
                    std::string_view parent = "/World");
  std::string sun(std::string_view name, const world::Lighting& lighting,
                  std::string_view parent = "/World");
  /** The camera, as UsdGeomCamera. */
  std::string camera(std::string_view name, const shape::space::Camera& camera,
                     std::string_view parent = "/World");

  /** Write the stage; false (with @p error) when USD refuses. */
  bool save(std::string* error = nullptr);

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

/** Read a USD file (.usd, .usda, .usdc, .usdz) into a Model: every
 *  UsdGeomMesh becomes a Part (xforms baked, faces triangulated, `st`
 *  as uvs, `displayColor` as colours, primvars as lanes, GeomSubsets as
 *  the "Material" lane with materialIndex), every point instancer's
 *  positions a faceless Part; a bound UsdPreviewSurface fills the
 *  Part's factors and texture references (bytes read from the file's
 *  neighbours). nullopt when the stage cannot be opened. */
std::optional<shape::import::Model> readModel(const std::filesystem::path& file,
                                              std::string* error = nullptr);

/** The bound UsdPreviewSurface material names, in slot order, that
 *  readModel() found — parallel to Part::materialIndex. */
struct ReadInfo {
  std::vector<std::string> materialNames;
};
std::optional<shape::import::Model> readModel(const std::filesystem::path& file,
                                              ReadInfo* info,
                                              std::string* error = nullptr);

}  // namespace sigil::usd
