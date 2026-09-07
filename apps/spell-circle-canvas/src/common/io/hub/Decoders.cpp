/** @file
 * The hub's construction and its decoder registry: the SigilImage
 * decoders registered by the constructor, and the lookup that answers a
 * typed ask with the decoder registered for its type.
 */

#include "sigilio/hub/Hub.h"

namespace sigil::io {

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

Hub::~Hub() = default;

void Hub::setDecoder(std::type_index type, Redecode decode) {
  const std::lock_guard lock(m_mutex);
  m_decoders[type] = std::move(decode);
}

Hub::Redecode Hub::registeredDecoder(std::type_index type) const {
  const std::lock_guard lock(m_mutex);
  const auto it = m_decoders.find(type);
  return it == m_decoders.end() ? Redecode{} : it->second;
}

}  // namespace sigil::io
