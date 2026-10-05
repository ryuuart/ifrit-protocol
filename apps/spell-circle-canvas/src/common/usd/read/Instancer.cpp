/** @file
 * A UsdGeomPointInstancer read two ways: as a faceless Part — its
 * positions with the local-to-world xform baked, and its scales' x as the
 * "size" lane — and as instanced geometry, its first prototype the stamp.
 */

#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <glm/gtc/quaternion.hpp>

#include "GfMatrix.h"
#include "ReadContext.h"
#include "sigilusd/read/Reader.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

void readInstancer(const UsdPrim& prim, ReadContext& context,
                   geometry::mesh::codec::decode::Model& model) {
  UsdGeomPointInstancer instancer(prim);
  VtVec3fArray positions;
  instancer.GetPositionsAttr().Get(&positions);
  if (positions.empty()) return;
  geometry::mesh::codec::decode::Part part;
  part.name = prim.GetName().GetString();
  for (const GfVec3f& p : positions)
    part.mesh.positions.emplace_back(p[0], p[1], p[2]);
  VtVec3fArray scales;
  if (instancer.GetScalesAttr().Get(&scales) &&
      scales.size() == positions.size()) {
    std::vector<float>& size = part.scalarLanes["size"];
    for (const GfVec3f& s : scales) size.push_back(s[0]);
  }
  const glm::mat4 world = fromGf(context.xforms.GetLocalToWorldTransform(prim));
  part.mesh.transform(world);
  model.parts.push_back(std::move(part));
}

namespace {

/** @p prim as instanced geometry in its own space; nullopt when it has no
 *  positions. */
std::optional<ReadInstances> instancesOf(const UsdPrim& prim,
                                         ReadContext& context) {
  UsdGeomPointInstancer instancer(prim);
  VtVec3fArray positions;
  instancer.GetPositionsAttr().Get(&positions);
  ReadInstances out;
  out.path = prim.GetPath().GetString();
  out.placement = fromGf(context.xforms.GetLocalToWorldTransform(prim));
  geometry::mesh::Instances& instances = out.instances;
  const size_t n = positions.size();
  for (const GfVec3f& p : positions)
    instances.positions.emplace_back(p[0], p[1], p[2]);
  VtVec3fArray scales;
  if (instancer.GetScalesAttr().Get(&scales) && scales.size() == n)
    for (const GfVec3f& s : scales)
      instances.scales.emplace_back(s[0], s[1], s[2]);
  VtQuathArray orientations;
  if (instancer.GetOrientationsAttr().Get(&orientations) &&
      orientations.size() == n)
    for (const GfQuath& q : orientations)
      instances.orientations.emplace_back(
          (float)q.GetReal(), (float)q.GetImaginary()[0],
          (float)q.GetImaginary()[1], (float)q.GetImaginary()[2]);

  UsdGeomPrimvarsAPI primvars(instancer);
  VtVec3fArray colors;
  VtFloatArray opacities;
  if (UsdGeomPrimvar color = primvars.GetPrimvar(TfToken("displayColor")))
    color.Get(&colors);
  if (UsdGeomPrimvar opacity = primvars.GetPrimvar(TfToken("displayOpacity")))
    opacity.Get(&opacities);
  if (colors.size() == n && n > 0)
    for (size_t i = 0; i < n; ++i)
      instances.colors.emplace_back(
          colors[i][0], colors[i][1], colors[i][2],
          opacities.size() == n ? opacities[i] : 1.0f);
  for (const UsdGeomPrimvar& primvar : primvars.GetPrimvarsWithValues()) {
    const std::string name = primvar.GetPrimvarName().GetString();
    if (name == "displayColor" || name == "displayOpacity") continue;
    VtFloatArray values;
    if (!primvar.Get(&values) || values.size() != n || n == 0) continue;
    instances.attributes[name].assign(values.begin(), values.end());
  }

  // THE STAMP: the first prototype, read as any mesh is and carried back
  // out of the world into the instancer's own space.
  SdfPathVector prototypes;
  instancer.GetPrototypesRel().GetTargets(&prototypes);
  if (!prototypes.empty()) {
    const UsdPrim proto = prim.GetStage()->GetPrimAtPath(prototypes.front());
    geometry::mesh::codec::decode::Model model;
    if (proto) readMesh(proto, context, model);
    if (!model.parts.empty()) {
      instances.stamp = std::move(model.parts.front().mesh);
      instances.stamp.transform(glm::inverse(out.placement));
    }
  }
  return out;
}

}  // namespace

std::optional<std::vector<ReadInstances>> readInstances(
    const std::filesystem::path& file, std::string* error) {
  UsdStageRefPtr stage = openStage(file, error);
  if (!stage) return std::nullopt;
  ReadContext context;
  context.stageDir = file.parent_path();
  std::vector<ReadInstances> found;
  for (const UsdPrim& prim : stage->Traverse())
    if (prim.IsA<UsdGeomPointInstancer>())
      if (std::optional<ReadInstances> read = instancesOf(prim, context))
        found.push_back(std::move(*read));
  return found;
}

}  // namespace sigil::usd
