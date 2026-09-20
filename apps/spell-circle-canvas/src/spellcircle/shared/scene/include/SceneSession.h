#pragma once

/** @file
 * Accepted scene state and arrival accounting shared by receiver hosts.
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "SceneModel.h"

/** THE RECEIVER PRODUCT: a scene description arriving as bytes, decoded,
 *  resolved into the geometry and labels it names, and drawn. The
 *  session that holds the accepted scene and counts what arrives, the
 *  document and statistics it answers with, the renderer that paints
 *  them, and the Qt items a host puts on screen. Reach for it to embed
 *  the receiver, or to add something the wire can describe. */
namespace spellcircle {

/** What one arriving datagram did to the session. `Invalid` is not a
 *  scene and leaves every piece of state standing; `Unchanged` is the
 *  scene already accepted, counted as an arrival but changing neither
 *  the document nor its generation; `Changed` is a new scene, which
 *  advances the generation. */
enum class SceneUpdate { Invalid, Unchanged, Changed };

/** A receiver's scene state, used synchronously on its owning thread.
 *  Invalid packets preserve all state. Repeated accepted bytes count as
 *  arrivals but preserve the document and its generation. */
class SceneSession {
 public:
  /** The clock every arrival is stamped on. */
  using Clock = std::chrono::steady_clock;

  /** Accepts a datagram and its receive time. Capture @p receivedAt when
   *  the transport receives the packet, before dispatching to the owner
   *  thread, so queue delays cannot change the reported arrival rate. */
  SceneUpdate ingest(const void* payload, size_t size,
                     Clock::time_point receivedAt = Clock::now());

  /** The scene as accepted; empty until one has been. */
  const SceneDocument& document() const { return m_document; }
  /** What the accepted scene holds, counted. */
  const SceneStats& stats() const { return m_stats; }
  /** A count that advances every time the document changes, so a
   *  drawing knows without comparing scenes. */
  uint64_t generation() const { return m_generation; }
  /** Whether the accepted scene holds anything to draw. */
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
