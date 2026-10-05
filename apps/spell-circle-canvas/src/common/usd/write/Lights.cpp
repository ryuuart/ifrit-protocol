/** @file
 * Lights: a point light or a spot as UsdLuxSphereLight (translated;
 * range as custom data; a spot oriented -Z along its direction with the
 * cone as UsdLux shaping — outer angle and inner-edge softness) and a
 * directional light as UsdLuxDistantLight, oriented so its -Z runs along
 * the direction; the light and shadow links as custom data.
 */

#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <sigilmaterial/color/Color.h>
#include <sigilworld/advanced/Light.h>

#include <glm/geometric.hpp>
#include <optional>
#include <string>
#include <vector>

#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

namespace {

/** @p link as custom data under @p key, where it narrows anything: its
 *  terms name collections and keys, which no prim of this stage stands
 *  for, so a USD collection could not hold them. */
void writeLink(UsdPrim& prim, const char* key, const world::Collection& link) {
  const auto terms = [&prim](const std::string& field,
                             const std::vector<std::string>& from) {
    if (from.empty()) return;
    prim.SetCustomDataByKey(TfToken(field), VtValue(VtArray<std::string>(
                                                from.begin(), from.end())));
  };
  terms(std::string(key) + ":include", link.include);
  terms(std::string(key) + ":exclude", link.exclude);
}

}  // namespace

std::string Writer::light(std::string_view name,
                          const world::PlacedLight& placed,
                          std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const world::LightDescription& light = placed.light;
  const material::Color color = light.color.value();
  const std::string path = impl.uniquePath(parent, name);
  if (light.kind != world::LightKind::Directional) {
    UsdLuxSphereLight sphere =
        UsdLuxSphereLight::Define(impl.stage, SdfPath(path));
    sphere.AddTranslateOp().Set(
        GfVec3d(placed.position.x, placed.position.y, placed.position.z));
    sphere.CreateRadiusAttr().Set(1.0f);
    sphere.CreateIntensityAttr().Set(light.intensity.value());
    sphere.CreateColorAttr().Set(GfVec3f(color.r, color.g, color.b));
    UsdPrim prim = sphere.GetPrim();
    prim.SetCustomDataByKey(TfToken("sigil:range"), VtValue(light.range));
    writeLink(prim, "sigil:lightLink", light.lightLink);
    writeLink(prim, "sigil:shadowLink", light.shadowLink);
    if (light.kind == world::LightKind::Spot) {
      // The cone is UsdLux shaping: it opens around the light's -Z, so
      // the prim is oriented to aim -Z along the direction, the outer
      // angle is the shaping cone angle, and the inner edge is the
      // softness — the fraction of the cone over which the light falls
      // off — so any UsdLux reader shades the same spot.
      // A spot with no direction to speak of keeps the prim's default
      // orientation: a rotation from a zero vector is a NaN.
      if (const std::optional<glm::vec3> d = aimedAlong(light.direction)) {
        const GfRotation rot(GfVec3d(0, 0, -1), GfVec3d(d->x, d->y, d->z));
        sphere.AddOrientOp().Set(GfQuatf(rot.GetQuat()));
      }
      UsdLuxShapingAPI shaping = UsdLuxShapingAPI::Apply(prim);
      shaping.CreateShapingConeAngleAttr().Set(light.outerAngle);
      const float softness =
          light.outerAngle > 0
              ? std::max(0.0f, 1.0f - light.innerAngle / light.outerAngle)
              : 0.0f;
      shaping.CreateShapingConeSoftnessAttr().Set(softness);
    }
    return path;
  }
  UsdLuxDistantLight distant =
      UsdLuxDistantLight::Define(impl.stage, SdfPath(path));
  // A distant light shines down its -Z; aim -Z along the direction, and
  // leave one with no direction facing the way a distant light faces.
  if (const std::optional<glm::vec3> d = aimedAlong(light.direction)) {
    const GfRotation rot(GfVec3d(0, 0, -1), GfVec3d(d->x, d->y, d->z));
    distant.AddOrientOp().Set(GfQuatf(rot.GetQuat()));
  }
  distant.CreateIntensityAttr().Set(light.intensity.value());
  distant.CreateColorAttr().Set(GfVec3f(color.r, color.g, color.b));
  UsdPrim prim = distant.GetPrim();
  writeLink(prim, "sigil:lightLink", light.lightLink);
  writeLink(prim, "sigil:shadowLink", light.shadowLink);
  return path;
}

}  // namespace sigil::usd
