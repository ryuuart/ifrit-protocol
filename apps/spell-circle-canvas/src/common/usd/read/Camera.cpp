/** @file
 * The camera read back: the prim's local-to-world is the camera-to-world
 * — its translation the eye, its -Z the view direction and its +Y the
 * up — the focal length against the vertical aperture is the vertical
 * field of view, and the clipping range the near and far planes. The
 * target rides the view direction at the focus distance, since a camera
 * sees the same thing wherever along that ray the point it looks at is
 * placed.
 */

#include <pxr/base/gf/vec2f.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/camera.h>

#include <cmath>
#include <glm/geometric.hpp>

#include "GfMatrix.h"
#include "ReadContext.h"
#include "sigilusd/read/Reader.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

std::optional<geometry::mesh::camera::Camera> readCamera(const UsdPrim& prim,
                                                         ReadContext& context) {
  if (!prim.IsA<UsdGeomCamera>()) return std::nullopt;
  const UsdGeomCamera cam(prim);
  const glm::mat4 toWorld =
      fromGf(context.xforms.GetLocalToWorldTransform(prim));
  geometry::mesh::camera::Camera camera;
  camera.eye = glm::vec3(toWorld[3]);
  camera.up = glm::normalize(glm::vec3(toWorld[1]));
  const glm::vec3 forward = -glm::normalize(glm::vec3(toWorld[2]));
  // USD's fallback focus distance is zero, which would put the target on
  // the eye and leave the view with no direction at all; one unit ahead
  // is the same view.
  float focus = 0;
  cam.GetFocusDistanceAttr().Get(&focus);
  camera.target = camera.eye + forward * (focus > 0 ? focus : 1.0f);
  float focal = 0;
  float aperture = 0;
  cam.GetFocalLengthAttr().Get(&focal);
  cam.GetVerticalApertureAttr().Get(&aperture);
  if (focal > 0 && aperture > 0)
    camera.fovYDeg = std::atan(aperture * 0.5f / focal) * 360.0f / (float)M_PI;
  GfVec2f clip;
  if (cam.GetClippingRangeAttr().Get(&clip)) {
    camera.zNear = clip[0];
    camera.zFar = clip[1];
  }
  return camera;
}

std::optional<std::vector<ReadCamera>> readCameras(
    const std::filesystem::path& file, std::string* error) {
  UsdStageRefPtr stage = openStage(file, error);
  if (!stage) return std::nullopt;
  ReadContext context;
  std::vector<ReadCamera> cameras;
  for (const UsdPrim& prim : stage->Traverse())
    if (std::optional<geometry::mesh::camera::Camera> camera =
            readCamera(prim, context))
      cameras.push_back({prim.GetPath().GetString(), *camera});
  return cameras;
}

}  // namespace sigil::usd
