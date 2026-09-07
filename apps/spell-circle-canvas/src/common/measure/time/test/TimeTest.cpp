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
#include <sigilmeasure/time/FrameTimer.h>
#include <sigilmeasure/time/Laps.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace sigil::measure;

// The one case in this file that reads the wall clock. It is here so
// that every other timing claim below is deterministic: sleeping and then
// asserting on a duration asserts the operating system's scheduler, which
// is not this library's to promise.
TEST(Timing, TheClocksAdvanceWithRealTimeAndResetSendsThemBack) {
  Stopwatch sw;
  Laps laps;
  double scoped = -1.0;
  {
    ScopedMs timed(scoped);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  const double slept = sw.elapsedMs();
  EXPECT_GE(slept, 1.0);
  EXPECT_GE(laps.mark("slept"), 1.0);
  EXPECT_GE(scoped, 1.0);
  // Reset is only observable once a span has been measured, which is why
  // it is claimed here rather than beside the deterministic cases. A
  // stopwatch that ignored reset would only ever read higher, so the
  // comparison is against the span already measured and not against a
  // ceiling the scheduler could cross.
  sw.reset();
  EXPECT_LT(sw.elapsedMs(), slept);
}

TEST(Laps, MarksAreNamedInOrderAndSumToTheTotal) {
  // Consecutive marks tile the span exactly — the end of one is the start
  // of the next — so their sum IS the total, whatever the durations turn
  // out to be.
  Laps laps;
  const double layout = laps.mark("layout");
  const double paint = laps.mark("paint");
  std::vector<std::string> names;
  double sum = 0.0;
  laps.each([&](std::string_view n, double ms) {
    names.emplace_back(n);
    sum += ms;
  });
  EXPECT_EQ(names, (std::vector<std::string>{"layout", "paint"}));
  EXPECT_EQ(laps.size(), 2u);
  EXPECT_DOUBLE_EQ(sum, layout + paint);
  EXPECT_DOUBLE_EQ(laps.totalMs(), sum);
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
  laps.each([&](std::string_view n, double) { seen = n; });
  EXPECT_EQ(seen, "Layout");
}

TEST(Laps, ResetForgetsTheMarksAndStartsThePhaseThere) {
  Laps laps;
  laps.mark("layout");
  laps.mark("paint");
  laps.reset();
  EXPECT_EQ(laps.size(), 0u);
  EXPECT_DOUBLE_EQ(laps.totalMs(), 0.0);
  laps.mark("again");
  EXPECT_EQ(laps.size(), 1u);
}

TEST(ScopedMs, LeavesItsTargetUntouchedUntilScopeExit) {
  // The target is assigned at destruction, not accumulated as the block
  // runs, so a caller may read the previous run's number until then.
  double ms = -1.0;
  {
    ScopedMs timed(ms);
    EXPECT_DOUBLE_EQ(ms, -1.0);
  }
  EXPECT_GE(ms, 0.0);
  EXPECT_NE(ms, -1.0);
}

TEST(FrameTimer, MarksFeedTheirLanes) {
  FrameTimer t(4);
  t.presented();  // seeds only
  EXPECT_TRUE(t.present().empty());
  t.begin();
  t.composed();
  t.finished();
  t.presented();
  EXPECT_EQ(t.work().size(), 1u);
  EXPECT_EQ(t.frame().size(), 1u);
  EXPECT_EQ(t.present().size(), 1u);
  EXPECT_GE(t.frame().last(), t.work().last());
  t.resetPresentation();
  EXPECT_TRUE(t.present().empty());
  EXPECT_EQ(t.work().size(), 1u);
  t.reset();
  EXPECT_TRUE(t.work().empty());
}

TEST(FrameTimer, HeadroomIsTheWorkCeiling) {
  FrameTimer t;
  EXPECT_DOUBLE_EQ(t.headroomFps(), 0.0);
  EXPECT_DOUBLE_EQ(t.presentedFps(), 0.0);
  t.addWork(4.0);
  t.addFrame(8.0);
  t.addPresent(20.0);
  EXPECT_DOUBLE_EQ(t.headroomFps(), 250.0);
  EXPECT_DOUBLE_EQ(t.presentedFps(), 50.0);
}
