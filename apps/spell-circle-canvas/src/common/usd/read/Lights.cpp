/** @file
 * Lights read back: a UsdLuxDistantLight as a directional light, a
 * UsdLuxSphereLight as a point light or — when the prim carries an
 * authored shaping cone — as a spot, and a UsdLuxDomeLight with no
 * picture on it as an ambient light, or a hemisphere light where it
 * carries `sigil:groundColor`. The prim's local-to-world says where it
 * stands and aims its -Z, which is the axis UsdLux opens a cone around,
 * and a dome's +Y, which is a hemisphere's up; `sigil:range` gives the
 * range a UsdLux light has no word for, and `sigil:lightLink` and
 * `sigil:shadowLink` the links. A directional, point or spot light casts
 * where the prim applies UsdLux's shadow schema with `shadow:enable` on,
 * its shadow made as the `sigil:shadow…` custom data says; a prim that
 * does not apply the schema casts nothing, which keeps a stage authored
 * without shadows in mind looking as it was authored, though UsdLux's own
 * fallback for the switch is on.
 *
 * UsdLux colour is linear and is encoded on the way in, as a light's
 * colour is typed; a sphere light's intensity is the radiance of its
 * surface, which a sphere of radius r delivers times pi r squared at a
 * distance of one — the point light's intensity.
 */

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shadowAPI.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include <glm/geometric.hpp>
#include <type_traits>

#include "GfMatrix.h"
#include "ReadContext.h"
#include "sigilusd/read/Reader.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

namespace {

constexpr float kPi = 3.14159265358979323846f;

/** The terms of a collection a light link stored as custom data, or the
 *  whole scene where the prim stores none. */
world::Collection linkOf(const UsdPrim& prim, const char* key) {
  world::Collection link;
  const auto terms = [&prim](const std::string& field,
                             std::vector<std::string>& into) {
    const VtValue stored = prim.GetCustomDataByKey(TfToken(field));
    if (!stored.IsHolding<VtArray<std::string>>()) return;
    for (const std::string& term : stored.UncheckedGet<VtArray<std::string>>())
      into.push_back(term);
  };
  terms(std::string(key) + ":include", link.include);
  terms(std::string(key) + ":exclude", link.exclude);
  return link;
}

/** Whether a light casts, and how its shadow is made: UsdLux's switch,
 *  where the prim applies the schema, and the custom data beside it. */
void readShadow(const UsdPrim& prim, world::LightDescription& light) {
  if (!prim.HasAPI<UsdLuxShadowAPI>()) return;
  bool enabled = false;
  if (const UsdAttribute attr = UsdLuxShadowAPI(prim).GetShadowEnableAttr())
    attr.Get(&enabled);
  light.castShadow = enabled;
  const auto number = [&prim](const char* key, auto& into) {
    const VtValue stored = prim.GetCustomDataByKey(TfToken(key));
    using Value = std::decay_t<decltype(into)>;
    if (stored.IsHolding<Value>()) into = stored.UncheckedGet<Value>();
  };
  number("sigil:shadowBias", light.shadow.bias);
  number("sigil:shadowNormalBias", light.shadow.normalBias);
  number("sigil:shadowRadius", light.shadow.radius);
  number("sigil:shadowMapSize", light.shadow.mapSize);
  number("sigil:shadowDistance", light.shadow.distance);
  number("sigil:shadowCascades", light.shadow.cascades);
}

}  // namespace

std::optional<world::PlacedLight> readLight(const UsdPrim& prim,
                                            ReadContext& context) {
  const bool distant = prim.IsA<UsdLuxDistantLight>();
  const bool sphere = prim.IsA<UsdLuxSphereLight>();
  const bool rect = prim.IsA<UsdLuxRectLight>();
  const bool disk = prim.IsA<UsdLuxDiskLight>();
  // A dome WITH a picture is an environment, read by `readEnvironments`;
  // one without is light arriving equally from everywhere.
  bool dome = false;
  if (prim.IsA<UsdLuxDomeLight>()) {
    SdfAssetPath picture;
    const UsdAttribute file = UsdLuxDomeLight(prim).GetTextureFileAttr();
    dome = !(file && file.HasAuthoredValue() && file.Get(&picture) &&
             !picture.GetAssetPath().empty());
  }
  if (!distant && !sphere && !dome && !rect && !disk) return std::nullopt;

  const glm::mat4 world = fromGf(context.xforms.GetLocalToWorldTransform(prim));
  // A UsdLux light shines down its -Z; the third basis vector of the
  // prim's placement is that axis, which is the direction a directional
  // light and a spot travel.
  const glm::vec3 direction = -glm::normalize(glm::vec3(world[2]));

  world::PlacedLight placed;
  placed.key = prim.GetName().GetString();
  world::LightDescription& light = placed.light;
  light.lightLink = linkOf(prim, "sigil:lightLink");
  light.shadowLink = linkOf(prim, "sigil:shadowLink");
  const UsdLuxLightAPI api(prim);
  float intensity = light.intensity.value();
  api.GetIntensityAttr().Get(&intensity);
  light.intensity = intensity;
  GfVec3f rgb(1, 1, 1);
  api.GetColorAttr().Get(&rgb);
  light.color = world::encodedColor({rgb[0], rgb[1], rgb[2]});
  light.direction = direction;

  if (dome) {
    const VtValue ground =
        prim.GetCustomDataByKey(TfToken("sigil:groundColor"));
    if (!ground.IsHolding<GfVec3f>()) {
      light.kind = world::LightKind::Ambient;
      return placed;
    }
    const GfVec3f g = ground.UncheckedGet<GfVec3f>();
    light.kind = world::LightKind::Hemisphere;
    light.groundColor = world::encodedColor({g[0], g[1], g[2]});
    light.direction = glm::normalize(glm::vec3(world[1]));
    return placed;
  }

  readShadow(prim, light);
  if (distant) {
    light.kind = world::LightKind::Directional;
    return placed;
  }

  placed.position = glm::vec3(world[3]);
  // A RECT or a DISK is ours of its shape: its intensity is the radiance
  // of its face, which a disk carries as what it delivers at one unit
  // over pi r². `normalize` spreads the stated power over the face, so it
  // is divided by the area here.
  bool normalized = false;
  api.GetNormalizeAttr().Get(&normalized);
  if (rect || disk) {
    placed.across = glm::normalize(glm::vec3(world[0]));
    if (rect) {
      light.kind = world::LightKind::Rect;
      float width = 1.0f;
      float height = 1.0f;
      UsdLuxRectLight(prim).GetWidthAttr().Get(&width);
      UsdLuxRectLight(prim).GetHeightAttr().Get(&height);
      light.width = width;
      light.height = height;
      if (normalized && width * height > 0.0f)
        light.intensity = intensity / (width * height);
      return placed;
    }
    light.kind = world::LightKind::Disk;
    float diskRadius = 0.5f;
    UsdLuxDiskLight(prim).GetRadiusAttr().Get(&diskRadius);
    light.radius = diskRadius;
    light.intensity =
        normalized ? intensity : intensity * kPi * diskRadius * diskRadius;
    return placed;
  }
  float radius = 1.0f;
  UsdLuxSphereLight(prim).GetRadiusAttr().Get(&radius);
  light.intensity = intensity * kPi * radius * radius;
  // The range is ours: a light authored by anything else keeps the
  // default rather than reaching nowhere.
  const VtValue stored = prim.GetCustomDataByKey(TfToken("sigil:range"));
  if (stored.IsHolding<float>()) light.range = stored.Get<float>();

  // A cone the stage actually authors is what makes a sphere light a
  // spot: UsdLux's fallback angle is a hemisphere, which is no cone at
  // all. The softness is the fraction of the cone the falloff eats, so
  // the inner edge is what is left of it.
  const UsdLuxShapingAPI shaping(prim);
  const UsdAttribute cone = shaping.GetShapingConeAngleAttr();
  float outerAngle = 0;
  if (cone && cone.HasAuthoredValue() && cone.Get(&outerAngle)) {
    float softness = 0;
    if (const UsdAttribute attr = shaping.GetShapingConeSoftnessAttr())
      attr.Get(&softness);
    light.kind = world::LightKind::Spot;
    light.outerAngle = outerAngle;
    light.innerAngle = outerAngle * (1.0f - softness);
    return placed;
  }
  // A sphere not marked to be treated as a point is a ball of its radius.
  bool point = false;
  UsdLuxSphereLight(prim).GetTreatAsPointAttr().Get(&point);
  if (!point && radius > 0.0f) {
    light.kind = world::LightKind::Sphere;
    light.radius = radius;
    if (normalized) light.intensity = intensity;
    return placed;
  }
  light.kind = world::LightKind::Point;
  return placed;
}

std::optional<std::vector<ReadLight>> readLights(
    const std::filesystem::path& file, std::string* error) {
  UsdStageRefPtr stage = openStage(file, error);
  if (!stage) return std::nullopt;
  ReadContext context;
  std::vector<ReadLight> lights;
  for (const UsdPrim& prim : stage->Traverse())
    if (std::optional<world::PlacedLight> light = readLight(prim, context))
      lights.push_back({prim.GetPath().GetString(), *light});
  return lights;
}

}  // namespace sigil::usd
