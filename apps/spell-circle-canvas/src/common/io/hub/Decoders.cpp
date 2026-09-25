/** @file
 * The hub's construction and its decoder registry: the lookup that
 * answers a typed ask with the decoder registered for its type, and the
 * one that binds a load's options into it.
 */

#include "Caches.h"
#include "sigilio/hub/Hub.h"

namespace sigil::io {

Hub::Hub() : m_caches(std::make_unique<Caches>()) {}

Hub::~Hub() = default;

void Hub::setDecoder(std::type_index type, Redecode decode,
                     Configure configure) {
  const std::lock_guard lock(m_mutex);
  m_caches->decoders[type] = std::move(decode);
  if (configure)
    m_caches->configured[type] = std::move(configure);
  else
    m_caches->configured.erase(type);
}

Hub::Redecode Hub::registeredDecoder(std::type_index type) const {
  const std::lock_guard lock(m_mutex);
  const auto it = m_caches->decoders.find(type);
  return it == m_caches->decoders.end() ? Redecode{} : it->second;
}

}  // namespace sigil::io
