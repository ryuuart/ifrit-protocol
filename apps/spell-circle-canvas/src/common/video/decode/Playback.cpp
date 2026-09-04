#include "sigilvideo/decode/Playback.h"

#include <moodycamel/blockingconcurrentqueue.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "Device.h"

namespace sigil::video {

struct Playback::Impl {
  using JobQueue = moodycamel::BlockingConcurrentQueue<Handle>;

  struct Slot {
    explicit Slot(std::shared_ptr<Video> source) : video(std::move(source)) {}

    std::shared_ptr<Video> video;
    std::mutex decodeMutex;
    double requestedSeconds = 0.0;
    size_t generation = 0;
    bool queued = false;
    bool decoding = false;
    VideoFrame latest;

    std::mutex presentationMutex;
    std::weak_ptr<void> mappedStorage;
    skgpu::graphite::Recorder* mappedRecorder = nullptr;
    sk_sp<SkImage> mappedImage;
  };

  explicit Impl(const Options& options)
      : deviceContext(device::makeContext(options.metalDevice)) {
    size_t workers = options.workerThreads;
    if (workers == 0) {
      const unsigned concurrency = std::thread::hardware_concurrency();
      workers = std::clamp<size_t>(concurrency ? concurrency / 2 : 2, 2, 8);
    }
    threads.reserve(workers);
    for (size_t i = 0; i < workers; ++i)
      threads.emplace_back([this](std::stop_token stop) { work(stop); });
  }

  ~Impl() {
    for (std::jthread& thread : threads) thread.request_stop();
  }

  Slot* slot(Handle handle) const {
    return handle < slots.size() ? slots[handle].get() : nullptr;
  }

  void pushRequest(Handle handle) {
    std::lock_guard lock(requestProducerMutex);
    if (!jobs.enqueue(requestProducer, handle)) std::terminate();
  }

  void work(std::stop_token stop) {
    JobQueue::producer_token_t producer(jobs);
    JobQueue::consumer_token_t consumer(jobs);
    while (!stop.stop_requested()) {
      Handle handle = 0;
      if (!jobs.wait_dequeue_timed(consumer, handle,
                                   std::chrono::milliseconds(10)))
        continue;
      Slot* current = slot(handle);
      if (!current) continue;

      double seconds = 0.0;
      size_t generation = 0;
      {
        std::lock_guard lock(current->decodeMutex);
        current->queued = false;
        current->decoding = true;
        seconds = current->requestedSeconds;
        generation = current->generation;
      }
      VideoFrame decoded = current->video->decodeAt(seconds);

      bool again = false;
      {
        std::lock_guard lock(current->decodeMutex);
        current->latest = std::move(decoded);
        current->decoding = false;
        if (current->generation != generation && !current->queued) {
          current->queued = true;
          again = true;
        }
      }
      if (again && !jobs.enqueue(producer, handle)) std::terminate();
    }
  }

  JobQueue jobs;
  JobQueue::producer_token_t requestProducer{jobs};
  std::mutex requestProducerMutex;
  std::vector<std::unique_ptr<Slot>> slots;
  std::vector<std::jthread> threads;
  std::shared_ptr<device::Context> deviceContext;
};

Playback::Playback() : Playback(Options{}) {}

Playback::Playback(const Options& options)
    : m_impl(std::make_unique<Impl>(options)) {}

Playback::~Playback() = default;

Playback::Handle Playback::add(std::shared_ptr<Video> video) {
  const Handle handle = m_impl->slots.size();
  m_impl->slots.push_back(std::make_unique<Impl::Slot>(std::move(video)));
  return handle;
}

void Playback::request(Handle handle, double seconds) {
  Impl::Slot* slot = m_impl->slot(handle);
  if (!slot || !slot->video) return;
  bool enqueue = false;
  {
    std::lock_guard lock(slot->decodeMutex);
    if (slot->latest && slot->latest.presentationSeconds <= seconds &&
        seconds <
            slot->latest.presentationSeconds + slot->latest.durationSeconds)
      return;
    if ((slot->queued || slot->decoding) && slot->requestedSeconds == seconds)
      return;
    slot->requestedSeconds = seconds;
    ++slot->generation;
    if (!slot->queued && !slot->decoding) {
      slot->queued = true;
      enqueue = true;
    }
  }
  if (enqueue) m_impl->pushRequest(handle);
}

bool Playback::ready(Handle handle) const {
  Impl::Slot* slot = m_impl->slot(handle);
  if (!slot) return false;
  std::lock_guard lock(slot->decodeMutex);
  return static_cast<bool>(slot->latest);
}

VideoFrame Playback::frame(Handle handle, skgpu::graphite::Recorder* recorder) {
  Impl::Slot* slot = m_impl->slot(handle);
  if (!slot) return {};
  VideoFrame result;
  {
    std::lock_guard lock(slot->decodeMutex);
    result = slot->latest;
  }
  if (!result.native || result.image || !recorder || !m_impl->deviceContext)
    return result;

  std::lock_guard lock(slot->presentationMutex);
  const std::shared_ptr<void> mapped = slot->mappedStorage.lock();
  if (mapped != result.native.storage || slot->mappedRecorder != recorder) {
    slot->mappedImage = device::wrapNativeFrame(result.native, recorder,
                                                *m_impl->deviceContext);
    slot->mappedStorage = result.native.storage;
    slot->mappedRecorder = recorder;
  }
  result.image = slot->mappedImage;
  return result;
}

size_t Playback::size() const { return m_impl->slots.size(); }

}  // namespace sigil::video
