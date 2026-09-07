/** @file
 * The environment map as a UsdLuxDomeLight: the panorama written beside
 * the stage, named on `inputs:texture:file` and declared lat-long, the
 * strength and the tint on the light's own attributes, and the node's
 * orientation as the prim's transform.
 *
 * A dome light is the one UsdLux shape whose payload is a picture, and
 * this is the one place in the writer where the format of that picture
 * is a decision rather than a convention. A panorama holds values above
 * one — that is what makes a sun a sun rather than a white disc the same
 * brightness as the sky beside it — and a stage this writer produces
 * must open the same way on every machine, so the format cannot be one
 * an optional backend supplies. That rules out the floating-point
 * formats and leaves the deepest one always present. So the panorama is
 * DIVIDED BY ITS PEAK, written as a sixteen-bit PNG, and the peak is
 * multiplied into the dome light's intensity. The ratios survive at
 * sixteen bits a channel, the total brightness is right, and it is right
 * through the standard attribute rather than through a custom one only
 * this library reads.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/source/Sink.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

namespace {

/** The panorama scaled into [0, 1] and written as a sixteen-bit PNG.
 *  Returns the factor it was divided by, or 0 when nothing was written. */
float writePanorama(const sk_sp<SkImage>& image,
                    const std::filesystem::path& path) {
  if (!image) return 0;
  const int w = image->width(), h = image->height();
  std::vector<float> pixels((size_t)w * h * 4);
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  if (!image->readPixels(
          nullptr, SkPixmap(info, pixels.data(), (size_t)w * 4 * sizeof(float)),
          0, 0))
    return 0;
  float peak = 1;
  for (size_t i = 0; i < pixels.size(); i += 4)
    for (int c = 0; c < 3; ++c) peak = std::max(peak, pixels[i + c]);
  for (size_t i = 0; i < pixels.size(); i += 4)
    for (int c = 0; c < 3; ++c) pixels[i + c] /= peak;

  SkBitmap bm;
  bm.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_F16_SkColorType, kPremul_SkAlphaType));
  if (!SkPixmap(info, pixels.data(), (size_t)w * 4 * sizeof(float))
           .readPixels(bm.pixmap()))
    return 0;
  // The pixmap door, not the image one: the sixteen bits a channel this
  // function exists to keep are the caller's choice, and a readback
  // would pick eight.
  const sk_sp<SkData> png = image::encodeImage(bm.pixmap(), image::Format::Png);
  if (!png || !io::writeBytes(path, png->data(), png->size())) return 0;
  return peak;
}

}  // namespace

std::string Writer::environmentMap(std::string_view name,
                                   const world::Environment& environment,
                                   const glm::mat3& orientation,
                                   std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage || !environment.valid()) return {};
  const std::string path = impl.uniquePath(parent, name);
  UsdLuxDomeLight dome = UsdLuxDomeLight::Define(impl.stage, SdfPath(path));

  const std::filesystem::path dir = impl.file.parent_path() / impl.textureDir();
  if (!impl.texturesDirReady) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    impl.texturesDirReady = true;
  }
  const std::string leaf =
      std::to_string(++impl.textureCounter) + "_environment.png";
  const float peak = writePanorama(environment.map.image(0), dir / leaf);
  if (peak > 0)
    dome.CreateTextureFileAttr().Set(
        SdfAssetPath((impl.textureDir() / leaf).generic_string()));
  // Every consumer of a dome light reads its texture as a lat-long
  // panorama; saying so is one attribute and saves a reader guessing
  // from the aspect ratio.
  dome.CreateTextureFormatAttr().Set(TfToken("latlong"));
  dome.CreateIntensityAttr().Set(environment.intensity *
                                 (peak > 0 ? peak : 1.0f));
  dome.CreateColorAttr().Set(
      GfVec3f(environment.tint.x, environment.tint.y, environment.tint.z));

  // The ROTATION of the placement is the whole of what a panorama can be
  // told: it is sampled by a direction, so a translation means nothing
  // to it. What the frame carries is the inverse — world into the
  // panorama's frame — and a prim's transform runs the other way.
  const glm::mat3 place = glm::inverse(orientation);
  const GfMatrix3d basis(place[0][0], place[0][1], place[0][2], place[1][0],
                         place[1][1], place[1][2], place[2][0], place[2][1],
                         place[2][2]);
  dome.AddOrientOp().Set(
      GfQuatf(GfRotation(GfMatrix4d(basis, GfVec3d(0, 0, 0)).ExtractRotation())
                  .GetQuat()));

  // The dials UsdLux has no word for ride as custom data rather than
  // being folded into the two it does: a reader that knows this writer
  // gets the set back as it was described, and one that does not still
  // gets a dome light that is right about its sky.
  UsdPrim prim = dome.GetPrim();
  const auto note = [&prim](const char* key, float value) {
    prim.SetCustomDataByKey(TfToken(key), VtValue(value));
  };
  note("sigil:diffuse", environment.diffuse);
  note("sigil:specular", environment.specular);
  note("sigil:roughnessBias", environment.roughnessBias);
  note("sigil:backdrop", environment.backdrop.intensity);
  note("sigil:backdropBlur", environment.backdrop.blur);
  note("sigil:groundRadius", environment.backdrop.groundRadius);
  return path;
}

}  // namespace sigil::usd
