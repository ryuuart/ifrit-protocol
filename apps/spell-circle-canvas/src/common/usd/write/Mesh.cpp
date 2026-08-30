/** @file
 * A Mesh as a UsdGeomMesh: its lanes as attributes and primvars, its
 * placement as a transform op, and its material slots bound — one
 * material over the whole mesh, or GeomSubsets by the "Material" lane.
 */

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/tokens.h>

#include <algorithm>
#include <cmath>

#include "GfMatrix.h"
#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

void Writer::Impl::fillMesh(UsdGeomMesh& usdMesh, const geometry::Mesh& mesh) {
  VtVec3fArray points;
  points.reserve(mesh.positions.size());
  for (const glm::vec3& p : mesh.positions) points.push_back({p.x, p.y, p.z});
  usdMesh.CreatePointsAttr().Set(points);
  VtIntArray counts((int)mesh.triangleCount(), 3);
  VtIntArray indices;
  indices.reserve(mesh.indices.size());
  for (uint32_t i : mesh.indices) indices.push_back((int)i);
  usdMesh.CreateFaceVertexCountsAttr().Set(counts);
  usdMesh.CreateFaceVertexIndicesAttr().Set(indices);
  usdMesh.CreateSubdivisionSchemeAttr().Set(UsdGeomTokens->none);
  if (mesh.normals.size() == mesh.positions.size()) {
    VtVec3fArray normals;
    for (const glm::vec3& n : mesh.normals) normals.push_back({n.x, n.y, n.z});
    usdMesh.CreateNormalsAttr().Set(normals);
    usdMesh.SetNormalsInterpolation(UsdGeomTokens->vertex);
  }
  UsdGeomPrimvarsAPI primvars(usdMesh);
  if (mesh.uvs.size() == mesh.positions.size()) {
    // USD's st has v UP the image; ours runs down.
    VtVec2fArray st;
    for (const glm::vec2& uv : mesh.uvs) st.push_back({uv.x, 1.0f - uv.y});
    primvars
        .CreatePrimvar(TfToken("st"), SdfValueTypeNames->TexCoord2fArray,
                       UsdGeomTokens->vertex)
        .Set(st);
  }
  if (mesh.colors.size() == mesh.positions.size()) {
    VtVec3fArray colors;
    VtFloatArray alphas;
    for (const glm::vec4& c : mesh.colors) {
      colors.push_back({c.r, c.g, c.b});
      alphas.push_back(c.a);
    }
    usdMesh.CreateDisplayColorPrimvar(UsdGeomTokens->vertex).Set(colors);
    usdMesh.CreateDisplayOpacityPrimvar(UsdGeomTokens->vertex).Set(alphas);
  }
  for (const auto& [name, lane] : mesh.prims) {
    if (name == "Material" || lane.size() != mesh.triangleCount()) continue;
    VtVec4fArray values;
    for (const glm::vec4& v : lane) values.push_back({v.x, v.y, v.z, v.w});
    primvars
        .CreatePrimvar(TfToken(identifier(name)),
                       SdfValueTypeNames->Float4Array, UsdGeomTokens->uniform)
        .Set(values);
  }
}

void Writer::Impl::bind(UsdGeomMesh& usdMesh, const geometry::Mesh& mesh,
                        const std::vector<world::Material>& slots,
                        std::string_view hint) {
  if (slots.empty()) return;
  const std::vector<glm::vec4>* lane = mesh.primIf("Material");
  if (slots.size() == 1 || !lane || lane->size() != mesh.triangleCount()) {
    UsdShadeMaterialBindingAPI::Apply(usdMesh.GetPrim())
        .Bind(UsdShadeMaterial(stage->GetPrimAtPath(
            material(slots.front(), std::string(hint) + "_material"))));
    return;
  }
  std::vector<VtIntArray> faces(slots.size());
  for (size_t t = 0; t < lane->size(); ++t) {
    int slot = (int)std::floor((*lane)[t].x + 0.5f);
    slot = std::clamp(slot, 0, (int)slots.size() - 1);
    faces[(size_t)slot].push_back((int)t);
  }
  for (size_t s = 0; s < slots.size(); ++s) {
    if (faces[s].empty()) continue;
    UsdGeomSubset subset = UsdGeomSubset::CreateGeomSubset(
        usdMesh, TfToken("slot" + std::to_string(s)), UsdGeomTokens->face,
        faces[s], UsdShadeTokens->materialBind);
    UsdShadeMaterialBindingAPI::Apply(subset.GetPrim())
        .Bind(UsdShadeMaterial(stage->GetPrimAtPath(material(
            slots[s], std::string(hint) + "_slot" + std::to_string(s)))));
  }
}

std::string Writer::mesh(std::string_view name, const geometry::Mesh& mesh,
                         const glm::mat4& model,
                         const std::vector<world::Material>& slots,
                         std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  UsdGeomMesh usdMesh = UsdGeomMesh::Define(impl.stage, SdfPath(path));
  usdMesh.AddTransformOp().Set(toGf(model));
  impl.fillMesh(usdMesh, mesh);
  impl.bind(usdMesh, mesh, slots, name);
  return path;
}

}  // namespace sigil::usd
