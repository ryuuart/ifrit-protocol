/** @file
 * The decoder registry: the SigilImage decoders registered by the
 * constructor, and the lookup that answers a typed ask with the
 * decoder registered for its type.
 */

#include "sigilloader/hub/Hub.h"

namespace sigil::loader {

Hub::Hub() {
  registerDecoder<sigil::image::ImageAsset>(
      [](const Bytes& bytes, std::string_view hint) {
        return sigil::image::decodeImage(bytes.bytes.data(), bytes.bytes.size(),
                                         {}, std::filesystem::path(hint));
      });
  registerDecoder<sigil::image::ChannelData>([](const Bytes& bytes,
                                                std::string_view hint) {
    return sigil::image::decodeChannels(bytes.bytes.data(), bytes.bytes.size(),
                                        std::filesystem::path(hint));
  });
}

Hub::Redecode Hub::registeredDecoder(std::type_index type) const {
  const auto it = m_decoders.find(type);
  return it == m_decoders.end() ? Redecode{} : it->second;
}

}  // namespace sigil::loader
