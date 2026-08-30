/** @file
 * Lights: a point light as UsdLuxSphereLight (translated, its range as
 * custom data) and a directional light — the sun — as
 * UsdLuxDistantLight, oriented so its -Z runs along the direction.
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
                          const world::LightComponent& light,
                          std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  if (light.type == world::LightComponent::Type::Point) {
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

std::string Writer::sun(std::string_view name, const world::Lighting& lighting,
                        std::string_view parent) {
  world::LightComponent sun;
  sun.type = world::LightComponent::Type::Directional;
  sun.direction = lighting.sunDirection;
  sun.color = lighting.sunColor;
  sun.intensity = lighting.sunIntensity;
  return light(name, sun, parent);
}

}  // namespace sigil::usd
