/** @file
 * The instruments a span of work is read with: the stopwatch, the laps
 * that tile one span between them, and the frame timer's three lanes.
 *
 * Exactly one case here reads the wall clock, and it owns every claim
 * that needs one. Every other timing claim is deterministic, because
 * sleeping and then asserting on a duration asserts the operating
 * system's scheduler rather than anything this library promises.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/advanced/FrameTimer.h>
#include <sigilmeasure/advanced/Laps.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace sigil::measure;
using namespace std::chrono_literals;

// The one case in this file that reads the wall clock. It is here so
// that every other timing claim below is deterministic: sleeping and then
// asserting on a duration asserts the operating system's scheduler, which
// is not this library's to promise.
TEST(Timing, TheClocksAdvanceWithRealTimeAndResetSendsThemBack) {
  Stopwatch watch;
  Laps laps;
  Duration scoped = -1s;
  {
    ScopedDuration timing(scoped);
    std::this_thread::sleep_for(2ms);
  }
  const Duration slept = watch.elapsed();
  EXPECT_GE(slept, 1ms);
  EXPECT_GE(laps.mark("slept"), 1ms);
  EXPECT_GE(scoped, 1ms);
  EXPECT_GE(timed([] { std::this_thread::sleep_for(2ms); }), 1ms);
  // Reset is only observable once a span has been measured, which is why
  // it is claimed here rather than beside the deterministic cases. A
  // stopwatch that ignored reset would only ever read higher, so the
  // comparison is against the span already measured and not against a
  // ceiling the scheduler could cross.
  watch.restart();
  EXPECT_LT(watch.elapsed(), slept);
}

TEST(Laps, MarksAreNamedInOrderAndSumToTheTotal) {
  // Consecutive marks tile the span exactly — the end of one is the start
  // of the next — so their sum IS the total, whatever the durations turn
  // out to be.
  Laps laps;
  const Duration layout = laps.mark("layout");
  const Duration paint = laps.mark("paint");
  std::vector<std::string> names;
  Duration sum{};
  laps.each([&](std::string_view name, Duration lap) {
    names.emplace_back(name);
    sum += lap;
  });
  EXPECT_EQ(names, (std::vector<std::string>{"layout", "paint"}));
  EXPECT_EQ(laps.size(), 2u);
  EXPECT_DOUBLE_EQ(sum.count(), (layout + paint).count());
  EXPECT_DOUBLE_EQ(laps.total().count(), sum.count());
}

// Whether `laps.mark(x)` compiles at all, for an argument of type T.
template <typename T, typename = void>
struct Marks : std::false_type {};
template <typename T>
struct Marks<
    T, std::void_t<decltype(std::declval<Laps&>().mark(std::declval<T>()))>>
    : std::true_type {};

TEST(Laps, RefuseANameThatWouldBeGoneBeforeItIsReadBack) {
  // The laps borrow their names, so a name built into a temporary would be
  // dangling by the time each() read it. That is a compile-time refusal,
  // not a value to assert on: what a caller may hand mark() is exactly a
  // name that outlives the timer.
  static_assert(Marks<const char*>::value, "a literal names a phase");
  static_assert(Marks<std::string_view>::value, "so does a view of one");
  static_assert(Marks<const std::string&>::value, "and a string it keeps");
  static_assert(!Marks<std::string>::value,
                "a string built into the call dies at the semicolon");
  static_assert(!Marks<std::string&&>::value,
                "and so does one that was moved from");

  // The borrowed name is read back, not a copy taken at the mark.
  std::string phase = "layout";
  Laps laps;
  laps.mark(phase);
  phase[0] = 'L';
  std::string seen;
  laps.each([&](std::string_view name, Duration) { seen = name; });
  EXPECT_EQ(seen, "Layout");
}

TEST(Laps, ResetForgetsTheMarksAndStartsThePhaseThere) {
  Laps laps;
  laps.mark("layout");
  laps.mark("paint");
  laps.reset();
  EXPECT_EQ(laps.size(), 0u);
  EXPECT_DOUBLE_EQ(laps.total().count(), 0.0);
  laps.mark("again");
  EXPECT_EQ(laps.size(), 1u);
}

TEST(ScopedDuration, LeavesItsTargetUntouchedUntilScopeExit) {
  // The target is assigned at destruction, not accumulated as the block
  // runs, so a caller may read the previous run's number until then.
  Duration spent = -1s;
  {
    ScopedDuration timing(spent);
    EXPECT_EQ(spent, -1s);
  }
  EXPECT_GE(spent, 0s);
}

TEST(FrameTimer, MarksFeedTheirLanes) {
  FrameTimer timer({.frames = 4});
  timer.presented();  // seeds only
  EXPECT_TRUE(timer.present().empty());
  timer.begin();
  timer.composed();
  timer.finished();
  timer.presented();
  EXPECT_EQ(timer.work().size(), 1u);
  EXPECT_EQ(timer.frame().size(), 1u);
  EXPECT_EQ(timer.present().size(), 1u);
  EXPECT_GE(timer.frame().latest(), timer.work().latest());
  timer.resetPresentation();
  EXPECT_TRUE(timer.present().empty());
  EXPECT_EQ(timer.work().size(), 1u);
  timer.reset();
  EXPECT_TRUE(timer.work().empty());
}

TEST(FrameTimer, HeadroomIsTheWorkCeiling) {
  FrameTimer timer;
  EXPECT_DOUBLE_EQ(timer.headroomFps(), 0.0);
  EXPECT_DOUBLE_EQ(timer.presentedFps(), 0.0);
  timer.addWork(4ms);
  timer.addFrame(8ms);
  timer.addPresent(20ms);
  EXPECT_DOUBLE_EQ(timer.headroomFps(), 250.0);
  EXPECT_DOUBLE_EQ(timer.presentedFps(), 50.0);
}

TEST(FrameTimer, AnIntervalAsLongAsAPauseIsNotAFrame) {
  FrameTimer timer({.pause = 1s});
  timer.addPresent(16ms);
  timer.addPresent(3s);  // a window drag
  EXPECT_EQ(timer.present().size(), 1u);
}

TEST(FrameTimer, TheSampleIsTheLanesAsPlainValues) {
  FrameTimer timer({.frames = 100});
  for (int frame = 1; frame <= 100; ++frame) {
    timer.addWork(std::chrono::duration<double, std::milli>(frame));
    timer.addFrame(std::chrono::duration<double, std::milli>(frame * 2.0));
  }
  const FrameSample sample = timer.sample();
  EXPECT_DOUBLE_EQ(Milliseconds(sample.work).count(), 50.5);
  EXPECT_DOUBLE_EQ(Milliseconds(sample.frame).count(), 101.0);
  EXPECT_NEAR(Milliseconds(sample.frameTail).count(), 198.02, 1e-9);
  EXPECT_DOUBLE_EQ(sample.headroomFps, 1000.0 / 50.5);
  timer.reset();
  EXPECT_DOUBLE_EQ(Milliseconds(sample.work).count(), 50.5);
}
