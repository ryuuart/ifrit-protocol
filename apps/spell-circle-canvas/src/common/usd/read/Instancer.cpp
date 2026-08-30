/** @file
 * A UsdGeomPointInstancer as a faceless Part: its positions with the
 * local-to-world xform baked, and its scales' x as the "size" lane.
 */

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/pointInstancer.h>

#include "GfMatrix.h"
#include "ReadContext.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

void readInstancer(const UsdPrim& prim, ReadContext& context,
                   geometry::decode::Model& model) {
  UsdGeomPointInstancer instancer(prim);
  VtVec3fArray positions;
  instancer.GetPositionsAttr().Get(&positions);
  if (positions.empty()) return;
  geometry::decode::Part part;
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

}  // namespace sigil::usd
