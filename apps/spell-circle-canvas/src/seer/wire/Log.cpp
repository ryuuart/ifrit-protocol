/** @file
 * Draining a feed into the log, and letting go of what no longer fits.
 */

#include "sigilseer/wire/Log.h"

#include <optional>
#include <utility>

namespace sigil::seer {

Log::Log(size_t capacity) : m_capacity(capacity) {}

size_t Log::drain(io::Feed& feed) {
  size_t taken = 0;
  while (std::optional<io::Message> message = feed.receive()) {
    ++taken;
    append(*message);
  }
  return taken;
}

void Log::append(const io::Message& message) {
  if (m_capacity == 0) {
    ++m_forgotten;
    return;
  }
  m_entries.push_back({message.arrivedAt().count(), message.revision(),
                       message.payload ? message.payload->size() : 0,
                       message.payload, message.sender()});
  if (m_entries.size() > m_capacity) {
    m_entries.pop_front();
    ++m_forgotten;
  }
}

void Log::clear() { m_entries.clear(); }

}  // namespace sigil::seer
