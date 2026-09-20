#pragma once

/** @file
 * @ingroup video-decode
 * Asynchronous presentation over a bounded decoder worker pool. Decode work
 * never blocks the render thread; the last complete frame remains available
 * while a newer presentation time is in flight.
 */

#include <cstddef>
#include <memory>
#include <optional>

#include "sigilvideo/decode/Decode.h"

namespace sigil::video {

/** Many independent video clocks sharing a bounded decode queue.
 *
 *  A clip registers once: `add()` answers the handle a clip already holds,
 *  since one `Video` must never be decoded by two workers at once, and a
 *  clip may be added while presentation is running. `request()` coalesces
 *  repeated asks that fall inside the same source frame and newer times
 *  replace queued stale work. `frame()` is called on one render thread; it
 *  maps a completed native frame into that thread's Graphite recorder and
 *  never waits for a decoder. */
class Playback {
 public:
  /** What `add()` answers and every other verb is addressed by. */
  using Handle = size_t;

  /** What a presentation pool is constructed with. Left alone it sizes its
   *  worker pool from the host and composes on the system device. */
  struct Options {
    /** Unset chooses a bounded count from the host's hardware concurrency.
     *  Zero runs no worker at all: `request()` decodes on the calling
     *  thread before it returns, so a deterministic host — a plate, a
     *  test — reads the answer from the next `frame()`. */
    std::optional<size_t> workerThreads;
    /** Metal device behind the recorder passed to `frame()`. */
    void* metalDevice = nullptr;
  };

  Playback();
  /** Opens a pool under @p options. */
  explicit Playback(const Options& options);
  ~Playback();
  Playback(const Playback&) = delete;
  Playback& operator=(const Playback&) = delete;

  /** Registers @p video, or answers the handle it was registered under. */
  Handle add(std::shared_ptr<Video> video);
  /** Asks for the frame covering @p seconds on @p handle's clip and
   *  returns without waiting for it. */
  void request(Handle handle, double seconds);
  /** Whether @p handle has produced at least one presentation frame. */
  bool ready(Handle handle) const;
  /** The newest complete frame for @p handle, bound to @p recorder when the
   *  decoder produced a native surface. Never waits: while newer work is in
   *  flight this is the frame before it, and it is empty until the first
   *  one completes. */
  VideoFrame frame(Handle handle, skgpu::graphite::Recorder* recorder);
  /** How many clips are registered. */
  size_t size() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::video
