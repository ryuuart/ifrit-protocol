/** @file
 * The resident set: which opened session the window is looking at, and
 * which one leaves when there is no room for another.
 */

#include "sigilsketch/live/Residency.h"

#include <algorithm>
#include <utility>

#include "sigilsketch/live/Host.h"

namespace sigil::sketch {

Residency::Residency(std::size_t capacity) noexcept
    // A set that holds nothing would open every sketch twice — once to
    // present it and once again the moment it was asked for.
    : m_capacity(std::max<std::size_t>(1, capacity)) {}

Residency::~Residency() = default;

Residency::Presented Residency::present(const std::string& key,
                                        const Open& open) {
  for (std::size_t i = 0; i < m_sessions.size(); ++i) {
    if (m_sessions[i].key != key) continue;
    Resident held = std::move(m_sessions[i]);
    m_sessions.erase(m_sessions.begin() + (std::ptrdiff_t)i);
    m_sessions.insert(m_sessions.begin(), std::move(held));
    // The stretch while something else held the window is not a frame
    // interval, so the next presentation starts one instead of
    // extending the one this session was paused in the middle of.
    if (m_sessions.front().host) m_sessions.front().host->resume();
    return {m_sessions.front().host.get(), false};
  }
  m_sessions.insert(m_sessions.begin(), Resident{key, open()});
  while (m_sessions.size() > m_capacity) m_sessions.pop_back();
  return {m_sessions.front().host.get(), true};
}

Host* Residency::presented() const {
  return m_sessions.empty() ? nullptr : m_sessions.front().host.get();
}

std::vector<std::string> Residency::keys() const {
  std::vector<std::string> out;
  out.reserve(m_sessions.size());
  for (const Resident& resident : m_sessions) out.push_back(resident.key);
  return out;
}

void Residency::clear() { m_sessions.clear(); }

}  // namespace sigil::sketch
