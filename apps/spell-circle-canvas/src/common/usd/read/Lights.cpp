/** @file
 * Lights read back: a UsdLuxDistantLight as a directional light, a
 * UsdLuxSphereLight as a point light or — when the prim carries an
 * authored shaping cone — as a spot. The prim's local-to-world says where
 * it stands and aims its -Z, which is the axis UsdLux opens a cone
 * around; `sigil:range` gives the range a UsdLux light has no word for,
 * and `sigil:lightLink` and `sigil:shadowLink` the links.
 */

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include <glm/geometric.hpp>

#include "GfMatrix.h"
#include "ReadContext.h"
#include "sigilusd/read/Reader.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

namespace {

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

}  // namespace

std::optional<world::PlacedLight> readLight(const UsdPrim& prim,
                                            ReadContext& context) {
  const bool distant = prim.IsA<UsdLuxDistantLight>();
  if (!distant && !prim.IsA<UsdLuxSphereLight>()) return std::nullopt;

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
  light.color = material::Color(rgb[0], rgb[1], rgb[2], 1);
  light.direction = direction;

  if (distant) {
    light.kind = world::LightKind::Directional;
    return placed;
  }

  placed.position = glm::vec3(world[3]);
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
