/** @file
 * The Skia-codec backend of the channel read: the first frame's
 * premultiplied N32 pixels normalized to 0..1 floats named R/G/B/A.
 */

#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkPixmap.h>

#include "Backends.h"

namespace sigil::image::backend {

std::optional<ChannelData> decodeChannelsWithSkia(const std::byte* bytes,
                                                  size_t size) {
  // LDR web formats: decode through the Skia path and normalize the
  // premultiplied N32 pixels to 0..1 floats named R/G/B/A.
  if (auto asset = ImageAsset::decode(SkData::MakeWithoutCopy(bytes, size))) {
    const sk_sp<SkImage>& image = asset->frames().front().image;
    SkPixmap pixmap;
    if (!image->peekPixels(&pixmap)) return std::nullopt;
    ChannelData channels;
    channels.width = image->width();
    channels.height = image->height();
    channels.names = {"R", "G", "B", "A"};
    channels.data.resize((size_t)channels.width * channels.height * 4);
    for (int y = 0; y < channels.height; ++y)
      for (int x = 0; x < channels.width; ++x) {
        const SkColor color = pixmap.getColor(x, y);
        float* dst =
            channels.data.data() + ((size_t)y * channels.width + x) * 4;
        dst[0] = SkColorGetR(color) / 255.0f;
        dst[1] = SkColorGetG(color) / 255.0f;
        dst[2] = SkColorGetB(color) / 255.0f;
        dst[3] = SkColorGetA(color) / 255.0f;
      }
    return channels;
  }
  return std::nullopt;
}

}  // namespace sigil::image::backend
