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
    if (m_capacity == 0) {
      ++m_forgotten;
      continue;
    }
    LogEntry entry;
    entry.at = arrival->at;
    entry.generation = arrival->generation;
    entry.size = arrival->bytes ? arrival->bytes->bytes.size() : 0;
    entry.bytes = std::move(arrival->bytes);
    entry.from = std::move(arrival->from);
    m_entries.push_back(std::move(entry));
    if (m_entries.size() > m_capacity) {
      m_entries.pop_front();
      ++m_forgotten;
    }
  }
  return taken;
}

void Log::clear() { m_entries.clear(); }

}  // namespace sigil::seer
