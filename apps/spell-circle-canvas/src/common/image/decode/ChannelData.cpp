/** @file
 * ChannelData's compositing: a channel group found by name or by index,
 * luminance repeated across RGB and a missing alpha filled with 1, landing
 * as RGBA_F32 for float data and N32 otherwise.
 */

#include "sigilimage/decode/ChannelData.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>

#include <algorithm>
#include <cstring>
#include <initializer_list>

namespace sigil::image {

int ChannelData::index(std::string_view name) const {
  for (size_t i = 0; i < names.size(); ++i)
    if (names[i] == name) return (int)i;
  return -1;
}

sk_sp<SkImage> ChannelData::makeImage(std::string_view layer) const {
  auto find = [&](std::initializer_list<const char*> candidates) {
    for (const char* candidate : candidates) {
      const std::string wanted = layer.empty()
                                     ? std::string(candidate)
                                     : std::string(layer) + "." + candidate;
      if (int i = index(wanted); i >= 0) return i;
    }
    return -1;
  };
  int r = find({"R", "r", "red", "Y"});
  int g = find({"G", "g", "green"});
  int b = find({"B", "b", "blue"});
  int a = find({"A", "a", "alpha"});
  if (r < 0 && g < 0 && b < 0 && layer.empty() && !names.empty()) {
    r = 0;
    g = names.size() > 1 ? 1 : -1;
    b = names.size() > 2 ? 2 : -1;
    a = names.size() > 3 ? 3 : -1;
  }
  if (r < 0 && g < 0 && b < 0) return nullptr;
  return makeImage(r, g, b, a);
}

sk_sp<SkImage> ChannelData::makeImage(int r, int g, int b, int a) const {
  const size_t stride = names.size();
  const size_t pixels = (size_t)width * height;
  std::vector<float> rgba(pixels * 4);
  for (size_t px = 0; px < pixels; ++px) {
    const float* src = data.data() + px * stride;
    float* dst = rgba.data() + px * 4;
    const float red = r >= 0 ? src[r] : 0.0f;
    dst[0] = red;
    dst[1] = g >= 0 ? src[g] : red;  // luminance repeats
    dst[2] = b >= 0 ? src[b] : red;
    dst[3] = a >= 0 ? src[a] : 1.0f;
  }
  const SkImageInfo info = SkImageInfo::Make(
      width, height, floatingPoint ? kRGBA_F32_SkColorType : kN32_SkColorType,
      kPremul_SkAlphaType);
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info)) return nullptr;
  if (floatingPoint) {
    std::memcpy(bitmap.getPixels(), rgba.data(), rgba.size() * sizeof(float));
  } else {
    for (size_t px = 0; px < pixels; ++px) {
      auto* dst = (uint8_t*)bitmap.getPixels() + px * 4;
      for (int c = 0; c < 4; ++c)
        dst[c] = (uint8_t)std::clamp(rgba[px * 4 + (size_t)c] * 255.0f + 0.5f,
                                     0.0f, 255.0f);
    }
  }
  bitmap.setImmutable();
  return bitmap.asImage();
}

}  // namespace sigil::image
