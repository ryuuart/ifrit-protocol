/** @file
 * readModel(): opens the stage, walks every prim, and hands meshes and
 * point instancers to their readers; the material slot table the walk
 * accumulates comes back as ReadInfo. The stage-opening every read door
 * in this feature shares lives here too.
 */

#include "sigilusd/read/Reader.h"

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/usd/prim.h>
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
  // A body's shadow flags ride as custom data on its prim, one value for
  // every part the prim became.
  const auto stated = [](const UsdPrim& prim, const char* key) {
    const VtValue value = prim.GetCustomDataByKey(TfToken(key));
    return !value.IsHolding<bool>() || value.Get<bool>();
  };
  for (const UsdPrim& prim : stage->Traverse()) {
    const size_t before = model.parts.size();
    if (prim.IsA<UsdGeomMesh>())
      readMesh(prim, context, model);
    else if (prim.IsA<UsdGeomPointInstancer>())
      readInstancer(prim, context, model);
    if (!info) continue;
    for (size_t added = before; added < model.parts.size(); ++added) {
      info->castShadow.push_back(stated(prim, "sigil:castShadow"));
      info->receiveShadow.push_back(stated(prim, "sigil:receiveShadow"));
    }
  }
  if (info) info->materialNames = std::move(context.materialNames);
  return model;
}

}  // namespace sigil::usd
