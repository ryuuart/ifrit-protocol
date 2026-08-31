/** @file
 * The camera as UsdGeomCamera: camera-to-world from the view matrix's
 * inverse, a 24 mm vertical aperture with the focal length that gives
 * the vertical field of view, the clipping range, and the distance to
 * what the camera looks at as its focus distance.
 */

#include <pxr/base/gf/vec2f.h>
#include <pxr/usd/usdGeom/camera.h>

#include <cmath>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include "GfMatrix.h"
#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

std::string Writer::camera(std::string_view name,
                           const geometry::mesh::camera::Camera& camera,
                           std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  UsdGeomCamera cam = UsdGeomCamera::Define(impl.stage, SdfPath(path));
  // Camera-to-world: the inverse of the view matrix.
  cam.AddTransformOp().Set(toGf(glm::inverse(camera.view())));
  // A 24mm-tall aperture with the focal length that gives the vertical
  // fov; the horizontal aperture follows the aspect a consumer sets.
  const float aperture = 24.0f;
  const float focal =
      aperture * 0.5f / std::tan(camera.fovYDeg * (float)M_PI / 360.0f);
  cam.CreateFocalLengthAttr().Set(focal);
  cam.CreateVerticalApertureAttr().Set(aperture);
  cam.CreateHorizontalApertureAttr().Set(aperture * 16.0f / 9.0f);
  cam.CreateClippingRangeAttr().Set(GfVec2f(camera.zNear, camera.zFar));
  // How far ahead the camera is looking: the only thing that says where
  // on the view ray the point it aims at sits, the view being the same
  // wherever along it that point is placed.
  cam.CreateFocusDistanceAttr().Set(glm::length(camera.target - camera.eye));
  return path;
}

}  // namespace sigil::usd
