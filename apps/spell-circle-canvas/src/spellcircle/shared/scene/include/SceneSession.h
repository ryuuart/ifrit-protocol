#pragma once

/** @file Accepted scene state and arrival accounting shared by receiver hosts.
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "SceneModel.h"

namespace spellcircle {

enum class SceneUpdate { Invalid, Unchanged, Changed };

/** A receiver's scene state, used synchronously on its owning thread.
 *  Invalid packets preserve all state. Repeated accepted bytes count as
 *  arrivals but preserve the document and its generation. */
class SceneSession {
 public:
  using Clock = std::chrono::steady_clock;

  /** Accepts a datagram and its receive time. Capture @p receivedAt when
   *  the transport receives the packet, before dispatching to the owner
   *  thread, so queue delays cannot change the reported arrival rate. */
  SceneUpdate ingest(const void* payload, size_t size,
                     Clock::time_point receivedAt = Clock::now());

  const SceneDocument& document() const { return m_document; }
  const SceneStats& stats() const { return m_stats; }
  uint64_t generation() const { return m_generation; }
  bool hasScene() const { return m_stats.hasGeometry(); }

  /** Accepted packets per second over the latest second of receive times.
   *  At least two distinct timestamps are needed. The last measured rate
   *  is held until two seconds after the latest arrival, then returns zero. */
  double packetRate(Clock::time_point now = Clock::now()) const;

  /** Removes the accepted scene, deduplication state and arrival history.
   *  Clearing an accepted scene advances the generation, including when
   *  that scene had no geometry. Clearing an already cleared session does
   *  not advance it. */
  void clear();

 private:
  void recordArrival(Clock::time_point receivedAt);

  SceneDocument m_document;
  SceneStats m_stats;
  uint64_t m_generation = 0;
  std::vector<uint8_t> m_payload;
  std::deque<Clock::time_point> m_arrivals;
};

}  // namespace spellcircle
