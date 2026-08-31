/** @file
 * readModel(): opens the stage, walks every prim, and hands meshes and
 * point instancers to their readers; the material slot table the walk
 * accumulates comes back as ReadInfo. The stage-opening every read door
 * in this feature shares lives here too.
 */

#include "sigilusd/read/Reader.h"

#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/pointInstancer.h>

#include "ReadContext.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

int ReadContext::slot(const UsdShadeMaterial& material) {
  if (!material) return -1;
  const std::string name = material.GetPath().GetString();
  for (size_t i = 0; i < materialNames.size(); ++i)
    if (materialNames[i] == name) return (int)i;
  materialNames.push_back(name);
  return (int)materialNames.size() - 1;
}

std::optional<geometry::mesh::codec::decode::Model> readModel(
    const std::filesystem::path& file, std::string* error) {
  return readModel(file, nullptr, error);
}

UsdStageRefPtr openStage(const std::filesystem::path& file,
                         std::string* error) {
  UsdStageRefPtr stage = UsdStage::Open(file.string());
  if (!stage && error) *error = "cannot open " + file.string();
  return stage;
}

std::optional<geometry::mesh::codec::decode::Model> readModel(
    const std::filesystem::path& file, ReadInfo* info, std::string* error) {
  UsdStageRefPtr stage = openStage(file, error);
  if (!stage) return std::nullopt;
  ReadContext context;
  context.stageDir = file.parent_path();
  geometry::mesh::codec::decode::Model model;
  for (const UsdPrim& prim : stage->Traverse()) {
    if (prim.IsA<UsdGeomMesh>())
      readMesh(prim, context, model);
    else if (prim.IsA<UsdGeomPointInstancer>())
      readInstancer(prim, context, model);
  }
  if (info) info->materialNames = std::move(context.materialNames);
  return model;
}

}  // namespace sigil::usd
