#pragma once

/** @file
 * Asynchronous presentation over a bounded decoder worker pool. Decode work
 * never blocks the render thread; the last complete frame remains available
 * while a newer presentation time is in flight.
 */

#include <cstddef>
#include <memory>

#include "sigilvideo/decode/Decode.h"

namespace sigil::video {

/** Many independent video clocks sharing a bounded decode queue.
 *
 *  Register players before presentation begins. `request()` coalesces repeated
 *  asks that fall inside the same source frame and newer times replace queued
 *  stale work. `frame()` is called on one render thread; it maps a completed
 *  native frame into that thread's Graphite recorder and never waits for a
 *  decoder. */
class Playback {
 public:
  using Handle = size_t;

  struct Options {
    /** Zero chooses a bounded count from the host's hardware concurrency. */
    size_t workerThreads = 0;
    /** Metal device behind the recorder passed to `frame()`. */
    void* metalDevice = nullptr;
  };

  Playback();
  explicit Playback(const Options& options);
  ~Playback();
  Playback(const Playback&) = delete;
  Playback& operator=(const Playback&) = delete;

  Handle add(std::shared_ptr<Video> video);
  void request(Handle handle, double seconds);
  /** Whether @p handle has produced at least one presentation frame. */
  bool ready(Handle handle) const;
  VideoFrame frame(Handle handle, skgpu::graphite::Recorder* recorder);
  size_t size() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::video
