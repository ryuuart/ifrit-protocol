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
  while (std::optional<io::Arrival> arrival = feed.receive()) {
    ++taken;
    append(*arrival);
  }
  return taken;
}

void Log::append(const io::Arrival& arrival) {
  if (m_capacity == 0) {
    ++m_forgotten;
    return;
  }
  m_entries.push_back({arrival.at, arrival.generation,
                       arrival.bytes ? arrival.bytes->bytes.size() : 0,
                       arrival.bytes, arrival.from});
  if (m_entries.size() > m_capacity) {
    m_entries.pop_front();
    ++m_forgotten;
  }
}

void Log::clear() { m_entries.clear(); }

}  // namespace sigil::seer
