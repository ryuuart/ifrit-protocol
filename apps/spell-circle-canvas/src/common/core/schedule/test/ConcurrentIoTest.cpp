/** @file
 * The blocking fan-out: every item handed out exactly once, items really
 * overlapping, the calling thread taking a share, and a failure that does
 * not cost the rest of the batch.
 */

#include <gtest/gtest.h>
#include <sigilcore/schedule/ConcurrentIo.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace sigil::core;

namespace {

TEST(ScheduleConcurrentIo, EveryItemRunsExactlyOnce) {
  const size_t count = schedule::concurrentIoWidth() * 4 + 3;
  std::vector<std::atomic_int> visits(count);
  for (std::atomic_int& visit : visits) visit = 0;

  schedule::concurrentIo(count, [&](size_t index) { ++visits[index]; });

  for (size_t i = 0; i != count; ++i) EXPECT_EQ(visits[i].load(), 1) << i;
}

TEST(ScheduleConcurrentIo, AnEmptyBatchNeverCallsTheBody) {
  int calls = 0;
  schedule::concurrentIo(size_t{0}, [&](size_t) { ++calls; });
  EXPECT_EQ(calls, 0);
}

/** Two items that each wait for the other to arrive are in flight at the
 *  same time, which is the property a caller with two things to wait on
 *  is here for. Each waits under its own deadline, so a machine that runs
 *  them one after the other reports a failed expectation rather than
 *  hanging. */
TEST(ScheduleConcurrentIo, ItemsAreInFlightTogether) {
  if (schedule::concurrentIoWidth() < 2) GTEST_SKIP() << "one wait at a time";

  std::atomic_int arrived{0};
  std::atomic_int sawBoth{0};
  schedule::concurrentIo(size_t{2}, [&](size_t) {
    ++arrived;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (arrived.load() < 2 && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    if (arrived.load() >= 2) ++sawBoth;
  });

  // Run one after the other, only the second item would find the first
  // had arrived; run together, each finds the other.
  EXPECT_EQ(sawBoth.load(), 2);
}

/** The caller waits for the batch either way, so it runs items rather
 *  than only joining: a batch of one starts no thread at all. */
TEST(ScheduleConcurrentIo, TheCallingThreadTakesAShare) {
  std::thread::id ran;
  schedule::concurrentIo(size_t{1},
                         [&](size_t) { ran = std::this_thread::get_id(); });
  EXPECT_EQ(ran, std::this_thread::get_id());
}

TEST(ScheduleConcurrentIo, ARangeOfItemsIsTheSameFanOut) {
  std::vector<std::string> uris;
  for (size_t i = 0; i != 40; ++i) uris.push_back("res://" + std::to_string(i));
  std::vector<std::string> fetched(uris.size());

  schedule::concurrentIo(uris.size(), [&](size_t index) {
    fetched[index] = uris[index] + "!";
  });

  for (size_t i = 0; i != uris.size(); ++i) EXPECT_EQ(fetched[i], uris[i] + "!");
}

/** One item's failure is not the batch's: the others still run, and the
 *  caller still learns. */
TEST(ScheduleConcurrentIo, AThrowingItemLeavesTheRestOfTheBatchRun) {
  constexpr size_t kCount = 64;
  std::vector<std::atomic_int> visits(kCount);
  for (std::atomic_int& visit : visits) visit = 0;

  const auto run = [&] {
    schedule::concurrentIo(kCount, [&](size_t index) {
      ++visits[index];
      if (index == kCount / 2) throw std::runtime_error("item");
    });
  };
  EXPECT_THROW(run(), std::runtime_error);
  for (size_t i = 0; i != kCount; ++i) EXPECT_EQ(visits[i].load(), 1) << i;
}

}  // namespace
