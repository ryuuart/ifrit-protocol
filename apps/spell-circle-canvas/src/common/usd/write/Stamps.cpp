/** @file
 * Instanced geometry as a UsdGeomPointInstancer with the stamp as its one
 * prototype: positions, scales and orientations, the instances' colours
 * as displayColor and displayOpacity, and each named scalar as a
 * per-instance primvar. A cloud of stamps is instanced the point
 * operators' way and written the same.
 */

#include <pxr/base/gf/quath.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>

#include "GfMatrix.h"
#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

std::string Writer::instances(std::string_view name,
                              const geometry::mesh::Instances& instances,
                              const glm::mat4& model,
                              const material::Material& material,
                              std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  UsdGeomPointInstancer instancer =
      UsdGeomPointInstancer::Define(impl.stage, SdfPath(path));
  instancer.AddTransformOp().Set(toGf(model));
  // The one prototype, under the instancer.
  const std::string protoPath = path + "/Prototypes/stamp";
  UsdGeomScope::Define(impl.stage, SdfPath(path + "/Prototypes"));
  UsdGeomMesh proto = UsdGeomMesh::Define(impl.stage, SdfPath(protoPath));
  impl.fillMesh(proto, instances.stamp);
  impl.bind(proto, instances.stamp, {material}, name);
  instancer.CreatePrototypesRel().AddTarget(SdfPath(protoPath));

  const size_t n = instances.size();
  VtVec3fArray positions;
  VtIntArray protoIndices((int)n, 0);
  VtVec3fArray scales;
  VtQuathArray orientations;
  for (size_t i = 0; i < n; ++i) {
    const glm::vec3& p = instances.positions[i];
    positions.push_back({p.x, p.y, p.z});
    const glm::vec3 s =
        i < instances.scales.size() ? instances.scales[i] : glm::vec3(1.0f);
    scales.push_back({s.x, s.y, s.z});
    if (i < instances.orientations.size()) {
      const glm::quat& q = instances.orientations[i];
      orientations.push_back(
          GfQuath((GfHalf)q.w, (GfHalf)q.x, (GfHalf)q.y, (GfHalf)q.z));
    }
  }
  instancer.CreatePositionsAttr().Set(positions);
  instancer.CreateProtoIndicesAttr().Set(protoIndices);
  instancer.CreateScalesAttr().Set(scales);
  if (n > 0 && orientations.size() == n)
    instancer.CreateOrientationsAttr().Set(orientations);
  UsdGeomPrimvarsAPI primvars(instancer);
  if (n > 0 && instances.colors.size() == n) {
    VtVec3fArray colors;
    VtFloatArray alphas;
    for (const glm::vec4& c : instances.colors) {
      colors.push_back({c.r, c.g, c.b});
      alphas.push_back(c.a);
    }
    primvars
        .CreatePrimvar(TfToken("displayColor"), SdfValueTypeNames->Color3fArray,
                       UsdGeomTokens->vertex)
        .Set(colors);
    primvars
        .CreatePrimvar(TfToken("displayOpacity"), SdfValueTypeNames->FloatArray,
                       UsdGeomTokens->vertex)
        .Set(alphas);
  }
  for (const auto& [attribute, values] : instances.attributes) {
    if (values.size() != n || !TfIsValidIdentifier(attribute)) continue;
    primvars
        .CreatePrimvar(TfToken(attribute), SdfValueTypeNames->FloatArray,
                       UsdGeomTokens->vertex)
        .Set(VtFloatArray(values.begin(), values.end()));
  }
  return path;
}

std::string Writer::stamps(std::string_view name,
                           const geometry::mesh::Cloud& cloud,
                           const geometry::mesh::Mesh& stamp,
                           const glm::mat4& model,
                           const material::Material& material,
                           std::string_view parent) {
  return instances(
      name,
      geometry::mesh::points::instances(
          cloud, stamp, geometry::mesh::points::stampOptions(cloud)),
      model, material, parent);
}

}  // namespace sigil::usd
