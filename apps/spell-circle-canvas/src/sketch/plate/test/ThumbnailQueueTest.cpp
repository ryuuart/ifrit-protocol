/** @file
 * The order stills are drawn in: what an ask does to the queue, what a
 * withdrawal does, and what is still true after the fill is ended.
 *
 * Every case here supplies its own render, so what is asserted is the
 * ORDER and nothing about what a sketch costs to photograph. The renders
 * below block on a latch rather than sleep: a case that waited on a clock
 * would be asserting how fast this machine is.
 */

#include <gtest/gtest.h>
#include <sigilsketch/plate/ThumbnailQueue.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

namespace {

using namespace sigil::sketch;

/** A gate a render waits at, so a case decides when one still ends. */
class Latch {
 public:
  void open() {
    {
      const std::lock_guard lock(m_mutex);
      m_open = true;
    }
    m_wake.notify_all();
  }
  void wait() {
    std::unique_lock lock(m_mutex);
    m_wake.wait(lock, [this] { return m_open; });
  }

 private:
  std::mutex m_mutex;
  std::condition_variable m_wake;
  bool m_open = false;
};

/** What every case reads back: which indices were rendered, in order,
 *  and what each was reported as. */
struct Log {
  std::mutex mutex;
  std::condition_variable wake;
  std::vector<int> rendered;
  std::vector<int> reported;

  void note(std::vector<int>& into, int index) {
    {
      const std::lock_guard lock(mutex);
      into.push_back(index);
    }
    wake.notify_all();
  }
  /** Waits until @p which holds @p count entries. A queue answers on a
   *  thread of its own, so every assertion about what it did waits for
   *  the doing rather than for a duration. */
  void until(const std::vector<int>& which, size_t count) {
    std::unique_lock lock(mutex);
    wake.wait(lock, [&] { return which.size() >= count; });
  }
  std::vector<int> renderedSoFar() {
    const std::lock_guard lock(mutex);
    return rendered;
  }
};

TEST(ThumbnailQueue, DrawsAFillInTheOrderItWasGiven) {
  Log log;
  ThumbnailQueue queue(
      [&log](int index, const std::atomic_bool&) {
        log.note(log.rendered, index);
        return ThumbnailOutcome::Wrote;
      },
      [&log](int index, ThumbnailOutcome, int) {
        log.note(log.reported, index);
      });
  queue.fill({4, 5, 6});
  log.until(log.reported, 3);
  EXPECT_EQ(log.renderedSoFar(), (std::vector<int>{4, 5, 6}));
}

/** A ROW ON SCREEN JUMPS THE QUEUE: what a reader is looking at is what
 *  the one render after the current one should be spent on. */
TEST(ThumbnailQueue, AnAskGoesToTheFrontOfTheQueue) {
  Log log;
  Latch first;
  ThumbnailQueue queue(
      [&](int index, const std::atomic_bool&) {
        log.note(log.rendered, index);
        if (index == 1) first.wait();
        return ThumbnailOutcome::Wrote;
      },
      [&log](int index, ThumbnailOutcome, int) {
        log.note(log.reported, index);
      });
  queue.fill({1, 2, 3});
  log.until(log.rendered, 1);  // 1 is in flight and waiting
  EXPECT_TRUE(queue.request(3));
  first.open();
  log.until(log.reported, 3);
  EXPECT_EQ(log.renderedSoFar(), (std::vector<int>{1, 3, 2}));
}

/** An ask for what is already being drawn is not a second ask for it. */
TEST(ThumbnailQueue, TheOneInFlightIsNotQueuedAgain) {
  Log log;
  Latch running;
  ThumbnailQueue queue(
      [&](int index, const std::atomic_bool&) {
        log.note(log.rendered, index);
        running.wait();
        return ThumbnailOutcome::Wrote;
      },
      [&log](int index, ThumbnailOutcome, int) {
        log.note(log.reported, index);
      });
  queue.fill({7});
  log.until(log.rendered, 1);
  EXPECT_FALSE(queue.request(7));
  EXPECT_EQ(queue.inFlight(), 7);
  running.open();
  log.until(log.reported, 1);
  EXPECT_EQ(log.renderedSoFar(), (std::vector<int>{7}));
}

/** A row that scrolled away withdraws its ask, so the one render is spent
 *  on what is still on screen. */
TEST(ThumbnailQueue, AWithdrawnAskIsNotDrawn) {
  Log log;
  Latch first;
  ThumbnailQueue queue(
      [&](int index, const std::atomic_bool&) {
        log.note(log.rendered, index);
        if (index == 1) first.wait();
        return ThumbnailOutcome::Wrote;
      },
      [&log](int index, ThumbnailOutcome, int) {
        log.note(log.reported, index);
      });
  queue.fill({1, 2, 3});
  log.until(log.rendered, 1);
  queue.cancel(2);
  EXPECT_EQ(queue.pending(), 1u);
  first.open();
  log.until(log.reported, 2);
  EXPECT_EQ(log.renderedSoFar(), (std::vector<int>{1, 3}));
}

/** A WALK ALREADY IN FLIGHT IS LEFT TO FINISH: a still is a walk from
 *  zero, so abandoning one that is nearly done spends the whole of its
 *  cost for nothing. */
TEST(ThumbnailQueue, WithdrawingTheOneInFlightDoesNotStopIt) {
  Log log;
  Latch running;
  ThumbnailQueue queue(
      [&](int index, const std::atomic_bool&) {
        log.note(log.rendered, index);
        running.wait();
        return ThumbnailOutcome::Wrote;
      },
      [&log](int index, ThumbnailOutcome, int) {
        log.note(log.reported, index);
      });
  queue.fill({9});
  log.until(log.rendered, 1);
  queue.cancel(9);
  EXPECT_EQ(queue.inFlight(), 9);
  running.open();
  log.until(log.reported, 1);
}

/** ENDING THE FILL RAISES THE STOP THE RENDER READS, and drops everything
 *  behind it: opening a sketch means the canvas is what draws now. */
TEST(ThumbnailQueue, EndingTheFillLetsGoOfTheWalkAndTheQueue) {
  Log log;
  Latch ended;
  std::atomic_bool sawStop{false};
  ThumbnailQueue queue(
      [&](int index, const std::atomic_bool& stop) {
        log.note(log.rendered, index);
        ended.wait();
        sawStop.store(stop.load());
        return ThumbnailOutcome::Stopped;
      },
      [&log](int index, ThumbnailOutcome, int) {
        log.note(log.reported, index);
      });
  queue.fill({1, 2, 3});
  log.until(log.rendered, 1);
  queue.endFill();
  ended.open();
  queue.stop();
  EXPECT_TRUE(sawStop.load());
  EXPECT_TRUE(queue.ended());
  EXPECT_EQ(queue.pending(), 0u);
  // A stopped walk is not reported: nothing is on disk and nothing is
  // known about the sketch.
  EXPECT_EQ(log.renderedSoFar(), (std::vector<int>{1}));
  const std::lock_guard lock(log.mutex);
  EXPECT_TRUE(log.reported.empty());
}

/** After the fill is ended nothing is queued again, whoever asks. */
TEST(ThumbnailQueue, NothingIsQueuedAfterTheFillEnds) {
  Log log;
  ThumbnailQueue queue(
      [&log](int index, const std::atomic_bool&) {
        log.note(log.rendered, index);
        return ThumbnailOutcome::Wrote;
      },
      nullptr);
  queue.endFill();
  EXPECT_FALSE(queue.request(2));
  queue.fill({3, 4});
  queue.stop();
  EXPECT_TRUE(log.renderedSoFar().empty());
}

/** A SKETCH THIS HOST COULD NOT DRAW IS NOT ASKED AGAIN. A failure says
 *  nothing about how long the sketch takes, only that it did not come
 *  out — and asking again would cost the same and answer the same. */
TEST(ThumbnailQueue, AFailedSketchIsNeverRetried) {
  Log log;
  ThumbnailQueue queue(
      [&log](int index, const std::atomic_bool&) {
        log.note(log.rendered, index);
        return ThumbnailOutcome::Failed;
      },
      [&log](int index, ThumbnailOutcome, int) {
        log.note(log.reported, index);
      });
  queue.fill({5});
  log.until(log.reported, 1);
  EXPECT_FALSE(queue.request(5));
  queue.fill({5});
  queue.stop();
  EXPECT_EQ(log.renderedSoFar(), (std::vector<int>{5}));
}

/** LEAVING IS NOT WAITING FOR A PICTURE: the destructor lets the walk go
 *  and joins, rather than waiting out a walk that can have minutes left
 *  in it. */
TEST(ThumbnailQueue, GoingAwayLetsGoOfTheWalkInFlight) {
  Log log;
  std::atomic_bool stopped{false};
  {
    ThumbnailQueue queue(
        [&](int index, const std::atomic_bool& stop) {
          log.note(log.rendered, index);
          // The walk the destructor is about to let go of: it reads the
          // stop between frames, exactly as a still's frame loop does.
          while (!stop.load()) {
          }
          stopped.store(true);
          return ThumbnailOutcome::Stopped;
        },
        nullptr);
    queue.fill({1});
    log.until(log.rendered, 1);
  }
  EXPECT_TRUE(stopped.load());
}

}  // namespace
