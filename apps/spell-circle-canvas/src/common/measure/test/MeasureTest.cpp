#include <gtest/gtest.h>
#include <sigilmeasure/Measure.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace sigil::measure;

// ---------------------------------------------------------------------------
// quantile

TEST(Quantile, EmptyReadsZero) { EXPECT_DOUBLE_EQ(quantile({}, 0.5), 0.0); }

TEST(Quantile, OneSampleReadsItselfEverywhere) {
  const double one[] = {7.5};
  EXPECT_DOUBLE_EQ(quantile(one, 0.0), 7.5);
  EXPECT_DOUBLE_EQ(quantile(one, 0.5), 7.5);
  EXPECT_DOUBLE_EQ(quantile(one, 1.0), 7.5);
}

TEST(Quantile, InterpolatesBetweenRanks) {
  const double four[] = {4, 1, 3, 2};  // unsorted on purpose
  EXPECT_DOUBLE_EQ(quantile(four, 0.0), 1.0);
  EXPECT_DOUBLE_EQ(quantile(four, 0.5), 2.5);
  EXPECT_DOUBLE_EQ(quantile(four, 1.0), 4.0);
  EXPECT_DOUBLE_EQ(quantile(four, 1.0 / 3.0), 2.0);
  EXPECT_NEAR(quantile(four, 0.99), 3.97, 1e-9);
}

TEST(Quantile, ClampsTheFraction) {
  const double two[] = {1, 2};
  EXPECT_DOUBLE_EQ(quantile(two, -1.0), 1.0);
  EXPECT_DOUBLE_EQ(quantile(two, 2.0), 2.0);
}

TEST(Quantile, LeavesTheInputAlone) {
  std::vector<double> v = {3, 1, 2};
  (void)quantile(v, 0.5);
  EXPECT_EQ(v, (std::vector<double>{3, 1, 2}));
}

// ---------------------------------------------------------------------------
// Samples

TEST(Samples, EmptyReadsZeroEverywhere) {
  Samples w(4);
  EXPECT_TRUE(w.empty());
  EXPECT_EQ(w.size(), 0u);
  EXPECT_DOUBLE_EQ(w.mean(), 0.0);
  EXPECT_DOUBLE_EQ(w.percentile(0.99), 0.0);
  EXPECT_DOUBLE_EQ(w.min(), 0.0);
  EXPECT_DOUBLE_EQ(w.max(), 0.0);
  EXPECT_DOUBLE_EQ(w.last(), 0.0);
}

TEST(Samples, WrapsAroundKeepingTheNewest) {
  Samples w(3);
  for (double v : {1.0, 2.0, 3.0, 4.0, 5.0}) w.add(v);
  EXPECT_EQ(w.size(), 3u);
  EXPECT_EQ(w.capacity(), 3u);
  EXPECT_EQ(w.samples(), (std::vector<double>{3, 4, 5}));
  EXPECT_DOUBLE_EQ(w.mean(), 4.0);
  EXPECT_DOUBLE_EQ(w.min(), 3.0);
  EXPECT_DOUBLE_EQ(w.max(), 5.0);
  EXPECT_DOUBLE_EQ(w.last(), 5.0);
  EXPECT_DOUBLE_EQ(w.percentile(0.5), 4.0);
}

TEST(Samples, OrderSurvivesExactlyOneLap) {
  Samples w(3);
  for (double v : {1.0, 2.0, 3.0}) w.add(v);
  EXPECT_EQ(w.samples(), (std::vector<double>{1, 2, 3}));
  w.add(4.0);
  EXPECT_EQ(w.samples(), (std::vector<double>{2, 3, 4}));
  EXPECT_DOUBLE_EQ(w.last(), 4.0);
}

TEST(Samples, ClearForgetsButKeepsCapacity) {
  Samples w(2);
  w.add(1.0);
  w.clear();
  EXPECT_TRUE(w.empty());
  EXPECT_EQ(w.capacity(), 2u);
  w.add(9.0);
  EXPECT_DOUBLE_EQ(w.last(), 9.0);
}

TEST(Samples, ZeroCapacityHoldsOne) {
  Samples w(0);
  w.add(1.0);
  w.add(2.0);
  EXPECT_EQ(w.size(), 1u);
  EXPECT_DOUBLE_EQ(w.last(), 2.0);
}

// ---------------------------------------------------------------------------
// Counters

TEST(Counters, AddsGetsAndReadsUnknownAsZero) {
  Counters c;
  EXPECT_EQ(c.get("never"), 0);
  c.add("a");
  c.add("a", 4);
  c.add("b", -2);
  EXPECT_EQ(c.get("a"), 5);
  EXPECT_EQ(c.get("b"), -2);
  EXPECT_EQ(c.size(), 2u);
}

TEST(Counters, EachVisitsInNameOrderAndResetKeepsNames) {
  Counters c;
  c.add("zeta", 3);
  c.add("alpha", 1);
  std::vector<std::string> names;
  std::vector<int64_t> counts;
  c.each([&](std::string_view n, int64_t v) {
    names.emplace_back(n);
    counts.push_back(v);
  });
  EXPECT_EQ(names, (std::vector<std::string>{"alpha", "zeta"}));
  EXPECT_EQ(counts, (std::vector<int64_t>{1, 3}));
  c.reset();
  EXPECT_EQ(c.size(), 2u);
  EXPECT_EQ(c.get("zeta"), 0);
  c.clear();
  EXPECT_EQ(c.size(), 0u);
}

// ---------------------------------------------------------------------------
// Stopwatch, Laps and FrameTimer

TEST(Stopwatch, ElapsedGrowsAndResets) {
  Stopwatch sw;
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  const double elapsed = sw.elapsedMs();
  EXPECT_GE(elapsed, 1.0);
  sw.reset();
  EXPECT_LT(sw.elapsedMs(), elapsed);
}

TEST(Laps, MarksTileTheSpanInOrder) {
  Laps laps;
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  const double layout = laps.mark("layout");
  const double paint = laps.mark("paint");
  EXPECT_GE(layout, 1.0);
  EXPECT_GE(paint, 0.0);
  EXPECT_LT(paint, layout);
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
  laps.reset();
  EXPECT_EQ(laps.size(), 0u);
  EXPECT_LT(laps.mark("again"), layout);
}

TEST(ScopedMs, AssignsAtScopeExit) {
  double ms = -1.0;
  {
    ScopedMs timed(ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_DOUBLE_EQ(ms, -1.0);
  }
  EXPECT_GE(ms, 0.5);
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

// ---------------------------------------------------------------------------
// Check

TEST(Check, IntegralIdentity) {
  const Check c = check("pieces", 12, 12);
  EXPECT_TRUE(c.pass);
  EXPECT_EQ(c.line(10, 4), "  pieces       12   PASS");
  const Check bad = check("pieces", 12, 11L);
  EXPECT_FALSE(bad.pass);
  EXPECT_EQ(bad.line(10, 4), "  pieces       11   FAIL want 12");
}

TEST(Check, ToleranceClausePrintsTheBand) {
  const Check ok = check("radius", 257.972, 257.9725, 0.001);
  EXPECT_TRUE(ok.pass);
  EXPECT_EQ(ok.expected, "257.972 \xc2\xb1 0.001");
  const Check bad = check("radius", 1.0, 1.5, 0.25);
  EXPECT_FALSE(bad.pass);
  EXPECT_EQ(bad.line(8, 4), "  radius    1.5   FAIL want 1 \xc2\xb1 0.25");
}

TEST(Check, StringsAndBools) {
  EXPECT_TRUE(check("name", std::string_view("a"), std::string_view("a")).pass);
  EXPECT_FALSE(
      check("name", std::string_view("a"), std::string_view("b")).pass);
  const Check b = check("closed", false);
  EXPECT_EQ(b.actual, "false");
  EXPECT_EQ(b.expected, "true");
  EXPECT_FALSE(b.pass);
}

TEST(Check, LongLabelsPushTheColumnRatherThanClip) {
  const Check c = check("a label longer than its column", 1, 1);
  EXPECT_EQ(c.line(4, 2), "  a label longer than its column  1   PASS");
}

TEST(Check, FailuresCountsAndTableSummarises) {
  Table t;
  t.add(check("a", 1, 1)).add(check("b", 1, 2)).add(check("c", true));
  EXPECT_EQ(t.failures(), 1);
  EXPECT_FALSE(t.pass());
  EXPECT_EQ(failures(t.rows), 1);
  const std::vector<std::string> lines = t.lines(4, 2);
  ASSERT_EQ(lines.size(), 4u);
  EXPECT_EQ(lines[0], "  a     1   PASS");
  EXPECT_EQ(lines[1], "  b     2   FAIL want 1");
  EXPECT_EQ(lines[2], "  c    true   PASS");
  EXPECT_EQ(lines[3], "  3 checks, 1 failed");
  Table all;
  all.add(check("x", 2, 2));
  EXPECT_EQ(all.lines().back(), "  1 checks, all passed");
  EXPECT_TRUE(Table{}.lines().empty());
}
