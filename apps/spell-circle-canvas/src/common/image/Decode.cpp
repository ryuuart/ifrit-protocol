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

/** Channel indices for the requested layer within `spec`: exact
 *  channel-name prefixes ("diffuse.R"), or the plain R/G/B/A (first
 *  four channels as fallback) when no layer is asked for. -1 = absent
 *  (alpha fills with 1, missing G/B repeat R for luminance sources). */
struct ChannelPick {
  int r = -1, g = -1, b = -1, a = -1;
  bool any() const { return r >= 0 || g >= 0 || b >= 0; }
};

ChannelPick pickChannels(const OIIO::ImageSpec &spec,
                         const std::string &layer) {
  ChannelPick pick;
  auto find = [&](std::initializer_list<const char *> names) {
    for (const char *name : names) {
      const std::string wanted =
          layer.empty() ? std::string(name) : layer + "." + name;
      for (int i = 0; i < spec.nchannels; ++i)
        if (spec.channel_name(i) == wanted)
          return i;
    }
    return -1;
  };
  pick.r = find({"R", "r", "red", "Y"});
  pick.g = find({"G", "g", "green"});
  pick.b = find({"B", "b", "blue"});
  pick.a = find({"A", "a", "alpha"});
  if (!pick.any() && layer.empty() && spec.nchannels > 0) {
    // No conventional names: take the leading channels positionally.
    pick.r = 0;
    pick.g = spec.nchannels > 1 ? 1 : -1;
    pick.b = spec.nchannels > 2 ? 2 : -1;
    pick.a = spec.nchannels > 3 ? 3 : -1;
  }
  return pick;
}

/** Finds the subimage whose name matches `layer` (multi-part EXR);
 *  -1 when layers live as channel prefixes in part 0 instead. */
int findSubimage(OIIO::ImageInput &input, const std::string &layer) {
  if (layer.empty())
    return -1;
  for (int index = 0; input.seek_subimage(index, 0); ++index) {
    const std::string name =
        input.spec().get_string_attribute("oiio:subimagename");
    if (name == layer)
      return index;
  }
  return -1;
}

std::optional<ImageAsset>
decodeWithOiio(const std::byte *bytes, size_t size,
               const DecodeOptions &options,
               const std::filesystem::path &pathHint) {
  OIIO::Filesystem::IOMemReader reader(
      const_cast<std::byte *>(bytes), size);
  auto input = OIIO::ImageInput::open(oiioName(pathHint), nullptr,
                                      &reader);
  if (!input) {
    (void)OIIO::geterror(); // consume: "not an image" is an answer
    return std::nullopt;
  }

  int subimage = findSubimage(*input, options.layer);
  std::string channelLayer = options.layer;
  if (subimage >= 0)
    channelLayer.clear(); // the part IS the layer; channels are plain
  else
    subimage = 0;
  if (!input->seek_subimage(subimage, 0))
    return std::nullopt;
  const OIIO::ImageSpec spec = input->spec();

  const ChannelPick pick = pickChannels(spec, channelLayer);
  if (!pick.any())
    return std::nullopt;

  // Everything reads as float; float sources stay float on the Skia
  // side (F32 raster) so HDR range survives, LDR sources go N32.
  const bool isFloat = spec.format == OIIO::TypeDesc::HALF ||
                       spec.format == OIIO::TypeDesc::FLOAT ||
                       spec.format == OIIO::TypeDesc::DOUBLE;
  const int w = spec.width, h = spec.height;
  std::vector<float> planes((size_t)w * h * spec.nchannels);
  if (!input->read_image(subimage, 0, 0, spec.nchannels,
                         OIIO::TypeDesc::FLOAT, planes.data()))
    return std::nullopt;

  std::vector<float> rgba((size_t)w * h * 4);
  for (size_t px = 0; px < (size_t)w * h; ++px) {
    const float *src = planes.data() + px * spec.nchannels;
    float *dst = rgba.data() + px * 4;
    const float r = pick.r >= 0 ? src[pick.r] : 0.0f;
    dst[0] = r;
    dst[1] = pick.g >= 0 ? src[pick.g] : r; // luminance repeats
    dst[2] = pick.b >= 0 ? src[pick.b] : r;
    dst[3] = pick.a >= 0 ? src[pick.a] : 1.0f;
  }

  // EXR carries associated (premultiplied) alpha; kPremul matches.
  const SkImageInfo info = SkImageInfo::Make(
      w, h, isFloat ? kRGBA_F32_SkColorType : kN32_SkColorType,
      kPremul_SkAlphaType);
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info))
    return std::nullopt;
  if (isFloat) {
    std::memcpy(bitmap.getPixels(), rgba.data(),
                rgba.size() * sizeof(float));
  } else {
    for (size_t px = 0; px < (size_t)w * h; ++px) {
      auto *dst = (uint8_t *)bitmap.getPixels() + px * 4;
      for (int c = 0; c < 4; ++c)
        dst[c] = (uint8_t)std::clamp(
            rgba[px * 4 + (size_t)c] * 255.0f + 0.5f, 0.0f, 255.0f);
    }
  }
  bitmap.setImmutable();
  return ImageAsset::wrap(bitmap.asImage());
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
  return decodeWithOiio(bytes, size, options, pathHint);
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
