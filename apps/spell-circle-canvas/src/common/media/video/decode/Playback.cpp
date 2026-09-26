/** @file
 * The decode pool: workers draining a queue of slots, each decoding its
 * video's newest ask and publishing the frame, and the render thread's
 * side that asks and reads without ever waiting.
 */

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

#include "PlaybackInternal.h"

namespace sigil::media {

Playback::Impl::Impl(const Options& options) {
  size_t workers = 0;
  if (options.workers) {
    workers = *options.workers;
  } else {
    const unsigned concurrency = std::thread::hardware_concurrency();
    workers = std::clamp<size_t>(concurrency ? concurrency / 2 : 2, 2, 8);
  }
  synchronous = workers == 0;
  threads.reserve(workers);
  for (size_t index = 0; index < workers; ++index)
    threads.emplace_back([this](std::stop_token stop) { work(stop); });
}

Playback::Impl::~Impl() {
  for (std::jthread& thread : threads) thread.request_stop();
}

std::shared_ptr<Playback::Impl::Slot> Playback::Impl::slot(size_t index) const {
  std::lock_guard lock(slotsMutex);
  return index < slots.size() ? slots[index] : nullptr;
}

size_t Playback::Impl::add(const std::shared_ptr<Video::Decoder>& decoder) {
  std::lock_guard lock(slotsMutex);
  slots.push_back(std::make_shared<Slot>(decoder));
  return slots.size() - 1;
}

void Playback::Impl::pushRequest(size_t index) {
  std::lock_guard lock(requestProducerMutex);
  if (!jobs.enqueue(requestProducer, index)) std::terminate();
}

bool Playback::Impl::decodeOnce(Slot& current) {
  double seconds = 0.0;
  size_t generation = 0;
  {
    std::lock_guard lock(current.mutex);
    current.queued = false;
    current.decoding = true;
    seconds = current.requestedSeconds;
    generation = current.generation;
  }
  // A video let go while its ask waited is simply not decoded.
  Frame decoded;
  if (const std::shared_ptr<Video::Decoder> decoder = current.decoder.lock())
    decoded = decoder->frameAt(seconds);

  std::lock_guard lock(current.mutex);
  if (decoded) current.latest = std::move(decoded);
  current.decoding = false;
  if (current.generation != generation && !current.queued &&
      !current.decoder.expired()) {
    current.queued = true;
    return true;
  }
  return false;
}

void Playback::Impl::work(std::stop_token stop) {
  JobQueue::producer_token_t producer(jobs);
  JobQueue::consumer_token_t consumer(jobs);
  while (!stop.stop_requested()) {
    size_t index = 0;
    if (!jobs.wait_dequeue_timed(consumer, index, std::chrono::milliseconds(10)))
      continue;
    const std::shared_ptr<Slot> current = slot(index);
    if (!current) continue;
    if (decodeOnce(*current) && !jobs.enqueue(producer, index)) std::terminate();
  }
}

Frame Playback::Impl::request(size_t index, double seconds) {
  const std::shared_ptr<Slot> current = slot(index);
  if (!current) return {};
  bool enqueue = false;
  {
    std::lock_guard lock(current->mutex);
    const Frame& latest = current->latest;
    const bool covered = latest && latest.time.count() <= seconds &&
                         seconds < (latest.time + latest.duration).count();
    const bool pending = (current->queued || current->decoding) &&
                         current->requestedSeconds == seconds;
    if (!covered && !pending) {
      current->requestedSeconds = seconds;
      ++current->generation;
      if (!current->queued && !current->decoding) {
        current->queued = true;
        enqueue = true;
      }
    }
  }
  if (enqueue) {
    if (synchronous) {
      // No worker exists: the ask is the decode, on this thread.
      while (decodeOnce(*current)) {
      }
    } else {
      pushRequest(index);
    }
  }
  std::lock_guard lock(current->mutex);
  return current->latest;
}

bool Playback::Impl::ready(size_t index) const {
  const std::shared_ptr<Slot> current = slot(index);
  if (!current) return false;
  std::lock_guard lock(current->mutex);
  return static_cast<bool>(current->latest);
}

Playback::Playback(Options options)
    : m_impl(std::make_shared<Impl>(options)) {}

Playback::~Playback() = default;

}  // namespace sigil::media
