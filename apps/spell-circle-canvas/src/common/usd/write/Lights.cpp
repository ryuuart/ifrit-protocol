/** @file
 * Lights: a point light or a spot as UsdLuxSphereLight (translated;
 * range as custom data; a spot oriented -Z along its direction with the
 * cone as UsdLux shaping — outer angle and inner-edge softness), a
 * directional light as UsdLuxDistantLight, oriented so its -Z runs along
 * the direction, and an ambient or hemisphere light as an untextured
 * UsdLuxDomeLight — the hemisphere's ground colour as custom data and its
 * up as the dome's +Y; the light and shadow links as custom data; and a
 * directional, point or spot light's shadow as UsdLux's `shadow:enable`,
 * with its bias, normal bias, filter radius, map size, reach and cascades
 * as custom data.
 *
 * UsdLux states colour in linear light, so every colour is written
 * decoded. A sphere light's intensity is the radiance of its surface: a
 * sphere of radius one at radiance L delivers L times pi at a distance of
 * one, so a point light's intensity — what it delivers there — is written
 * over pi, and the prim is marked to be treated as a point.
 */

#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/shadowAPI.h>
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

constexpr float kPi = 3.14159265358979323846f;

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

/** WHETHER A LIGHT CASTS, as UsdLux's own shadow switch — written false as
 *  well as true, because a reader that knows no better takes an unstated
 *  switch as on — and, when it casts, how its shadow is made, which
 *  UsdLux has no words for, as custom data. */
void writeShadow(UsdPrim& prim, const world::LightDescription& light) {
  UsdLuxShadowAPI::Apply(prim).CreateShadowEnableAttr().Set(light.castShadow);
  if (!light.castShadow) return;
  const world::LightShadow& shadow = light.shadow;
  prim.SetCustomDataByKey(TfToken("sigil:shadowBias"), VtValue(shadow.bias));
  prim.SetCustomDataByKey(TfToken("sigil:shadowNormalBias"),
                          VtValue(shadow.normalBias));
  prim.SetCustomDataByKey(TfToken("sigil:shadowRadius"),
                          VtValue(shadow.radius));
  prim.SetCustomDataByKey(TfToken("sigil:shadowMapSize"),
                          VtValue(shadow.mapSize));
  prim.SetCustomDataByKey(TfToken("sigil:shadowDistance"),
                          VtValue(shadow.distance));
  prim.SetCustomDataByKey(TfToken("sigil:shadowCascades"),
                          VtValue(shadow.cascades));
}

}  // namespace

std::string Writer::light(std::string_view name,
                          const world::PlacedLight& placed,
                          std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const world::LightDescription& light = placed.light;
  const glm::vec3 color = world::linearColor(light.color.value());
  const GfVec3f linear(color.r, color.g, color.b);
  const std::string path = impl.uniquePath(parent, name);
  if (light.kind == world::LightKind::Ambient ||
      light.kind == world::LightKind::Hemisphere) {
    // Light arriving from everywhere is a dome with no picture on it.
    UsdLuxDomeLight dome = UsdLuxDomeLight::Define(impl.stage, SdfPath(path));
    dome.CreateIntensityAttr().Set(light.intensity.value());
    dome.CreateColorAttr().Set(linear);
    UsdPrim prim = dome.GetPrim();
    if (light.kind == world::LightKind::Hemisphere) {
      // UsdLux has no two-coloured dome: the sky is the dome's colour,
      // which is what a reader that knows no better shows, and the
      // ground rides beside it. The dome's +Y is its up.
      const glm::vec3 ground = world::linearColor(light.groundColor.value());
      prim.SetCustomDataByKey(TfToken("sigil:groundColor"),
                              VtValue(GfVec3f(ground.r, ground.g, ground.b)));
      if (const std::optional<glm::vec3> up = aimedAlong(light.direction)) {
        const GfRotation rot(GfVec3d(0, 1, 0), GfVec3d(up->x, up->y, up->z));
        dome.AddOrientOp().Set(GfQuatf(rot.GetQuat()));
      }
    }
    writeLink(prim, "sigil:lightLink", light.lightLink);
    writeLink(prim, "sigil:shadowLink", light.shadowLink);
    return path;
  }
  if (light.kind != world::LightKind::Directional) {
    UsdLuxSphereLight sphere =
        UsdLuxSphereLight::Define(impl.stage, SdfPath(path));
    sphere.AddTranslateOp().Set(
        GfVec3d(placed.position.x, placed.position.y, placed.position.z));
    sphere.CreateRadiusAttr().Set(1.0f);
    sphere.CreateTreatAsPointAttr().Set(true);
    sphere.CreateIntensityAttr().Set(light.intensity.value() / kPi);
    sphere.CreateColorAttr().Set(linear);
    UsdPrim prim = sphere.GetPrim();
    prim.SetCustomDataByKey(TfToken("sigil:range"), VtValue(light.range));
    writeLink(prim, "sigil:lightLink", light.lightLink);
    writeLink(prim, "sigil:shadowLink", light.shadowLink);
    writeShadow(prim, light);
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
  distant.CreateColorAttr().Set(linear);
  UsdPrim prim = distant.GetPrim();
  writeLink(prim, "sigil:lightLink", light.lightLink);
  writeLink(prim, "sigil:shadowLink", light.shadowLink);
  writeShadow(prim, light);
  return path;
}

}  // namespace sigil::usd
