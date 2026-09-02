/** @file
 * The OpenImageIO encoder: RGBA scanline EXR written straight into
 * memory through an IOProxy, so nothing here touches a filesystem.
 * Compiled to nothing without SIGILIMAGE_HAS_OIIO_ENCODE.
 */

#ifdef SIGILIMAGE_HAS_OIIO_ENCODE

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <vector>

#include "Backends.h"

namespace sigil::image::backend {

sk_sp<SkData> encodeExrWithOiio(const SkPixmap& pixels) {
  const int w = pixels.width(), h = pixels.height();
  if (w <= 0 || h <= 0) return nullptr;

  // One interleaved RGBA float plane, whatever the source colour type:
  // EXR's channel model is float, so the conversion is the encode.
  std::vector<float> plane((size_t)w * h * 4);
  const SkImageInfo dst = SkImageInfo::Make(
      w, h, kRGBA_F32_SkColorType, pixels.alphaType(), pixels.refColorSpace());
  if (!pixels.readPixels(
          SkPixmap(dst, plane.data(), (size_t)w * 4 * sizeof(float)), 0, 0))
    return nullptr;

  std::vector<unsigned char> encoded;
  OIIO::Filesystem::IOVecOutput sink(encoded);
  // The name selects the writer plugin and nothing else — the proxy is
  // where the bytes actually land.
  auto output = OIIO::ImageOutput::create("resource.exr", &sink);
  if (!output) {
    (void)OIIO::geterror();  // consume: "no EXR writer" is an answer
    return nullptr;
  }

  // HALF is EXR's native storage and halves the file for the range a
  // rendered panorama actually carries; a caller who needs the full
  // float mantissa is asking for a different format.
  OIIO::ImageSpec spec(w, h, 4, OIIO::TypeDesc::HALF);
  spec.channelnames = {"R", "G", "B", "A"};
  spec.alpha_channel = 3;
  if (!output->open("resource.exr", spec)) {
    (void)OIIO::geterror();
    return nullptr;
  }
  const bool wrote = output->write_image(OIIO::TypeDesc::FLOAT, plane.data());
  const bool closed = output->close();
  if (!wrote || !closed) {
    (void)OIIO::geterror();
    return nullptr;
  }
  return SkData::MakeWithCopy(encoded.data(), encoded.size());
}

}  // namespace sigil::image::backend

#endif  // SIGILIMAGE_HAS_OIIO_ENCODE
