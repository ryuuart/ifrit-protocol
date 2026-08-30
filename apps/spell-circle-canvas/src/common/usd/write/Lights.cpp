/** @file
 * Emitters: a point light or a spot as UsdLuxSphereLight (translated,
 * its range and any cone as custom data) and a sun as UsdLuxDistantLight,
 * oriented so its -Z runs along the direction.
 */

#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include <glm/geometric.hpp>

#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

std::string Writer::light(std::string_view name,
                          const world::light::Light& light,
                          std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  if (light.kind != world::light::Kind::Sun) {
    UsdLuxSphereLight sphere =
        UsdLuxSphereLight::Define(impl.stage, SdfPath(path));
    sphere.AddTranslateOp().Set(
        GfVec3d(light.position.x, light.position.y, light.position.z));
    sphere.CreateRadiusAttr().Set(1.0f);
    sphere.CreateIntensityAttr().Set(light.intensity);
    sphere.CreateColorAttr().Set(
        GfVec3f(light.color.x, light.color.y, light.color.z));
    sphere.GetPrim().SetCustomDataByKey(TfToken("sigil:range"),
                                        VtValue(light.range));
    if (light.kind == world::light::Kind::Spot) {
      // A cone on a sphere light has no UsdLux shape here, so the
      // direction and the two angles ride beside it.
      const glm::vec3 d = glm::normalize(light.direction);
      sphere.GetPrim().SetCustomDataByKey(TfToken("sigil:direction"),
                                          VtValue(GfVec3f(d.x, d.y, d.z)));
      sphere.GetPrim().SetCustomDataByKey(TfToken("sigil:coneInner"),
                                          VtValue(light.innerDeg));
      sphere.GetPrim().SetCustomDataByKey(TfToken("sigil:coneOuter"),
                                          VtValue(light.outerDeg));
    }
    return path;
  }
  UsdLuxDistantLight distant =
      UsdLuxDistantLight::Define(impl.stage, SdfPath(path));
  // A distant light shines down its -Z; aim -Z along the direction.
  const glm::vec3 d = glm::normalize(light.direction);
  const GfRotation rot(GfVec3d(0, 0, -1), GfVec3d(d.x, d.y, d.z));
  distant.AddOrientOp().Set(GfQuatf(rot.GetQuat()));
  distant.CreateIntensityAttr().Set(light.intensity);
  distant.CreateColorAttr().Set(
      GfVec3f(light.color.x, light.color.y, light.color.z));
  return path;
}

}  // namespace sigil::usd
