/** @file
 * A UsdGeomMesh as a Part: every face-vertex becomes a vertex so
 * face-varying `st` and normals survive, faces fan-triangulate, the
 * local-to-world xform is baked into positions, `st`'s v is flipped
 * back, and GeomSubsets become the "Material" lane.
 */

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/tokens.h>

#include <algorithm>
#include <utility>

#include "GfMatrix.h"
#include "Primvar.h"
#include "ReadContext.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

void readMesh(const UsdPrim& prim, ReadContext& context,
              geometry::mesh::codec::decode::Model& model) {
  UsdGeomMesh usdMesh(prim);
  VtVec3fArray points;
  VtIntArray counts, indices;
  usdMesh.GetPointsAttr().Get(&points);
  usdMesh.GetFaceVertexCountsAttr().Get(&counts);
  usdMesh.GetFaceVertexIndicesAttr().Get(&indices);
  if (points.empty() || counts.empty()) return;
  geometry::mesh::codec::decode::Part part;
  part.name = prim.GetName().GetString();
  geometry::mesh::Mesh& mesh = part.mesh;
  VtVec3fArray normals;
  usdMesh.GetNormalsAttr().Get(&normals);
  const TfToken normalsInterp = usdMesh.GetNormalsInterpolation();
  UsdGeomPrimvarsAPI primvars(usdMesh);
  VtVec2fArray st;
  VtIntArray stIndices;
  TfToken stInterp;
  if (UsdGeomPrimvar pv = primvars.GetPrimvar(TfToken("st"))) {
    pv.Get(&st);
    pv.GetIndices(&stIndices);
    stInterp = pv.GetInterpolation();
  }
  VtVec3fArray colors;
  VtIntArray colorIndices;
  TfToken colorInterp;
  if (UsdGeomPrimvar pv = usdMesh.GetDisplayColorPrimvar()) {
    pv.Get(&colors);
    pv.GetIndices(&colorIndices);
    colorInterp = pv.GetInterpolation();
  }
  const std::vector<GfVec3f> nPerFv =
      normals.empty() ? std::vector<GfVec3f>{}
                      : perFaceVertex(normals, normalsInterp, counts, indices,
                                      VtIntArray());
  const std::vector<GfVec2f> stPerFv =
      st.empty() ? std::vector<GfVec2f>{}
                 : perFaceVertex(st, stInterp, counts, indices, stIndices);
  const std::vector<GfVec3f> cPerFv =
      colors.empty()
          ? std::vector<GfVec3f>{}
          : perFaceVertex(colors, colorInterp, counts, indices, colorIndices);
  // Face -> subset material slot (the "Material" lane), then the fan.
  std::vector<int> faceSlot(counts.size(), -1);
  const std::vector<UsdGeomSubset> subsets = UsdGeomSubset::GetGeomSubsets(
      usdMesh, UsdGeomTokens->face, UsdShadeTokens->materialBind);
  for (const UsdGeomSubset& subset : subsets) {
    const int slot = context.slot(
        UsdShadeMaterialBindingAPI(subset.GetPrim()).ComputeBoundMaterial());
    VtIntArray faces;
    subset.GetIndicesAttr().Get(&faces);
    for (int f : faces)
      if (f >= 0 && (size_t)f < faceSlot.size()) faceSlot[(size_t)f] = slot;
  }
  const UsdShadeMaterial bound =
      UsdShadeMaterialBindingAPI(prim).ComputeBoundMaterial();
  const int wholeSlot = context.slot(bound);
  if (bound) {
    readMaterial(bound, context.stageDir, part);
    part.materialIndex = wholeSlot;
  }
  std::vector<glm::vec4> laneValues;
  size_t fv = 0;
  for (size_t f = 0; f < counts.size(); ++f) {
    const int n = counts[f];
    const uint32_t base = (uint32_t)mesh.positions.size();
    for (int k = 0; k < n; ++k, ++fv) {
      const GfVec3f& p = points[(size_t)indices[fv]];
      mesh.positions.emplace_back(p[0], p[1], p[2]);
      if (!nPerFv.empty()) {
        const GfVec3f& nn = nPerFv[fv];
        mesh.normals.emplace_back(nn[0], nn[1], nn[2]);
      }
      if (!stPerFv.empty()) {
        const GfVec2f& uv = stPerFv[fv];
        mesh.uvs.emplace_back(uv[0], 1.0f - uv[1]);
      }
      if (!cPerFv.empty()) {
        const GfVec3f& c = cPerFv[fv];
        mesh.colors.emplace_back(c[0], c[1], c[2], 1);
      }
    }
    for (int k = 1; k + 1 < n; ++k) {
      mesh.indices.insert(mesh.indices.end(),
                          {base, base + (uint32_t)k, base + (uint32_t)k + 1});
      const int slot = faceSlot[f] >= 0 ? faceSlot[f] : wholeSlot;
      laneValues.emplace_back((float)std::max(slot, 0), 0, 0, 0);
    }
  }
  if (!subsets.empty() || wholeSlot >= 0)
    mesh.prim("Material", {0, 0, 0, 0}) = std::move(laneValues);
  if (mesh.normals.size() != mesh.positions.size()) mesh.normals.clear();
  if (mesh.uvs.size() != mesh.positions.size()) mesh.uvs.clear();
  if (mesh.colors.size() != mesh.positions.size()) mesh.colors.clear();
  // The subset materials: the FIRST subset's material fills the part's
  // factors when the mesh as a whole binds none.
  if (!bound && !subsets.empty())
    readMaterial(UsdShadeMaterialBindingAPI(subsets.front().GetPrim())
                     .ComputeBoundMaterial(),
                 context.stageDir, part);
  const glm::mat4 world = fromGf(context.xforms.GetLocalToWorldTransform(prim));
  mesh.transform(world);
  if (mesh.normals.empty() && !mesh.indices.empty()) mesh.computeNormals();
  if (!mesh.indices.empty()) model.parts.push_back(std::move(part));
}

}  // namespace sigil::usd
