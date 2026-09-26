#pragma once

/** @file
 * The pool's half of a video: one slot per video opened on it, the newest
 * frame each slot's worker finished, and a queue of slots with an ask
 * outstanding that a bounded set of workers drains. Private to the decode
 * feature.
 */

#include <moodycamel/blockingconcurrentqueue.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

#include "DecodeInternal.h"

namespace sigil::media {

struct Playback::Impl {
  using JobQueue = moodycamel::BlockingConcurrentQueue<size_t>;

  struct Slot {
    explicit Slot(std::weak_ptr<Video::Decoder> source)
        : decoder(std::move(source)) {}

    std::weak_ptr<Video::Decoder> decoder;
    std::mutex mutex;
    double requestedSeconds = 0.0;
    size_t generation = 0;
    bool queued = false;
    bool decoding = false;
    Frame latest;
  };

  explicit Impl(const Options& options);
  ~Impl();

  /** Registers @p decoder and answers its slot. */
  size_t add(const std::shared_ptr<Video::Decoder>& decoder);
  /** Asks for the frame covering @p seconds on @p slot's clip and answers
   *  the newest one finished, never waiting — unless the pool has no
   *  worker, when the ask is decoded before it returns. */
  Frame request(size_t slot, double seconds);
  /** Whether @p slot has produced a frame. */
  bool ready(size_t slot) const;

  std::shared_ptr<Slot> slot(size_t index) const;
  void pushRequest(size_t index);
  /** One dequeued job: decode the slot's newest ask and publish it; true
   *  when a newer ask arrived meanwhile and the slot goes round again. */
  bool decodeOnce(Slot& current);
  void work(std::stop_token stop);

  JobQueue jobs;
  JobQueue::producer_token_t requestProducer{jobs};
  std::mutex requestProducerMutex;
  mutable std::mutex slotsMutex;
  std::vector<std::shared_ptr<Slot>> slots;
  std::vector<std::jthread> threads;
  bool synchronous = false;
};

}  // namespace sigil::media
