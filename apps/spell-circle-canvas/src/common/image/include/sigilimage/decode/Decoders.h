#pragma once

/** @file
 * @ingroup image-decode
 * THIS LIBRARY'S DECODERS, and the one call that puts them on a hub. A
 * hub answers a URI with bytes and hands those bytes to whatever decoder
 * is registered for the type asked for; after `registerDecoders(hub)` an
 * image is `load<ImageAsset>(uri)` on that hub — with this library's own
 * `DecodeOptions` as `load<ImageAsset>(uri, {.layer = "diffuse"})` — and
 * its float planes are `load<ChannelData>(uri)`, cached and reloaded like
 * anything else the hub holds. `registerDecoders` is a template over the
 * hub, so this library knows no resource library and depends on nothing
 * that one is made of.
 */

#include <filesystem>
#include <string_view>

#include "sigilimage/decode/ChannelData.h"
#include "sigilimage/decode/Decode.h"

namespace sigil::image {

/** Puts the ImageAsset decoder — every format decodeImage() routes, with
 *  the load's DecodeOptions — and the ChannelData decoder on @p hub. A
 *  host calls this once, wherever it builds its hub.
 *  @trap Registering a type again replaces the decoder later asks run, so
 *  a host wanting its own image decode registers it AFTERWARDS. */
template <typename Hub>
void registerDecoders(Hub& hub) {
  registerDecoder<ImageAsset>(hub,
      [](const auto& bytes, std::string_view hint,
         const DecodeOptions& options) {
        return decodeImage(bytes.data(), bytes.size(), options,
                           std::filesystem::path(hint));
      });
  registerDecoder<ChannelData>(hub,
      [](const auto& bytes, std::string_view hint) {
        return decodeChannels(bytes.data(), bytes.size(),
                              std::filesystem::path(hint));
      });
}

}  // namespace sigil::image
