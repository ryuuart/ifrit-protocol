/** @file
 * Channels' compositing: a channel group found by name or by index,
 * luminance repeated across RGB and a missing alpha filled with 1, landing
 * as RGBA_F32 for float data and N32 otherwise.
 */

#include "sigilmedia/image/Channels.h"
#include "sigilmedia/advanced/Skia.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "Backends.h"
#include "sigilmedia/core/Image.h"

namespace sigil::media {

namespace {

/** The channels @p pick names as one picture: float data as RGBA float,
 *  LDR as N32. A channel the data does not carry reads as missing,
 *  whether the caller said so with -1 or named an index past the
 *  channels there are: a picture with a hole in it beats a read past
 *  the end. */
sk_sp<SkImage> pickChannels(const Channels& channels, ChannelPick pick) {
  const size_t stride = channels.names.size();
  const size_t pixels = (size_t)channels.width * channels.height;
  std::vector<float> rgba(pixels * 4);
  const auto held = [stride](int channel) {
    return channel >= 0 && (size_t)channel < stride;
  };
  for (size_t pixel = 0; pixel < pixels; ++pixel) {
    const float* source = channels.data.data() + pixel * stride;
    float* destination = rgba.data() + pixel * 4;
    const float red = held(pick.red) ? source[pick.red] : 0.0f;
    destination[0] = red;
    destination[1] = held(pick.green) ? source[pick.green] : red;  // luminance repeats
    destination[2] = held(pick.blue) ? source[pick.blue] : red;
    destination[3] = held(pick.alpha) ? source[pick.alpha] : 1.0f;
  }
  const SkImageInfo info = SkImageInfo::Make(
      channels.width, channels.height,
      channels.floatingPoint ? kRGBA_F32_SkColorType : kN32_SkColorType,
      kPremul_SkAlphaType);
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info)) return nullptr;
  if (channels.floatingPoint) {
    std::memcpy(bitmap.getPixels(), rgba.data(), rgba.size() * sizeof(float));
  } else {
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
      auto* destination = (uint8_t*)bitmap.getPixels() + pixel * 4;
      for (int channel = 0; channel < 4; ++channel)
        destination[channel] = (uint8_t)std::clamp(
            rgba[pixel * 4 + (size_t)channel] * 255.0f + 0.5f, 0.0f, 255.0f);
    }
  }
  bitmap.setImmutable();
  return bitmap.asImage();
}

std::shared_ptr<const Image> documentOf(sk_sp<SkImage> picture) {
  if (!picture) return nullptr;
  return Image::of(std::move(picture));
}

}  // namespace

namespace backend {

sk_sp<SkImage> composite(const Channels& channels, std::string_view layer) {
  const auto find = [&](std::initializer_list<const char*> candidates) {
    for (const char* candidate : candidates) {
      const std::string wanted = layer.empty()
                                     ? std::string(candidate)
                                     : std::string(layer) + "." + candidate;
      if (const int found = channels.index(wanted); found >= 0) return found;
    }
    return -1;
  };
  ChannelPick pick{.red = find({"R", "r", "red", "Y"}),
                   .green = find({"G", "g", "green"}),
                   .blue = find({"B", "b", "blue"}),
                   .alpha = find({"A", "a", "alpha"})};
  const size_t count = channels.names.size();
  if (pick.red < 0 && pick.green < 0 && pick.blue < 0 && layer.empty() &&
      count > 0) {
    pick = {.red = 0,
            .green = count > 1 ? 1 : -1,
            .blue = count > 2 ? 2 : -1,
            .alpha = count > 3 ? 3 : -1};
  }
  if (pick.red < 0 && pick.green < 0 && pick.blue < 0) return nullptr;
  return pickChannels(channels, pick);
}

}  // namespace backend

int Channels::index(std::string_view name) const {
  for (size_t found = 0; found < names.size(); ++found)
    if (names[found] == name) return (int)found;
  return -1;
}

std::shared_ptr<const Image> Channels::image(std::string_view layer) const {
  return documentOf(backend::composite(*this, layer));
}

std::shared_ptr<const Image> Channels::image(ChannelPick pick) const {
  return documentOf(pickChannels(*this, pick));
}

}  // namespace sigil::media
