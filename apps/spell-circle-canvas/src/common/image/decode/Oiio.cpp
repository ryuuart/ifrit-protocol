/** @file
 * The OpenImageIO backend: every channel of every same-sized part read
 * into one interleaved ChannelData, and the probe that lists layers
 * from channel prefixes and named parts. Compiled to nothing without
 * SIGILIMAGE_HAS_OIIO.
 */

#ifdef SIGILIMAGE_HAS_OIIO

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "Backends.h"

namespace sigil::image::backend {

namespace {

/** OIIO reads by name hint + bytes; give it the real filename when we
 *  have one so format detection is exact. */
std::string oiioName(const std::filesystem::path& pathHint) {
  return pathHint.empty() ? std::string("resource")
                          : pathHint.filename().string();
}

}  // namespace

/** Reads EVERY channel of the source as ChannelData: subimage 0's
 *  channels under their own names, plus any named same-size part's
 *  channels prefixed "part." (multi-part EXR layers become uniform
 *  with channel-prefix layers).
 *
 *  A DDS cube map arrives as OpenImageIO presents it: one subimage
 *  holding the six faces stacked into a column, +x -x +y -y +z -z from
 *  the top, at the base mip level — the same column the KTX reader
 *  builds, so a cube sheet reads by its aspect ratio whichever
 *  container it came from. */
std::optional<ChannelData> decodeChannelsWithOiio(
    const std::byte* bytes, size_t size,
    const std::filesystem::path& pathHint) {
  OIIO::Filesystem::IOMemReader reader(const_cast<std::byte*>(bytes), size);
  auto input = OIIO::ImageInput::open(oiioName(pathHint), nullptr, &reader);
  if (!input) {
    (void)OIIO::geterror();  // consume: "not an image" is an answer
    return std::nullopt;
  }

  ChannelData channels;
  for (int part = 0; input->seek_subimage(part, 0); ++part) {
    const OIIO::ImageSpec spec = input->spec();
    if (part == 0) {
      channels.width = spec.width;
      channels.height = spec.height;
    } else if (spec.width != channels.width || spec.height != channels.height) {
      continue;  // differently-sized parts can't interleave
    }
    channels.floatingPoint |= spec.format == OIIO::TypeDesc::HALF ||
                              spec.format == OIIO::TypeDesc::FLOAT ||
                              spec.format == OIIO::TypeDesc::DOUBLE;
    std::string partName;
    if (part != 0)
      partName = std::string(spec.get_string_attribute("oiio:subimagename"));
    std::vector<float> plane((size_t)spec.width * spec.height * spec.nchannels);
    if (!input->read_image(part, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT,
                           plane.data()))
      continue;
    const size_t oldCount = channels.names.size();
    for (int c = 0; c < spec.nchannels; ++c)
      channels.names.push_back(partName.empty()
                                   ? std::string(spec.channel_name(c))
                                   : partName + "." +
                                         std::string(spec.channel_name(c)));
    // Re-interleave into the combined layout.
    const size_t newCount = channels.names.size();
    std::vector<float> combined((size_t)channels.width * channels.height *
                                newCount);
    const size_t pixels = (size_t)channels.width * channels.height;
    for (size_t px = 0; px < pixels; ++px) {
      float* dst = combined.data() + px * newCount;
      if (oldCount)
        std::memcpy(dst, channels.data.data() + px * oldCount,
                    oldCount * sizeof(float));
      for (int c = 0; c < spec.nchannels; ++c)
        dst[oldCount + (size_t)c] = plane[px * spec.nchannels + (size_t)c];
    }
    channels.data = std::move(combined);
  }
  if (channels.names.empty()) return std::nullopt;
  return channels;
}

std::optional<ImageProbe> probeWithOiio(const std::byte* bytes, size_t size,
                                        const std::filesystem::path& pathHint) {
  OIIO::Filesystem::IOMemReader reader(const_cast<std::byte*>(bytes), size);
  auto input = OIIO::ImageInput::open(oiioName(pathHint), nullptr, &reader);
  if (!input) {
    (void)OIIO::geterror();  // consume: "not an image" is an answer
    return std::nullopt;
  }
  const OIIO::ImageSpec& spec = input->spec();
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
    if (!name.empty() && std::find(info.layers.begin(), info.layers.end(),
                                   name) == info.layers.end())
      info.layers.push_back(name);
  }
  return info;
}

}  // namespace sigil::image::backend

#endif  // SIGILIMAGE_HAS_OIIO
