#pragma once

/** @file
 * The reader: a USD stage's meshes, point instancers and materials
 * poured into geometry::decode::Model, the same currency every other
 * format lands in.
 */

#include <sigilgeometry/codec/Model.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sigil::usd {

/** The bound UsdPreviewSurface material names, in slot order, that
 *  readModel() found — parallel to Part::materialIndex. */
struct ReadInfo {
  std::vector<std::string> materialNames;
};

/** Read a USD file (.usd, .usda, .usdc, .usdz) into a Model: every
 *  UsdGeomMesh becomes a Part (xforms baked, faces triangulated, `st`
 *  as uvs, `displayColor` as colours, primvars as lanes, GeomSubsets as
 *  the "Material" lane with materialIndex), every point instancer's
 *  positions a faceless Part; a bound UsdPreviewSurface fills the
 *  Part's factors and texture references (bytes read from the file's
 *  neighbours). nullopt when the stage cannot be opened. */
std::optional<geometry::decode::Model> readModel(
    const std::filesystem::path& file, std::string* error = nullptr);
std::optional<geometry::decode::Model> readModel(
    const std::filesystem::path& file, ReadInfo* info,
    std::string* error = nullptr);

}  // namespace sigil::usd
