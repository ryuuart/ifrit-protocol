/** @file
 * The hub's construction and its decoder registry, keyed by the name a
 * meaning declares.
 */

#include "Caches.h"
#include "sigilio/hub/Hub.h"

namespace sigil::io {

Hub::Hub(HubOptions options)
    : m_caches(std::make_unique<Caches>()),
      m_networkCacheDirectory(std::move(options.network.cacheDirectory)),
      m_networkPolicy(options.network.policy),
      m_networkTransport(std::move(options.network.transport)),
      m_transportSchemes(std::move(options.transports)) {
  for (auto& [prefix, directory] : options.mounts)
    mount(prefix, std::move(directory));
}

Hub::~Hub() = default;

void Hub::setDecoder(const detail::Meaning& meaning, Redecode decode,
                     Configure configure) {
  const std::lock_guard lock(m_mutex);
  m_caches->decoders.insert_or_assign(
      std::string(meaning.name),
      Caches::Registered{std::string(meaning.type), std::move(decode),
                         std::move(configure)});
}

const Hub::Caches::Registered* Hub::Caches::registered(
    const detail::Meaning& meaning) const {
  const auto found = decoders.find(meaning.name);
  if (found == decoders.end() || found->second.type != meaning.type)
    return nullptr;
  return &found->second;
}

}  // namespace sigil::io
