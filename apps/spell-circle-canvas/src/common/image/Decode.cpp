#include "sigilimage/Decode.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>

#ifdef SIGILIMAGE_HAS_OIIO
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#endif

#include <algorithm>
#include <cstring>

namespace sigil::image {

namespace {

#ifdef SIGILIMAGE_HAS_OIIO

/** OIIO reads by name hint + bytes; give it the real filename when we
 *  have one so format detection is exact. */
std::string oiioName(const std::filesystem::path &pathHint) {
  return pathHint.empty() ? std::string("resource")
                          : pathHint.filename().string();
}

/** Reads EVERY channel of the source as ChannelData: subimage 0's
 *  channels under their own names, plus any named same-size part's
 *  channels prefixed "part." (multi-part EXR layers become uniform
 *  with channel-prefix layers). */
std::optional<ChannelData>
decodeChannelsWithOiio(const std::byte *bytes, size_t size,
                       const std::filesystem::path &pathHint) {
  OIIO::Filesystem::IOMemReader reader(
      const_cast<std::byte *>(bytes), size);
  auto input = OIIO::ImageInput::open(oiioName(pathHint), nullptr,
                                      &reader);
  if (!input) {
    (void)OIIO::geterror(); // consume: "not an image" is an answer
    return std::nullopt;
  }

  ChannelData channels;
  for (int part = 0; input->seek_subimage(part, 0); ++part) {
    const OIIO::ImageSpec spec = input->spec();
    if (part == 0) {
      channels.width = spec.width;
      channels.height = spec.height;
    } else if (spec.width != channels.width ||
               spec.height != channels.height) {
      continue; // differently-sized parts can't interleave
    }
    channels.floatingPoint |= spec.format == OIIO::TypeDesc::HALF ||
                              spec.format == OIIO::TypeDesc::FLOAT ||
                              spec.format == OIIO::TypeDesc::DOUBLE;
    std::string partName;
    if (part != 0)
      partName =
          std::string(spec.get_string_attribute("oiio:subimagename"));
    std::vector<float> plane((size_t)spec.width * spec.height *
                             spec.nchannels);
    if (!input->read_image(part, 0, 0, spec.nchannels,
                           OIIO::TypeDesc::FLOAT, plane.data()))
      continue;
    const size_t oldCount = channels.names.size();
    for (int c = 0; c < spec.nchannels; ++c)
      channels.names.push_back(
          partName.empty()
              ? std::string(spec.channel_name(c))
              : partName + "." + std::string(spec.channel_name(c)));
    // Re-interleave into the combined layout.
    const size_t newCount = channels.names.size();
    std::vector<float> combined((size_t)channels.width *
                                channels.height * newCount);
    const size_t pixels = (size_t)channels.width * channels.height;
    for (size_t px = 0; px < pixels; ++px) {
      float *dst = combined.data() + px * newCount;
      if (oldCount)
        std::memcpy(dst, channels.data.data() + px * oldCount,
                    oldCount * sizeof(float));
      for (int c = 0; c < spec.nchannels; ++c)
        dst[oldCount + (size_t)c] =
            plane[px * spec.nchannels + (size_t)c];
    }
    channels.data = std::move(combined);
  }
  if (channels.names.empty())
    return std::nullopt;
  return channels;
}

std::optional<ImageProbe>
probeWithOiio(const std::byte *bytes, size_t size,
              const std::filesystem::path &pathHint) {
  OIIO::Filesystem::IOMemReader reader(
      const_cast<std::byte *>(bytes), size);
  auto input = OIIO::ImageInput::open(oiioName(pathHint), nullptr,
                                      &reader);
  if (!input) {
    (void)OIIO::geterror(); // consume: "not an image" is an answer
    return std::nullopt;
  }
  const OIIO::ImageSpec &spec = input->spec();
  ImageProbe info;
  info.format = input->format_name();
  info.width = spec.width;
  info.height = spec.height;
  info.channels = spec.nchannels;
  info.floatingPoint = spec.format == OIIO::TypeDesc::HALF ||
                       spec.format == OIIO::TypeDesc::FLOAT ||
                       spec.format == OIIO::TypeDesc::DOUBLE;
  for (int i = 0; i < spec.nchannels; ++i) {
    const std::string name = spec.channel_name(i);
    info.channelNames.push_back(name);
    // Channel-prefix layers: "diffuse.R" contributes layer "diffuse".
    if (const size_t dot = name.rfind('.'); dot != std::string::npos) {
      const std::string layer = name.substr(0, dot);
      if (std::find(info.layers.begin(), info.layers.end(), layer) ==
          info.layers.end())
        info.layers.push_back(layer);
    }
  }
  // Multi-part layers (named subimages).
  for (int index = 1; input->seek_subimage(index, 0); ++index) {
    const std::string name =
        input->spec().get_string_attribute("oiio:subimagename");
    if (!name.empty() &&
        std::find(info.layers.begin(), info.layers.end(), name) ==
            info.layers.end())
      info.layers.push_back(name);
  }
  return info;
}

#endif // SIGILIMAGE_HAS_OIIO

} // namespace

std::optional<ImageAsset>
decodeImage(const std::byte *bytes, size_t size,
            const DecodeOptions &options,
            const std::filesystem::path &pathHint) {
  // Layer selection is an OIIO concept; the Skia-codec path handles
  // the web formats (and their animation) best, so it goes first
  // otherwise. SkCodec sniffs bytes and fails fast on foreign formats.
  if (options.layer.empty()) {
    if (auto asset = ImageAsset::decode(
            SkData::MakeWithoutCopy(bytes, size)))
      return asset;
  }
#ifdef SIGILIMAGE_HAS_OIIO
  if (auto channels = decodeChannelsWithOiio(bytes, size, pathHint))
    if (sk_sp<SkImage> image = channels->makeImage(options.layer))
      return ImageAsset::wrap(std::move(image));
  return std::nullopt;
#else
  (void)pathHint;
  return std::nullopt;
#endif
}

int ChannelData::index(std::string_view name) const {
  for (size_t i = 0; i < names.size(); ++i)
    if (names[i] == name)
      return (int)i;
  return -1;
}

sk_sp<SkImage> ChannelData::makeImage(std::string_view layer) const {
  auto find = [&](std::initializer_list<const char *> candidates) {
    for (const char *candidate : candidates) {
      const std::string wanted =
          layer.empty() ? std::string(candidate)
                        : std::string(layer) + "." + candidate;
      if (int i = index(wanted); i >= 0)
        return i;
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
  if (r < 0 && g < 0 && b < 0)
    return nullptr;
  return makeImage(r, g, b, a);
}

sk_sp<SkImage> ChannelData::makeImage(int r, int g, int b, int a) const {
  const size_t stride = names.size();
  const size_t pixels = (size_t)width * height;
  std::vector<float> rgba(pixels * 4);
  for (size_t px = 0; px < pixels; ++px) {
    const float *src = data.data() + px * stride;
    float *dst = rgba.data() + px * 4;
    const float red = r >= 0 ? src[r] : 0.0f;
    dst[0] = red;
    dst[1] = g >= 0 ? src[g] : red; // luminance repeats
    dst[2] = b >= 0 ? src[b] : red;
    dst[3] = a >= 0 ? src[a] : 1.0f;
  }
  const SkImageInfo info = SkImageInfo::Make(
      width, height,
      floatingPoint ? kRGBA_F32_SkColorType : kN32_SkColorType,
      kPremul_SkAlphaType);
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info))
    return nullptr;
  if (floatingPoint) {
    std::memcpy(bitmap.getPixels(), rgba.data(),
                rgba.size() * sizeof(float));
  } else {
    for (size_t px = 0; px < pixels; ++px) {
      auto *dst = (uint8_t *)bitmap.getPixels() + px * 4;
      for (int c = 0; c < 4; ++c)
        dst[c] = (uint8_t)std::clamp(
            rgba[px * 4 + (size_t)c] * 255.0f + 0.5f, 0.0f, 255.0f);
    }
  }
  bitmap.setImmutable();
  return bitmap.asImage();
}

std::optional<ChannelData>
decodeChannels(const std::byte *bytes, size_t size,
               const std::filesystem::path &pathHint) {
  // LDR web formats: decode through the Skia path and normalize the
  // premultiplied N32 pixels to 0..1 floats named R/G/B/A.
  if (auto asset = ImageAsset::decode(
          SkData::MakeWithoutCopy(bytes, size))) {
    const sk_sp<SkImage> &image = asset->frames().front().image;
    SkPixmap pixmap;
    if (!image->peekPixels(&pixmap))
      return std::nullopt;
    ChannelData channels;
    channels.width = image->width();
    channels.height = image->height();
    channels.names = {"R", "G", "B", "A"};
    channels.data.resize((size_t)channels.width * channels.height * 4);
    for (int y = 0; y < channels.height; ++y)
      for (int x = 0; x < channels.width; ++x) {
        const SkColor color = pixmap.getColor(x, y);
        float *dst = channels.data.data() +
                     ((size_t)y * channels.width + x) * 4;
        dst[0] = SkColorGetR(color) / 255.0f;
        dst[1] = SkColorGetG(color) / 255.0f;
        dst[2] = SkColorGetB(color) / 255.0f;
        dst[3] = SkColorGetA(color) / 255.0f;
      }
    return channels;
  }
#ifdef SIGILIMAGE_HAS_OIIO
  return decodeChannelsWithOiio(bytes, size, pathHint);
#else
  (void)pathHint;
  return std::nullopt;
#endif
}

std::optional<ImageProbe>
probeImage(const std::byte *bytes, size_t size,
           const std::filesystem::path &pathHint) {
  if (auto sniff = ImageAsset::probe(SkData::MakeWithoutCopy(bytes, size)))
    return sniff; // Skia path: web formats, channels stay the N32 four
#ifdef SIGILIMAGE_HAS_OIIO
  return probeWithOiio(bytes, size, pathHint);
#else
  (void)pathHint;
  return std::nullopt;
#endif
}

} // namespace sigil::image
