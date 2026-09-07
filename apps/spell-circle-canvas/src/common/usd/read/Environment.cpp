/** @file
 * The environment map read back: every UsdLuxDomeLight on the stage as
 * the dials it was written with and the name of the panorama file, which
 * the caller decodes — this library opens no image, the way it opens no
 * texture for a material either.
 */

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdLux/domeLight.h>

#include "GfMatrix.h"
#include "ReadContext.h"
#include "sigilusd/read/Reader.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

std::optional<std::vector<ReadEnvironment>> readEnvironments(
    const std::filesystem::path& file, std::string* error) {
  UsdStageRefPtr stage = openStage(file, error);
  if (!stage) return std::nullopt;
  ReadContext context;
  std::vector<ReadEnvironment> out;
  for (const UsdPrim& prim : stage->Traverse()) {
    const UsdLuxDomeLight dome(prim);
    if (!prim.IsA<UsdLuxDomeLight>()) continue;

    ReadEnvironment read;
    read.path = prim.GetPath().GetString();
    SdfAssetPath asset;
    if (dome.GetTextureFileAttr().Get(&asset))
      read.texture = asset.GetAssetPath();

    dome.GetIntensityAttr().Get(&read.environment.intensity);
    GfVec3f rgb(1, 1, 1);
    dome.GetColorAttr().Get(&rgb);
    read.environment.tint = {rgb[0], rgb[1], rgb[2]};

    // The prim's placement puts the panorama in the world; what a frame
    // carries is the inverse, which is what takes a world direction into
    // the panorama's own frame.
    const glm::mat4 world =
        fromGf(context.xforms.GetLocalToWorldTransform(prim));
    read.orientation = glm::inverse(glm::mat3(world));

    // The dials UsdLux has no word for. A dome light authored by
    // anything else keeps the defaults rather than reaching nowhere.
    const auto note = [&prim](const char* key, float& into) {
      const VtValue stored = prim.GetCustomDataByKey(TfToken(key));
      if (stored.IsHolding<float>()) into = stored.Get<float>();
    };
    note("sigil:diffuse", read.environment.diffuse);
    note("sigil:specular", read.environment.specular);
    note("sigil:roughnessBias", read.environment.roughnessBias);
    note("sigil:backdrop", read.environment.backdrop.intensity);
    note("sigil:backdropBlur", read.environment.backdrop.blur);
    note("sigil:groundRadius", read.environment.backdrop.groundRadius);
    out.push_back(std::move(read));
  }
  return out;
}

}  // namespace sigil::usd
