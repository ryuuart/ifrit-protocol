/** @file
 * Emitters read back: a UsdLuxDistantLight as a sun, a UsdLuxSphereLight
 * as a point light or — when the prim carries an authored shaping cone —
 * as a spot. The prim's local-to-world says where it stands and aims its
 * -Z, which is the axis UsdLux opens a cone around; `sigil:range` gives
 * the range a UsdLux light has no word for.
 */

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
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

std::optional<world::light::Light> readLight(const UsdPrim& prim,
                                             ReadContext& context) {
  const bool distant = prim.IsA<UsdLuxDistantLight>();
  if (!distant && !prim.IsA<UsdLuxSphereLight>()) return std::nullopt;

  const glm::mat4 world = fromGf(context.xforms.GetLocalToWorldTransform(prim));
  // A UsdLux light shines down its -Z; the third basis vector of the
  // prim's placement is that axis, which is the direction for a sun and
  // the aim for a spot.
  const glm::vec3 direction = -glm::normalize(glm::vec3(world[2]));

  const UsdLuxLightAPI api(prim);
  const world::light::Light defaults;
  float intensity = defaults.intensity;
  api.GetIntensityAttr().Get(&intensity);
  GfVec3f rgb(1, 1, 1);
  api.GetColorAttr().Get(&rgb);
  const glm::vec4 color(rgb[0], rgb[1], rgb[2], 1);

  if (distant) return world::light::sun(direction, color, intensity);

  const glm::vec3 position(world[3]);
  // The range is ours: a light authored by anything else keeps the
  // default rather than reaching nowhere.
  float range = defaults.range;
  const VtValue stored = prim.GetCustomDataByKey(TfToken("sigil:range"));
  if (stored.IsHolding<float>()) range = stored.Get<float>();

  // A cone the stage actually authors is what makes a sphere light a
  // spot: UsdLux's fallback angle is a hemisphere, which is no cone at
  // all. The softness is the fraction of the cone the falloff eats, so
  // the inner edge is what is left of it.
  const UsdLuxShapingAPI shaping(prim);
  const UsdAttribute cone = shaping.GetShapingConeAngleAttr();
  float outerDeg = 0;
  if (cone && cone.HasAuthoredValue() && cone.Get(&outerDeg)) {
    float softness = 0;
    if (const UsdAttribute attr = shaping.GetShapingConeSoftnessAttr())
      attr.Get(&softness);
    return world::light::spot(position, direction, outerDeg,
                              outerDeg * (1.0f - softness), color, intensity,
                              range);
  }
  return world::light::point(position, color, intensity, range);
}

std::optional<std::vector<ReadLight>> readLights(
    const std::filesystem::path& file, std::string* error) {
  UsdStageRefPtr stage = openStage(file, error);
  if (!stage) return std::nullopt;
  ReadContext context;
  std::vector<ReadLight> lights;
  for (const UsdPrim& prim : stage->Traverse())
    if (std::optional<world::light::Light> light = readLight(prim, context))
      lights.push_back({prim.GetPath().GetString(), *light});
  return lights;
}

}  // namespace sigil::usd
