#include "SceneSession.h"

#include <algorithm>
#include <cstring>

namespace spellcircle {

SceneUpdate SceneSession::ingest(const void* payload, size_t size,
                                 Clock::time_point receivedAt) {
  if (payload && !m_payload.empty() && size == m_payload.size() &&
      std::memcmp(payload, m_payload.data(), size) == 0) {
    recordArrival(receivedAt);
    return SceneUpdate::Unchanged;
  }

  const std::optional<SceneStats> decoded = m_document.decode(payload, size);
  if (!decoded) return SceneUpdate::Invalid;

  const auto* bytes = static_cast<const uint8_t*>(payload);
  m_payload.assign(bytes, bytes + size);
  m_stats = *decoded;
  ++m_generation;
  recordArrival(receivedAt);
  return SceneUpdate::Changed;
}

void SceneSession::recordArrival(Clock::time_point receivedAt) {
  // Receive timestamps carry the interval; processing several queued
  // packets in one owner-thread turn must not make that interval zero.
  if (!m_arrivals.empty() &&
      receivedAt - m_arrivals.back() >= std::chrono::seconds(2))
    m_arrivals.clear();
  const auto position =
      std::upper_bound(m_arrivals.begin(), m_arrivals.end(), receivedAt);
  m_arrivals.insert(position, receivedAt);
  const Clock::time_point oldest = m_arrivals.back() - std::chrono::seconds(1);
  while (m_arrivals.front() < oldest) m_arrivals.pop_front();
}

double SceneSession::packetRate(Clock::time_point now) const {
  if (m_arrivals.size() < 2 || now < m_arrivals.back() ||
      now - m_arrivals.back() >= std::chrono::seconds(2))
    return 0;
  const double elapsed =
      std::chrono::duration<double>(m_arrivals.back() - m_arrivals.front())
          .count();
  return elapsed > 0 ? static_cast<double>(m_arrivals.size() - 1) / elapsed : 0;
}

void SceneSession::clear() {
  if (!m_payload.empty()) ++m_generation;
  m_document.clear();
  m_stats = {};
  m_payload.clear();
  m_arrivals.clear();
}

}  // namespace spellcircle
