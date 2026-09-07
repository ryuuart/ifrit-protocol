/** @file
 * The fan-out for calls that block: plain threads, started for one batch
 * and joined before it returns.
 *
 * Nothing of the task runtime is named here, which is the whole point —
 * a fetch that waits twenty seconds for a server must not be holding a
 * worker some unrelated parallel range is waiting on.
 */

#include <sigilcore/schedule/ConcurrentIo.h>

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

namespace sigil::core::schedule {

namespace {

/** How many waits one core is given. A thread that is waiting for a disk
 *  or a server is off the processor while it waits, so the useful width
 *  is larger than the core count rather than equal to it. */
constexpr size_t kWaitsPerCore = 2;

}  // namespace

size_t concurrentIoWidth() {
  const unsigned reported = std::thread::hardware_concurrency();
  const size_t cores = reported == 0 ? 1 : reported;
  return cores * kWaitsPerCore;
}

void detail::overIoItems(size_t count, void* body, ItemBody run) {
  if (count == 0) return;
  const size_t width = std::min(count, concurrentIoWidth());

  std::atomic_size_t next{0};
  std::mutex failureMutex;
  std::exception_ptr failure;
  // Indices are taken rather than dealt out in advance: one item may wait
  // on a server and the next on a warm page cache, so a thread that
  // finished early takes the next item instead of the batch ending on
  // whichever thread drew the slowest ones.
  const auto drain = [&] {
    for (size_t index = next.fetch_add(1); index < count;
         index = next.fetch_add(1)) {
      try {
        run(body, index);
      } catch (...) {
        const std::lock_guard lock(failureMutex);
        if (!failure) failure = std::current_exception();
      }
    }
  };

  std::vector<std::thread> helpers;
  helpers.reserve(width - 1);
  for (size_t helper = 1; helper < width; ++helper) {
    try {
      helpers.emplace_back(drain);
    } catch (const std::system_error&) {
      // A machine that will not start another thread is not a failure of
      // the batch: the threads already started, and the calling thread
      // below, run every item between them.
      break;
    }
  }
  // The caller waits for this batch either way, so it takes a share of it
  // rather than parking on a join.
  drain();
  for (std::thread& helper : helpers) helper.join();

  if (failure) std::rethrow_exception(failure);
}

}  // namespace sigil::core::schedule
