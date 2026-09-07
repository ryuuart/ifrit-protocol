/** @file
 * What a divided range promises: every item run exactly once, the grain
 * alone deciding whether anything leaves the calling thread, and a body's
 * exception reaching the caller.
 */

#include <gtest/gtest.h>
#include <sigilcore/schedule/Parallel.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace sigil::core;

namespace {

/** Every chunk the body was called with, and by which thread. */
struct Chunks {
  std::mutex mutex;
  std::vector<std::pair<size_t, size_t>> ranges;
  std::vector<std::thread::id> threads;

  void record(size_t first, size_t last) {
    const std::lock_guard lock(mutex);
    ranges.emplace_back(first, last);
    threads.push_back(std::this_thread::get_id());
  }
};

TEST(ScheduleParallel, ChunksCoverTheRangeExactlyOnce) {
  constexpr size_t kCount = 10'000;
  std::vector<int> visits(kCount, 0);
  Chunks chunks;

  schedule::parallelFor(kCount, size_t{64}, [&](size_t first, size_t last) {
    EXPECT_LT(first, last);
    EXPECT_LE(last, kCount);
    chunks.record(first, last);
    for (size_t i = first; i != last; ++i) visits[i] = 1;
  });

  EXPECT_EQ(std::accumulate(visits.begin(), visits.end(), 0), (int)kCount);
  // A range larger than one grain is divided, which is the whole of the
  // rule: no separate count decides it.
  EXPECT_GT(chunks.ranges.size(), 1u);

  std::vector<int> covered(kCount, 0);
  for (const auto& [first, last] : chunks.ranges)
    for (size_t i = first; i != last; ++i) ++covered[i];
  for (size_t i = 0; i != kCount; ++i) EXPECT_EQ(covered[i], 1) << "item " << i;
}

TEST(ScheduleParallel, ARangeNoLargerThanTheGrainStaysOnTheCaller) {
  Chunks chunks;
  schedule::parallelFor(
      size_t{4096}, size_t{4096},
      [&](size_t first, size_t last) { chunks.record(first, last); });

  ASSERT_EQ(chunks.ranges.size(), 1u);
  EXPECT_EQ(chunks.ranges.front().first, 0u);
  EXPECT_EQ(chunks.ranges.front().second, 4096u);
  EXPECT_EQ(chunks.threads.front(), std::this_thread::get_id());
}

TEST(ScheduleParallel, AnEmptyRangeNeverCallsTheBodyAndAZeroGrainIsOneItem) {
  int calls = 0;
  schedule::parallelFor(size_t{0}, size_t{16},
                        [&](size_t, size_t) { ++calls; });
  EXPECT_EQ(calls, 0);

  std::vector<int> visits(64, 0);
  schedule::parallelFor(size_t{64}, size_t{0}, [&](size_t first, size_t last) {
    for (size_t i = first; i != last; ++i) visits[i] = 1;
  });
  EXPECT_EQ(std::accumulate(visits.begin(), visits.end(), 0), 64);
}

/** The count and the grain are the caller's own integer type, because a
 *  group index is one and a point index is another. */
TEST(ScheduleParallel, AnUnsignedIndexOtherThanSizeTypeIsCarriedThrough) {
  std::atomic_uint32_t seen{0};
  schedule::parallelFor(
      uint32_t{1000}, uint32_t{8},
      [&](uint32_t first, uint32_t last) { seen += last - first; });
  EXPECT_EQ(seen.load(), 1000u);
}

TEST(ScheduleParallel, ForEachRunsOverEveryElement) {
  std::vector<int> values(5000);
  std::iota(values.begin(), values.end(), 0);

  schedule::parallelForEach(values, 32, [](int& value) { value *= 2; });

  for (size_t i = 0; i != values.size(); ++i) EXPECT_EQ(values[i], (int)i * 2);
}

TEST(ScheduleParallel, ABodyThatThrowsReachesTheCaller) {
  const auto run = [] {
    schedule::parallelFor(size_t{10'000}, size_t{64}, [](size_t first, size_t) {
      if (first == 0) throw std::runtime_error("body");
    });
  };
  EXPECT_THROW(run(), std::runtime_error);
}

}  // namespace
