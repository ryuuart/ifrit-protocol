#pragma once

/** @file
 * What one readModel() call carries from prim to prim — the stage's
 * directory for resolving texture files, the material slot table, the
 * xform cache — and the per-prim readers each translation unit
 * provides. Internal to the read feature: it names USD types, and USD
 * is private to the library.
 */

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdShade/material.h>
#include <sigilgeometry/codec/Model.h>

#include <filesystem>
#include <string>
#include <vector>

namespace sigil::usd {

struct ReadContext {
  std::filesystem::path stageDir;
  /** Bound material prim paths in the order first met; a Part's
   *  materialIndex indexes this. */
  std::vector<std::string> materialNames;
  pxr::UsdGeomXformCache xforms;

  /** The slot of @p material, appending it when it is new; -1 for none. */
  int slot(const pxr::UsdShadeMaterial& material);
};

/** A UsdGeomMesh prim as a Part appended to @p model: unwelded per
 *  face-vertex, fan-triangulated, xform baked, subsets as the "Material"
 *  lane. Skipped when it has no points or no faces. */
void readMesh(const pxr::UsdPrim& prim, ReadContext& context,
              geometry::decode::Model& model);

/** A UsdGeomPointInstancer prim as a faceless Part appended to @p model:
 *  its positions with the xform baked and its scales as the "size" lane.
 *  Skipped when it has no positions. */
void readInstancer(const pxr::UsdPrim& prim, ReadContext& context,
                   geometry::decode::Model& model);

/** A UsdPreviewSurface (the shader a material's surface output connects
 *  to) into a Part's material fields; texture bytes are read from files
 *  resolved against @p stageDir. */
void readMaterial(const pxr::UsdShadeMaterial& material,
                  const std::filesystem::path& stageDir,
                  geometry::decode::Part& part);

}  // namespace sigil::usd
