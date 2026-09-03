/** @file
 * The OpenImageIO encoder: scanline EXR written straight into memory
 * through an IOProxy, so nothing here touches a filesystem — RGBA from
 * a pixmap, or every channel under the name it carries. Compiled to
 * nothing without SIGILIMAGE_HAS_OIIO_ENCODE.
 */

#ifdef SIGILIMAGE_HAS_OIIO_ENCODE

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <sigilimage/decode/ChannelData.h>

#include <string>
#include <vector>

#include "Backends.h"

namespace sigil::image::backend {

namespace {

/** The one writer this file opens, and where its bytes land. The name
 *  selects the plugin and nothing else. */
std::unique_ptr<OIIO::ImageOutput> exrWriter(OIIO::Filesystem::IOVecOutput& sink) {
  auto output = OIIO::ImageOutput::create("resource.exr", &sink);
  if (!output) (void)OIIO::geterror();  // consume: "no EXR writer" is an answer
  return output;
}

/** The index of the channel named exactly @p name, or -1. Spelled here
 *  rather than through ChannelData's own lookup so the encode feature
 *  keeps linking nothing of the decode one. */
int channelNamed(const ChannelData& channels, const char* name) {
  for (size_t i = 0; i < channels.names.size(); ++i)
    if (channels.names[i] == name) return (int)i;
  return -1;
}

}  // namespace

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
  auto output = exrWriter(sink);
  if (!output) return nullptr;

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

sk_sp<SkData> encodeExrChannelsWithOiio(const ChannelData& channels) {
  const int w = channels.width, h = channels.height;
  const int count = (int)channels.names.size();
  if (w <= 0 || h <= 0 || count <= 0) return nullptr;
  // The names ARE the layout: one plane per name, interleaved. A value
  // whose two halves disagree describes no file.
  if (channels.data.size() != (size_t)w * h * count) return nullptr;

  std::vector<unsigned char> encoded;
  OIIO::Filesystem::IOVecOutput sink(encoded);
  auto output = exrWriter(sink);
  if (!output) return nullptr;

  // HALF, exactly as the RGBA door writes: it is EXR's native storage,
  // and a caller who needs the full float mantissa is asking for a
  // different format.
  OIIO::ImageSpec spec(w, h, count, OIIO::TypeDesc::HALF);
  spec.channelnames = channels.names;
  // WHICH CHANNEL MEANS WHAT, where the names say so. EXR reads "A" as
  // coverage and "Z" as depth by convention, and a reader that knows
  // that is told rather than left to guess; a set naming neither says
  // -1, which is "none of them", and not 0, which would name the first.
  spec.alpha_channel = channelNamed(channels, "A");
  spec.z_channel = channelNamed(channels, "Z");
  if (!output->open("resource.exr", spec)) {
    (void)OIIO::geterror();
    return nullptr;
  }
  const bool wrote =
      output->write_image(OIIO::TypeDesc::FLOAT, channels.data.data());
  const bool closed = output->close();
  if (!wrote || !closed) {
    (void)OIIO::geterror();
    return nullptr;
  }
  return SkData::MakeWithCopy(encoded.data(), encoded.size());
}

}  // namespace sigil::image::backend

#endif  // SIGILIMAGE_HAS_OIIO_ENCODE
