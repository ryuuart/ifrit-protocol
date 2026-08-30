#pragma once

/** @file
 * Writer::Impl — the stage under construction, the paths already taken,
 * the materials already authored and the images already written. Each
 * kind of prim is authored by its own translation unit through the
 * methods declared here. Internal to the write feature: it names USD
 * types, and USD is private to the library.
 */

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilusd/write/Writer.h"

namespace sigil::usd {

/** A valid USD identifier from any name: every character that is not
 *  alphanumeric or an underscore becomes an underscore, and a name that
 *  is empty or starts with a digit gains a leading underscore. */
std::string identifier(std::string_view name);

struct Writer::Impl {
  std::filesystem::path file;
  WriteOptions options;
  pxr::UsdStageRefPtr stage;
  std::set<std::string> usedPaths;
  /** Materials already authored, by pointer identity of their images and
   *  value of their scalars — the same material placed twice binds one
   *  prim. */
  std::vector<std::pair<world::Material, pxr::SdfPath>> materials;
  int textureCounter = 0;
  bool texturesDirReady = false;
  /** One file per image, however many materials share it. */
  std::map<const SkImage*, std::string> writtenImages;

  /** "<parent>/<identifier(name)>", suffixed "_2", "_3", ... until it is
   *  one the stage has not used. */
  std::string uniquePath(std::string_view parent, std::string_view name);

  /** The texture directory relative to the stage file. */
  std::filesystem::path textureDir() const;

  /** Writes @p image as a PNG beside the stage (once per image, however
   *  many materials share it) and returns its stage-relative asset path;
   *  nullopt for a null image or a failed encode. */
  std::optional<std::string> textureAsset(const sk_sp<SkImage>& image,
                                          const char* role);

  /** The material as UsdPreviewSurface, authored once per distinct
   *  material under /World/Materials. Only the base of a layered
   *  material is expressible; the layer count rides as custom data. */
  pxr::SdfPath material(const world::Material& m, std::string_view hint);

  /** The mesh's lanes onto a UsdGeomMesh (positions, normals, st,
   *  displayColor, prim lanes as uniform primvars). */
  void fillMesh(pxr::UsdGeomMesh& usdMesh, const geometry::Mesh& mesh);

  /** Bind @p slots: one material over the whole mesh, or GeomSubsets by
   *  the "Material" lane. */
  void bind(pxr::UsdGeomMesh& usdMesh, const geometry::Mesh& mesh,
            const std::vector<world::Material>& slots, std::string_view hint);
};

}  // namespace sigil::usd
