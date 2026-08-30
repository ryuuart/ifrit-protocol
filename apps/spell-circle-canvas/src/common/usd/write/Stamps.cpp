/** @file
 * A Cloud of stamps as a UsdGeomPointInstancer with the stamp mesh as
 * its one prototype: positions from the cloud, the "size" lane as
 * scales, "dir" (or "normal") as orientations, "tint" as displayColor.
 */

#include <pxr/base/gf/quatd.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <glm/geometric.hpp>

#include "GfMatrix.h"
#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

std::string Writer::stamps(std::string_view name, const geometry::Cloud& cloud,
                           const geometry::Mesh& stamp, const glm::mat4& model,
                           const world::Material& material,
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
  impl.fillMesh(proto, stamp);
  impl.bind(proto, stamp, {material}, name);
  instancer.CreatePrototypesRel().AddTarget(SdfPath(protoPath));

  const size_t n = cloud.size();
  VtVec3fArray positions;
  VtIntArray protoIndices((int)n, 0);
  VtVec3fArray scales;
  VtQuathArray orientations;
  const std::vector<float>* size = cloud.scalarIf("size");
  const std::vector<glm::vec3>* dir = cloud.vectorIf("dir");
  if (!dir) dir = cloud.vectorIf("normal");
  const std::vector<glm::vec4>* tint = cloud.colorIf("tint");
  for (size_t i = 0; i < n; ++i) {
    const glm::vec3& p = cloud.positions[i];
    positions.push_back({p.x, p.y, p.z});
    const float s = size && i < size->size() ? (*size)[i] : 1.0f;
    scales.push_back({s, s, s});
    if (dir && i < dir->size()) {
      // The stamp's +z along dir: the rotation taking (0,0,1) to dir.
      const glm::vec3 d = glm::normalize((*dir)[i]);
      const GfRotation rot(GfVec3d(0, 0, 1), GfVec3d(d.x, d.y, d.z));
      const GfQuatd q = rot.GetQuat();
      orientations.push_back(
          GfQuath((GfHalf)q.GetReal(), (GfHalf)q.GetImaginary()[0],
                  (GfHalf)q.GetImaginary()[1], (GfHalf)q.GetImaginary()[2]));
    }
  }
  instancer.CreatePositionsAttr().Set(positions);
  instancer.CreateProtoIndicesAttr().Set(protoIndices);
  instancer.CreateScalesAttr().Set(scales);
  if (orientations.size() == n)
    instancer.CreateOrientationsAttr().Set(orientations);
  if (tint && tint->size() == n) {
    VtVec3fArray colors;
    VtFloatArray alphas;
    for (const glm::vec4& c : *tint) {
      colors.push_back({c.r, c.g, c.b});
      alphas.push_back(c.a);
    }
    UsdGeomPrimvarsAPI primvars(instancer);
    primvars
        .CreatePrimvar(TfToken("displayColor"), SdfValueTypeNames->Color3fArray,
                       UsdGeomTokens->vertex)
        .Set(colors);
    primvars
        .CreatePrimvar(TfToken("displayOpacity"), SdfValueTypeNames->FloatArray,
                       UsdGeomTokens->vertex)
        .Set(alphas);
  }
  return path;
}

}  // namespace sigil::usd
